#include "../Datum.hpp"
#include "../Request.hpp"
#include "../Server.hpp"
#include "../values/All.hpp"

#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <random>
#include <ctime>
#include <thread>
#include <chrono>

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

using namespace std;
using namespace DowowNetwork;

// Create a server.
Server server;

void HandlerStop(Connection *conn, Request *req) {
    cout << "Received the stop request" << endl;
    // Handling 'stop'.
    server.Stop(0);
}

void HandlerBye(Connection *conn, Request *req) {
    conn->Disconnect();
}

void HandlerPing(Connection *conn, Request *req) {
    // get the value
    auto number_v = req->Get<Value32S>("number");
    // no value
    if (!number_v) {
        conn->Disconnect(true, false);
        delete req;
        return;
    }
    // the value
    int32_t number = number_v->Get();

    // the response
    if (number < 3) {
        Request pong("pong");
        pong.Emplace<Value32S>("number", number + 1);
        conn->Push(pong);
    } else {
        Request bye("bye");
        conn->Push(bye);
    }
}

void HandlerConnected(Server *server, Connection *conn) {
    std::cout << "Connected" << endl;

    // Setup the handlers.
    conn->SetHandlerNamed("ping", HandlerPing);
    conn->SetHandlerNamed("bye", HandlerBye);
    conn->SetHandlerNamed("stop", HandlerStop);

    // Send a request to test dafault handler.
    Request yay("yay_it_works");
    yay.Emplace<ValueStr>("text", "Yay! It works!");
    yay.Emplace<Value64U>("very_big_number", 0xfffffffffffff);
    yay.Emplace<Value32U>("not_so_big_number", 0xffffffff);
    yay.Emplace<Value16U>("pretty_big_number", 0xffff);
    yay.Emplace<Value8U>("small_number", 0xff);
    yay.Emplace<Value8S>("negative_number", -0x10);
    yay.Emplace<Value64U>("hours_spent_to_develop_this_thing", 0xffffffffffffffff);
    conn->Push(yay);
}

void HandlerDisconnected(Server *s, Connection *c) {
    // Log.
    std::cout << "Disconnected" << endl;
}

int main(int argc, char** argv) {
    // Setup handlers for connection and disconnection.
    server.SetConnectedHandler(HandlerConnected);
    server.SetDisconnectedHandler(HandlerDisconnected);

    // Limit maximum connections amount to 5.
    server.SetMaxConnections(5);

    // Start the server.
    server.StartTcp("127.0.0.1", 23050);

    // Log.
    cout << "Started on 127.0.0.1:23050" << endl;

    // Wait for stop.
    server.WaitForStop();

    // Log.
    cout << "Stopped" << endl;

    return 0;
}
