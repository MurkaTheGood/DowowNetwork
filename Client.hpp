#ifndef __DOWOW_NETWORK__CLIENT_H_
#define __DOWOW_NETWORK__CLIENT_H_

#include "Connection.hpp"
#include "SocketType.hpp"

namespace DowowNetwork {
    class Client : public Connection {
    private:
        // file descriptor for a socket trying to connect to the server
        int temp_socket_fd = -1;
    public:
        // creates a client that is not connected anywhere
        Client();

        // connect to a TCP server.
        // if nonblocking: returns true if ok, false if failed; use IsConnected to check status
        // if blocking: returns true if connected, false if failed
        bool ConnectTCP(std::string ip, uint16_t port, bool nonblocking = false);
        // connect to a UNIX server.
        // if nonblocking: returns true if ok, false if failed; use IsConnected to check status
        // if blocking: returns true if connected, false if failed
        bool ConnectUNIX(std::string socket_path, bool nonblocking = false);

        // handle nonblocking connecting
        void HandleConnecting();

        // returns true if trying to connect
        bool IsConnecting();
        // returns true if connected
        bool IsConnected();

        // delete
        ~Client();
    };
}

#endif
