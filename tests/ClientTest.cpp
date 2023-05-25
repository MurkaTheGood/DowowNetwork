#include "../Datum.hpp"
#include "../Request.hpp"
#include "../Client.hpp"
#include "../values/All.hpp"

#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <random>
#include <ctime>

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <time.h>

using namespace std;
using namespace DowowNetwork;

void HandlerPong(Connection *c, Request *r) {
    auto number_v = r->Get<Value32S>("number");
    if (!number_v) {
        cout << "No number!" << endl;
        delete r;
        return;
    }
    int32_t number = number_v->Get();
    cout << "Received 'pong': " << number << endl;
    
    Request ping("ping");
    ping.Emplace<Value32S>("number", number);
    c->Push(ping);

    delete r;
    return;
}

void HandlerBye(Connection *c, Request *r) {
    delete r;
    c->Disconnect(true);
}

int main(int argc, char** argv) {
    // Create a client.
    Client client;
    // Setup the named handlers.
    // * They are started in new threads when
    // * a request with specified name is received.
    client.SetHandlerNamed("pong", HandlerPong);
    client.SetHandlerNamed("bye", HandlerBye);

    // Connect to the server with 30 seconds timeout.
    if (!client.ConnectTcp("127.0.0.1", 23050, 30)) {
        cout << "Failed to connect to 127.0.0.1:23050" << endl;
        return 1;
    }

    // Log.
    cout << "Connected to 127.0.0.1:23050" << endl;

    // Create an initial request.
    Request ping("ping");
    ping.Emplace<Value32S>("number", 0);

    // Push the initial request to the send queue.
    // * The request becomes scheduled to be sent
    // * and will eventually get sent by background
    // * thread.
    client.Push(ping);

    // Wait for disconnection.
    client.WaitForStop();

    cout << "Closed" << endl;

    return 0;
}
