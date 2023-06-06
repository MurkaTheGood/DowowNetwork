#include "InternalConnection.hpp"

#include <cstring>

#include <endian.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include "Utils.hpp"

#include <iostream>

void DowowNetwork::InternalConnection::HandlerBootstapper(
        InternalConnection *c,
        RequestHandler h,
        Request *r,
        int stopped_event)
{
    // call the handler
    (*h)(c, r);

    // signal about stop
    Utils::WriteEventFd(stopped_event, 1);
}

void DowowNetwork::InternalConnection::BackgroundFunction(InternalConnection *c) {
    // create the timers for keep-alive mechanism
    int our_timer = timerfd_create(CLOCK_MONOTONIC, 0);
    int their_timer = timerfd_create(CLOCK_MONOTONIC, 0);
    // start the timers
    Utils::SetTimerFdTimeout(our_timer, CONN_OT_INTERVAL);
    Utils::SetTimerFdTimeout(their_timer, CONN_TT_INTERVAL);

    // descriptors to poll
    pollfd *fds = 0;
    // the background loop itself
    while (true) {
        // delete old descriptors
        delete[] fds;

        // calculate new amount of descriptors
        uint32_t basic_a = 5;
        uint32_t hse_a = c->handler_stop_events.size();
        uint32_t fds_a = basic_a + hse_a;

        // new file descriptors to poll
        fds = new pollfd[fds_a];
        // to_stop
        fds[0].fd = c->to_stop_event;
        fds[0].events = POLLIN;
        // socket
        fds[1].fd = c->socket_fd;
        fds[1].events =
            (!c->is_disconnecting ? POLLIN : 0) |
            (c->HasSomethingToSend() | c->is_disconnecting ? POLLOUT : 0) |
            POLLHUP;
        // push
        fds[2].fd = c->push_event;
        fds[2].events = POLLIN;
        // our timer
        fds[3].fd = our_timer;
        fds[3].events = POLLIN;
        // their timer
        fds[4].fd = their_timer;
        fds[4].events = POLLIN;

        // handler stopped events
        int i = 0;
        for (auto& p: c->handler_stop_events) {
            fds[basic_a + i].fd = p.first;
            fds[basic_a + i].events = POLLIN;
            ++i;
        }

        // poll
        poll(fds, fds_a, -1);

        // to_stop
        if (fds[0].revents & POLLIN) {
            break;
        }
        // socket
        if (fds[1].revents & POLLIN) {
            if (!c->Recv())
                // recv failed, disconnect
                break;
        }
        if (fds[1].revents & POLLOUT) {
            if (!c->Send())
                // send failed, disconnect
                break;
        }
        if (fds[1].revents & POLLHUP) {
            // hang up
            break;
        }
        // push
        if (fds[2].revents & POLLIN) {
            // reset the eventfd
            Utils::ReadEventFd(fds[2].fd, 0);
        }
        // our timer expired, generate traffic
        if (fds[3].revents & POLLIN) {
            // dummy request
            Request *r = new Request("_");
            r->SetId(0);
            c->Push(r, false, false, 0);
            // reset the timer
            Utils::ReadEventFd(fds[3].fd, -1);
            Utils::SetTimerFdTimeout(fds[3].fd, CONN_OT_INTERVAL);
        }
        // their timer expired, disconnect
        if (fds[4].revents & POLLIN) {
            break;
        }
        // stopped handlers check
        for (uint32_t i = 0; i < hse_a; ++i) {
            // the handler has finished its execution - join
            if (fds[basic_a + i].revents & POLLIN) {
                // get the fd
                int fd = fds[basic_a + i].fd;
                // close the fd
                close(fd);

                // join and delete the thread
                c->handler_stop_events[fd]->join();
                delete c->handler_stop_events[fd];
                // remove from map
                c->handler_stop_events.erase(fd);
            }
        }
    }
    // cleanup
    delete[] fds;

    // TODO: lock

    // stopping, so delete the timers
    close(our_timer);
    close(their_timer);

    // shutdown and close the socket
    shutdown(c->socket_fd, SHUT_RDWR);
    close(c->socket_fd);

    // delete existing buffer
    delete[] c->send_buffer;
    delete[] c->recv_buffer;
    
    // wait for all the threads to finish
    while (c->handler_stop_events.size()) {
        uint32_t hse_a = c->handler_stop_events.size();
        pollfd *fds =
            new pollfd[hse_a];
        
        uint32_t cnt = 0;
        for (auto i: c->handler_stop_events) {
            fds[cnt].fd = i.first;
            fds[cnt].events = POLLIN;
            ++cnt;
        }

        poll(fds, hse_a, -1);

        for (uint32_t i = 0; i < hse_a; ++i) {
            // not this one
            if (!(fds[i].revents & POLLIN)) continue;

            // this one
            int fd = fds[i].fd;
            close(fd);

            // thread
            c->handler_stop_events[fd]->join();
            delete c->handler_stop_events[fd];

            c->handler_stop_events.erase(fd);
        }
    }

    // clear the send queue
    for (auto i: c->send_queue)
        delete i;

    // signal about stop
    Utils::WriteEventFd(c->stopped_event, 1);
}

