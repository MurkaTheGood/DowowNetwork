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

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

using namespace std;
using namespace DowowNetwork;

void SleepMS(long ms) {
    timespec ts { 0, ms * 1000 * 1000 };
    nanosleep(&ts, 0);
}

void CheckBlocking(Server& server) {
    cout << "[ACCEPTING]" << endl;
    Connection* conn = server.AcceptOne();
    assert(conn && "Failed to accept connection");
    
    // ********
    cout << "[PING-PONG TEST]" << endl;
    int32_t hello_random = rand();
    cout << "* sending server_info (random is " << hello_random << ")" << endl;
    Request hello_req("server_info");
    hello_req.Emplace<ValueStr>("type", "Test server");
    hello_req.Emplace<Value32U>("ver", 1);
    hello_req.Emplace<Value32S>("random", hello_random);
    conn->Push(hello_req);
    
    // ********
    uint8_t monologue_loops = 0;
    Request *res = conn->Pull();
    assert(res && "res == 0");
    assert(res->GetName() == "client_info" && "invalid request name");
    assert(res->Get<Value32S>("random") && "argument does not exist");
    assert(res->Get<Value8U>("loops") && "argument does not exist");
    assert(res->Get<Value32S>("random")->Get() == hello_random && "random mismatch");
    monologue_loops = res->Get<Value8U>("loops")->Get();
    delete res;

    // ********
    cout << "[MONOLOGUE-" << (int)monologue_loops << " TEST]" << endl;
    for (uint8_t i = 0; i < monologue_loops; i++) {
        Request* loop_req = new Request("monologue_loop");
        loop_req->Emplace<Value8U>("loop_id", i);
        conn->Push(loop_req, false);
    }

    // ********
    cout << "[CLOSE-ME TEST]" << endl;
    Request* close_me_req = conn->Pull();
    assert(close_me_req && "didn't receive anything");
    assert(close_me_req->GetName() == "close_me" && "invalid name");
    assert(close_me_req->Get<Value32U>("timeout") && "no timeout");
    uint32_t timeout = close_me_req->Get<Value32U>("timeout")->Get();
    cout << "* sleeping for " << (int)timeout << " seconds" << endl;
    sleep(timeout);
    conn->Close();
    cout << "* connection is closed" << endl;
    sleep(1);
    delete conn;
}

int main(int argc, char** argv) {
    // enable randomization
    srand(time(0));

    // the value of the test
    cout << "Please start the ./ClientTest" << endl;

    // testing the blocking TCP server
    cout << "[STARTING THE TCP BLOCKING SERVER]" << endl;
    Server server;
    bool result = server.StartTCP(
        "127.0.0.1",                // IP (using localhost)
        23050                       // port
    );
    if (!result) cerr << "errno: " << errno << endl;
    assert(server.GetType() == SocketTypeTCP && "Server failed to start, errno is logged above");
    cout << "[TCP SERVER ON " << server.GetTcpIpString() << ":" << server.GetTcpPort() << "]" << endl;
    CheckBlocking(server);
    server.Close();
    cout << "[TCP PASSED]" << endl;

    cout << "[STARTING THE UNIX BLOCKING SERVER]" << endl;
    result = server.StartUNIX("unix_socket.sock", true);
    if (!result) cerr << "errno: " << errno << endl;
    assert(server.GetType() == SocketTypeUNIX && "Server failed to start, errno is logged above");
    cout << "[UNIX SERVER ON " << server.GetUNIXPath() << "]" << endl;
    CheckBlocking(server);
    server.Close();
    cout << "[UNIX PASSED]" << endl;

    return 0;
}
