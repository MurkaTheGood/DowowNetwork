/*!
    \file

    This file defines the Connection class and related typedefs.
*/

#ifndef __DOWOW_NETWORK__CONNECTION_H_
#define __DOWOW_NETWORK__CONNECTION_H_

#include <queue>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>

#ifdef DUMP_CONNECTIONS
#include <fstream>
#include <iostream>
#include <ctime>
#endif

#include <mutex>
#include <thread>

#include "Utils.hpp"
#include "SocketType.hpp"
#include "Request.hpp"

namespace DowowNetwork {
    // declare the connection for typedef
    class Connection;

    //! The function pointer type for Request-related handlers.
    /*!
        \param conn the Connection that calls the handler
        \param req the Request that something occurs with
        \param id the ID of the Request that something occurs with

        \return
            The function must return true if the Request is processed.
    */
    typedef void (*RequestHandler)(Connection* conn, Request* req);

    /// A connection between two endpoints.
    class Connection {
    private:
        // Zero time for pselect()
        const timespec zero_time { 0, 0 };

        // main mutex
        // std::recursive_mutex mutex_main;

        // mutex for send queue
        std::recursive_mutex mutex_sq;
        // mutex for receive queue
        std::recursive_mutex mutex_rq;
        // mutex for free request id
        std::recursive_mutex mutex_fri;

        /// The ID of the free request.
        uint32_t free_request_id = 1;
        /// Should this connection occupy even request IDs or odd ones?
        bool is_even_request_parts = false;

        /// Is graceful disconnection in progress?
        bool is_disconnecting = false;

        /// The socket file descriptor.
        int socket_fd = -1;

        /// The socket type.
        /// SocketTypeUndefined is interpreted as 'not connected'
        uint8_t socket_type = SocketTypeUndefined;

        /// The maximum amount of bytes we will attempt to send
        /// at a time.
        uint32_t send_block_size = 1024;
        /// The send buffer.
        char* send_buffer = 0;
        /// The length of the send buffer.
        uint32_t send_buffer_length = 0;
        /// The offset of the send buffer.
        uint32_t send_buffer_offset = 0;
        /// The queue of requests to send.
        std::queue<Request*> send_queue;

        /// The maximum amount of bytes we will attemp to receive
        /// at a time.
        uint32_t recv_block_size = 1024;
        /// The maxiumum allowed size of the receive buffer.
        /// The connection will be closed if they'll try to violate
        /// this limit.
        uint32_t recv_buffer_max_length = 16 * 1024;
        /// The receive buffer.
        char* recv_buffer = 0;
        /// The receive buffer length.
        uint32_t recv_buffer_length = 0;
        /// The receive buffer offset.
        uint32_t recv_buffer_offset = 0;
        /// The queue of the received requests.
        std::queue<Request*> recv_queue;
        /// Is receiving the request length right now?
        bool is_recv_length = true;

        /// Session data.
        void* session_data = 0;

        /// The pointer to the default request handler.
        RequestHandler handler_default = 0;
        /// The map of the pointers to the named request handlers.
        std::map<std::string, RequestHandler> handlers_named;

        /// Whether should create a new thread for each handler instance.
        bool mt_handlers = true;

        /// Push() event
        int push_event = -1;
        /// 'to_stop' event
        int to_stop_event = -1;
        /// 'stopped' event
        int stopped_event = -1;
        /// our still-alive timer
        int our_sa_timer = -1;
        /// their not-alive timer
        int their_na_timer = -1;
        /// receive event
        int receive_event = -1;

        /// Our keep_alive interval
        time_t our_sa_interval = 10;
        /// The amount of time they are considered disconnected after.
        time_t their_na_interval = 60;

        /// Polling thread function
        static void ConnThreadFunc(Connection* conn);

        /// Check if has something to send.
        bool HasSomethingToSend();

        /// Pass the Requests through assigned handlers.
        /*!
            \return
                true if the Request is processed by some
                handler.
        */
        bool PassThroughHandlers(Request* req);
    protected:
        /// This constructor does nothing.
        /*!
            It is declared protected so that the derived classes could
            define their own initialization process.
        */
        Connection();

