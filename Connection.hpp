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

#ifdef MULTITHREADING_TWEAKS
#include <mutex>
#endif

#include "SocketType.hpp"
#include "Request.hpp"

namespace DowowNetwork {
    // declare the connection for typedef
    class Connection;

    // the first argument is the connection that called the handler
    // the second argument is the request that is being handled
    // the third argument is the ID of the request
    // must return true if processed, false if further processing is needed
    typedef bool (*RequestHandler)(Connection*, Request*, uint32_t);

    /// A connection between two endpoints.
    class Connection {
    private:
        // Zero time for pselect()
        const timespec zero_time { 0, 0 };

        #ifdef MULTITHREADING_TWEAKS
        // mutex for send()
        std::recursive_mutex mutex_send;
        // mutex for recv()
        std::recursive_mutex mutex_recv;
        // mutex for send_queue
        std::recursive_mutex mutex_send_queue;
        // mutex for recv_queue
        std::recursive_mutex mutex_recv_queue;
        // mutex for free_request_id
        std::recursive_mutex mutex_free_request_id;
        // mutex for handlers logic
        std::recursive_mutex mutex_handlers;
        // mutex for subscription map
        std::recursive_mutex mutex_subscription_map;
        #else
        // these are used as placeholder
        const int mutex_send = 0;
        const int mutex_recv = 0;
        const int mutex_send_queue = 0;
        const int mutex_recv_queue = 0;
        const int mutex_free_request_id = 0;
        const int mutex_handlers = 0;
        const int mutex_subscription_map = 0;
        #endif

        /// The ID of the free request.
        uint32_t free_request_id = 1;
        /// Should this connection occupy even request IDs or odd ones?
        bool is_even_request_parts = false;

        /// The next time we should send keep_alive request.
        time_t our_next_ka = 0;
        /// If they do not send us anything by that time, the connection
        /// will be closed.
        time_t their_next_ka = 0;

        /// Our keep_alive interval
        time_t our_ka_interval = 10;
        /// The amount of time they are considered disconnected after.
        time_t their_ka_interval_limit = 60;

        /// Use built-in keep_alive mechanism?
        bool use_keep_alive = true;

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

        /// Using nonblocking I/O?
        bool nonblocking = false;

        /// Session data.
        void* session_data = 0;

        /// The map used in Execute() function to track
        /// new requests.
        std::map<uint32_t, Request*> subscription_map;

        /// The pointer to the default request handler.
        RequestHandler handler_default = 0;
        /// The map of the pointers to the named request handlers.
        std::map<std::string, RequestHandler> handlers_named;

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
            It declared protected so that the derived classes could
            define their own initialization process.
        */
        Connection();

        /// Delete the send buffer.
        void DeleteSendBuffer();
        /// Delete the receive buffer.
        void DeleteRecvBuffer();

        /// Perform I/O operations related to receival.
        /*!
            The behavior of this method differs in blocking and
            nonblocking modes:
            -   In blocking mode it will receive data until the entire
                Request is received.
            -   In nonblocking mode it will receive as much data as possible
                with one recv() call. Returns immidiately.

            If the Request is received:
            1.  An attempt to deserialize it will be done. On failure the
                Request is just dropped. On success it will be passed to the
                next step.
            2.  The Request will be passed through all the set handlers using
                PassThroughHandlers() method. If it returns true then
                the Request is deleted. If it returns false then the Request
                is added to the receive queue.
            
            \param nonblocking should perform nonblocking I/O?
        */
        void Receive(bool nonblocking);

        /// Perform I/O operations related to sending.
        /*!
            1.  If send buffer exists then the method will send
                the data from it.
            2.  If no send buffer exists then the method will pop
                one Request from the send queue.
            3.  If the queue is empty then the method returns
                immidiately.

            The behavior of this method differs in blocking and
            nonblocking modes:
            -   In blocking mode it will send data until the entire
                Request is sent.
            -   In nonblocking mode it will send as much data as possible
                with one send() call. Returns immidiately.

            \param nonblocking should perform nonblocking I/O?
        */
        void Send(bool nonblocking);

