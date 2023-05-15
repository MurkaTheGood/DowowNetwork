#include "Connection.hpp"

#include <cstring>
#include <endian.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "Utils.hpp"

#ifdef VERBOSE_DEBUG
#include <iostream>
using namespace std;
#define PRINT_DEBUG(str) { std::cerr << str << std::endl; }
#else
// will be optimized out
#define PRINT_DEBUG(str) {  }
#endif

#ifdef MULTITHREADING_TWEAKS
#define MTLock(name, mut) std::lock_guard<typeof(mut)> name(mut);
#else
#define MTLock(name, mut) {};
#endif

bool DowowNetwork::Connection::PassThroughHandlers(Request* r) {
    // no request
    if (!r) return false;

    MTLock(__hm, mutex_handlers);

    // get the named handler
    auto h = GetHandlerNamed(r->GetName());
    // try to handle
    if (h && (*h)(this, r, r->GetId()))
        return true;

    // default handler
    h = GetHandlerDefault();

    // couldn't handle using named handler, using default
    if (h && (*h)(this, r, r->GetId()))
        return true;
    
    return false;
}

DowowNetwork::Connection::Connection() {
    DeleteSendBuffer();
    DeleteRecvBuffer();
}

void DowowNetwork::Connection::DeleteSendBuffer() {
    MTLock(__sm, mutex_send);

    delete[] send_buffer;
    send_buffer = 0;
    send_buffer_length = 0;
    send_buffer_offset = 0;

    PRINT_DEBUG("Connection: deleted the send buffer");
}

void DowowNetwork::Connection::DeleteRecvBuffer() {
    MTLock(__rm, mutex_recv);

    delete[] recv_buffer;
    recv_buffer = 0;
    recv_buffer_length = 0;
    recv_buffer_offset = 0;
    is_recv_length = true;

    PRINT_DEBUG("Connection: deleted the receive buffer");
}

void DowowNetwork::Connection::Receive(bool nonblocking) {
    MTLock(__rm, mutex_recv);

    // receiving request length
    while (is_recv_length) {
        // nonblocking - check if can't read
        if (nonblocking && !Utils::SelectRead(socket_fd))
            break;

        // try to receive the request length to buffer length
        // (effectively interchangable definitions)
        int recv_res = recv(
            socket_fd,
            &recv_buffer_length,
            sizeof(recv_buffer_length) - recv_buffer_offset,
            0);

        // check results
        if (recv_res == -1 || recv_res == 0) {
            PRINT_DEBUG("Connection: length receival; recv() returned " << recv_res)

            // the connection is broken
            Close();
            break;
        } else {
            // update their KA
            their_next_ka = time(0) + their_ka_interval_limit;

            // good, increase the offset
            recv_buffer_offset += recv_res;

            // check if received the length
            if (recv_buffer_offset == sizeof(recv_buffer_length)) {
                // get the buffer length
                recv_buffer_length = le32toh(recv_buffer_length);

                // check if too big
                if (recv_buffer_length > recv_buffer_max_length) {
                    PRINT_DEBUG("Connection: length receival; request size violation (" << recv_buffer_length << "), closing");

                    Close();
                    break;
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

                // break the loop
                break;
            }
        }
    }
    // receiving the request itself
    while (!is_recv_length) {
        // can't blocking read while nonblocking
        if (nonblocking && !Utils::SelectRead(socket_fd))
            break;

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
            PRINT_DEBUG("Connection: body receival; recv() returned " << recv_res)

            // connection is broken
            Close();
            break;
        } else {
            // debug log
            PRINT_DEBUG("Connection: body receival; received " << recv_res << " bytes");
            
            // update their KA
            their_next_ka = time(0) + their_ka_interval_limit;

            // increase the offset
            recv_buffer_offset += recv_res;

            // check if received everything
            if (recv_buffer_offset == recv_buffer_length) {
                // try to deserialize
                Request* req = new Request();
                uint32_t used = req->Deserialize(recv_buffer, recv_buffer_length);

                if (used == 0) {
                    // debug log
                    PRINT_DEBUG(
                        "Connection: failed to deserialize the request, deleted");

                    // fail :-(
                    delete req;
                } else {
                    // check if keep_alive
                    if (use_keep_alive && req->GetName() == "keep_alive") {
                        PRINT_DEBUG("Connection: received a keep-alive request");
                        delete req;
                    } else {
                        // debug log
                        PRINT_DEBUG(
                            "Connection: received request is pushed to recv_queue");

                        // try to process using handlers
                        if (!PassThroughHandlers(req)) {
                            // not processed - push to queue
                            MTLock(__rqm, mutex_recv_queue);
                            recv_queue.push(req);
                        } else {
                            // processed - delete
                            delete req;
                        }
                    }
                }
                // delete the buffer
                DeleteRecvBuffer();
                break;
            }
        }
    }
}

