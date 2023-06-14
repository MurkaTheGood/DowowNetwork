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

void DowowNetwork::Client::ConnThreadFunc(Client *c, sockaddr addr, int timeout) {
    // temp socket file descriptor
    int temp_socket = socket(addr.sa_family, SOCK_STREAM | SOCK_NONBLOCK, 0);

    // failed to create the temp socket
    if (temp_socket == -1) {
        Utils::WriteEventFd(c->connect_event, 1);
        return;
    }

    // connect
    int connect_res = connect(temp_socket, &addr, sizeof(addr));

    // checking if error, and error means "in progress"
    if (connect_res == -1 && errno == EINPROGRESS) {
        // wait for result by polling the socket for POLLOUT
        connect_res = !Utils::SelectWrite(c->temp_socket_fd, timeout);
    }

    // three cases are possible:
    // 1. connect_res == -1, means that a socket error occured
    // 2. connect_res == true, means that SelectWrite returned false
    //    indicating an error
    // 3. connect_res == false, means that SelectWrite returned true
    //    indicating success
    // we're handling cases 1 and 2, 'cause both of them indicate
    // error.
    if (connect_res) {
        // fail - close the socket
        close(temp_socket);
        // notify
        Utils::WriteEventFd(c->connect_event, 1);
        return;
    }

    // connected
    c->conn = new Connection(temp_socket);

    // setup the request id part
    c->conn->SetEvenFriPart(true);

    // notify about finish
    Utils::WriteEventFd(c->connect_event, 1);
}

DowowNetwork::Client::Client() {
    connect_event = eventfd(0, 0);
}


bool DowowNetwork::Client::ConnectTcp(std::string ip, uint16_t port, int timeout) {
    // create the address to connect to
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(port);
    // IP parsing
    if (!inet_aton(ip.c_str(), &addr.sin_addr)) {
        // failed to parse IP
        return false;
    }

    // create a connecting thread
    connecting_thread =
        new std::thread(ConnThreadFunc, this, *(sockaddr*)(&addr), timeout);

    // POLLIN to check if connect thread has finished
    Utils::ReadEventFd(connect_event, -1);

    // return the result
    return conn && conn->IsConnected();
}

bool DowowNetwork::Client::ConnectUnix(std::string socket_path, int timeout) {
    // sockaddr
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path.c_str(), socket_path.size() + 1);

    // thread a connecting thread
    connecting_thread =
        new std::thread(ConnThreadFunc, this, *(sockaddr*)(&addr), timeout);

    // POLLIN
    Utils::ReadEventFd(connect_event, -1);

    // return the result
    return conn && conn->IsConnected();
}

bool DowowNetwork::Client::IsConnecting() {
    return !Utils::ReadEventFd(connect_event, 0);
}

DowowNetwork::Connection *DowowNetwork::Client::GetConnection() {
    return conn;
}

DowowNetwork::Connection *DowowNetwork::Client::operator()() {
    return GetConnection();
}

DowowNetwork::Client::~Client() {
    // join the thread
    if (connecting_thread) {
        connecting_thread->join();
        delete connecting_thread;
    }
    // terminate the connection
    if (conn) {
        delete conn;
    }
    // close event
    close(connect_event);
}
