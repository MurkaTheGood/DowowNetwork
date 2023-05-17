/*!
    \file

    This file defines the Server class and related typedefs.
*/

#ifndef __DOWOW_NETWORK__SERVER_H_
#define __DOWOW_NETWORK__SERVER_H_

#include <string>
#include <list>

#include "Connection.hpp"
#include "Request.hpp"
#include "SocketType.hpp"

namespace DowowNetwork {
    // declaration for typedef below
    class Server;

    //! The function pointer type for Server-Connection-related handlers.
    /*!
        \param server the server that calls the handler
        \param conn the connection that something occurs with
    */
    typedef void (*ConnectionHandler)(Server *server, Connection *conn);

    /// Server.
    /*!
        The endpoint that accepts the connections.
    */
    class Server {
    private:
        // the file descriptor of server socket
        int socket_fd = -1;

        // is non-blocking?
        bool nonblocking = false;

        // used socket type (undefined by default)
        uint8_t socket_type = SocketTypeUndefined; 

        // unix
        std::string unix_socket_path;

        // tcp (host byteorder)
        uint32_t tcp_socket_address;
        uint16_t tcp_socket_port;

        // handler for new connections
        ConnectionHandler new_connection_handler;
    public:
        /// Server constructor.
        Server();

        /// Set whether the server must be nonblocking.
        /*!
            - In nonblocking mode the accept operations return immidiately.
            - In blocking mode the accept operations return once someone is
                accepted or an error occurs.

            \param nonblocking whether the Server must be nonblocking

            \sa Accept(), AcceptOne().
        */
        void SetNonblocking(bool nonblocking);

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


        /// Accept pending clients and add them to the list.
        /*!
            Behavior differs:
            -   In blocking mode: will block the execution until someone
                connects (no timeout at all).
            -   In nonblocking mode: will return immidiately after accepting
                pending clients and adding them to the list.

            \param connections the list of connections.

            \warning The method caller is the new owner of new Connections.
                    Delete them using delete operator.

            \sa AcceptOne().
        */
        void Accept(std::list<Connection*>& connections);

        /// Accept one client.
        /*!
            Behavior differs:
            -   In blocking mode: will block the execution until someone
                connects (no timeout at all).
            -   In nonblocking mode: accept the first client in queue of
                pending clients, return.

            \return
                - If someone connected: pointer to the new connection.
                - If nobody connected: null-pointer.

            \warning The method caller is the new owner of a new Connection.
                    Delete it using delete operator.

            \sa Accept().
        */
        Connection* AcceptOne();

        /// Set the New Connection Handler.
        /*!
            That handler is called when a new client is accepted.

            \param handler the new handler (or 0 to reset)
        */
        void SetNewConnectionHandler(ConnectionHandler handler);
        /// Get the New Connection Handler.
        /*!
            \return Assigned New Connection Handler.
        */
        ConnectionHandler GetNewConnectionHandler();

        /// Close the server.
        /*!
            Will close the server socket.

            \warning
                    All accepted connection are in your ownership! You
                    must close them by yourself!
        */
        void Close();

        /// Server destructor.
        /*!
            Calls Close() method.
        */
        ~Server();
    };
}

#endif
