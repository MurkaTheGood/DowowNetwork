#ifndef __DOWOW_NETWORK__CONNECTION_H_
#define __DOWOW_NETWORK__CONNECTION_H_

#include <queue>
#include <cstdint>
#include <cstdlib>

#ifdef DUMP_CONNECTIONS
#include <fstream>
#include <iostream>
#include <ctime>
#endif

#include "SocketType.hpp"
#include "Request.hpp"

namespace DowowNetwork {
    class Connection {
    private:
        const timespec zero_time { 0, 0 };
    protected:
        // the socket that used for communication
        int socket_fd = -1;

        // the socket type
        // 'undefined' is interpreted as not connected
        uint8_t socket_type = SocketTypeUndefined;

        // the send buffer block size
        uint32_t send_block_size = 1024;
        // the send buffer
        char* send_buffer = 0;
        // the send buffer length
        uint32_t send_buffer_length = 0;
        // the send buffer offset
        uint32_t send_buffer_offset = 0;
        // requests to send
        std::queue<Request*> send_queue;

        // the receive block size
        uint32_t recv_block_size = 1024;
        // max size of the receive buffer;
        // will be disconnected in case of violating this limit
        uint32_t recv_buffer_max_length = 16 * 1024;
        // the receive buffer
        char* recv_buffer = 0;
        // the receive buffer length
        uint32_t recv_buffer_length = 0;
        // the receive buffer offset
        uint32_t recv_buffer_offset = 0;
        // received requests
        std::queue<Request*> recv_queue;
        // is receiving the length?
        bool is_recv_length = true;

        // nonblocking I/O?
        bool nonblocking;

        // this constructor can only be used in derived classes
        Connection();

        // delete the send buffer
        void DeleteSendBuffer();
        // delete the receive buffer
        void DeleteRecvBuffer();

        // receive logic.
        // automatically pushes the received Requests to recv_queue.
        // if nonblocking, will not block. Will receive as must data per one call as possible.
        // if blocking, will block until the entire request is received.
        void Receive(bool nonblocking);

        // send logic.
        // sends the request(s) from send_queue.
        // if nonblocking, will not block. Will send as much data per one call as possible.
        // if blocing, will send until everything is sent.
        void Send(bool nonblocking);

        // pops the request from send_queue to send_buffer.
        // does not delete the old buffer.
        // returns true if there was something to pop.
        bool PopSendQueue();

        // initialize the connection with existing socket
        void InitializeByFD(int socket_fd);
    public:
        // create a new connection, based on existing socket.
        Connection(int socket_fd);

        // set the connection to be blocking or nonblocking
        void SetBlocking(bool blocking);
        // check if blocking
        bool IsBlocking();

        // handle the nonblocking IO (works only when nonblocking)
        void Handle();

        // Push the request to the send queue.
        // If you don't want do use the passed request later, set must_copy = false.
        // The passed request will be deleted once it is sent, if it wasn't copied.
        void Push(Request* req, bool must_copy = true);
        // Push the request to the send queue.
        // The request is copied.
        void Push(Request& req);

        // pull the request from receive queue
        // !DELETE IT BY YOURSELF!
        Request* Pull();

        // close the connection
        void Close();
        // true if closed (socket_type == SocketTypeUndefined)
        bool IsClosed();

        // returns the socket type.
        // returns SocketTypeUndefined if not connected
        uint8_t GetType();

        virtual ~Connection();
    };
}

#endif
