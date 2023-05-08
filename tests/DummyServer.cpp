/*#include "../Datum.hpp"
#include "../Request.hpp"
#include "../Server.hpp"
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
#include <time.h>

using namespace std;
using namespace DowowNetwork;

bool is_running = true;

// this function creates some files for testing
void SleepMS(long ms) {
    timespec ts { 0, ms * 1000 * 1000 };
    nanosleep(&ts, 0);
}

// request handler
void RequestHandler(Connection* conn, Request* req) {
    // shortly
    std::string cmd = req->GetName();
    cout << "Command '" << cmd << "' is received" << endl;

    if (cmd == "bye") {
        conn->Close();
        cout << "closed" << endl;
    } else if (cmd == "stop") {
        is_running = false;
    } else if (cmd == "echo") {
        conn->Push(req);
        cout << "echoed" << endl;
    } else if (cmd == "print") {
        Value* v = req->GetArgument("message");
        if (!v || v->GetType() != ValueTypeStr) {
            ValueStr error_message = "'message' is missing or not string";
            Request* res = new Request("error");
            res->SetArgument("text", &error_message);
            conn->Push(res);
            delete res;
        } else {
            cout << "print: " << ((ValueStr*)v)->Get() << endl;
        }
    } else {
        cout << req->ToString() << endl;
    }
}

int main(int argc, char** argv) {
    // enable randomization
    srand(time(0));

    // the value of the test
    cout << "This server just dumps all received requests, it's also nonblocking" << endl;

    // starting the server
    cout << "[STARTING THE SERVER]" << endl;
    Server server;
    bool result = server.StartTCP(
        "0.0.0.0",                  // IP (using all addresses)
        23050                       // port
    );
    server.SetBlocking(false);
    if (!result) {
        cerr << "errno: " << errno << endl;
        server.Close();
        return 1;
    }
    cout << "[TCP SERVER IS RUNNING ON " << server.GetTcpIpString() << ":" << server.GetTcpPort() << "]" << endl;

    list<Connection*> conns;
    double time_passed = 0;
    double next_time = 1;
    while (is_running) {
        // accept new clients
        auto new_cl = server.Accept();
        if (new_cl.size()) {
            // add new clients to the list
            conns.splice(conns.end(), new_cl);
            // log
            cout << "Connected clients: " << conns.size() << endl;
        }

        // handle connections
        for (auto& conn: conns) {
            conn->Handle();
        }

        // check if received
        for (auto& conn: conns) {
            Request* req = conn->Pull();
            if (req) {
                RequestHandler(conn, req);
                delete req;
            }
        }
        
        // remove dead
        auto dead = conns.begin();

        while ((dead = std::find_if(
            conns.begin(),
            conns.end(),
            [](Connection* conn) {
                bool closed = conn->IsClosed();
                if (closed) {
                    cout << "Deleting" << endl;
                    delete conn;
                }
                return closed; }
        )) != conns.end()) {
            conns.erase(dead);
        }


        // sleep a bit
        SleepMS(10);
        time_passed += 0.01;

        if (time_passed >= next_time) {
            cout << ".";
            cout.flush();
            next_time = time_passed + 1;
        }
    }
    cout << "[STOPPING THE SERVER]" << endl;
    server.Close();
    for (auto& i: conns) delete i;
    cout << "[BYE]" << endl;

    return 0;
}

*/

int main() {
    return 0;
}
