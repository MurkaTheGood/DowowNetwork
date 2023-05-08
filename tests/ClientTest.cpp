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
#include <time.h>

using namespace std;
using namespace DowowNetwork;

bool is_running = true;

// this function creates some files for testing
void SleepMS(long ms) {
    timespec ts { 0, ms * 1000 * 1000 };
    nanosleep(&ts, 0);
}

void CheckBlocking(Client& client) {
    // amount of loops in monologue test
    uint8_t monologue_loops = 100 + (rand() % 100);

    cout << "[PING-PONG TEST]" << endl;

    // request to parse (will be received later)
    Request* received = 0;
    // random number from hello
    int32_t hello_random = 0;

    // ********
    received = client.Pull();
    cout << "* received server_info" << endl;
    assert(received && "received == 0");
    assert(received->GetName() == "server_info" && "invalid request name");
    assert(received->Get<ValueStr>("type") && "argument does not exist");
    assert(received->Get<Value32U>("ver") && "argument does not exist");
    assert(received->Get<Value32S>("random") && "argument does not exist");
    cout << 
        "\tServer type: " << received->Get<ValueStr>("type")->Get() << endl;
    cout <<
        "\tServer version: " << received->Get<Value32U>("ver")->Get() << endl;
    cout <<
        "\tServer's random number: " << received->Get<Value32S>("random")->Get() << endl;
    hello_random = received->Get<Value32S>("random")->Get();
    delete received;

    // ********
    cout << "* sending client_info (loops is " << (int)monologue_loops << ")" << endl;
    Request hello_res("client_info");
    hello_res.Emplace<Value32S>("random", hello_random);
    hello_res.Emplace<Value8U>("loops", monologue_loops);
    client.Push(hello_res);

    cout << "[MONOLOGUE-" << (int)monologue_loops << " TEST]" << endl;
    for (uint8_t i = 0; i < monologue_loops; i++) {
        Request* res = client.Pull();
        assert(res && "res == 0");
        assert(res->GetName() == "monologue_loop" && "invalid request name");
        assert(res->Get<Value8U>("loop_id") && "argument does not exist");
        assert(res->Get<Value8U>("loop_id")->Get() == i && "invalid loop_id");
        delete res;
    }

    // *******
    cout << "[CLOSE-ME TEST]" << endl;

    Request close_me("close_me");
    close_me.Emplace<Value32U>("timeout", 2 + (rand() % 5));
    client.Push(close_me);

    assert(client.IsConnected() && "connection lost");
    Request* res = client.Pull();
    assert(res == 0 && "res must be 0");
    assert(!client.IsConnected() && "must be disconnected");
    
    cout << "[PASSED]" << endl;
}

int main(int argc, char** argv) {
    // enable randomization
    srand(time(0));

    // address
    std::string ip = "127.0.0.1";
    uint16_t port = 23050;

    // connect TCP
    Client client;
    if (!client.ConnectTCP(ip, port, false)) {
        cout << "Failed to connect to " << ip << ":" << port << endl;
        return 1;
    }
    cout << "[CONNECTED TO " << ip << ":" << port << "]" << endl;
    CheckBlocking(client);

    // sleep before using UNIX sockets
    sleep(3);

    if (!client.ConnectUNIX("unix_socket.sock", false)) {
        cout << "Failed to connect to unix_socket.sock" << endl;
        return 1;
    }
    cout << "[CONNECTED TO unix_socket.sock]" << endl;
    CheckBlocking(client);

    return 0;
}
