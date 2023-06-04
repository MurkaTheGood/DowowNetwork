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
   
    // YOU MAY NEVER SLEEP WHEN NOT USING MULTITHREADED HANDLERS!
    // Sleeping so we can check WaitForStop().
    sleep(1);

    // Create a response.
    Request ping("ping");
    ping.Emplace<Value32S>("number", number);

    // Send the response.
    c->Push(ping);
}

void HandlerBye(Connection *c, Request *r) {
    // Log.
    cout << "'bye' handler invoked" << endl;

    // Create the stop request (if the second test is invoked).
    if (c->tag == "second") {
        Request stop("stop");
        c->Push(stop);
        c->Disconnect(false, false);
    }
}

void HandlerDefault(Connection *c, Request *r) {
    // Log.
    cout << "Default handler invoked, received request:" << endl;
    cout << r->ToString() << endl;
}

// Create a client and setup it
Client *CreateClient() {
    // Create the client.
    Client *client = new Client();

    // Setup the named handlers.
    client->SetHandlerNamed("pong", HandlerPong);
    client->SetHandlerNamed("bye", HandlerBye);

    // Setup the default handler.
    // * It's executed when no appropriate named handler
    // * is found.
    client->SetHandlerDefault(HandlerDefault);

    // Return the client.
    return client;
}

int main(int argc, char** argv) {
    // Arguments.
    bool stress_test = false;
    bool valgrind = false;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (string(argv[i]) == "--help" ||
                string(argv[i]) == "-h" ||
                string(argv[i]) == "?")
            {
                cout << "Use this test to perform basic test of TCP and UNIX connectivity" << endl;
                cout << "For stress test:" << endl;
                cout << "    " << argv[0] << " -s" << endl;
                cout << "For memory analysis of client:" << endl;
                cout << "    valgrind " << argv[0] << " -v" << endl;
                return 0;
            } else if (string(argv[i]) == "-s") {
                cout << "STRESS TEST" << endl;
                stress_test = true;
            } else if (string(argv[i]) == "-v") {
                cout << "CLIENT MEMORY ANALYSIS (valgrind)" << endl;
                valgrind = true;
            }
        }
    }
    // Client pointer.
    Client *client = 0;

    // Create an initial request.
    Request ping("ping");
    ping.Emplace<Value32S>("number", 0);

    // ****************************************
    // Testing the forced disconnection feature
    // ****************************************
    for (int i = 0; i < (stress_test | valgrind ? 10 : 1); i++) {
        cout << "[TRANSMISSION-FORCED TEST #" << i << "]" << endl;
        // create the client
        client = CreateClient();

        cout << "Connecting..." << endl;
        // Connect to the server.
        if (!client->ConnectTcp("127.0.0.1", 23050, -1)) {
            cout << "Failed to connect to 127.0.0.1:23050!" << endl;
            return 1;
        }

        // Log.
        cout << "Connected to 127.0.0.1:23050" << endl;

        // Push the initial request to the send queue.
        // * The request becomes scheduled to be sent
        // * and will eventually get sent by background
        // * thread.
        client->Push(ping);

        cout << "AAAAA" << endl;
        // Wait for disconnection.
        client->WaitForStop(stress_test ? 0 : 1);
        cout << "BBBBB" << endl;

        // Check error.
        if (!client->IsConnected()) {
            cout << "Connection's lost before it was intended to be so" << endl;
            return 1;
        }

        cerr << "Still connected" << endl;

        // Forced disconnection.
        client->Disconnect(true, true);

        cerr << "DISCONNECTED!" << endl;

        // Check error.
        if (client->IsConnected()) {
            cout << "Connection isn't lost but it ought to be" << endl;
            return 1;
        }

        // Log.
        cout << "Disconnected" << endl;

        delete client;

        cout << "[TRANSMISSION-FORCED TEST #" << i << " PASSED]" << endl;
    }

    // ******************************************
    // Testing the graceful disconnection feature
    // ******************************************
    cout << "[TRANSMISSION-GRACEFUL TEST]" << endl;

    // Create the client
    client = CreateClient();

    // Log.
    cout << "Reconnecting..." << endl;

    // Mark the connection with a tag.
    client->tag = "second";
    // Connect to the server.
    if (!client->ConnectTcp("127.0.0.1", 23050, -1)) {
        cout << "Failed to reconnect to 127.0.0.1:23050!" << endl;
        return 1;
    }

    // Log.
    cout << "Connected to 127.0.0.1:23050" << endl;

    // Reuse the initial request.
    client->Push(ping);

    // Wait for disconnection (indefinitely long this time).
    client->WaitForStop();

    if (client->IsConnected()) {
        cout << "Connection isn't lost but it ought to be" << endl;
        return 1;
    }

    // Log.
    cout << "Disconnected" << endl;

    // delete the client
    delete client;

    cout << "[TRANSMISSION-GRACEFUL TEST FINISHED]" << endl;

    sleep(1);

    return 0;
}
