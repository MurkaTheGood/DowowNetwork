/*!
    \file

    This file defines the Client class.
*/

#ifndef __DOWOW_NETWORK__CLIENT_H_
#define __DOWOW_NETWORK__CLIENT_H_

#include "Connection.hpp"
#include "SocketType.hpp"

#include <mutex>

namespace DowowNetwork {
    /// Client
    /*!
        The endpoint that initiates the connection.
        This class is derived from connection as they share a lot.
    */
    class Client : public Connection {
    private:
        // mutex for temp socket
        std::mutex mutex_tsfd;

        // file descriptor for a socket trying to connect to the server
        int temp_socket_fd = -1;

        // connecting thread event
        int connect_event = -1;

        static void TcpThreadFunc(Client *client, std::string ip, uint16_t port, int timeout = 30);
        static void UnixThreadFunc(Client *client, std::string unix_path, int timeout = 30);
    protected:
    public:
        /// Create a new client.
        /*!
            The created client will not be connected anywhere.

            \param nonblocking must be nonblocking?
            \sa ConnectTcp(), ConnectUnix().
        */
        Client();

        /// Connect to a TCP server.
        bool ConnectTcp(std::string ip, uint16_t port, int timeout = 30);
        /// Connect to a UNIX server.
        bool ConnectUnix(std::string socket_path, int timeout = 30);

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
