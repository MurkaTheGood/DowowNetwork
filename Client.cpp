#include "Client.hpp"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <endian.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <time.h>
#include <fcntl.h>
#include <thread>
#include <errno.h>

#include <iostream>
using namespace std;

#include "Utils.hpp"

void DowowNetwork::Client::TcpThreadFunc(Client *c, std::string ip, uint16_t port, int timeout) {
    // connection in progress
    std::lock_guard<std::mutex> __tsfdm(c->mutex_tsfd);

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
        Utils::WriteEventFd(c->connect_event, 1);
        return;
    }

    // connect
    int connect_res = connect(
        c->temp_socket_fd,
        (sockaddr*)&addr,
        sizeof(addr));

    // in progress
    if (connect_res == -1 && errno == EINPROGRESS) {
        // wait for result
        connect_res = !Utils::SelectRead(c->temp_socket_fd, timeout);
    }

    // fail
    if (connect_res) {
        // fail
        close(c->temp_socket_fd);
        c->temp_socket_fd = -1;
        // notify
        Utils::WriteEventFd(c->connect_event, 1);
        return;
    }

    // connected
    c->InitializeByFD(c->temp_socket_fd);
    // unset temp socket fd
    c->temp_socket_fd = -1;

    // setup the request id part
    c->SetEvenRequestIdsPart(true);

    // notify
    Utils::WriteEventFd(c->connect_event, 1);
}

void DowowNetwork::Client::UnixThreadFunc(Client *c, std::string socket_path, int timeout) {
    // connection in progress
    std::lock_guard<std::mutex> __tsfdm(c->mutex_tsfd);

    // create the address to connect to
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path.c_str(), socket_path.size() + 1);

    // connect
    int connect_res = connect(c->temp_socket_fd, (sockaddr*)&addr, sizeof(addr));

    // in progress
    if (connect_res == -1 && errno == EINPROGRESS) {
        // wait for result
        connect_res = !Utils::SelectRead(c->temp_socket_fd, timeout);
    }

    // fail
    if (connect_res) {
        // fail
        close(c->temp_socket_fd);
        c->temp_socket_fd = -1;
        // notify
        Utils::WriteEventFd(c->connect_event, 1);
        return;
    }

    // connected
    c->InitializeByFD(c->temp_socket_fd);
    c->temp_socket_fd = -1;

    // request part setup
    c->SetEvenRequestIdsPart(true);

    // notify
    Utils::WriteEventFd(c->connect_event, 1);
}

DowowNetwork::Client::Client() : Connection() {

}


bool DowowNetwork::Client::ConnectTcp(std::string ip, uint16_t port, int timeout) {
    // check if connected or connecting
    if (IsConnected() || IsConnecting())
        return false;

cout << "A" << endl;
    // temp socket fd
    temp_socket_fd =
        socket(AF_INET, SOCK_STREAM, 0);

    // failed to create the temp socket
    if (temp_socket_fd == -1)
        return false;
cout << "A" << endl;

    // make socket nonblocking
    int nb_state = 1;
    fcntl(temp_socket_fd, F_SETFL, nb_state);
cout << "A" << endl;

    // create event
    connect_event = eventfd(0, 0);
cout << "A" << endl;

    std::thread t(TcpThreadFunc, this, ip, port, timeout);
    t.detach();

    // read
    Utils::ReadEventFd(connect_event, -1);
cout << "O" << endl;

    // close event
    close(connect_event);
    connect_event = -1;

    // return the result
    return IsConnected();
}

bool DowowNetwork::Client::ConnectUnix(std::string socket_path, int timeout) {
    // check if connected or connecting
    if (IsConnected() || IsConnecting())
        return false;

    // temp socket fd
    temp_socket_fd =
        socket(AF_UNIX, SOCK_STREAM, 0);

    // failed to create the temp socket
    if (temp_socket_fd == -1)
        return false;

    // make socket nonblocking
    int nb_state = 1;
    fcntl(temp_socket_fd, F_SETFL, nb_state);

    // create event
    connect_event = eventfd(0, 0);

    // create a thread for connection
    std::thread t(UnixThreadFunc, this, socket_path, timeout);
    t.detach();

    // read
    Utils::ReadEventFd(connect_event, -1);

    // close event
    close(connect_event);
    connect_event = -1;

    // return the result
    return IsConnected();
}

bool DowowNetwork::Client::IsConnecting() {
    return temp_socket_fd != -1;
}

DowowNetwork::Client::~Client() {
    // connection in progress - wait for it to finish
    std::lock_guard<std::mutex> __tsfdm(mutex_tsfd);
}