void DowowNetwork::InternalConnection::HandleReceived(Request *r) {
    // check if subscription exists
    // TODO lock
    auto it = push_subscribe.find(r->GetId());
    if (it != push_subscribe.end()) {
        it->second.second = r;
        Utils::WriteEventFd(it->first, 1);
        return;
    }
    // TODO unlock

    // check if we've got any appropriate handler
    // NAMED
    RequestHandler h = GetHandlerNamed(r->GetName());
    // DEFAULT
    if (!h) h = GetHandlerDefault();

    // we have the handler
    if (h) {
        // create the stopped event
        int efd = eventfd(0, 0);

        std::thread *t = new std::thread(
                HandlerBootstapper, this, h, r, efd);
        handler_stop_events[efd] = t;
        return;
    }

    // TODO lock

    // no handler - add to the queue then
    recv_queue.push_back(r);
}

bool DowowNetwork::InternalConnection::HasSomethingToSend() {
    // TODO lock

    return send_queue.size() || send_buffer;
}

bool DowowNetwork::InternalConnection::PopSendQueue() {
    // TODO lock

    if (send_queue.empty())
        return false;

    Request *r = send_queue.front();
    send_queue.erase(send_queue.begin());

    send_buffer = r->Serialize();
    send_buffer_size = r->GetSize();
    send_buffer_offset = 0;

    delete r;

    return true;
}

bool DowowNetwork::InternalConnection::Send() {
    // no buffer - update it
    if (!send_buffer) {
        if (!PopSendQueue()) {
            // empty queue, disconnect
            return false;
        }
    }

    // send
    int s_res = send(
            socket_fd,
            send_buffer + send_buffer_offset,
            send_buffer_size - send_buffer_offset,
            MSG_NOSIGNAL);

    // failed
    if (s_res == -1) {
        last_errno = errno;
        return false;
    }

    // disconnected
    if (s_res == 0) {
        return false;
    }

    // sent
    send_buffer_offset += s_res;

    // check if sent everything
    if (send_buffer_offset == send_buffer_size) {
        // sent everything, cleanup
        delete[] send_buffer;
        send_buffer = 0;
    }

    return true;
}

bool DowowNetwork::InternalConnection::Recv() {
    // receive buffer exists, then we receive the content
    if (recv_buffer) {
        // receive the data
        int r_res = recv(
            socket_fd,
            recv_buffer + recv_buffer_offset,
            recv_buffer_size - recv_buffer_offset,
            0);
        // error
        if (r_res == -1) {
            last_errno = errno;
            return false;
        }
        // disconnected
        if (r_res == 0) {
            return false;
        }
        
        // received data
        recv_buffer_offset += r_res;
        // finished
        if (recv_buffer_offset == recv_buffer_size) {
            // try to parse
            Request *r = new Request();
            uint32_t used = 
                r->Deserialize(recv_buffer, recv_buffer_size);

            // delete the buffer
            delete[] recv_buffer;
            recv_buffer = 0;
            recv_buffer_offset = 0;

            // failed, did not deserialize
            if (!used) {
                delete r;
            }

            // success, handle the request
            HandleReceived(r);
        }

        return true;
    }
    // receive buffer does not exist, then we receive the length
    else {
        // so called "buffer"
        char *buf = reinterpret_cast<char*>(&recv_buffer_size);
        const uint32_t buf_size = sizeof(recv_buffer_size);
        // receive the length
        int r_res = recv(
            socket_fd,
            buf + recv_buffer_offset,
            buf_size - recv_buffer_offset,
            0);
        // couldn't receive
        if (r_res == -1) {
            last_errno = errno;
            return false;
        }
        // disconnected
        if (r_res == 0) {
            return false;
        }

        // received
        recv_buffer_offset += r_res;
        // received everything
        if (recv_buffer_offset == buf_size) {
            // copy the original
            uint32_t old = buf_size;

            // from be32 to host
            recv_buffer_size = le32toh(recv_buffer_size);
            recv_buffer_offset = sizeof(recv_buffer_size);

            // create the buffer
            recv_buffer = new char[recv_buffer_size];
            memcpy(recv_buffer, &old, sizeof(old));
        }

        // success
        return true;
    }
}

