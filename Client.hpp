/*!
    \file

    This file defines the Client class.
*/

#ifndef __DOWOW_NETWORK__CLIENT_H_
#define __DOWOW_NETWORK__CLIENT_H_

#include "Connection.hpp"
#include "SocketType.hpp"

#include <thread>
#include <mutex>

#include <sys/socket.h>

namespace DowowNetwork {
    /// Client
    /*!
        The endpoint that initiates the connection.
        It creates a socket, connects it to the server
        and initializes the connection with connected
        socket.
    */
    class Client final {
    private:
        // the socket that is connecting
        int temp_socket_fd = -1;

        // connecting thread event
        int connect_event = -1;

        // the Connection
        Connection *conn = 0;

        // the thread
        std::thread *connecting_thread = 0;

        static void ConnThreadFunc(Client *client, sockaddr addr, int timeout);
    public:
        /// Create a new client.
        /*!
            The created client will not be connected anywhere.
            \sa ConnectTcp(), ConnectUnix().
        */
        Client();

        /// Connect to a TCP server.
        bool ConnectTcp(std::string ip, uint16_t port, int timeout = 30);
        /// Connect to a UNIX server.
        bool ConnectUnix(std::string socket_path, int timeout = 30);

        /// Check if connecting right now.
        /*!
            \return true if trying to connect to the server.
            \sa IsConnected().
        */
        bool IsConnecting();

        /// Get the Connection.
        /*!
            \return Connection pointer if connected,
                    null-pointer on failure.
        */
        Connection *GetConnection();
        /// Get the Connection.
        /// \sa GetConnection()
        Connection *operator()();

        /// Client destructor.
        ~Client();
    };
}

#endif
