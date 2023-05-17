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

#ifdef MULTITHREADING_TWEAKS
#include <mutex>
#endif

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
    typedef bool (*RequestHandler)(Connection* conn, Request* req, uint32_t id);

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
        //! Effectively just calls InitializeByFD().
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

        /// Push the Request to the remote endpoint and wait for response.
        /*!
            Behavior differs in blocking and nonblocking modes:
            1.  In blocking mode: will Push() the Request and return only
                when the response is received (or a problem occured)
            2.  In nonblocking mode: will Push() the Request, subscribe for
                its ID and wait for response. Poll()ing must be done in a
                separate thread.

            \param request the request to execute
            \param must_copy must the request get copied or stolen?
            \param timeout the max time to wait for response.

            \return
                - On success: the response (you must delete it by yourself
                    with delete operator)
                - On failure or timeout: null pointer.

            \warning
                    If the Connection is nonblocking then the polling
                    must be done in a separate thread.

            \sa Push(), Poll().
        */
        Request* Execute(Request *request, bool must_copy = true, time_t timeout = 30);
        /// Push the Request to the remote endpoint and wait for response.
        /*!
            \sa Execute(Request*, bool, time_t)
        */
        Request* Execute(Request &request, time_t timeout = 30);

        /// Pull the Request from the receive queue.
        /*!
            \return
                - If the receive queue is empty then null pointer.
                - If the receive queue is not empty then the first
                    Request from it.

            \warning
                You must delete the returned pointer by yourself with
                delete operator.

            \sa Push(), Execute()
        */
        Request* Pull();

        /// Disconnect.
        /*!
            Performs the disconnection.

            - On forced disconnection: the connection is closed immidiately.
            - On graceful disconnection: the data from the send queue and send
                buffer is sent, then the connection is closed.

            \param forced must the forced disconnection be performed?
        */
        void Disconnect(bool forced = false);
        /// Check if connected.
        /*!
            \return
                true if connected.
        */
        bool IsConnected();
        /// Check if disconnection is in progress.
        /*!
            \return
                true if Disconnect() was called and the connection
                will be closed soon.
            
            \warning
                No new requests must be pushed while disconnecting!
        */
        bool IsDisconnecting();

        /// Get the socket type.
        /*!
            \return
                The type of the socket, as defined in SocketType enum.

            \warning
                If not connected then SocketTypeUndefined is returned.

            \sa SocketType.hpp.
        */
        uint8_t GetType();

        /// Set the default handler.
        /*!
            This handler will be called if conditions are satisfied:
            - no named handlers were called,
            - the request is not pushed via Execute() method.

            \param h the handler to set
        */
        void SetHandlerDefault(RequestHandler h);
        /// Get the default handler.
        /*!
            \return The default handler. Null pointer if not set.
        */
        RequestHandler GetHandlerDefault();

        /// Set the named handler.
        /*!
            Set null pointer to remove the handler.

            \param name the name of the handler
            \param h the handler to set
        */
        void SetHandlerNamed(std::string name, RequestHandler h);
        /// Get the named handler.
        /*!
            \param name the name of the handler

            \return
                - If the handler is set: the handler
                - If no handler is set: null pointer
        */
        RequestHandler GetHandlerNamed(std::string name);

        /// Set the size of the send block.
        /*!
            \param bs the new size of the send block (maximum
                    amount of bytes that can be sent per
                    one send() call)

            \warning
                    Setting this parameter to low value
                    will decrease the transfer speed.

            \sa GetSendBlockSize().
        */
        void SetSendBlockSize(uint32_t bs);
        /// Get the size of the send block
        /*!
            \return
                The size of the send block.

            \sa SetSendBlockSize().
        */
        uint32_t GetSendBlockSize();

        /// Set the size of the receive block.
        /*!
            \param bs the new size of the receive block (maximum
                    amount of bytes that can be received per
                    one recv() call)

            \warning
                    Setting this parameter to low value
                    will decrease the transfer speed.

            \sa GetRecvBlockSize().
        */
        void SetRecvBlockSize(uint32_t bs);
        /// Get the size of the receive block
        /*!
            \return
                The size of the receive block.

            \sa SetRecvBlockSize().
        */
        uint32_t GetRecvBlockSize();

        /// Set the maximum size of Request to receive.
        /*!
            If the request that is to be received has size
            more than this value then the Connection will be
            forcefully closed.

            \param size the maximum size of Request that
                    can be received.

            \warning
                    Beware that the Request that violates this
                    limit will cause the disconnection! Do not set
                    too small values!

            \sa GetMaxRequestSize().
        */
        void SetMaxRequestSize(uint32_t size);
        /// Get the maximum size of Request to receive.
        /*!
            \return
                    The maximum size of the Request that can
                    be received.

            \sa SetMaxRequestSize().
        */
        uint32_t GetMaxRequestSize();

        /// Set the session data.
        /*!
            This function stores the pointer to the session
            data object so that you could access it later without
            using additional maps. This function does not copy the
            passed session data object.

            \param data the pointer to the session data to set.
            \warning
                    The session data is not copied! The data won't
                    get deleted in the destructor, you have to delete
                    it by yourself when it is no longer needed.
             \sa GetSessionData().
        */
        template<class T> void SetSessionData(T* data) {
            if (!data) {
                // data is to be reset
                session_data = 0;
            } else {
                // store the data pointer
                session_data = reinterpret_cast<T*>(data);
            }
        }

        /// Get the session data.
        /*!
            This function will cash the untyped session data pointer
            saved in the Connection to the specfied type.
            If no session data is set then null pointer is returned.

            \return
                    The pointer to the session data.

            \sa SetSessionData().
        */
        template<class T> T* GetSessionData() {
            // not set
            if (!session_data) return 0;
            return reinterpret_cast<T*>(session_data);
        }

        /// The connection destructor.
        /*!
            \warning
                    This destructor does not delete the session data!

            \sa SetSessionData().
        */
        virtual ~Connection();
    };
}

#endif