void DowowNetwork::Connection::Send(bool nonblocking) {
    MTLock(__sm, mutex_send);
    MTLock(__sqm, mutex_send_queue);

    // pop the send queue if the buffer is empty
    if (!send_buffer && send_queue.size())
        PopSendQueue();

    // check if has data to send
    while (send_buffer) {
        // nonblocking, but can't write
        if (nonblocking && !Utils::SelectWrite(socket_fd))
            break;

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
            // debug log
            PRINT_DEBUG("Connection: send() returned " << send_res);

            // connection is broken
            Close();
            return;
        } else {
            // update our KA
            our_next_ka = time(0) + our_ka_interval;

            // debug log
            PRINT_DEBUG("Connection: sent " << send_res << " bytes");

            // increase offset
            send_buffer_offset += send_res;
            // sent everything
            if (send_buffer_offset == send_buffer_length) {
                PRINT_DEBUG("Connection: the request is sent!")
                DeleteSendBuffer();
            }
            // nonblocking - break
            if (nonblocking) break;
        }
    }
}

void DowowNetwork::Connection::Close() {
    // lock everything
    MTLock(__sm, mutex_send);
    MTLock(__rm, mutex_recv);
    MTLock(__sqm, mutex_send_queue);
    MTLock(__rqm, mutex_recv_queue);

    // check if already closed
    if (socket_type == SocketTypeUndefined) return;

    // reset the buffers
    DeleteRecvBuffer();
    DeleteSendBuffer();

    // close the socket
    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);

    // delete all the queued data
    while (send_queue.size()) {
        Request* r = send_queue.front();
        delete r;
        send_queue.pop();
    }
    while (recv_queue.size()) {
        Request* r = recv_queue.front();
        delete r;
        recv_queue.pop();
    }

    // flag
    socket_type = SocketTypeUndefined;
    PRINT_DEBUG("Connection: closed");
}

bool DowowNetwork::Connection::PopSendQueue() {
    MTLock(__sqm, mutex_send_queue);

    // check if no data in queue
    if (!send_queue.size())
        return false;

    // popping
    Request* req = send_queue.front();
    send_queue.pop();

    // serializing
    {
        MTLock(__sm, mutex_send);
        send_buffer = req->Serialize();
        send_buffer_length = req->GetSize();
        send_buffer_offset = 0;
    }

    // deleting the request
    delete req;

    return true;
}

void DowowNetwork::Connection::InitializeByFD(int socket_fd) {
    // lock everything
    MTLock(__sm, mutex_send);
    MTLock(__rm, mutex_recv);
    MTLock(__sqm, mutex_send_queue);
    MTLock(__rqm, mutex_recv_queue);
    MTLock(__frim, mutex_free_request_id);

    // do nothing if already connected
    if (IsConnected()) return;

    // reset keep-alive timers
    our_next_ka = time(0) + our_ka_interval;
    their_next_ka = time(0) + their_ka_interval_limit;

    // resetting some data
    is_disconnecting = false;

    // reset IDs
    free_request_id = 1;
    SetEvenRequestIdsPart(false);

    // assign the socket
    this->socket_fd = socket_fd;

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
            Close();
            break;
    }
}

void DowowNetwork::Connection::SetEvenRequestIdsPart(bool state) {
    // lock
    MTLock(__frim, mutex_free_request_id);
    
    // set the state
    is_even_request_parts = state;

    // the id
    if ((is_even_request_parts && (free_request_id % 2)) ||
        (!is_even_request_parts && !(free_request_id % 2)))
    {
        free_request_id++;
    }
}

void DowowNetwork::Connection::SubPoll() {
    
}

DowowNetwork::Connection::Connection(int socket_fd) : Connection() {
    InitializeByFD(socket_fd);
    SetEvenRequestIdsPart(true);
}

void DowowNetwork::Connection::SetNonblocking(bool nonblocking) {
    // update
    this->nonblocking = nonblocking;
}

bool DowowNetwork::Connection::IsNonblocking() {
    return nonblocking;
}

