/*!
    \file

    This file defines the Server class and related typedefs.
*/

#ifndef __DOWOW_NETWORK__SERVER_H_
#define __DOWOW_NETWORK__SERVER_H_

#include <string>
#include <list>

#include <sys/eventfd.h>
#include <mutex>

#include "Connection.hpp"
#include "Request.hpp"
#include "SocketType.hpp"

namespace DowowNetwork {
    // declaration for typedef below
    class Server;

    typedef void (*ConnectionHandler)(Server *server, Connection *conn);

    class Server {
    private:
        // server socket
        int socket_fd = -1;
        // accepted clients
        std::list<Connection*> connections;

        // socket type (undefined if server is not running)
        uint8_t socket_type = SocketTypeUndefined; 

        // unix socket path
        std::string unix_socket_path;

        // tcp (host byteorder)
        uint32_t tcp_socket_address;
        uint16_t tcp_socket_port;

        //! 'stopped' event.
        //! Occurs when the server is stopped.
        //! Being created only when the shutdown is requested.
        int stopped_event = -1;
        //! 'to_stop' event.
        //! Occurs when the server shutdown is requested.
        int to_stop_event = -1;

        // mutex for any operations
        std::recursive_mutex mutex_server;

        //! Handler for new connections.
        //! Called right after the polling thread for
        //! connection is started.
        ConnectionHandler connected_handler;
        //! Handler for disconnection.
        //! Called
        ConnectionHandler disconnected_handler;

        /// Accept one client.
        Connection* AcceptOne();

        static void ServerThreadFunc(Server *server);
    public:
        /// Server constructor.
        Server();

        /// Start the UNIX domain server.
        /*!
            Does nothing if already started.

            \param socket_path the path to the UNIX socket (will be created)
            \param delete_old_file whether the old file must be deleted if it exists

            \return
                true on success, false on failure

            \warning
                If delete_old_file is false but the file exists then an
                error will likely occur.
        */
        bool StartUnix(std::string socket_path, bool delete_old_file = true);
        /// Get the UNIX socket path.
        /*!
            \return
                The path to the UNIX socket.

            \warning
                If the server is not running in UNIX domain then this method
                should not be called.

            \sa StartUnix();
        */
        std::string GetUnixPath();

        /// Start the TCP domain server.
        /*!
            Does nothing if already started.

            \param ip the ip to use in format "127.0.0.1" or "0.0.0.0" and
                    so on. Hostnames are not supported!
            \param port the port to use

            \return
                true on success, false on failure.
        */
        bool StartTcp(std::string ip, uint16_t port);
        /// Get the TCP IP as 32-bit unsigned integer.
        /*!
            \return The IP of the server as 32-bit unsigned integer in host
                    byteorder.

            \warning
                If the server is not running in TCP domain then this method
                should not be called.

            \sa StartTcp();
        */
        uint32_t GetTcpIp();
        /// Get the TCP IP as string.
        /*!
            \return The IP of the server in string format.

            \warning
                If the server is not running in TCP domain then this method
                should not be called.

            \sa StartTcp();
        */
        std::string GetTcpIpString();
        /// Get the TCP port.
        /*!
            \return The port of the server as 16-bit unsigned integer in
                    host byteorder.

            \warning
                If the server is not running in TCP domain then this method
                should not be called.

            \sa StartTcp();
        */
        uint16_t GetTcpPort();

        /// Get the socket type.
        /*!
            \return
                The socket type as defined in enum SocketType.
                Undefined type means the server isn't started.

            \sa SocketType.hpp.
        */
        uint8_t GetType();


        /// Set the 'connected' handler.
        inline void SetConnectedHandler(ConnectionHandler handler) {
            mutex_server.lock();
            connected_handler = handler;
            mutex_server.unlock();
        }
        /// Get the 'connected' handler.
        inline ConnectionHandler GetConnectedHandler() {
            return connected_handler;
        }
        /// Set the 'disconnected' handler.
        inline void SetDisconnectedHandler(ConnectionHandler handler) {
            mutex_server.lock();
            disconnected_handler = handler;
            mutex_server.unlock();
        }
        /// Get the 'disconnected' handler.
        inline ConnectionHandler GetDisconnectedHandler() {
            return disconnected_handler;
        }

        //! Close the server.
        void Stop(int timeout = 0);

        //! Wait for server to stop.
        void WaitForStop(int timeout = -1);

        /// Server destructor.
        /*!
            Calls Stop(-1) method.
        */
        ~Server();
    };
}

#endif
