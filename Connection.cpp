#include "Connection.hpp"

#include <cstring>
#include <endian.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <ctime>
#include <unistd.h>

#include "Utils.hpp"

#include <iostream>
using namespace std;

#ifdef VERBOSE_DEBUG
#include <iostream>
using namespace std;
#define PRINT_DEBUG(str) { std::cerr << str << std::endl; }
#else
// will be optimized out
#define PRINT_DEBUG(str) {  }
#endif

#define MTLock(name, mut) std::lock_guard<typeof(mut)> name(mut);

bool DowowNetwork::Connection::HasSomethingToSend() {
    MTLock(__msq, mutex_sq);
    return
        send_queue.size() ||
        send_buffer_length != send_buffer_offset;
}

void DowowNetwork::Connection::ConnThreadFunc(Connection* c) {
    while (true) {
        // lock connected/disconnecting mutex
        c->mutex_id.lock();
        // create the fds for poll
        pollfd pollfds[5];
        // add 'to_stop' event
        pollfds[0].fd = c->to_stop_event;
        pollfds[0].events = POLLIN;
        // add socket fd
        // remark:  we wait for input only if not disconnecting,
        //          we wait for output only if we have something
        //          to send.
        pollfds[1].fd = c->socket_fd;
        pollfds[1].events =
            (!c->is_disconnecting ? POLLIN : 0) |
            (c->HasSomethingToSend() ? POLLOUT : 0);
        // our still-alive timer
        pollfds[2].fd = c->our_sa_timer;
        pollfds[2].events = POLLIN;
        // their not-alive timer
        pollfds[3].fd = c->their_na_timer;
        pollfds[3].events = POLLIN;
        // push event
        pollfds[4].fd = c->push_event;
        pollfds[4].events = POLLIN;
        // unlock connected/disconnecting mutex
        c->mutex_id.unlock();

        // let's poll!
        poll(
            pollfds,
            sizeof(pollfds) / sizeof(pollfds[0]),
            -1);

        // ***************
        // 'to_stop' event
        // ***************
        if (pollfds[0].revents & POLLIN) {
            uint64_t void_val;
            read(pollfds[0].fd, &void_val, sizeof(void_val));
            break;
        }

        // *************
        // socket events
        // *************
        if (pollfds[1].revents & POLLIN) {
            if (!c->Receive()) {
                // error
                break;
            }
        }
        if (pollfds[1].revents & POLLOUT) {
            if (!c->Send()) {
                // error
                break;
            }
        }
        // *********************
        // our still-alive event
        // *********************
        if (pollfds[2].revents & POLLIN) {
            // packet for bandwidth generation
            Request *keep_alive = new Request("_");
            c->Push(keep_alive, false, 0, false);

            // read the value (or else the timer will break)
            Utils::ReadEventFd(pollfds[0].fd, -1);
            // update the timeout
            Utils::SetTimerFdTimeout(
                c->our_sa_timer,
                c->our_sa_interval);
        }
        // *********************
        // their not-alive event
        // *********************
        if (pollfds[3].revents & POLLIN) {
            // close, they're timed out.
            break;
        }
        // **********
        // push event
        // **********
        if (pollfds[4].revents & POLLIN) {
            uint64_t void_val;
            read(pollfds[4].fd, &void_val, sizeof(void_val));
        }
    }

    // lock the bt mutex
    MTLock(__mbt, c->mutex_bt);

    // notify the Pull() callers that the receive is finished
    c->mutex_re.lock();
    for (auto fd: c->receive_events) {
        Utils::WriteEventFd(fd, 1);
    }
    c->mutex_re.unlock();

    // lock the reference counter
    c->mutex_ra.lock();
    // still referenced by external code, setup event
    if (c->GetRefs())
        c->not_needed_event = eventfd(0, 0);
    // unlock the reference counter
    c->mutex_ra.unlock();

    // needed by someone
    if (c->not_needed_event != -1) {
        // wait until the Connection is not needed anymore
        Utils::SelectRead(c->not_needed_event, -1);

        // close the event
        close(c->not_needed_event);
        c->not_needed_event = -1;
    }

    // shutdown and close the socket
    shutdown(c->socket_fd, SHUT_RDWR);
    close(c->socket_fd);

    // delete buffers
    c->DeleteSendBuffer();
    c->DeleteRecvBuffer();

    // delete the send queue
    c->mutex_sq.lock();
    while (c->send_queue.size()) {
        delete c->send_queue.front();
        c->send_queue.pop();
    }
    c->mutex_sq.unlock();

    // mark as undefined
    c->socket_type = SocketTypeUndefined;

    // notify about stop
    Utils::WriteEventFd(c->stopped_event, 1);
}

