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

#include "../values/All.hpp"
#include "../Connection.hpp"

using namespace std;
using namespace DowowNetwork;

// Connect to the server on localhost.
Connection *Connect(uint16_t port) {
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

    return new Connection(socket_fd);
}


int main() {
    // connect
    auto c = Connect(23060);
    assert(c->IsConnected());

    // wait a bit
    cout << "[LOG] Waiting for a second" << endl;
    sleep(1);

    // check if the queue is empty (as it should be)
    cout << "[LOG] Checking if the receive queue is not empty" << endl;
    assert(!c->Pull(0) && "must return 0");

    // response
    Request *r = 0;
    // make five instant pings
    for (int i = 0; i < 5; i++) {
        cout << "[LOG] Pushing 'ping' request" << endl;

        // Create and send the ping request
        Request *ping = new Request("ping");
        c->Push(ping);

        // Wait for response for 3 seconds
        cout << "[LOG] Waiting for a response (not functional)..." << endl;
        r = c->Pull(3);

        // Check for errors
        assert(r && "must not be 0");
        assert(r->GetName() == "pong" && "name must be 'pong'");
        cout << "[OK ] Received response of size " << r->GetSize() << " and ID " << r->GetId() << endl;
        delete r;
    }

    // make get request with 5 seconds timeout
    Request *get = new Request("get");
    cout << "[LOG] Pushing 'get' request (functional)" << endl;
    r = c->Push(get, true, false, 5);

    // check for errors
    assert(r && "Response to 'get' must not be 0");
    assert(r->GetName() == "response" && "Response to 'get' must have name 'response'");
    cout << "[OK ] Received functional response of size " << r->GetSize() << endl;
    delete r;

    // hang request with 10 seconds timeout (it hangs for 5 seconds)
    Request *hang = new Request("hang");
    hang->Emplace<Value32U>("s", 5);
    cout << "[LOG] Pushing 'hang(5)' request (functional)" << endl;
    r = c->Push(hang, true, false, 10);
    assert(r && "a request must be received");
    cout << "[OK ] Received a response for 'hang(5)'" << endl;
    delete r;

    // hang request with 1 second timeout (it hangs for 5 seconds)
    hang = new Request("hang");
    hang->Emplace<Value32U>("s", 5);
    cout << "[LOG] Pushing 'hang(5)' request (functional), waiting for 1s" << endl;
    r = c->Push(hang, true, false, 1);
    assert(!r && "a request must not be received");
    cout << "[OK ] Not received a response for 'hang(5)' (that means good)" << endl;

    // Disconnecting
    delete c;
    cout << "[LOG] Disconnected" << endl;
    c = Connect(23060);
    // hang request with 1 second timeout again
    hang = new Request("hang");
    hang->Emplace<Value32U>("s", 5);
    cout << "[LOG] Pushing 'hang(5)' request (functional), waiting for 1s, and stopping the server" << endl;
    r = c->Push(hang, true, false, 1);
    assert(!r && "a request must not be received");
    cout << "[OK ] Not received a response for 'hang(5)' (that means good)" << endl;
    cout << "[LOG] Requesting server stop" << endl;
    Request *stop = new Request("stop");
    c->Push(stop);
    c->Disconnect(false);
    c->WaitForStop();
    delete c;

    cout << "[PASSED]" << endl;

    return 0;
}