        /// Close the connection.
        void Close();

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

        /// This method is called every Poll() call.
        virtual void SubPoll();
    public:
        /// The connection tag.
        /// Can be used for any purposes.
        std::string tag;

        /// Default constructor.
        /// Effectively just calls InitializeByFD().
        Connection(int socket_fd);

        /// Set the connection to be blocking or nonblocking.
        /*!
            \param nonblocking should use nonblocking I/O?
        */
        void SetNonblocking(bool nonblocking);
        /// Is using nonblocking I/O?
        /*!
            \return
                true if nonblocking I/O is the active mode.
        */
        bool IsNonblocking();

        /// Handle the nonblocking I/O.
        /*!
            Effectively calls Send() and Receive(), maintains built-in
            keep_alive mechanism.

            \warning Must be called only if nonblocking mode is active!
        */
        void Poll();

        /// Push the Request to the remote endpoint.
        /*!
            Behavior differs:
                -   In blocking mode: will send the entire Request to the
                    remote endpoint. Will return once the operation is
                    completed or failed.
                -   In nonblocking mode: will push the Request to the queue.
                    The sending is performed in Poll() function later.

            \param request the request to send
            \param to_copy must the request be copied?
            \param change_request_id must the request ID be changed to a
                    free one?
            
            \warning
                    If the Request is not copied then you must not access
                    it after passing it to this method!
            \warning
                    change_request_id parameter must be false if you're
                    trying to send a response to another request!

            \return
                    - On success: the ID of the sent request.
                    - On failure: 0.
        */
        uint32_t Push(Request* request, bool to_copy = true, bool change_request_id = true);
        /// Push the Request to the remote endpoint.
        /*!
            Effectively calls Push(Request*, bool, bool) but automatically
            copies the request.

            \warning
                Please check 'See also' section to avoid errors!

            \sa Push(Request*, bool, bool)
        */
        uint32_t Push(Request& req, bool change_request_id = true);

        // Send the request and treat it as a command.
        // Behavior:
        //  a)  in blocking mode it will send the request and wait
        //      for response with specific ID. No separate thread is needed.
        //  b)  in non-blocking mode it will Push() the request and
        //      wait for response with its ID. WARNING! For proper
        //      functioning of this method, it must be run in a separate
        //      thread relative to Poll()ing loop (if nonblocking mode)
        Request* Execute(Request *req, bool must_copy = true, time_t timeout = 30);
        Request* Execute(Request &req, time_t timeout = 30);

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

        // set the default handler
        void SetHandlerDefault(RequestHandler h);
        // get the default handler
        RequestHandler GetHandlerDefault();

        // set the named handler
        void SetHandlerNamed(std::string name, RequestHandler h);
        // get the named handler
        RequestHandler GetHandlerNamed(std::string name);

        // the send block size
        void SetSendBlockSize(uint32_t bs);
        uint32_t GetSendBlockSize();

        // the recv block size
        void SetRecvBlockSize(uint32_t bs);
        uint32_t GetRecvBlockSize();

        // the max size of request to receive
        void SetMaxRequestSize(uint32_t size);
        uint32_t GetMaxRequestSize();

        // Sets the session data. Does not copy it.
        // To remove session data, pass 0 as data.
        // BEWARE: the data will not get deleted in the destructor,
        // you must delete the data by yourself.
        template<class T> void SetSessionData(T* data) {
            if (!data) {
                // data is to be reset
                session_data = 0;
            } else {
                // store the data pointer
                session_data = reinterpret_cast<T*>(data);
            }
        }

        // gets the session data.
        template<class T> T* GetSessionData() {
            // not set
            if (!session_data) return 0;
            return reinterpret_cast<T*>(session_data);
        }

        virtual ~Connection();
    };
}

#endif
