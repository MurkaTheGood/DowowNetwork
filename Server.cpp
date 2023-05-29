#include <sys/socket.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/eventfd.h>

#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <fcntl.h>
#include <endian.h>
#include <errno.h>

#include <thread>
#include <algorithm>

#include <iostream>

#include "Utils.hpp"
#include "Server.hpp"

DowowNetwork::Connection* DowowNetwork::Server::AcceptOne() {
    // accept
    int temp_fd = accept4(socket_fd, 0, 0, 0);
    // failed
    if (temp_fd == -1) return 0;

    // create a connection
    Connection *conn = new Connection(temp_fd);

    // call the handler if set
    if (GetConnectedHandler()) {
        (*GetConnectedHandler())(this, conn);
    }

    // success
    return conn;
}

void DowowNetwork::Server::ServerThreadFunc(Server *s) {
    // free id for a new connection
    uint32_t free_conn_id = 1;

    // infinite loop
    while (true) {
        // whether should accept new clients
        bool accept_new = true;
        if (s->max_connections >= 0) {
            accept_new =
                static_cast<int32_t>(s->connections.size()) <
                s->max_connections;
        }

        pollfd *pollfds = new pollfd[2 + s->connections.size()];
        // server socket
        pollfds[0].fd = accept_new ? s->socket_fd : -1;
        pollfds[0].events = POLLIN;
        // 'to_stop' event
        pollfds[1].fd = s->to_stop_event;
        pollfds[1].events = POLLIN;

        // ask all connections for a 'closed' event
        size_t i = 0;
        for (auto c : s->connections) {
            pollfds[2 + i].fd = c->GetStoppedEvent();
            pollfds[2 + i].events = POLLIN;
            ++i;
        }

        // poll events
        poll(
            pollfds,
            2 + s->connections.size(),
            -1);

        // lock
        std::lock_guard<typeof(s->mutex_server)> __sm(s->mutex_server);

        // ***************************
        // check if 'to_stop' occurred
        // ***************************
        if (pollfds[1].revents & POLLIN) {
            // free pollfds memory
            delete[] pollfds;
            // break the thread loop
            break;
        }

        // ************************************
        // check if some connections are closed
        // ************************************
        std::list<Connection*> deleted_conns;
        i = 0;
        for (auto c : s->connections) {
            if (pollfds[2 + i++].revents & POLLIN) {
                // call 'disconnected' handler
                (*s->GetDisconnectedHandler())(s, c);
                // delete the connection
                delete c;
                deleted_conns.push_back(c);
            }
        }
        // remove deleted connections
        for (auto c : deleted_conns)
            s->connections.remove(c);

        // ***********************
        // check if new connection
        // ***********************
        if (pollfds[0].revents & POLLIN) {
            // accept
            Connection *new_conn = s->AcceptOne();
            // not an error
            if (new_conn) {
                // assign the id
                new_conn->id = free_conn_id++;
                // add to the list of connections
                s->connections.push_back(new_conn);
            }
        }

        // *************************************
        // no new connections, no deleted - quit
        // *************************************
        else if (!deleted_conns.size()) {
            delete[] pollfds;
            break;
        }

        // free memory
        delete[] pollfds;
        continue;
    }

    // lock
    std::lock_guard<typeof(s->mutex_server)> __sm(s->mutex_server);

    // force open connections to close
    // make all the open connections close
    for (auto c : s->connections) {
        c->Disconnect(true);
        c->WaitForStop(-1);
        delete c;
    }
    s->connections.clear();

    // close the server socket
    close(s->socket_fd);
    // close to_stop fd
    close(s->to_stop_event);
    
    // clear
    s->connections.clear();

    // notify about stop
    Utils::WriteEventFd(s->stopped_event, 1);

    // UNIX socket - delete it
    if (s->GetType() == SocketTypeUnix)
        unlink(s->unix_socket_path.c_str());

    // reset some data
    s->socket_fd = -1;
    s->to_stop_event = -1;
    s->socket_type = SocketTypeUndefined;
}

DowowNetwork::Server::Server() {
    stopped_event = eventfd(0, 0);
}

