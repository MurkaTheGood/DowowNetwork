/*!
    \file

    This file defines the Connector class.
*/

#ifndef __DOWOW_NETWORK__CONNECTOR_H_
#define __DOWOW_NETWORK__CONNECTOR_H_

#include "Connection.hpp"
#include "SocketType.hpp"

#include <thread>
#include <mutex>

#include <sys/socket.h>

namespace DowowNetwork {
    //! Connector
    /*!
        Connector is an entity that manages
        the connection to the server in the
        background.
    */
    class Connector final {
    private:
        //! An event that happens when the
        //! background thread quits.
        int bg_thread_event = -1;
        //! The thread that manages the
        //! connection process.
        std::thread *connecting_thread = 0;
        //! The resulting Connection.
        Connection *conn = 0;

        //! errno of background thread.
        int connector_errno = 0;

        //! The event for forced termination of
        //! the background thread
        int forced_stop_event = -1;

        //! Mutex
        std::recursive_mutex universal_mutex;

        //! The function that manages the
        //! connection process.
        static void ConnThreadFunc(Connector *c, sockaddr addr, int timeout);
    public:
        //! Create a new connector.
        /*!
            \sa ConnectTcp(), ConnectUnix().
        */
        Connector();

        //! Begin to connect to a TCP server in background.
        bool ConnectTcp(std::string ip, uint16_t port, int timeout = 30, bool wait_for_finish = true);
        //! Begin to connect to a UNIX server in background.
        bool ConnectUnix(std::string socket_path, int timeout = 30, bool wait_for_finish = true);

        //! Wait for success/failure.
        /*! \return true if connected, false if not or timed out.
            \param t timeout in seconds
        */
        bool Wait(int t = -1);

        //! Cancels the background thread safely.
        void Cancel();

        //! Check if connecting right now.
        /*!
            \return true if trying to connect to the server.
        */
        bool IsConnecting();

        //! Get the connect event.
        /*! \warning Do not close returned FD.
         */
        int GetConnectEvent();

        /// Get the Connection.
        /*!
            \warning The returned pointer is owned by you, when
                     it is returned.
            \return Connection pointer if connected,
                    null-pointer on failure or not connected yet.
        */
        Connection *GetConnection();

        /// Get the errno.
        int GetErrno();

        /// Connector destructor.
        ~Connector();
    };
}

#endif
