/*!
 *  \file
 *
 * This file implements the test of Push() method.
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

#include "../../values/All.hpp"
#include "../../InternalConnection.hpp"

using namespace std;
using namespace DowowNetwork;

// Connect to the server on localhost.
InternalConnection *Connect(uint16_t port) {
    // temp variable
    int temp = 0;

    // create the socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(socket_fd != -1);

    // create the address
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htobe16(port);
    sin.sin_addr.s_addr = htobe32(INADDR_LOOPBACK);

    // connect
    temp = connect(socket_fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    assert(temp != -1 && "connect");

    cout << "[LOG] Connected to 127.0.0.1:" << port << endl;

    return new InternalConnection(socket_fd);
}


int main() {
    auto c = Connect(23060);
    assert(c->IsConnected());

    cout << "[LOG] Waiting for a second" << endl;
    // wait a bit
    sleep(1);

    cout << "[LOG] Checking if the receive queue is not empty" << endl;
    // check if empty queue
    assert(!c->Pull(0) && "must return 0");

    Request *r = 0;
    for (int i = 0; i < 5; i++) {
        cout << "[LOG] Pushing 'ping' request" << endl;

        // send the ping request
        Request *ping = new Request("ping");
        c->Push(ping);

        cout << "[LOG] Waiting for a response (not functional)..." << endl;
        r = c->Pull(-1);
        assert(r && "must not be 0");
        assert(r->GetName() == "pong" && "name must be 'pong'");
        cout << "[OK ] Received response of size " << r->GetSize() << endl;
        delete r;
        sleep(1);
    }

    cout << "[LOG] Pushing 'get' request (functional)" << endl;
    Request *get = new Request("get");
    r = c->Push(get, true, false, 5);
    assert(r && "Response to 'get' must not be 0");
    assert(r->GetName() == "response" && "Response to 'get' must have name 'response'");
    cout << "[OK ] Received functional response of size " << r->GetSize() << endl;
    delete r;

    cout << "[LOG] Requesting server stop" << endl;
    Request *stop = new Request("stop");
    c->Push(stop);
    c->Disconnect(false);
    c->WaitForStop();
    delete c;

    cout << "[PASSED]" << endl;

    return 0;
}