        /// Delete the send buffer.
        void DeleteSendBuffer();
        /// Delete the receive buffer.
        void DeleteRecvBuffer();

        /// Perform I/O operations related to receival.
        /*!
            This method will receive as much data as possible
            with one recv() call. The call is blocking, so perform
            checks for availability beforehand.

            If the Request is received:
            1.  An attempt to deserialize it will be done. On failure the
                Request is just deleted. On success it will be passed to the
                next step.
            2.  The Request will be passed through all the set handlers. If
                no handler reports successful handling, then the Request will
                be added to the receive queue.

            \return
                true if no connection errors occured.
                false if connection is lost.
        */
        bool Receive();

        /// Perform I/O operations related to sending.
        /*!
            1.  If send buffer exists then the method will send
                the data from it.
            2.  If no send buffer exists then the method will pop
                one Request from the send queue.
            3.  If the queue is empty then the method returns
                immidiately.

            In nonblocking mode it will send as much data as possible
            with one send() call. The call is blocking.

            \return
                true if no connection errors occured.
                false if connection is lost.
        */
        bool Send();

        /// Pop one element from the send queue.
        /*!
            Pops the first element of the send_queue and
            serializes it into send buffer.

            \return
                    true if there was something to pop.

            \warning
                    This method does not delete the old buffer
                    if it exists!
        */
        bool PopSendQueue();

        /// Initialize the connection with an existing socket.
        /*!
            Does nothing if already connected.
            It automatically guesses the domain of the socket,
            sets the initial values of keep_alive timers.

            \param socket_fd the socket that the connection is initialized
                with.
        */
        void InitializeByFD(int socket_fd);

        /// Set the part of the request IDs.
        /*!
            Our requests will have even IDs if we occupy the
            even part of IDs.

            \param state should we occupy the even part?
        */
        void SetEvenRequestIdsPart(bool state);
    public:
        /// The connection tag.
        /// Can be used for any purposes.
        std::string tag;

        /// Default constructor.
        //! Effectively just calls InitializeByFD().
        Connection(int socket_fd);

        void SetOurSaInterval(time_t interval);
        time_t GetOurSaInterval();

        void SetTheirNaIntervalLimit(time_t interval);
        time_t GetTheirNaIntervalLimit();

        /// MT-Safe
        Request* Push(Request* request, bool to_copy = true, int timeout = 0, bool change_request_id = true);
        /// MT-Safe
        Request* Push(Request& req, int timeout = 0, bool change_request_id = true);

        /// MT-Safe
        Request* Pull(int timeout = 0);

        /// \warning    Do not call this function in non-multithreaded handler
        //              if you are also waiting for join. Deadlock will happen.
        void Disconnect(bool forced = false, bool wait_for_join = false);
        bool IsConnected();
        bool IsDisconnecting();

        uint8_t GetType();

        int GetStoppedEvent();
        bool WaitForStop(int timeout = -1);

        void SetHandlerDefault(RequestHandler h);
        RequestHandler GetHandlerDefault();

        void SetHandlerNamed(std::string name, RequestHandler h);
        RequestHandler GetHandlerNamed(std::string name);

        void SetHandlersMT(bool state);
        bool GetHandlersMT();

        void SetSendBlockSize(uint32_t bs);
        uint32_t GetSendBlockSize();

        void SetRecvBlockSize(uint32_t bs);
        uint32_t GetRecvBlockSize();

        void SetMaxRequestSize(uint32_t size);
        uint32_t GetMaxRequestSize();

        template<class T> void SetSessionData(T* data) {
            if (!data) {
                // data is to be reset
                session_data = 0;
            } else {
                // store the data pointer
                session_data = reinterpret_cast<T*>(data);
            }
        }

        template<class T> T* GetSessionData() {
            // not set
            if (!session_data) return 0;
            return reinterpret_cast<T*>(session_data);
        }

        virtual ~Connection();
    };
}

#endif
