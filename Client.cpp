#include "Client.hpp"

#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <time.h>
#include <thread>

#include "Utils.hpp"

void DowowNetwork::Client::TcpThreadFunc(Client *c, std::string ip, uint16_t port, int timeout) {
    // create the address to connect to
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(port);
    // ip
    if (!inet_aton(ip.c_str(), &addr.sin_addr)) {
        // invalid
        close(c->temp_socket_fd);
        c->temp_socket_fd = -1;
        // notify
        Utils::WriteToEventFd(c->connect_event, 1);
        return;
    }

    // connect
    int connect_res = connect(
        c->temp_socket_fd,
        (sockaddr*)&addr,
        sizeof(addr));

    if (connect_res == -1) {
        // fail
        close(c->temp_socket_fd);
        c->temp_socket_fd = -1;
        // notify
        Utils::WriteToEventFd(c->connect_event, 1);
        return;
    }

    // connected
    c->InitializeByFD(c->temp_socket_fd);
    c->temp_socket_fd = -1;

    // setup the request id part
    c->SetEvenRequestIdsPart(true);

    // notify
    Utils::WriteToEventFd(c->connect_event, 1);
}

void DowowNetwork::Client::UnixThreadFunc(Client *c, std::string socket_path, int timeout) {
    // create the address to connect to
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path.c_str(), socket_path.size() + 1);

    // connect
    int connect_res = connect(c->temp_socket_fd, (sockaddr*)&addr, sizeof(addr));

    if (connect_res == -1) {
        // fail
        close(c->temp_socket_fd);
        c->temp_socket_fd = -1;
        // notify
        Utils::WriteToEventFd(c->connect_event, 1);
        return;
    }

    // connected
    c->InitializeByFD(c->temp_socket_fd);
    c->temp_socket_fd = -1;

    // request part setup
    c->SetEvenRequestIdsPart(true);

    // notify
    Utils::WriteToEventFd(c->connect_event, 1);
}

DowowNetwork::Client::Client() : Connection() {

}


void DowowNetwork::Client::ConnectTcp(std::string ip, uint16_t port, int timeout) {
    // check if connected or connecting
    if (IsConnected() || IsConnecting())
        return;

    // temp socket fd
    temp_socket_fd =
        socket(AF_INET, SOCK_STREAM, 0);
    // failed to create the temp socket
    if (temp_socket_fd == -1)
        return;

    // create an event for connect
    connect_event = eventfd(0, 0);

    std::thread t(TcpThreadFunc, this, ip, port, timeout);
    t.detach();

    // wait for thread to exit
    pollfd pollfds { connect_event, POLLIN, 0 };
    poll(&pollfds, 1, timeout);
}

void DowowNetwork::Client::ConnectUnix(std::string socket_path, int timeout) {
    // check if connected or connecting
    if (IsConnected() || IsConnecting())
        return;

    // temp socket fd
    temp_socket_fd =
        socket(AF_UNIX, SOCK_STREAM, 0);

    // failed to create the temp socket
    if (temp_socket_fd == -1)
        return;

    std::thread t(UnixThreadFunc, this, socket_path, timeout);
    t.detach();

    // wait for thread to exit
    pollfd pollfds { connect_event, POLLIN, 0 };
    poll(&pollfds, 1, timeout);
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
