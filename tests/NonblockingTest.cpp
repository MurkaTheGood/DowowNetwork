#include "../Server.hpp"
#include "../Client.hpp"
#include "../values/All.hpp"

#include <iostream>
#include <cassert>

using namespace std;
using namespace DowowNetwork;

void Test(Server& server, Client& client) {
    std::list<Connection*> conns;

    // data for testing
    int times_echoed = 0;
    bool sent = false;

    // accept the client
    bool is_working = true;
    while (is_working || client.IsConnected()) {
        // accept new clients
        server.Accept(conns);

        // handle connections
        for (auto &conn: conns) {
            // handle I/O
            conn->Poll();

            // receive
            Request* in_req = conn->Pull();
            if (in_req) {
                std::string name = in_req->GetName();
                if (name == "echo") {
                    conn->Push(in_req, true);
                    cout << "* server echoes..." << endl;
                }
                if (name == "close") {
                    Request close_req("close_permitted");
                    conn->Push(close_req);
                    cout << "* server permits connection closure" << endl;
                }
                delete in_req;
            }

            // check if dead
            if (!conn->IsConnected()) {
                is_working = false;
                delete conn;
            }
        }

        // handle client
        client.Poll();

        Request* response = client.Pull();
        if (response) {
            if (response->GetName() == "echo") {
                assert(response->Get<Value32S>("times")->Get() == times_echoed);
                times_echoed++;
                sent = false;
                cout << "* client sees: server echoed!" << endl;
            } else if (response->GetName() == "close_permitted") {
                cout << "* client sees: server has permitted closure" << endl;
                client.Disconnect();
            }
            delete response;
        }

        if (times_echoed != 100 && !sent) {
            Request req("echo");
            req.Emplace<Value32S>("times", times_echoed);
            client.Push(req);
            sent = true;
        } else {
            if (!sent) {
                Request closer("close");
                client.Push(closer);
                sent = true;
            }
        }
    }

    assert(times_echoed == 100 && "must echo 100 times");

    cout << "[PASSED]" << endl;
}

int main() {
    // create the client and the server
    Client client(true);
    Server server;

    // set the server to be nonblocking
    server.SetNonblocking(true);
    
    // start the server
    server.StartTcp("127.0.0.1", 23060);

    // connect the client
    client.ConnectTcp("127.0.0.1", 23060);

    cout << "[TCP]" << endl;
    Test(server, client);

    server.Close();

    // start the unix server
    if (!server.StartUnix("nonblocking.sock")) {
        cerr << "failed to start unix server: " << errno << endl;
        return 1;
    }

    // connect
    client.ConnectUnix("nonblocking.sock");

    cout << "[UNIX]" << endl;
    Test(server, client);

    server.Close();

    return 0;
}
