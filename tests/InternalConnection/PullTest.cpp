/*!
 *  \file
 *
 * This file implements the test of Pull() method.
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
    // connect
    auto c = Connect(23060);
    assert(c->IsConnected());

    // wait a bit
    cout << "[LOG] Waiting for a second" << endl;
    sleep(1);

    // check if the queue is empty (as it should be)
    cout << "[LOG] Checking if the receive queue is not empty" << endl;
    assert(!c->Pull(0) && "must return 0");

    // check for Pull with timeout
    cout << "[LOG] Checking if Pull() with timeout of 2 seconds will block" << endl;
    assert(!c->Pull(2) && "must return 0");

    // make 5 hang requests with different timings
    for (int i = 0; i < 5; i++) {
        int s = 1 + i * 2;
        Request *hang = new Request("hang");
        hang->Emplace<Value32U>("s", s);
        c->Push(hang);
        cout << "[LOG] Pushed hang(" << s << ")" << endl;
    }

    cout << "[LOG] Waiting for 5 pulls..." << endl;

    for (int i = 0; i < 5; i++) {
        auto r = c->Pull(-1);
        assert(r && "must not be 0");
        delete r;
        cout << "[OK ] Received response #" << i + 1 << endl;
    }


    // stopping the server
    Request *stop = new Request("stop");
    c->Push(stop);
    c->Disconnect(false);
    c->WaitForStop();
    delete c;

    cout << "[PASSED]" << endl;

    return 0;
}
