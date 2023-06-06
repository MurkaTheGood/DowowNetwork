/*!
 *  \file
 *
 *  The server program for all the InternalConnection tests.
 *  It does not use any library facilities to accept connection
 *  beacuse they're not done by the moment of coding :-)
 */

#include <algorithm>
#include <iostream>
#include <list>

#include <cassert>

#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>

#include "../../InternalConnection.hpp"

using namespace std;
using namespace DowowNetwork;

int socket_fd = -1;
bool to_work = true;

// Setup the server.
void Setup() {
    // temp variable
    int temp = 0;

    // create the socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(socket_fd != -1);

    // enable address resusage
    int reuse = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // create the address
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htobe16(23060);
    sin.sin_addr.s_addr = htobe32(INADDR_LOOPBACK);

    // bind the socket
    temp = bind(socket_fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    cout << errno << endl;
    assert(temp != -1 && "bind");

    // set to listen
    temp = listen(socket_fd, SOMAXCONN);
    assert(temp != -1 && "listen");

    cout << "Started the server on 127.0.0.1:23060" << endl;
}

// The handler for ping request.
void HandlerPing(InternalConnection *c, Request *r) {
    cout << "[REQ] " << c->id << ": " << r->GetName() << " (" << r->GetSize() << " bytes)" << endl;
    Request res("pong");
    c->Push(res);
}

// The handler for 'get'
void HandlerGet(InternalConnection *c, Request *r) {
    cout << "[REQ] " << c->id << ": " << r->GetName() << endl;
    
    Request *res = new Request("response");
    res->SetId(r->GetId());
    c->Push(res, false);
}

// The handler for 'hang'
void HandlerHang(InternalConnection *c, Request *r) {
    cout << "[REQ] " << c->id << ": " << r->GetName() << endl;
    cout << "[REQ] Sleeping for 15s..." << endl;

    // hang for 15 seconds
    sleep(15);

    cout << "[REQ] Slept well, resuming!" << endl;

    Request *res = new Request("response");
    res->SetId(r->GetId());
    c->Push(res, false);
};

// The handler for 'stop'
void HandlerStop(InternalConnection *c, Request *r) {
    cout << "[REQ] " << c->id << ": " << r->GetName() << endl;

    to_work = false;
};

int main() {
    // setup the server
    Setup();

    // the list of all connected clients
    std::list<InternalConnection*> conns;
    // free id for connection
    uint32_t cid = 0;

    pollfd *fds = 0;
    while (to_work) {
        // cleanup
        delete[] fds;
        // setup the fds
        uint32_t fds_a = 1 + conns.size();
        fds = new pollfd[fds_a];

        // server socket
        fds[0].fd = socket_fd;
        fds[0].events = POLLIN;
        // stopped events
        uint32_t i = 0;
        for (auto &c: conns) {
            fds[1 + i].fd = c->GetStoppedEvent();
            fds[1 + i].events = POLLIN;
            i++;
        }

        // poll
        poll(fds, fds_a, -1);

        // check the poll results
        for (i = 1; i < fds_a; i++) {
            if (fds[i].revents & POLLIN) {
                // find the connection
                auto it = std::find_if(conns.begin(), conns.end(),
                        [&](InternalConnection *c) {
                            return c->GetStoppedEvent() == fds[i].fd;
                        });

                // stopped
                uint32_t id = (*it)->id;
                cout << "[OLD] Conenction #" << id << " was disconnected, deleting..." << endl;
                delete (*it);
                cout << "[OLD] Connection #" << id << " was deleted" << endl;

                // remove from the list
                conns.erase(it);
            }
        }
        // socket event
        if (fds[0].revents & POLLIN) {
            int fd = accept(socket_fd, 0, 0);
            assert(fd != -1 && "accept");

            InternalConnection *n = new InternalConnection(fd);
            n->id = cid++;
            n->SetHandlerNamed("ping", HandlerPing);
            n->SetHandlerNamed("get", HandlerGet);
            n->SetHandlerNamed("hang", HandlerHang);
            n->SetHandlerNamed("stop", HandlerStop);
            conns.push_back(n);

            cout << "[NEW] Accepted connection #" << n->id << endl;
        }
    }
    cout << "[LOG] Something caused the server shutdown!" << endl;
    for (auto i : conns) {
        uint32_t id = i->id;

        cout << "[LOG] Deleting connection #" << id << "..." << endl;
        delete i;
        cout << "[LOG] Deleted connection #" << id << endl;
    }
    cout << "[LOG] Disconnected everybody" << endl;

    delete[] fds;
    close(socket_fd);

    return 0;
}
