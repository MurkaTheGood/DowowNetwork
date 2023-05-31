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
    // Predeclare the connection for typedef
    class Connection;

    //! Request-related handler prototype.
    /*!
        \param c Connection that calls the handler
        \param r Request that something occurs with
    */
    typedef void (*RequestHandler)(Connection* c, Request* r);

    //! A connection between two endpoints.
    class Connection {
    private:
        //! mutex for send queue
        std::recursive_mutex mutex_sq;
        //! mutex for receive queue
        std::recursive_mutex mutex_rq;
        //! mutex for free request id
        std::recursive_mutex mutex_fri;
        //! mutex for refs amount
        std::recursive_mutex mutex_ra;
        //! mutex for 'connected' and 'disconnecting' states
        std::recursive_mutex mutex_cd;
        //! mutex for background thread pointer
        std::recursive_mutex mutex_bt;

        //! The ID of the free request.
        uint32_t free_request_id = 1;
        //! Use even or odd request ids?
        bool is_even_request_parts = false;
        //! Is disconnection in progress?
        bool is_disconnecting = false;

        //! The socket file descriptor.
        int socket_fd = -1;

        //! The socket type.
        //! SocketTypeUndefined is interpreted as 'not connected'
        uint8_t socket_type = SocketTypeUndefined;

        //! The maximum amount of bytes we will attempt to send
        //! at a time.
        uint32_t send_block_size = 1024;
        //! The send buffer.
        const char* send_buffer = 0;
        //! The length of the send buffer.
        uint32_t send_buffer_length = 0;
        //! The offset of the send buffer.
        uint32_t send_buffer_offset = 0;
        //! The queue of requests to send.
        std::queue<Request*> send_queue;

        //! The maximum amount of bytes we will attempt to receive
        //! at a time.
        uint32_t recv_block_size = 1024;
        //! The maxiumum allowed size of the receive buffer.
        //! The connection will be closed if they'll try to violate
        //! this limit.
        uint32_t recv_buffer_max_length = 16 * 1024;
        //! The receive buffer.
        char* recv_buffer = 0;
        //! The receive buffer length.
        uint32_t recv_buffer_length = 0;
        //! The receive buffer offset.
        uint32_t recv_buffer_offset = 0;
        //! The queue of the received requests.
        std::queue<Request*> recv_queue;
        //! Is receiving the request length right now?
        bool is_recv_length = true;

        //! Session data.
        void* session_data = 0;

        //! The pointer to the default request handler.
        RequestHandler handler_default = 0;
        //! The map of the pointers to the named request handlers.
        std::map<std::string, RequestHandler> handlers_named;

        //! Push() event
        int push_event = -1;
        //! 'to_stop' event
        int to_stop_event = -1;
        //! 'stopped' event
        int stopped_event = -1;
        //! receive event
        int receive_event = -1;
        //! 'not_needed' event
        int not_needed_event = -1;
        //! our still-alive timer
        int our_sa_timer = -1;
        //! their not-alive timer
        int their_na_timer = -1;

        //! Our keep_alive interval
        time_t our_sa_interval = 10;
        //! The amount of time they are considered disconnected after.
        time_t their_na_interval = 60;

        //! Amount of links to this connection outside the library.
        uint32_t refs_amount = 0;

        //! Background thread.
        std::thread *background_thread = 0;

        //! Polling thread function.
        /*!
         * This function is running in background and
         * handling I/O and events.
         */
        static void ConnThreadFunc(Connection* conn);

        //! Check if has something to send.
        //! /return send_buffer || send_queue.size()
        bool HasSomethingToSend();

        //! Pass the Requests through assigned handlers.
        /*!
            \return
                Was the Request processed by some handler?
        */
        bool PassThroughHandlers(Request* req);
    protected:
        /// This constructor does nothing.
        /*!
            It is declared protected so that the derived classes could
            define their own initialization process.
        */
        Connection();

        //! Delete the send buffer.
        void DeleteSendBuffer();
        //! Delete the receive buffer.
        //! Also resets is_recv_length.
        void DeleteRecvBuffer();

        //! Perform receive I/O.
        /*! \return     true if no errors occured, false if the connection
         *              is broken.
         *  \warning    This function isn't MT-Safe and must only
         *              be called from within ConnThreadFunc()!
         */
        bool Receive();
        //! Perform send I/O.
        /*! Automatically calls PopSendQueue if needed.
         *  \return     true if no errors occured, false if the connection
         *              is broken.
         *  \warning    This function isn't MT-Safe and must only
         *              be called from within ConnThreadFunc()!
         */
        bool Send();

        //! send_queue -> send_buffer.
        /*! Pops the first request in send_queue and
            puts it to the send buffer.
            \return true if the queue wasn't empty.
            \warning The old buffer is not deleted!
        */
        bool PopSendQueue();

        //! Initialize the connection with connected socket.
        /*! - Automatically guesses the domain.
         *  - Creates the 'push', 'to stop' events.
         *  - Creates 'our still alive', 'their not alive' timers.
         *  - Resets the 'stopped' event.
         *  - Resets the free request id (even/odd is not touched).
         *  - Clears the receive queue.
         *  - Starts the background thread.
         *  \warning Not MT-Safe!
         */
        void InitializeByFD(int socket_fd);

        //! Set the even or odd request ids part.
        void SetEvenRequestIdsPart(bool state);
    public:
        //! Connection ID.
        uint32_t id = 0;
        //! The connection tag.
        std::string tag;

        //! Default constructor.
        //! Effectively just calls InitializeByFD().
        Connection(int socket_fd);

        //! Set 'our still alive' timer interval.
        void SetOurSaInterval(time_t interval);
        //! Get 'our still alive' timer interval.
        time_t GetOurSaInterval();

        //! Set 'their not alive' timer interval limit.
        void SetTheirNaIntervalLimit(time_t interval);
        //! Get 'their not alive' timer interval limit.
        time_t GetTheirNaIntervalLimit();

        //! Push the Request to the send queue.
        /*! MT-Safe. The request will get sent in the background thread.
         *  - timeout > 0: seconds to wait for the response
         *  - timeout == 0: do not wait for the response
         *  - timeout < 0: wait for the response infinitely long
         *  \param r the Request to send
         *  \param to_copy to copy the Request?
         *  \param timeout how long to wait for response?
         *  \param change_id to change the request ID to a free one?
         *  \return
         *      -   If timeout is non-zero then a pointer to the response [YOURS]
         *          or null-pointer (on disconnection or timed out)
         *      -   If timeout is zero then null-pointer.
         *  \warning
         *      [YOURS] Please make sure you delete the returned Request
         *      when it's not needed anymore.
         */
        Request* Push(Request* r, bool to_copy = true, int timeout = 0, bool change_id = true);
        //! \sa Push(Request*, bool, int, bool) 
        Request* Push(const Request& r, int timeout = 0, bool change_id = true);

        //! Pull the request from the receive queue.
        /*! MT-Safe.
         *  \param timeout how long to wait for request?
         *  \return The pulled request on success, null-pointer if
         *          the queue is empty.
         */
        Request* Pull(int timeout = 0);

        /// MT-Safe
        uint32_t GetRefs();
        /// MT-Safe
        void IncreaseRefs();
        /// MT-Safe
        void DecreaseRefs();

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