bool DowowNetwork::Connection::PassThroughHandlers(Request* r) {
    // no request
    if (!r) return false;

    // get the named handler
    auto h = GetHandlerNamed(r->GetName());
    // try to handle
    if (h) {
        if (mt_handlers) {
            std::thread t(HandlerBootstrapper, this, h, r);
            t.detach();
        } else {
            (*h)(this, r);
        }
        return true;
    }

    // default handler
    h = GetHandlerDefault();

    // couldn't handle using named handler, using default
    if (h) {
        if (mt_handlers) {
            std::thread t(HandlerBootstrapper, this, h, r);
            t.detach();
        } else {
            (*h)(this, r);
        }
        return true;
    }
    
    return false;
}

void DowowNetwork::Connection::HandlerBootstrapper(Connection *c, RequestHandler h, Request *r) {
    // TODO: code for event setup
    
    (*h)(c, r);
    delete r;

    // TODO: code for event signalling
}

DowowNetwork::Connection::Connection() {
    DeleteSendBuffer();
    DeleteRecvBuffer();

    // create eventfds
    stopped_event = eventfd(0, 0);
    push_event = eventfd(0, 0);
    to_stop_event = eventfd(0, 0);
    // create the timers for keep-alive mechanism
    our_sa_timer = timerfd_create(CLOCK_MONOTONIC, 0);
    their_na_timer = timerfd_create(CLOCK_MONOTONIC, 0);
}

void DowowNetwork::Connection::DeleteSendBuffer() {
    delete[] send_buffer;
    send_buffer = 0;
    send_buffer_length = 0;
    send_buffer_offset = 0;
}

void DowowNetwork::Connection::DeleteRecvBuffer() {
    delete[] recv_buffer;
    recv_buffer = 0;
    recv_buffer_length = 0;
    recv_buffer_offset = 0;
    is_recv_length = true;
}

bool DowowNetwork::Connection::Receive() {
    // receiving the request length
    if (is_recv_length) {
        // try to receive the request length to buffer length
        // (effectively interchangable definitions)
        int recv_res = recv(
            socket_fd,
            &recv_buffer_length,
            sizeof(recv_buffer_length) - recv_buffer_offset,
            0);

        // check results
        if (recv_res == -1 || recv_res == 0) {
            // the connection is broken
            return false;
        } else {
            // good, increase the offset
            recv_buffer_offset += recv_res;

            // check if received the length
            if (recv_buffer_offset == sizeof(recv_buffer_length)) {
                // get the buffer length
                recv_buffer_length = le32toh(recv_buffer_length);

                // check if too big
                if (recv_buffer_length > recv_buffer_max_length) {
                    // the connection is broken
                    return false;
                }

                // offset for data must be set to be after the size
                recv_buffer_offset = sizeof(recv_buffer_length);
                // create the buffer
                recv_buffer = new char[recv_buffer_length];

                // initialize the first 4 bytes
                uint32_t rblle32 = htole32(recv_buffer_length);
                memcpy(recv_buffer, &rblle32, sizeof(rblle32));

                // going to receive the body
                is_recv_length = false;
            }
        }
    }
    // receiving the request itself
    else {
        // bytes left to receive
        uint32_t bytes_left =
            recv_buffer_length - recv_buffer_offset;

        // receive
        int recv_res = recv(
            socket_fd,
            recv_buffer + recv_buffer_offset,
            bytes_left < recv_block_size ? bytes_left : recv_block_size,
            0);

        // check results
        if (recv_res == -1 || recv_res == 0) {
            // connection is broken
            return false;
        } else {
            // increase the offset
            recv_buffer_offset += recv_res;

            // check if received everything
            if (recv_buffer_offset == recv_buffer_length) {
                // try to deserialize
                Request* req = new Request();
                uint32_t used = req->Deserialize(recv_buffer, recv_buffer_length);

                if (used == 0) {
                    // fail :-(
                    delete req;
                } else {
                    // check if bandwidth generation
                    if (req->GetName() == "_") {
                        // just delete it
                        delete req;
                    } else {
                        // try to process using handlers
                        if (!PassThroughHandlers(req)) {
                            // handlers did not process it
                            mutex_rq.lock();
                            recv_queue.push(req);
                            mutex_rq.unlock();

                            // has receive_events set up?
                            mutex_re.lock();
                            if (receive_events.size()) {
                                Utils::WriteEventFd(*receive_events.begin(), 1);
                            }
                            mutex_re.unlock();
                        }
                    }
                }
                // delete the buffer
                DeleteRecvBuffer();
            }
        }
    }

    // update the timeout
    Utils::SetTimerFdTimeout(
        their_na_timer,
        their_na_interval);

    // good
    return true;
}

