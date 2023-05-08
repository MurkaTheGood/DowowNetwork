#include "Client.hpp"

#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>

#include "Utils.hpp"

DowowNetwork::Client::Client() : Connection() {

}

bool DowowNetwork::Client::ConnectTCP(std::string ip, uint16_t port, bool nonblocking) {
    // check if connected or connecting
    if (!IsClosed() || IsConnecting()) return false;

    // save the block state
    this->nonblocking = nonblocking;

    // temp socket fd
    temp_socket_fd = socket(AF_INET, SOCK_STREAM | (nonblocking ? SOCK_NONBLOCK : 0), 0);
    // failed to create the temp socket
    if (temp_socket_fd == -1) return false;

    // create the address to connect to
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(port);
    // ip
    if (!inet_aton(ip.c_str(), &addr.sin_addr)) {
        // invalid
        close(temp_socket_fd);
        temp_socket_fd = -1;
        return false;
    }

    // connect
    int connect_res = connect(temp_socket_fd, (sockaddr*)&addr, sizeof(addr));

    if (connect_res == -1) {
        if (!nonblocking) {
            // the socket is blocking and we failed to connect
            close(temp_socket_fd);
            temp_socket_fd = -1;
            return false;
        }
        // the socket is nonblocking, connecting in background
        if (errno == EINPROGRESS) {
            return true;
        }
        // error
        close(temp_socket_fd);
        temp_socket_fd = -1;
        return false;
    }

    // connected
    InitializeByFD(temp_socket_fd);
    temp_socket_fd = -1;

    return true;
}

bool DowowNetwork::Client::ConnectUNIX(std::string socket_path, bool nonblocking) {
    // check if connected or connecting
    if (!IsClosed() || IsConnecting()) return false;

    // save the block state
    this->nonblocking = nonblocking;

    // temp socket fd
    temp_socket_fd = socket(AF_UNIX, SOCK_STREAM | (nonblocking ? SOCK_NONBLOCK : 0), 0);
    // failed to create the temp socket
    if (temp_socket_fd == -1) return false;

    // create the address to connect to
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path.c_str(), socket_path.size() + 1);

    // connect
    int connect_res = connect(temp_socket_fd, (sockaddr*)&addr, sizeof(addr));

    if (connect_res == -1) {
        if (!nonblocking) {
            // the socket is blocking and we failed to connect
            close(temp_socket_fd);
            temp_socket_fd = -1;
            return false;
        }
        // the socket is nonblocking, connecting in background
        if (errno == EINPROGRESS) {
            return true;
        }
        // error
        close(temp_socket_fd);
        temp_socket_fd = -1;
        return false;
    }

    // connected
    InitializeByFD(temp_socket_fd);
    temp_socket_fd = -1;

    return true;
}

void DowowNetwork::Client::HandleConnecting() {
    // do nothing if not connecting
    if (!IsConnecting()) return;

    if (Utils::SelectWrite(temp_socket_fd)) {
        // check if succeed
        int socket_error;
        socklen_t socket_error_len = sizeof(socket_error);
        getsockopt(
            temp_socket_fd,
            SOL_SOCKET,
            SO_ERROR,
            &socket_error,
            &socket_error_len);

        // error?
        if (socket_error) {
            close(temp_socket_fd);
        } else {
            InitializeByFD(temp_socket_fd);
        }
        temp_socket_fd = -1;
    }
}

bool DowowNetwork::Client::IsConnecting() {
    return temp_socket_fd != -1;
}

bool DowowNetwork::Client::IsConnected() {
    // the socket is connected if it's not closed nor connecting.
    return !IsClosed() && !IsConnecting();
}

DowowNetwork::Client::~Client() {
    
}
