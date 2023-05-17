/*!
    \file

    This file defines the Client class.
*/

#ifndef __DOWOW_NETWORK__CLIENT_H_
#define __DOWOW_NETWORK__CLIENT_H_

#include "Connection.hpp"
#include "SocketType.hpp"

namespace DowowNetwork {
    /// Client
    /*!
        The endpoint that initiates the connection.
        This class is derived from connection as they share a lot.
    */
    class Client : public Connection {
    private:
        // file descriptor for a socket trying to connect to the server
        int temp_socket_fd = -1;
    protected:
        void SubPoll() final;
    public:
        /// Create a new client.
        /*!
            The created client will not be connected anywhere.

            \param nonblocking must be nonblocking?
            \sa ConnectTcp(), ConnectUnix().
        */
        Client(bool nonblocking = false);

        /// Connect to a TCP server.
        /*!
            Behavior differs:
            -   In blocking mode blocks until gets connected or an error occurs.
            -   In nonblocking mode will begin connecting to the server and
                return immidiately.

            \return
                -   Blocking: true if connected, false if not.
                -   Nonblocking: true if began to connect, false on error.
                    If returns true then IsConnected() and IsConnecting()
                    must be used to check status.

            \param ip the string representation of server IP (something like 127.0.0.1, 192.168.1.1 and so on...)
            \param port the server port

            \sa IsConnected(), IsConnecting().
        */
        bool ConnectTcp(std::string ip, uint16_t port);
        /// Connect to a UNIX server.
        /*!
            Behavior differs:
            -   In blocking mode blocks until gets connected or an error occurs.
            -   In nonblocking mode will begin connecting to the server and
                return immidiately.

            \return
                -   Blocking: true if connected, false if not.
                -   Nonblocking: true if began to connect, false on error.
                    If returns true then IsConnected() and IsConnecting()
                    must be used to check status.

            \param socket_path the path to the UNIX socket we're connecting to

            \sa IsConnected(), IsConnecting().
        */
        bool ConnectUnix(std::string socket_path);

        /// Check if connecting right now.
        /*!
            \return
                true if trying to connect to the server.

            \sa IsConnected().
        */
        bool IsConnecting();

        /// Client destructor.
        /*!
            Disconnects if needed.
        */
        ~Client();
    };
}

#endif
