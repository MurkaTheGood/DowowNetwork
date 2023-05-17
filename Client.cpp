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

void DowowNetwork::Client::SubPoll() {
    // do nothing if not connecting
    if (!IsConnecting()) return;

    // check if can write (i.e. send data)
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
            // set even request IDs part
            SetEvenRequestIdsPart(true);
        }
        temp_socket_fd = -1;
    }
}

DowowNetwork::Client::Client(bool nonblocking) : Connection() {
    // set the nonblocking state
    SetNonblocking(nonblocking);
}


bool DowowNetwork::Client::ConnectTcp(std::string ip, uint16_t port) {
    // check if connected or connecting
    if (IsConnected() || IsConnecting())
        return false;

    // temp socket fd
    temp_socket_fd =
        socket(AF_INET, SOCK_STREAM | (IsNonblocking() ? SOCK_NONBLOCK : 0), 0);
    // failed to create the temp socket
    if (temp_socket_fd == -1)
        return false;

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
        if (!IsNonblocking()) {
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

    // setup the request id part
    SetEvenRequestIdsPart(true);

    return true;
}

bool DowowNetwork::Client::ConnectUnix(std::string socket_path) {
    // check if connected or connecting
    if (IsConnected() || IsConnecting())
        return false;

    // temp socket fd
    temp_socket_fd =
        socket(AF_UNIX, SOCK_STREAM | (IsNonblocking() ? SOCK_NONBLOCK : 0), 0);

    // failed to create the temp socket
    if (temp_socket_fd == -1)
        return false;

    // create the address to connect to
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path.c_str(), socket_path.size() + 1);

    // connect
    int connect_res = connect(temp_socket_fd, (sockaddr*)&addr, sizeof(addr));

    if (connect_res == -1) {
        if (!IsNonblocking()) {
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

bool DowowNetwork::Client::IsConnecting() {
    return temp_socket_fd != -1;
}

DowowNetwork::Client::~Client() {
    // close the connecting socket
    if (temp_socket_fd != -1) {
        close(temp_socket_fd);
    }
}
