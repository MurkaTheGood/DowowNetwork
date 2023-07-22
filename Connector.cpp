#include "Connector.hpp"

#include <time.h>
#include <fcntl.h>
#include <endian.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/eventfd.h>
#include <sys/poll.h>

#include <thread>

#include <iostream>
using namespace std;

#include "Utils.hpp"

void DowowNetwork::Connector::ConnThreadFunc(Connector *c, sockaddr addr, int timeout) {
    // The socket that's trying to connect to the server.
    int temp_socket = socket(addr.sa_family, SOCK_STREAM | SOCK_NONBLOCK, 0);

    // Check if we can't even create a socket
    if (temp_socket == -1) {
        // Export the errno from this thread
        c->connector_errno = errno;
        // Signal the outer code about thread finish
        Utils::WriteEventFd(c->bg_thread_event, 1);
        return;
    }

    // Begin to connect
    int connect_res = connect(temp_socket, &addr, sizeof(addr));

    // Check if an error happened and that error is "in progress"
    if (connect_res == -1 && errno == EINPROGRESS) {
        // poll() the socket and the cancel event
        pollfd pollfds[2] = {
            { temp_socket, POLLOUT, 0 },
            { c->forced_stop_event, POLLIN, 0 }
        };
        poll(pollfds, 2, timeout * 1000);
        // Operation is cancelled
        if (pollfds[1].revents & POLLIN) {
            // Close the the temporary socket
            close(temp_socket);
            // Signal the outer code about thread finish
            Utils::WriteEventFd(c->bg_thread_event, 1);
            // Quit the thread
            return;
        }
        
        // an error occured
        if (Utils::IsPollError(pollfds[0].revents))
            connect_res = -1;
        else
            connect_res = !(pollfds[0].revents & POLLOUT);
    }

    // We're here, so three cases are possible:
    // 1. connect_res == -1, means that a socket error occured
    // 2. connect_res == true, means that SelectWrite returned false
    //    indicating an error
    // 3. connect_res == false (the same as connect_res == 0), means
    //    that SelectWrite returned true, indicating success.
    // We're handling cases 1 and 2 only, because both of them indicate
    //    an error.
    if (connect_res != 0) {
        // Save the error for the calling thread
        c->connector_errno = errno;
        // Close the socket
        close(temp_socket);
        // Signal the outer code about thread finish
        Utils::WriteEventFd(c->bg_thread_event, 1);
        return;
    }

    // Lock
    c->universal_mutex.lock();

    // We're connected! Create a new Connection object
    // for our socket.
    c->conn = new Connection(temp_socket);

    // We're a client, so our request IDs must be
    // even numbers. The requests of the server will
    // therefore be odd numbers and we won't observe
    // the collision of IDs.
    c->conn->SetEvenFriPart(true);

    // Unlock
    c->universal_mutex.unlock();

    // Notify the outer thread about finish.
    Utils::WriteEventFd(c->bg_thread_event, 1);
}

DowowNetwork::Connector::Connector() {
    // Create the bg_thread_event
    bg_thread_event = eventfd(0, 0);
    forced_stop_event = eventfd(0, 0);
}

bool DowowNetwork::Connector::ConnectTcp(std::string ip, uint16_t port, int timeout, bool wait_for_finish) {
    // Quit, the Connector object is not reusable
    if (connecting_thread)
        return false;

    // Create the address to connect to
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(port);
    // IP parsing
    if (!inet_aton(ip.c_str(), &addr.sin_addr)) {
        // Save the errno
        this->connector_errno = errno;
        // Failed to parse the IP address
        return false;
    }

    // Create a connecting thread
    connecting_thread =
        new std::thread(
                ConnThreadFunc,
                this,
                *(sockaddr*)(&addr),
                timeout);

    // Check if we need to wait for background thread
    if (wait_for_finish)
        // Yes, wait for the thread
        Utils::ReadEventFd(bg_thread_event, -1);
    else
        // No, just return true
        return true;

    // Lock the universal mutex
    std::lock_guard<typeof(universal_mutex)> __g(universal_mutex);

    // Return the result
    return conn && conn->IsConnected();
}

bool DowowNetwork::Connector::ConnectUnix(std::string socket_path, int timeout, bool wait_for_finish) {
    // Quit, the Connector object is not reusable
    if (connecting_thread)
        return false;

    // sockaddr
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path.c_str(), socket_path.size() + 1);

    // Create a connecting thread
    connecting_thread =
        new std::thread(
                ConnThreadFunc,
                this,
                *(sockaddr*)(&addr),
                timeout);

    // Check if we need to wait for background thread
    if (wait_for_finish)
        // Yes, wait for the thread
        Utils::ReadEventFd(bg_thread_event, -1);
    else
        // No, just return true
        return true;

    // Lock the universal mutex
    std::lock_guard<typeof(universal_mutex)> __g(universal_mutex);

    // Return the result
    return conn && conn->IsConnected();
}

bool DowowNetwork::Connector::Wait(int t) {
    // waiting for the bg_thread_event
    if (!Utils::SelectRead(bg_thread_event, t))
        // timed out
        return false;

    // Lock the universal mutex
    std::lock_guard<typeof(universal_mutex)> __g(universal_mutex);

    // check if connected
    return conn && conn->IsConnected();
}

void DowowNetwork::Connector::Cancel() {
    Utils::WriteEventFd(forced_stop_event, 1);
}

bool DowowNetwork::Connector::IsConnecting() {
    // "connecting" if:
    // - the connect event did not happen,
    // - the connecting_thread exists
    return
        !Utils::SelectRead(bg_thread_event, 0) &&
        connecting_thread;
}

int DowowNetwork::Connector::GetConnectEvent() {
    return bg_thread_event;
}

DowowNetwork::Connection *DowowNetwork::Connector::GetConnection() {
    // Lock the universal mutex
    std::lock_guard<typeof(universal_mutex)> __g(universal_mutex);

    // Disown the connection
    Connection* c = conn;
    conn = 0;

    // Return the connection
    return c;
}

int DowowNetwork::Connector::GetErrno() {
    return connector_errno;
}

DowowNetwork::Connector::~Connector() {
    // Cancel the background thread
    Cancel();

    // Lock the universal mutex
    std::lock_guard<typeof(universal_mutex)> __g(universal_mutex);

    // Background thread exists
    if (connecting_thread) {
        // Join the background thread
        connecting_thread->join();
        // Delete the background thread
        delete connecting_thread;
    }
    // Terminate the connection
    delete conn;
    // Close the events
    close(bg_thread_event);
    close(forced_stop_event);
}
