#include "../DowowConnection.hpp"

#include <cassert>
#include <iostream>
#include <thread>
#include <list>

using namespace DowowNetwork;
using namespace std;

bool to_work = true;

void DefaultHandler(DowowConnection* conn, Request* req) {

}

void HandlerThread(DowowConnection* conn) {
    // attach 
    while (conn->IsConnected()) {
        conn->Handle();
    }
    delete conn;
}

int main() {
    Server server;
    server.SetBlocking(false);
    if (!server.StartTCP("127.0.0.1", 40035)) {
        cout << "Failed to start the server on 127.0.0.1:40035" << endl;
        return 1;
    }

    list<thread*> threads;

    while (to_work) {
        // accept one client
        Connection* conn = server.AcceptOne();

        // create dowow connection
        DowowConnection* d_conn = new DowowConnection(conn);

        // start a new thread
        threads.push_back(new thread(HandlerThread, d_conn));
    }


    return 0;
}