DowowNetwork::InternalConnection::InternalConnection(int fd) {
    // not using mutexes here, because it is a constructor

    // check if invalid fd
    if (!fd) return;

    // create the events
    socket_fd = fd;
    stopped_event = eventfd(0, 0);
    to_stop_event = eventfd(0, 0);
    push_event = eventfd(0, 0);

    // create the background thread
    bg_thread = new std::thread(BackgroundFunction, this);
}

DowowNetwork::Request *DowowNetwork::InternalConnection::Push(
        Request *r, bool c_id, bool copy, int timeout)
{
    // TODO lock the background thread

    // allow pushing only if connected
    if (!IsConnected()) {
        if (!copy)
            delete r;
        return 0;
    }

    // copy?
    if (copy) {
        Request *nr = new Request();
        nr->CopyFrom(r);
        r = nr;
    }

    // change id?
    if (c_id) {
        // TODO lock FRI
        r->SetId(free_rid);
        free_rid += 2;
        // TODO unlock FRI
    }

    // store the RI
    uint32_t ri = r->GetId();

    // TODO lock send_queue
    send_queue.push_back(r);
    // push event
    Utils::WriteEventFd(push_event, 1);
    // TODO unlock send_queue
    
    // wait for response?
    if (timeout == 0)
        return 0;

    // yeah, wait
    int efd = eventfd(0, 0);
    
    // TODO lock
    std::pair<int, Request*> rp { efd, 0 };
    push_subscribe[ri] = rp;
    // TODO unlock

    // wait for completion
    pollfd pollfds { efd, POLLIN, 0 };
    poll(&pollfds, 1, timeout * 1000);

    // TODO lock
    Request* res = push_subscribe[ri].second;
    push_subscribe.erase(ri);
    // TODO unlock

    close(efd);
    return res;
}

DowowNetwork::Request *DowowNetwork::InternalConnection::Push(
        const Request &r, bool c_id, int t)
{
    Request* copy = new Request();
    copy->CopyFrom(&r);

    return Push(copy, c_id, t);
}

DowowNetwork::Request *DowowNetwork::InternalConnection::Pull(int timeout) {
    // TODO lock

    // not connected, quit now
    if (!IsConnected())
        return 0;

    // TODO lock
    if (recv_queue.size()) {
        Request *r = recv_queue.front();
        recv_queue.erase(recv_queue.begin());
        return r;
    }

    // empty queue, timeout is 0, so return
    if (timeout == 0)
        return 0;

    // setup the event
    int efd = eventfd(0, 0);

    // TODO lock
    pull_subscribe[efd] = 0;
    // TODO unlock
    
    // poll
    pollfd pollfds { efd, POLLIN, 0 };
    poll(&pollfds, 1, timeout * 1000);

    // TODO lock
    Request *res = pull_subscribe[efd];
    pull_subscribe.erase(efd);
    // TODO unlock
    
    close(efd);
    return res;
}

bool DowowNetwork::InternalConnection::WaitForStop(int timeout) {
    // poll the stopped event
    pollfd pollfds { stopped_event, POLLIN, 0 };
    poll(&pollfds, 1, timeout * 1000);

    // check if stopped
    return pollfds.revents & POLLIN;
}

void DowowNetwork::InternalConnection::Disconnect(bool forced) {
    // forced, so just signal to stop
    if (forced)
        Utils::WriteEventFd(to_stop_event, 1);
    // graceful, mark as disconnecting
    else
        is_disconnecting = true;
}

int DowowNetwork::InternalConnection::GetStoppedEvent() {
    return stopped_event;
}

bool DowowNetwork::InternalConnection::IsConnected() {
    // the right way to check connection
    return !WaitForStop(0);
}

void DowowNetwork::InternalConnection::SetHandlerNamed(std::string name, RequestHandler h) {
    // TODO lock

    // remove the handler
    if (!h)
        handlers_named.erase(name);
    // set the handler
    else
        handlers_named[name] = h;
}

DowowNetwork::RequestHandler DowowNetwork::InternalConnection::GetHandlerNamed(std::string name) {
    // TODO lock

    // set?
    auto it = handlers_named.find(name);
    // no
    if (it == handlers_named.end())
        return 0;
    
    // yes
    return it->second;
}

void DowowNetwork::InternalConnection::SetHandlerDefault(RequestHandler h) {
    // TODO lock
    
    handler_default = h;
}

DowowNetwork::RequestHandler DowowNetwork::InternalConnection::GetHandlerDefault() {
    // TODO lock

    return handler_default;
}

DowowNetwork::InternalConnection::~InternalConnection() {
    // finita la commedia
    Disconnect(true);
    // wait for stop
    WaitForStop(-1);

    // close the events
    close(stopped_event);
    close(to_stop_event);
    close(push_event);

    // join and delete
    bg_thread->join();
    delete bg_thread;
}