#ifndef __DOWOW_NETWORK__CONNECTION_H_
#define __DOWOW_NETWORK__CONNECTION_H_

#include <queue>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
        // time for pselect()
        const timespec zero_time { 0, 0 };

        // true if graceful disconnection is in progress
        bool is_disconnecting = false;

        // the socket that is used for communication
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
        bool nonblocking = false;

        // session data
        void* session_data = 0;
        // session data length
        uint32_t session_data_len = 0;
    protected:
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
        // if blocing, will send until the first from the queue is sent.
        void Send(bool nonblocking);

        // close the connection
        void Close();

        // pops the request from send_queue to send_buffer.
        // does not delete the old buffer.
        // returns true if there was something to pop.
        bool PopSendQueue();

        // initialize the connection with existing socket
        void InitializeByFD(int socket_fd);

        // additional Poll() logic to define in derived classes
        virtual void SubPoll();
    public:
        // connection tag
        std::string tag;

        // create a new connection, based on existing socket.
        Connection(int socket_fd);

        // set the connection to be blocking or nonblocking
        void SetNonblocking(bool nonblocking);
        // check if nonblocking
        bool IsNonblocking();

        // handle the nonblocking IO (works only when nonblocking)
        void Poll();

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

        // disconnect.
        // if forced, then it will be closed immidiately.
        // if not forced, then all the remaining data will be sent.
        void Disconnect(bool forced = false);
        // true if connected (socket_type == SocketTypeUndefined)
        bool IsConnected();
        // true if graceful disconnection is in progress
        bool IsDisconnecting();

        // returns the socket type.
        // returns SocketTypeUndefined if not connected
        uint8_t GetType();

        // sets the session data (makes a copy).
        // to remove it at all, pass 0 as data.
        template<class T> void SetSessionData(T* data) {
            // delete old session data
            if (session_data)
                free(session_data);

            if (!data) {
                session_data = 0;
            } else {
                session_data_len = sizeof(T);
                session_data = malloc(session_data_len);
                memcpy(session_data, data, session_data_len);
            }
        }

        // gets the session data.
        template<class T> T* GetSessionData(bool return_copy = false) {
            // not set
            if (!session_data) return 0;
            // sizes do not match
            if (sizeof(T) != session_data_len) return 0;

            T* to_return = 0;

            if (return_copy) {
                to_return = new T();
                memcpy(to_return, session_data, session_data_len);
            } else {
                to_return = reinterpret_cast<T*>(session_data);
            }

            return to_return;
        }

        virtual ~Connection();
    };
}

#endif
