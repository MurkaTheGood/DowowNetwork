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

DowowNetwork::Connection::Connection() {
    DeleteSendBuffer();
    DeleteRecvBuffer();
}

void DowowNetwork::Connection::DeleteSendBuffer() {
    delete[] send_buffer;
    send_buffer = 0;
    send_buffer_length = 0;
    send_buffer_offset = 0;

    PRINT_DEBUG("Connection: deleted the send buffer");
}

void DowowNetwork::Connection::DeleteRecvBuffer() {
    delete[] recv_buffer;
    recv_buffer = 0;
    recv_buffer_length = 0;
    recv_buffer_offset = 0;
    is_recv_length = true;

    PRINT_DEBUG("Connection: deleted the receive buffer");
}

void DowowNetwork::Connection::Receive(bool nonblocking) {
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
            return;
        } else {
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
                    return;
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
            return;
        } else {
            // debug log
            PRINT_DEBUG("Connection: body receival; received " << recv_res << " bytes");
            
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
                    // debug log
                    PRINT_DEBUG(
                        "Connection: received request is pushed to recv_queue");
                    
                    // good :-)
                    recv_queue.push(req);
                }
                // delete the buffer
                DeleteRecvBuffer();
                break;
            }
        }
    }
}

void DowowNetwork::Connection::Send(bool nonblocking) {
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

    // resetting some data
    is_disconnecting = false;

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

void DowowNetwork::Connection::SubPoll() {
    
}

DowowNetwork::Connection::Connection(int socket_fd) : Connection() {
    InitializeByFD(socket_fd);
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
    Send(nonblocking);

    // disconnecting
    if (is_disconnecting) {
        if (send_queue.size() == 0 &&
            send_buffer_offset == send_buffer_length)
        {
            // close the connection, as all data is sent
            Close();
        }
    }
}

void DowowNetwork::Connection::Push(Request* req, bool must_copy) {
    // not connected or disconnecting
    if (!IsConnected() || IsDisconnecting()) {
        // delete the data if it is not copied
        if (!must_copy)
            delete req;
        return;
    }

    // copy the request if needed
    if (must_copy) {
        Request* copy = new Request();
        copy->CopyFrom(req);
        req = copy;
    }

    // push to the queue
    send_queue.push(req);

    // if the connection is set to be blocking, then send immidiately
    if (!nonblocking) {
        PRINT_DEBUG("Connection: using blocking Send() operation");
        Send(nonblocking); 
    }
}

void DowowNetwork::Connection::Push(Request& req) {
    // reuse code
    Push(&req, true);
}

DowowNetwork::Request* DowowNetwork::Connection::Pull() {
    // not checking if connected, because the pulling
    // may be needed after the disconnection

    // blocking mode, so we should do I/O here.
    // if not connected, then do not try to receive.
    if (!nonblocking && IsConnected())
        Receive(nonblocking);

    // no requests in queue
    if (recv_queue.size() == 0)
        return 0;

    // get the request
    Request* req = recv_queue.front();
    recv_queue.pop();
    return req;
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

DowowNetwork::Connection::~Connection() {
    // delete the session data
    SetSessionData<int*>(0);

    // force disconnection
    Close();
}
