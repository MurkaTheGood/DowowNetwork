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

void HandlerStop(Connection *conn, Request *req) {
    conn->tag = "STOP";
    conn->Disconnect();
}

void HandlerBye(Connection *conn, Request *req) {
    conn->Disconnect();
    delete req;
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
    if (number < 10) {
        Request pong("pong");
        pong.Emplace<Value32S>("number", number + 1);
        conn->Push(pong);
    } else {
        Request bye("bye");
        conn->Push(bye);
    }

    delete req;
}

void HandlerConnected(Server *server, Connection *conn) {
    std::cout << "Connected" << endl;

    // init handlers
    conn->SetHandlerNamed("ping", HandlerPing);
    conn->SetHandlerNamed("bye", HandlerBye);
    conn->SetHandlerNamed("stop", HandlerStop);

    // send initial pong
    Request pong("pong");
    pong.Emplace<Value32S>("number", 0);
    conn->Push(pong);
}

void HandlerDisconnected(Server *s, Connection *c) {
    std::cout << "Disconnected" << endl;
    if (c->tag == "STOP") {
        s->Stop();
    }
}

int main(int argc, char** argv) {
    Server server;
    server.SetConnectedHandler(HandlerConnected);
    server.SetDisconnectedHandler(HandlerDisconnected);
    server.StartTcp("127.0.0.1", 23050);
    cout << "Started!" << endl;
    server.WaitForStop();
    cout << "Stopped!" << endl;
    return 0;
}