bool DowowNetwork::Connection::Send() {
    // lock the send queue
    mutex_sq.lock();
    // pop the send queue if the buffer is empty
    if (!send_buffer && send_queue.size())
        PopSendQueue();
    // unlock the send queue
    mutex_sq.unlock();

    // check if has data to send
    if (send_buffer) {
        // bytes left to send
        uint32_t left_to_send =
            send_buffer_length - send_buffer_offset;

        // write to the socket.
        // MSG_NOSIGNAL to disable broken pipe signal
        int send_res = send(
            socket_fd,
            send_buffer + send_buffer_offset,
            left_to_send < send_block_size ? left_to_send : send_block_size,
            MSG_NOSIGNAL);

        // check result
        if (send_res == -1 || send_res == 0) {
            // the connection is broken
            return false;
        } else {
            // increase offset
            send_buffer_offset += send_res;
            // sent everything
            if (send_buffer_offset == send_buffer_length) {
                DeleteSendBuffer();
                
                // lock
                MTLock(__mid, mutex_id);

                // check if disconnecting and no data left
                if (is_disconnecting &&
                    !HasSomethingToSend())
                {
                    // let the background thread think that
                    // the connection is dead.
                    return false;
                }
            }
        }
    }

    // update the timeout
    Utils::SetTimerFdTimeout(
        our_sa_timer,
        our_sa_interval);

    // success
    return true;
}

bool DowowNetwork::Connection::PopSendQueue() {
    // Lock the send queue
    MTLock(__msq, mutex_sq);

    // check if no data in queue
    if (!send_queue.size())
        return false;

    // popping
    Request* req = send_queue.front();
    send_queue.pop();

    // serializing
    send_buffer = req->Serialize();
    send_buffer_length = req->GetSize();
    send_buffer_offset = 0;

    // deleting the request
    delete req;

    return true;
}

void DowowNetwork::Connection::InitializeByFD(int socket_fd) {
    // lock the backgroung thread
    MTLock(__mbt, mutex_bt);
    // do not initialize if already did
    if (background_thread)
        return;

    // get the type
    int socket_domain;
    socklen_t len = sizeof(socket_domain);
    getsockopt(
        socket_fd,
        SOL_SOCKET,
        SO_DOMAIN,
        &socket_domain,
        &len
    );

    // determine the type
    switch (socket_domain) {
        case AF_UNIX: socket_type = SocketTypeUnix; break;
        case AF_INET: socket_type = SocketTypeTcp; break;
        // the type is not supported
        default:
            return;
    }

    // reset IDs
    mutex_fri.lock();
    free_request_id = is_even_request_parts ? 2 : 1;
    mutex_fri.unlock();

    // assign the socket
    this->socket_fd = socket_fd;

    // reset timers
    Utils::SetTimerFdTimeout(
        our_sa_timer,
        our_sa_interval);
    Utils::ReadEventFd(our_sa_timer, 0);
    Utils::SetTimerFdTimeout(
        their_na_timer,
        their_na_interval);
    Utils::ReadEventFd(their_na_timer, 0);

    // create the background thread
    background_thread = new std::thread(ConnThreadFunc, this);
}

void DowowNetwork::Connection::SetEvenRequestIdsPart(bool state) {
    // lock
    MTLock(__mfri, mutex_fri);
    
    // set the state
    is_even_request_parts = state;

    // the id
    if ((is_even_request_parts && (free_request_id % 2)) ||
        (!is_even_request_parts && !(free_request_id % 2)))
    {
        free_request_id++;
    }
}

