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
        std::recursive_mutex mutex_main;

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

        /// Our keep_alive interval
        time_t our_sa_interval = 10;
        /// The amount of time they are considered disconnected after.
        time_t their_na_interval = 60;

        /// Polling thread function
        static void ConnThreadFunc(Connection* conn);

        /// Check if has something to send.
        inline bool HasSomethingToSend() {
            return
                send_queue.size() ||
                send_buffer_length != send_buffer_offset;
        }

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

        /// Set our (local) keep-alive interval.
        /*!
            That is the interval the keep-alive requests are sent to
            the remote endpoint. The timer is reset after calling this
            function. If interval is less than 1 then it will be set to 1.

            \param interval the interval in seconds.
        */
        inline void SetOurSaInterval(time_t interval) {
            our_sa_interval = interval < 1 ? 1 : interval;
            Utils::SetTimerFdTimeout(our_sa_timer, our_sa_interval);
        }
        /// Get our (local) keep-alive interval.
        /*!
            That is the interval the keep-alive requests are sent to
            the remote endpoint.

            \return
                    Our (local) keep-alive interval.
        */
        inline time_t GetOurSaInterval() {
            return our_sa_interval;
        }

        /// Set their (remote) keep-alive interval limit.
        /*!
            The connection will be closed if we don't receive anything
            from the remote endpoint within this amount of time. The
            timer is reset after calling this function. If interval is
            less than 1 then it will be set to 1.

            \param interval the interval in seconds.
        */
        inline void SetTheirKaIntervalLimit(time_t interval) {
            their_na_interval = interval < 1 ? 1 : interval;
            Utils::SetTimerFdTimeout(their_na_timer, their_na_interval);
        }
        /// Get their (remote) keep-alive interval limit.
        /*!
            The connection will be closed if we don't receive anything
            from the remote endpoint within this amount of time.

            \return
                    Their (remote) keep-alive interval limit.
        */
        inline time_t GetTheirKaIntervalLimit() {
            return their_na_interval;
        }

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
        Request* Push(Request* request, bool to_copy = true, int timeout = 0, bool change_request_id = true);
        /// Push the Request to the remote endpoint.
        /*!
            Effectively calls Push(Request*, bool, bool) but automatically
            copies the request.

            \warning
                Please check 'See also' section to avoid errors!

            \sa Push(Request*, bool, bool)
        */
        Request* Push(Request& req, int timeout = 0, bool change_request_id = true);

        /// Pull the Request from the receive queue.
        /*!
            Behavior:
            1.  timeout > 0: wait for data for specified amount of time
            2.  timeout == 0: if there's no data - return immidiately
            3.  timeout < 0: wait for data forever

            \param
                timeout the amount of time to wait

            \return
                - no data: null-pointer
                - error: null-pointer
                - success: a valid pointer to the Request

            \warning
                You must delete the returned pointer by yourself with
                delete operator.

            \sa Push()
        */
        Request* Pull(int timeout = 0);

        /// Disconnect.
        /*!
            Performs the disconnection.

            - On forced disconnection: the connection is closed immidiately.
            - On graceful disconnection: the data from the send queue and send
                buffer is sent, then the connection is closed.

            \param forced must the forced disconnection be performed?
        */
        void Disconnect(bool forced = false, bool wait_for_join = false);
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

        /// Get the eventfd for disconnection check.
        /*!
            The returned file descriptor will become readble once
            the connection is closed.

            \warning
                You have to close the descriptor by yourself after
                poll()in it, select()ing it or doing any other multiplexing.
        */
        int GetStoppedEvent();


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
