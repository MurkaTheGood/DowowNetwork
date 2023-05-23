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
    cout << number << endl;
    
    Request ping("ping");
    ping.Emplace<Value32S>("number", number);
    // c->Push(ping);

    delete r;
    return;
}

void HandlerBye(Connection *c, Request *r) {
    delete r;
    // c->Disconnect(true);
}

int main(int argc, char** argv) {
    // address
    std::string ip = "127.0.0.1";
    uint16_t port = 23050;

    Client client;
    client.SetHandlerNamed("pong", HandlerPong);
    client.SetHandlerNamed("bye", HandlerBye);
    client.ConnectTcp(ip, port, -1);

    if (!client.IsConnected()) {
        cout << "Failed to connect to " << ip << ":" << port << endl;
        return 1;
    }

    cout << "Connected" << endl;

    int i = 0;
    while (client.IsConnected()) {
        Request req("ping");
        req.Emplace<Value32S>("number", i++);
        client.Push(req);

        cout << i << endl;
        sleep(1);
    }

    cout << "Closed" << endl;

    return 0;
}