DowowNetwork::Connection::Connection(int socket_fd) : Connection() {
    // set even/odd
    SetEvenRequestIdsPart(true);
    // initialize
    InitializeByFD(socket_fd);
}

void DowowNetwork::Connection::SetOurSaInterval(time_t interval) {
    // lock
    MTLock(__mosai, mutex_osai);
    // update
    our_sa_interval = interval < 1 ? 1 : interval;
    // reset the timer
    Utils::SetTimerFdTimeout(our_sa_timer, our_sa_interval);
}

time_t DowowNetwork::Connection::GetOurSaInterval() {
    // lock
    MTLock(__mosai, mutex_osai);
    // return
    return our_sa_interval;
}

void DowowNetwork::Connection::SetTheirNaIntervalLimit(time_t interval) {
    // lock
    MTLock(__mtnai, mutex_tnai);
    // update
    their_na_interval = interval < 1 ? 1 : interval;
    // reset the timer
    Utils::SetTimerFdTimeout(their_na_timer, their_na_interval);
}

time_t DowowNetwork::Connection::GetTheirNaIntervalLimit() {
    // lock
    MTLock(__mtnai, mutex_tnai);
    // return
    return their_na_interval;
}

DowowNetwork::Request* DowowNetwork::Connection::Push(Request* req, bool must_copy, int timeout, bool change_request_id) {
    // lock the send queue
    MTLock(__msq, mutex_sq);

    // copy the request if needed
    if (must_copy) {
        Request* copy = new Request();
        copy->CopyFrom(req);
        req = copy;
    }

    // must change the request id
    if (change_request_id) {
        // lock the free request id
        MTLock(__mfri, mutex_fri);
        // set the id
        req->SetId(free_request_id);
        // increase the free request ID by 2, as each side has a half of all IDs
        free_request_id += 2;
    }

    // push to queue
    send_queue.push(req);

    // notify the thread
    Utils::WriteEventFd(push_event, 1);

    // TODO: waiting for response
    return 0;
}

DowowNetwork::Request* DowowNetwork::Connection::Push(const Request& req, int timeout, bool change_request_id) {
    // copy
    Request *copy = new Request();
    copy->CopyFrom(&req);
    // reuse code
    return Push(copy, false, timeout, change_request_id);
}

DowowNetwork::Request* DowowNetwork::Connection::Pull(int timeout) {
    // lock the receive queue
    mutex_rq.lock();

    // check if the queue is not empty
    if (recv_queue.size()) {
        // pop
        Request *req = recv_queue.front();
        recv_queue.pop();

        // unlock
        mutex_rq.unlock();
        return req;
    }

    // If we've got here, then the queue is empty.
    // Let's find out what we do next.

    // no timeout is set, or not connected - just return
    if (!timeout || !IsConnected()) {
        mutex_rq.unlock();
        return 0;
    }

    // the timeout is set, add our event to the
    // receive events list

    // lock
    mutex_re.lock();
    // set up receive event
    int re = eventfd(0, 0);
    // add to the list
    receive_events.push_back(re);
    // set up pollfds
    pollfd pollfds { re, POLLIN, 0 };

    // unlock
    mutex_rq.unlock();
    // unlock
    mutex_re.unlock();

    // poll
    poll(&pollfds, 1, timeout);

    // TODO: here we've got a race condition.
    // We need some mechanism to deliver the
    // received request directly to this method
    // call, e.g. map?

    // lock
    mutex_rq.lock();
    // lock
    mutex_re.lock();

    // get the result
    Request *res = Pull(0);

    // close the eventfd
    close(re);
    // remove from list
    receive_events.remove(re);

    // unlock
    mutex_rq.unlock();
    // unlock
    mutex_re.unlock();

    return res;
}

uint32_t DowowNetwork::Connection::GetRefs() {
    // lock
    MTLock(__mra, mutex_ra);
    // return
    return refs_amount;
}

void DowowNetwork::Connection::IncreaseRefs() {
    // lock
    mutex_ra.lock();
    // increment
    refs_amount++;
    // unlock
    mutex_ra.unlock();
}

void DowowNetwork::Connection::DecreaseRefs() {
    // lock
    mutex_ra.lock();
    // decrement
    refs_amount--;
    // the background thread awaits loneliness ;-(
    if (not_needed_event != -1)
        Utils::WriteEventFd(not_needed_event, 1);
    // unlock
    mutex_ra.unlock();
}