bool DowowNetwork::Server::StartUnix(std::string path, bool dof) {
    // lock
    std::lock_guard<typeof(mutex_server)> __sm(mutex_server);

    // the server is already started
    if (GetType() != SocketTypeUndefined)
        return false;

    // try to create a socket
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    // check if failed
    if (socket_fd == -1) return false;

    // check if file exists
    if (dof && !access(path.c_str(), F_OK)) {
        // delete the file
        if (unlink(path.c_str()) == -1) {
            // failed to delete
            close(socket_fd);
            return false;
        }
    }

    // bind
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(
        &addr.sun_path,
        path.c_str(),
        path.size() + 1);

    int bind_res = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));

    // failed to bind
    if (bind_res == -1) {
        close(socket_fd);
        return false;
    }

    // set the socket to listen
    if (listen(socket_fd, SOMAXCONN) == -1) {
        // fail
        close(socket_fd);
        return false;
    }

    // success
    socket_type = SocketTypeUnix;
    unix_socket_path = path;

    // create eventfd
    to_stop_event = eventfd(0, 0);

    // start the thread
    std::thread t(ServerThreadFunc, this);
    t.detach();

    return true;
}

std::string DowowNetwork::Server::GetUnixPath() {
    return unix_socket_path;
}

bool DowowNetwork::Server::StartTcp(std::string ip, uint16_t port) {
    std::lock_guard<typeof(mutex_server)> __sm(mutex_server);

    // the server is already started
    if (GetType() != SocketTypeUndefined)
        return false;

    // try to create a socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    // check if failed
    if (socket_fd == -1)
        return false;

    // allow reuse
    int reuse_flag = 1;
    setsockopt(
        socket_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse_flag,
        sizeof(reuse_flag));

    // prepare socket address
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(port);
    // parse the IP address
    if (!inet_aton(ip.c_str(), &addr.sin_addr)) {
        // invalid address
        close(socket_fd);
        return false;
    }

    // bind
    int bind_res = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    if (bind_res == -1) {
        // failed to bind
        close(socket_fd);
        return false;
    }

    // set the socket to be listening
    if (listen(socket_fd, SOMAXCONN) == -1) {
        // fail
        close(socket_fd);
        return false;
    }

    // success
    socket_type = SocketTypeTcp;
    tcp_socket_address = be32toh(addr.sin_addr.s_addr);
    tcp_socket_port = port;

    // create eventfd
    to_stop_event = eventfd(0, 0);

    // start the thread
    std::thread t(ServerThreadFunc, this);
    t.detach();

    return true;
}

uint32_t DowowNetwork::Server::GetTcpIp() {
    return tcp_socket_address;
}

std::string DowowNetwork::Server::GetTcpIpString() {
    // convert the tcp socket address to string
    in_addr addr{ htobe32(tcp_socket_address) };
    return inet_ntoa(addr);
}

uint16_t DowowNetwork::Server::GetTcpPort() {
    return tcp_socket_port;
}

uint8_t DowowNetwork::Server::GetType() {
    return socket_type;
}

void DowowNetwork::Server::SetMaxConnections(int32_t c) {
    max_connections = c;
}

int32_t DowowNetwork::Server::GetMaxConnections() {
    return max_connections;
}

DowowNetwork::SafeConnection *DowowNetwork::Server::GetConnection(std::string tag) {
    auto it = std::find_if(
        connections.begin(),
        connections.end(),
        [&](Connection *c) {
            return c->tag == tag;
        });

    if (it == connections.end()) return 0;

    return new SafeConnection(*it);
}

DowowNetwork::SafeConnection *DowowNetwork::Server::GetConnection(uint32_t id) {
    auto it = std::find_if(
        connections.begin(),
        connections.end(),
        [&](Connection *c) {
            return c->id == id;
        });

    if (it == connections.end()) return 0;

    return new SafeConnection(*it);
}

void DowowNetwork::Server::Stop(int timeout) {
    // check if server is not started
    if (GetType() == SocketTypeUndefined) return;

    // make the thread stop
    Utils::WriteEventFd(to_stop_event, 1);

    WaitForStop(timeout);
}

void DowowNetwork::Server::WaitForStop(int timeout) {
    // not started
    if (GetType() == SocketTypeUndefined) return;

    // wait for stopped event
    Utils::SelectRead(stopped_event, timeout);
}

DowowNetwork::Server::~Server() {
    // stop
    Stop(-1);

    // close the stopped eventfd
    close(stopped_event);
}
