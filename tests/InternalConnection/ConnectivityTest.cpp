/*!
 *  \file
 *
 * This file implements basic connectivity test, i.e. successful connection,
 * forced disconnection, graceful disconnection, waiting for finish.
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
    cout << "[LOG] Testing connection..." << endl;
    auto c = Connect(23060);
    assert(c->IsConnected());
    cout << "[OK ] Connection reports that it's connected" << endl;

    cout << "[LOG] Testing forced disconnection" << endl;
    c->Disconnect(true);
    cout << "[LOG] Waiting for disconnection" << endl;

    assert(c->WaitForStop(-1) && "failed forced disconnection");
    assert(!c->IsConnected());
    cout << "[OK ] Disconnected forcefully" << endl;

    assert(c->WaitForStop(-1));
    cout << "[OK ] WaitForStop(-1) returned true after disconnection" << endl;

    assert(c->WaitForStop(0));
    cout << "[OK ] WaitForStop(0) returned true after disconnection" << endl;

    assert(c->WaitForStop(30));
    cout << "[OK ] WaitForStop(30) returned true after disconnection" << endl;

    cout << "[LOG] Testing deletion (use valgrind to analyze)" << endl;
    delete c;
    cout << "[OK ] Deleted" << endl;

    cout << "[LOG] Generating a large request..." << endl;
    Request *r = new Request("ping");
    for (int i = 0; i < 1000; i++) {
        r->Emplace<Value64U>("value_" + to_string(i), i);
    }
    cout << "[LOG] Generated a request of size " << r->GetSize() << endl;

    c = Connect(23060);
    cout << "[LOG] Pushing the request" << endl;
    assert(c->Push(r) == 0 && "must return null-pointer when no timeout");
    cout << "[LOG] Graceful disconnection..." << endl;
    c->Disconnect(false);
    assert(c->WaitForStop(-1) && "failed graceful disconnection");
    delete c;

    cout << "[MET] Requesting the server stop..." << endl;
    c = Connect(23060);
    c->Push(new Request("stop"));
    sleep(10);
    c->Disconnect(false);
    c->WaitForStop(-1);
    delete c;

    cout << "[OK ] Passed" << endl;

    return 0;
}