void DowowNetwork::Connection::Disconnect(bool forced, bool wait_for_join) {
    // check if not connected
    if (!IsConnected()) {
        return;
    }

    // lock is_disconnecting flag
    mutex_id.lock();

    // check if requesting forced disconnection
    if (forced) {
        // forced
        Utils::WriteEventFd(to_stop_event, 1);
    } else if (!is_disconnecting) {
        // mark for disconnection
        is_disconnecting = true;
        // graceful
        Request *dummy = new Request("_");
        // push a dummy request so that the
        // thread will call Send() and
        // eventually stop
        Push(dummy, false, 0, false);
    }

    mutex_id.unlock();

    // waiting for join
    if (wait_for_join)
        WaitForStop(-1);
}

bool DowowNetwork::Connection::IsConnected() {
    // connected if the socket type is not undefined
    return socket_type != SocketTypeUndefined;
}

bool DowowNetwork::Connection::IsDisconnecting() {
    // mutex
    MTLock(__mid, mutex_id);
    // disconnecting when still connected but the flag is set
    return is_disconnecting && IsConnected();
}

uint8_t DowowNetwork::Connection::GetType() {
    return socket_type;
}

int DowowNetwork::Connection::GetStoppedEvent() {
    return stopped_event;
}

bool DowowNetwork::Connection::WaitForStop(int timeout) {
    // no background thread, so let's assume already stopped
    if (!background_thread) return true;

    // stopped?
    bool state = Utils::SelectRead(GetStoppedEvent(), timeout);
    // if stopped and the thread can be joined
    if (state && background_thread->joinable())
        background_thread->join();

    return state;
}

void DowowNetwork::Connection::SetHandlerDefault(RequestHandler h) {
    MTLock(__mhd, mutex_hd);
    handler_default = h;
}

DowowNetwork::RequestHandler DowowNetwork::Connection::GetHandlerDefault() {
    MTLock(__mhd, mutex_hd);
    return handler_default;
}

void DowowNetwork::Connection::SetHandlerNamed(std::string name, RequestHandler h) {
    MTLock(__mhn, mutex_hn);

    // must delete and is set
    if (h == 0) {
        auto it = handlers_named.find(name);
        if (it != handlers_named.end()) {
            handlers_named.erase(name);
        }
        return;
    }
    // set the new handler
    handlers_named[name] = h;
}

DowowNetwork::RequestHandler DowowNetwork::Connection::GetHandlerNamed(std::string name) {
    MTLock(__mhn, mutex_hn);

    auto it = handlers_named.find(name);
    // is not set
    if (it == handlers_named.end()) return 0;
    
    // is set
    return it->second;
}

void DowowNetwork::Connection::SetSendBlockSize(uint32_t bs) {
    // not less than 1
    if (bs < 1) bs = 1;

    MTLock(__msbs, mutex_sbs);
    send_block_size = bs;
}

uint32_t DowowNetwork::Connection::GetSendBlockSize() {
    MTLock(__msbs, mutex_sbs);
    return send_block_size;
}

void DowowNetwork::Connection::SetRecvBlockSize(uint32_t bs) {
    // not less than 1
    if (bs < 1) bs = 1;

    MTLock(__mrbs, mutex_rbs);
    recv_block_size = bs;
}

uint32_t DowowNetwork::Connection::GetRecvBlockSize() {
    MTLock(__mrbs, mutex_rbs);
    return recv_block_size;
}

void DowowNetwork::Connection::SetMaxRequestSize(uint32_t size) {
    // not less than 10 (request header + 1 symbol of request name)
    if (size < 10) size = 10;

    MTLock(__mrbml, mutex_rbml);

    recv_buffer_max_length = size;
}

uint32_t DowowNetwork::Connection::GetMaxRequestSize() {
    MTLock(__mrbml, mutex_rbml);
    return recv_buffer_max_length;
}

DowowNetwork::Connection::~Connection() {
    // when deleting the connection,
    // we must disconnect by force
    // and wait for stop.
    Disconnect(true, true);

    // close the eventfds
    close(stopped_event);
    close(push_event);
    close(to_stop_event);
    close(our_sa_timer);
    close(their_na_timer);
}
