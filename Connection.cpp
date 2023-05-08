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
    PRINT_DEBUG("Connection: deleted send buffer");
}

void DowowNetwork::Connection::DeleteRecvBuffer() {
    delete[] recv_buffer;
    recv_buffer = 0;
    recv_buffer_length = 0;
    recv_buffer_offset = 0;
    is_recv_length = true;
    #ifdef VERBOSE_DEBUG
    cerr << 
        "Connection: deleted receive buffer"
    << endl;
    #endif
}

void DowowNetwork::Connection::Receive(bool nonblocking) {
    // check if not connected
    if (IsClosed()) return;

    // receiving request length
    while (is_recv_length) {
        // nonblocking - check if can't read
        if (nonblocking && !Utils::SelectRead(socket_fd)) break;

        // try to receive the request length to buffer length
        // (effectively interchangable definitions)
        int recv_res = recv(
            socket_fd,
            &recv_buffer_length,
            sizeof(recv_buffer_length) - recv_buffer_offset,
            0);

        // check results
        if (recv_res == -1 || recv_res == 0) {
            #ifdef VERBOSE_DEBUG
            cerr << 
                "Connection: receiving request length; recv() returned " << recv_res
            << endl;
            #endif
            // the connection is broken
            Close();
            return;
        } else {
            // good
            recv_buffer_offset += recv_res;
            if (recv_buffer_offset == sizeof(recv_buffer_length)) {
                // get the buffer length
                recv_buffer_length = le32toh(recv_buffer_length);
                // check if too big
                if (recv_buffer_length > recv_buffer_max_length) {
                    #ifdef VERBOSE_DEBUG
                    cerr << 
                        "Connection: request size violates limit of " << recv_buffer_length
                    << endl;
                    #endif
                    Close();
                    return;
                }

                // offset is 4, as the first 4 bytes are already received
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
        // can't read
        if (nonblocking && !Utils::SelectRead(socket_fd)) break;

        // bytes left to receive
        uint32_t bytes_left = recv_buffer_length - recv_buffer_offset;
        // receive
        int recv_res = recv(
            socket_fd,
            recv_buffer + recv_buffer_offset,
            bytes_left < recv_block_size ? bytes_left : recv_block_size,
            0);
        // check results
        if (recv_res == -1 || recv_res == 0) {
            #ifdef VERBOSE_DEBUG
            cerr << 
                "Connection: receiving request; recv() returned " << recv_res
            << endl;
            #endif
            // connection is broken
            Close();
            return;
        } else {
            // debug log
            PRINT_DEBUG("Connection: received " << recv_res << " bytes");
            
            recv_buffer_offset += recv_res;
            // check if done
            if (recv_buffer_offset == recv_buffer_length) {
                Request* req = new Request();
                uint32_t used = req->Deserialize(recv_buffer, recv_buffer_length);
                if (used == 0) {
                    // debug log
                    PRINT_DEBUG("Connection: failed to deserialize the request, deleted");

                    // fail :-(
                    delete req;
                } else {
                    // debug log
                    PRINT_DEBUG("Connection: received request is pushed to recv_queue");
                    
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
    // can't send anything via closed connection
    if (IsClosed()) return;

    // fill the buffer if it is empty
    if (send_buffer_offset == send_buffer_length && send_queue.size()) {
        DeleteSendBuffer();
        PopSendQueue();
    }

    // check if has data to send
    while (send_buffer_offset != send_buffer_length) {
        // nonblocking, but can't write
        if (nonblocking && !Utils::SelectWrite(socket_fd)) break;

        // bytes left to send
        uint32_t left_to_send = send_buffer_length - send_buffer_offset;

        // write
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
                DeleteSendBuffer();
                PopSendQueue();
            }
            // nonblocking - break
            if (nonblocking) break;
        }
    }
}

bool DowowNetwork::Connection::PopSendQueue() {
    // check if has data
    if (!send_queue.size()) return false;

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
    // do nothing if open
    if (!IsClosed()) return;

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

    switch (socket_domain) {
        case AF_UNIX: socket_type = SocketTypeUNIX; break;
        case AF_INET: socket_type = SocketTypeTCP; break;
        default: Close(); break;
    }
}

DowowNetwork::Connection::Connection(int socket_fd) : Connection() {
    InitializeByFD(socket_fd);
}

void DowowNetwork::Connection::SetBlocking(bool blocking) {
    // check if closed
    if (IsClosed()) return;

    // update
    nonblocking = !blocking;
}

bool DowowNetwork::Connection::IsBlocking() {
    return !nonblocking;
}

void DowowNetwork::Connection::Handle() {
    // closed
    if (IsClosed()) return;
    // the connection is blocking - do nothing here
    if (!nonblocking) return;

    // receive nonblocking
    Receive(nonblocking);
    
    // send nonblocking
    Send(nonblocking);
}

void DowowNetwork::Connection::Push(Request* req, bool must_copy) {
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
    // closed
    if (IsClosed()) return 0;

    // blocking socket, so this function must block
    if (!nonblocking) Receive(nonblocking);

    // no requests
    if (recv_queue.size() == 0)
        return 0;

    // get the request
    Request* req = recv_queue.front();
    recv_queue.pop();
    return req;
}

void DowowNetwork::Connection::Close() {
    // do not close if already closed
    if (IsClosed()) return;

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
    #ifdef VERBOSE_DEBUG
    cerr << 
        "Connection: closed using Connection::Close()"
    << endl;
    #endif
}

bool DowowNetwork::Connection::IsClosed() {
    return socket_type == SocketTypeUndefined;
}

uint8_t DowowNetwork::Connection::GetType() {
    return socket_type;
}

DowowNetwork::Connection::~Connection() {
    Close();
}
