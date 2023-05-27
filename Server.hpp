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
#include "SafeConnection.hpp"
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
        // maximum amount of connected clients, negative for unlimited
        int32_t max_connections = -1;

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

        bool StartUnix(std::string socket_path, bool delete_old_file = true);
        std::string GetUnixPath();

        bool StartTcp(std::string ip, uint16_t port);
        uint32_t GetTcpIp();
        std::string GetTcpIpString();
        uint16_t GetTcpPort();

        uint8_t GetType();

        void SetMaxConnections(int32_t c);
        int32_t GetMaxConnections();

        SafeConnection *GetConnection(std::string tag);
        SafeConnection *GetConnection(uint32_t id);

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
