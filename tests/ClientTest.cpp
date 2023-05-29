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
    // Getting the Value object.
    auto number_v = r->Get<Value32S>("number");
    // Checking if not present.
    if (!number_v) {
        delete r;
        return;
    }
    // Getting the value itself.
    int32_t number = number_v->Get();

    // Log.
    cout << "Received 'pong': " << number << ", waiting for a second" << endl;
    
    // Sleeping so we can check WaitForStop().
    sleep(1);

    // Create a response.
    Request ping("ping");
    ping.Emplace<Value32S>("number", number);

    // Send the response.
    c->Push(ping);

    // Delete the request.
    delete r;
}

void HandlerBye(Connection *c, Request *r) {
    // Delete the request and disconnect. 
    delete r;

    // Create the stop request.
    Request stop("stop");
    c->Push(stop);
    c->Disconnect(false, false);
}

void HandlerDefault(Connection *c, Request *r) {
    // Log.
    cout << "Default handler work, received request:" << endl;
    cout << r->ToString() << endl;

    // Delete the request.
    delete r;
}

int main(int argc, char** argv) {
    // Create a client.
    Client client;

    // TEMPORARILY UNSUPPORTED DUE TO BUGS
    // Make the client execute handlers in separate thread.
    // * That's default behavior but let's leave it here
    // * for demonstration purposes!
    // client.SetHandlersMT(true);

    // Setup the named handlers.
    client.SetHandlerNamed("pong", HandlerPong);
    client.SetHandlerNamed("bye", HandlerBye);

    // Setup the default handler.
    // * It's executed when no appropriate named handler
    // * is found.
    client.SetHandlerDefault(HandlerDefault);

    cout << "Connecting" << endl;
    // Connect to the server with 30 seconds timeout.
    if (!client.ConnectTcp("127.0.0.1", 23050, -1)) {
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

    // Log.
    cout << "Disconnected. Waiting for a second so all the threads will quit." << endl;

    // sleep(1);

    return 0;
}
