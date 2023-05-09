#ifndef __DOWOW_NETWORK__SERVER_H_
#define __DOWOW_NETWORK__SERVER_H_

#include <string>
#include <list>

#include "Connection.hpp"
#include "Request.hpp"
#include "SocketType.hpp"

namespace DowowNetwork {
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
    public:
        Server();

        // Set the server to be blocking/nonblocking
        void SetNonblocking(bool nonblocking);

        // Create the UNIX socket server on specified path.
        // If the file on socket_path exists, it will be deleted if delete_old_file == true.
        // Returns true of success, false on failure.
        bool StartUnix(std::string socket_path, bool delete_old_file = true);
        // Get the unix socket path
        std::string GetUnixPath();

        // Create the TCP socket server on specified address.
        // Returns true on success, false on failure.
        bool StartTcp(std::string ip, uint16_t port);
        // Get the TCP IP
        uint32_t GetTcpIp();
        // Get the TCP ip as string
        std::string GetTcpIpString();
        // Get the TCP port
        uint16_t GetTcpPort();

        // Get the socket type.
        // Returns undefined type if failed to start the server.
        uint8_t GetType();

        // Accept new clients and add them to list.
        // !Blocking mode: block the execution until someone connects (no timeout)!
        // !Nonblocking mode: will return immidiately. If there were clients awaiting
        //          for connection, they will get connected and added to the list
        //          passed by the reference.
        void Accept(std::list<Connection*>& connections);

        // Accept only one connection
        // Returns zero-pointer if impossible.
        // !Blocking mode: block the execution until someone connects (no timeout)!
        // !Nonblocking mode: will return immidiately. If there were clients awaiting
        //          for connection, only the first will get connected.
        Connection* AcceptOne();

        // close the server
        void Close();

        ~Server();
    };
}

#endif
