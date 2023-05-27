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
        // create the fds for poll
        pollfd pollfds[5];
        // add 'to_stop' event
        pollfds[0].fd = c->to_stop_event;
        pollfds[0].events = POLLIN;
        // add socket fd
        pollfds[1].fd = c->socket_fd;
        pollfds[1].events =
            POLLIN |
            (c->HasSomethingToSend() ? POLLOUT : 0) |
            (c->is_disconnecting ? POLLOUT : 0);
        // our still-alive timer
        pollfds[2].fd = c->our_sa_timer;
        pollfds[2].events = POLLIN;
        // their keep-alive timer
        pollfds[3].fd = c->their_na_timer;
        pollfds[3].events = POLLIN;
        // push event (it's not handled, we just need poll to return)
        pollfds[4].fd = c->push_event;
        pollfds[4].events = POLLIN;

        // let's poll!
        poll(
            pollfds,
            sizeof(pollfds) / sizeof(pollfds[0]),
            -1);

        // nothing to send, and disconnecting
        if (c->is_disconnecting && !c->HasSomethingToSend())
            break;

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
            Request *keep_alive = new Request("_");
            c->Push(keep_alive);

            // read the value (or else the timer will break)
            uint64_t void_buf;
            read(pollfds[2].fd, &void_buf, sizeof(void_buf));

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

    // mark as disconnecting
    c->is_disconnecting = true;

    // notify all events if no external references
    if (c->receive_event != -1) {
        Utils::WriteEventFd(c->receive_event, 1);
    }

    // close
    shutdown(c->socket_fd, SHUT_RDWR);
    close(c->socket_fd);
    close(c->push_event);
    close(c->our_sa_timer);
    close(c->their_na_timer);
    close(c->to_stop_event);

    // delete
    c->DeleteSendBuffer();
    c->DeleteRecvBuffer();
    while (c->send_queue.size()) {
        delete c->send_queue.front();
        c->send_queue.pop();
    }

    // mark as undefined
    c->socket_type = SocketTypeUndefined;

    // notify about stop (if no external references)
    if (!c->GetRefs())
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
            std::thread t(*h, this, r);
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
            std::thread t(*h, this, r);
            t.detach();
        } else {
            (*h)(this, r);
        }
        return true;
    }
    
    return false;
}

