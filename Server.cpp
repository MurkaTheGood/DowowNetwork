#include <sys/socket.h>
#include <sys/types.h>

#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <fcntl.h>
#include <endian.h>
#include <errno.h>

#ifdef VERBOSE_DEBUG
#include <iostream>
using namespace std;
#endif

#include "Utils.hpp"
#include "Server.hpp"

DowowNetwork::Server::Server() {
    // here goes nothing
}

void DowowNetwork::Server::SetBlocking(bool blocking) {
    // success
    nonblocking = !blocking;
}

bool DowowNetwork::Server::StartUNIX(std::string path, bool dof) {
    // the server is already started
    if (GetType() != SocketTypeUndefined) return false;

    // try to create a socket
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    // check if failed
    if (socket_fd == -1) return false;

    // check if file exists
    if (dof && !access(path.c_str(), F_OK)) {
        // delete the file
        if (unlink(path.c_str()) == -1) {
            #ifdef VERBOSE_DEBUG
            cout << "[" << __FILE__ << ":" << __LINE__ << "]: "
                << "failed to delete old socket file " << path
                << endl;
            #endif
            // failed to delete
            close(socket_fd);
            return false;
        }
    }

    // bind
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(&addr.sun_path, path.c_str(), path.size() + 1);
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        #ifdef VERBOSE_DEBUG
        cout << "[" << __FILE__ << ":" << __LINE__ << "]: "
            << "failed to bind the socket to file " << path
            << endl;
        #endif
        // failed to bind
        close(socket_fd);
        return false;
    }

    // set the socket to listen
    if (listen(socket_fd, SOMAXCONN) == -1) {
        #ifdef VERBOSE_DEBUG
        cout << "[" << __FILE__ << ":" << __LINE__ << "]: "
            << "failed to make the socket listen"
            << endl;
        #endif
        // failb
        close(socket_fd);
        return false;
    }

    // success
    socket_type = SocketTypeUNIX;
    unix_socket_path = path;
    #ifdef VERBOSE_DEBUG
    cout << "[" << __FILE__ << ":" << __LINE__ << "]: "
        << "started UNIX socket server on " << path
        << endl;
    #endif

    return true;
}

std::string DowowNetwork::Server::GetUNIXPath() {
    return unix_socket_path;
}

bool DowowNetwork::Server::StartTCP(std::string ip, uint16_t port) {
    // the server is already started
    if (GetType() != SocketTypeUndefined) return false;

    // try to create a socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    // check if failed
    if (socket_fd == -1) return false;

    // allow reuse
    int reuse_flag = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_flag, sizeof(reuse_flag));

    // prepare socket address
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(port);
    // get the IP address
    if (!inet_aton(ip.c_str(), &addr.sin_addr)) {
        // invalid address
        close(socket_fd);
        return false;
    }
    // bind
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        // failed to bind
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
    socket_type = SocketTypeTCP;
    tcp_socket_address = be32toh(addr.sin_addr.s_addr);
    tcp_socket_port = port;

    return true;
}

uint32_t DowowNetwork::Server::GetTcpIp() {
    return tcp_socket_address;
}

std::string DowowNetwork::Server::GetTcpIpString() {
    // convert the tcp socket address to string
    in_addr addr{htobe32(tcp_socket_address)};
    return inet_ntoa(addr);
}

uint16_t DowowNetwork::Server::GetTcpPort() {
    return tcp_socket_port;
}

uint8_t DowowNetwork::Server::GetType() {
    return socket_type;
}

void DowowNetwork::Server::Accept(std::list<Connection*> &conns) {
    // check if server is not started
    if (GetType() == SocketTypeUndefined)
        return;

    while (true) {
        // accept
        Connection* conn = AcceptOne();

        // check if failed
        if (conn == 0) break;

        // add to the list
        conns.push_back(conn);

        // blocking, should break
        if (!nonblocking) break;
    }
}

DowowNetwork::Connection* DowowNetwork::Server::AcceptOne() {
    // not connected
    if (GetType() == SocketTypeUndefined) return 0;

    // nonblocking and can't read
    if (nonblocking && !Utils::SelectRead(socket_fd)) return 0;

    // accept
    int temp_fd = accept4(socket_fd, 0, 0, 0);
    // failed
    if (temp_fd == -1) return 0;


    // create a connection
    Connection *conn = new Connection(temp_fd);
    conn->SetBlocking(!nonblocking);
    // success
    return conn;
}

void DowowNetwork::Server::Close() {
    // check if server is not started
    if (GetType() == SocketTypeUndefined) return;

    // UNIX socket - delete it
    if (GetType() == SocketTypeUNIX)
        unlink(unix_socket_path.c_str());

    // close the server socket
    close(socket_fd);

    // set the socket type to undefined
    socket_type = SocketTypeUndefined;
}

DowowNetwork::Server::~Server() {
    Close();
}