void DowowNetwork::Connection::Poll() {
    // subpoll
    SubPoll();

    // closed
    if (!IsConnected()) return;

    // the connection is blocking - do nothing here
    if (!nonblocking) return;

    // receive nonblocking
    Receive(nonblocking);
    
    // send nonblocking
    if (IsConnected())
        Send(nonblocking);

    // keep-alive
    if (use_keep_alive && IsConnected()) {
        // time for our keep-alive
        if (time(0) >= our_next_ka) {
            // create the keep-alive request
            Request *keep_alive_req = new Request("keep_alive");
            // reserved ID
            keep_alive_req->SetId(0);
            // send
            Push(keep_alive_req, false, false);
            // increase our net keep-alive
            our_next_ka = time(0) + our_ka_interval;
        }
        // they are dead :-(
        if (time(0) >= their_next_ka) {
            Close();
        }
    }

    // disconnecting
    if (is_disconnecting && IsConnected()) {
        MTLock(__sqm, mutex_send_queue);
        if (send_queue.size() == 0 &&
            send_buffer_offset == send_buffer_length)
        {
            // close the connection, as all data is sent
            Close();
        }
    }
}

uint32_t DowowNetwork::Connection::Push(Request* req, bool must_copy, bool change_request_id) {
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
        // acquire lock
        MTLock(__frim, mutex_free_request_id);
        // store the request id
        req_id = free_request_id;
        // set the id
        req->SetId(req_id);
        // increase the free request ID by 2, as each side has a half of all IDs
        free_request_id += 2;
    }

    // push to the queue
    {
        MTLock(__sqm, mutex_send_queue);
        send_queue.push(req);
    }

    // if the connection is set to be blocking, then send immidiately
    if (!nonblocking) {
        PRINT_DEBUG("Connection: using blocking Send() operation");
        Send(nonblocking);
    }

    return req_id;
}

uint32_t DowowNetwork::Connection::Push(Request& req, bool change_request_id) {
    // reuse code
    return Push(&req, true, change_request_id);
}

DowowNetwork::Request* DowowNetwork::Connection::Pull() {
    // not checking if connected, because the pulling
    // may be needed after the disconnection

    // blocking mode, so we should do I/O here.
    // if not connected, then do not try to receive.
    if (!nonblocking && IsConnected())
        Receive(nonblocking);

    // no requests in queue
    MTLock(__rqm, mutex_recv_queue);
    if (recv_queue.size() == 0)
        return 0;

    // get the request
    Request* req = recv_queue.front();
    recv_queue.pop();
    return req;
}

DowowNetwork::Request* DowowNetwork::Connection::Execute(Request *req, bool must_copy, time_t timeout) {
    // not connected
    if (!IsConnected()) return 0;

    // request id
    uint32_t req_id = 0;

    // nonblocking
    if (IsNonblocking()) {
        {
            // lock the recv_queue
            MTLock(__rqm, mutex_recv_queue);
            // push
            req_id = Push(req, must_copy);
            // add to subscription
            subscription_map[req_id] = 0;
        }
        // time limit
        time_t time_limit = time(0) + timeout;

        // wait
        while (time(0) < time_limit) {
            {
                MTLock(__smm, mutex_subscription_map);
                if (subscription_map[req_id]) {
                    // response received
                    Request* res = subscription_map[req_id];
                    subscription_map.erase(req_id);
                    // return
                    return res;
                }
            }
            // sleep a bit

        }

        // unsubscribe
        MTLock(__smm, mutex_subscription_map);
        subscription_map.erase(req_id);

        // no response
        return 0;
    } else {
        
    }
}

DowowNetwork::Request* DowowNetwork::Connection::Execute(Request &req, time_t timeout) {
    return Execute(&req, true, timeout);
}

void DowowNetwork::Connection::Disconnect(bool forced) {
    // check if not connected
    if (!IsConnected()) return;

    // check if requesting graceful disconnection
    if (!forced) {
        // already disconnecting
        if (IsDisconnecting()) return;

        // mark for disconnection
        is_disconnecting = true;

        // quit
        return;
    }

    // close the connection
    Close();
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

void DowowNetwork::Connection::SetHandlerDefault(RequestHandler h) {
    MTLock(__hm, mutex_handlers);
    handler_default = h;
}

DowowNetwork::RequestHandler DowowNetwork::Connection::GetHandlerDefault() {
    MTLock(__hm, mutex_handlers);
    return handler_default;
}

void DowowNetwork::Connection::SetHandlerNamed(std::string name, RequestHandler h) {
    MTLock(__hm, mutex_handlers);

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
    MTLock(__hm, mutex_handlers);

    auto it = handlers_named.find(name);
    // is not set
    if (it == handlers_named.end()) return 0;
    
    // is set
    return it->second;
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
    // BEWARE: Session data must be deleted in outer code,
    // as the type can not be stored.

    // force disconnection
    Close();
}