DowowNetwork::Connection::Connection() {
    DeleteSendBuffer();
    DeleteRecvBuffer();

    // create a stopped event
    stopped_event = eventfd(0, 0);
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
                    // check if keep_alive
                    if (req->GetName() == "_") {
                        // just delete it
                        delete req;
                    } else {
                        // try to process using handlers
                        if (!PassThroughHandlers(req)) {
                            mutex_rq.lock();
                            recv_queue.push(req);
                            mutex_rq.unlock();

                            // queue updated, notify outer code
                            if (receive_event != -1) {
                                Utils::WriteEventFd(receive_event, 1);
                            }
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
    // pop the send queue if the buffer is empty
    if (!send_buffer && send_queue.size())
        PopSendQueue();

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
    // do nothing if already connected
    if (IsConnected()) return;

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

    // initialize the events
    push_event = eventfd(0, 0);
    to_stop_event = eventfd(0, 0);

    // reset the 'stopped' event
    Utils::ReadEventFd(stopped_event, 0);

    // create the timers for keep-alive mechanism
    our_sa_timer = timerfd_create(CLOCK_MONOTONIC, 0);
    their_na_timer = timerfd_create(CLOCK_MONOTONIC, 0);

    // start the timers
    Utils::SetTimerFdTimeout(our_sa_timer, our_sa_interval);
    Utils::SetTimerFdTimeout(their_na_timer, their_na_interval);

    // not disconnecting
    is_disconnecting = false;

    // reset IDs
    free_request_id = is_even_request_parts ? 2 : 1;

    // cleanup receive queue
    while (recv_queue.size()) {
        delete recv_queue.front();
        recv_queue.pop();
    }

    // assign the socket
    this->socket_fd = socket_fd;

    // start the polling thread
    std::thread polling_thread(ConnThreadFunc, this);
    polling_thread.detach();
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
    InitializeByFD(socket_fd);
    SetEvenRequestIdsPart(true);
}

void DowowNetwork::Connection::SetOurSaInterval(time_t interval) {
    our_sa_interval = interval < 1 ? 1 : interval;
    Utils::SetTimerFdTimeout(our_sa_timer, our_sa_interval);
}

time_t DowowNetwork::Connection::GetOurSaInterval() {
    return our_sa_interval;
}

void DowowNetwork::Connection::SetTheirNaIntervalLimit(time_t interval) {
    their_na_interval = interval < 1 ? 1 : interval;
    Utils::SetTimerFdTimeout(their_na_timer, their_na_interval);
}

time_t DowowNetwork::Connection::GetTheirNaIntervalLimit() {
    return their_na_interval;
}

DowowNetwork::Request* DowowNetwork::Connection::Push(Request* req, bool must_copy, int timeout, bool change_request_id) {
    // lock the send queue
    MTLock(__msq, mutex_sq);

    // not connected or disconnecting
    if (!IsConnected() || IsDisconnecting()) {
        // delete the data if it is not copied
        if (!must_copy)
            delete req;
        return 0;
    }

    // copy the request if needed
    if (must_copy) {
        Request* copy = new Request();
        copy->CopyFrom(req);
        req = copy;
    }

    // the id used
    uint32_t req_id = req->GetId();
    // must change the request id
    if (change_request_id) {
        // lock the free request id
        MTLock(__mfri, mutex_fri);
        // store the request id
        req_id = free_request_id;
        // set the id
        req->SetId(req_id);
        // increase the free request ID by 2, as each side has a half of all IDs
        free_request_id += 2;
    }

    // push to queue
    send_queue.push(req);

    // notify the thread
    Utils::WriteEventFd(push_event, 1);

    return 0;
}

DowowNetwork::Request* DowowNetwork::Connection::Push(Request& req, int timeout, bool change_request_id) {
    // reuse code
    return Push(&req, true, timeout, change_request_id);
}

DowowNetwork::Request* DowowNetwork::Connection::Pull(int timeout) {
    // not checking if connected, because the pulling
    // may be needed after the disconnection

    // lock the receive queue
    mutex_rq.lock();

    // check if has elements
    if (recv_queue.size()) {
        // pop
        Request *req = recv_queue.front();
        recv_queue.pop();

        // unlock
        mutex_rq.unlock();
        return req;
    }

    // no timeout
    if (!timeout) {
        mutex_rq.unlock();
        return 0;
    }

    // someone is already waiting for Pull, quit
    if (receive_event != -1) {
        mutex_rq.unlock();
        return 0;
    }

    // set up receive event
    receive_event = eventfd(0, 0);

    // set up pollfds
    pollfd pollfds { receive_event, POLLIN, 0 };

    // unlock
    mutex_rq.unlock();

    // poll
    poll(&pollfds, 1, timeout);

    // lock
    mutex_rq.lock();

    // get the result
    Request *res = Pull(0);

    // close the eventfd
    close(receive_event);
    receive_event = -1;

    // unlock
    mutex_rq.unlock();

    return res;
}

uint32_t DowowNetwork::Connection::GetRefs() {
    MTLock(__mra, mutex_ra);
    return refs_amount;
}

void DowowNetwork::Connection::IncreaseRefs() {
    mutex_ra.lock();
    refs_amount++;
    mutex_ra.unlock();
}

void DowowNetwork::Connection::DecreaseRefs() {
    mutex_ra.lock();
    refs_amount--;
    if (is_disconnecting && !refs_amount)
        Utils::WriteEventFd(stopped_event, 1);
    mutex_ra.unlock();
}

void DowowNetwork::Connection::Disconnect(bool forced, bool wait_for_join) {
    // check if not connected or disconnecting
    if (!IsConnected()) {
        return;
    }

    // check if requesting graceful disconnection
    if (!forced) {
        // already disconnecting
        if (IsDisconnecting()) {
            return;
        }

        // mark for disconnection
        is_disconnecting = true;
        // interrupt the thread
        Utils::WriteEventFd(push_event, 1);
    } else {
        // forced
        Utils::WriteEventFd(to_stop_event, 1);
    }

    // waiting for join
    if (wait_for_join)
        WaitForStop(-1);
}

bool DowowNetwork::Connection::IsConnected() {
    // connected if we can define the socket type
    return GetType() != SocketTypeUndefined;
}

bool DowowNetwork::Connection::IsDisconnecting() {
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
    int stop_event = GetStoppedEvent();
    pollfd pollfds { stop_event, POLLIN, 0 };

    // return true if the connection is closed
    return poll(&pollfds, 1, timeout) > 0;
}

void DowowNetwork::Connection::SetHandlerDefault(RequestHandler h) {
    handler_default = h;
}

DowowNetwork::RequestHandler DowowNetwork::Connection::GetHandlerDefault() {
    return handler_default;
}

void DowowNetwork::Connection::SetHandlerNamed(std::string name, RequestHandler h) {
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
    auto it = handlers_named.find(name);
    // is not set
    if (it == handlers_named.end()) return 0;
    
    // is set
    return it->second;
}

void DowowNetwork::Connection::SetHandlersMT(bool state) {
    mt_handlers = state;
}

bool DowowNetwork::Connection::GetHandlersMT() {
    return mt_handlers;
}

void DowowNetwork::Connection::SetSendBlockSize(uint32_t bs) {
    // not less than 1
    if (bs < 1) bs = 1;
    send_block_size = bs;
}

uint32_t DowowNetwork::Connection::GetSendBlockSize() {
    return send_block_size;
}

void DowowNetwork::Connection::SetRecvBlockSize(uint32_t bs) {
    // not less than 1
    if (bs < 1) bs = 1;
    recv_block_size = bs;
}

uint32_t DowowNetwork::Connection::GetRecvBlockSize() {
    return recv_block_size;
}

void DowowNetwork::Connection::SetMaxRequestSize(uint32_t size) {
    // not less than 10 (request header + 1 symbol of request name)
    if (size < 10) size = 10;

    recv_buffer_max_length = size;
}

uint32_t DowowNetwork::Connection::GetMaxRequestSize() {
    return recv_buffer_max_length;
}

DowowNetwork::Connection::~Connection() {
    Disconnect(true, true);

    close(stopped_event);
}
