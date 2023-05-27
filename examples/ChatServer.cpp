// This examples shows how to use nonblocking server, nonblocking connections
// and handlers

#include "../Server.hpp"
#include "../values/All.hpp"

#include <algorithm>
#include <list>
#include <atomic>
#include <iostream>

using namespace std;
using namespace DowowNetwork;

// Structure for a chat user.
struct Participant {
    // the nickname
    std::string nickname;
    // amount of sent message
    uint32_t messages_sent;

    // assigned connection id
    uint32_t id;
};

// List of all chat participants.
list<Participant> participants;

Server server;

void IssueMessage(string from, string to, string text) {
    cout << "[" << from << " -> " << (to.empty() ? string("EVERYONE") : to) << "]: " << text << endl;
    // prepare the request
    Request req("message");
    req.Emplace<ValueStr>("from", from);
    req.Emplace<ValueStr>("text", text);
    // send to everyone
    if (to.empty()) {
        for (auto i : participants) {
            // get a specific connection
            auto c = server.GetConnection(i.id);

            // send
            (*c)()->Push(req);

            // delete
            delete c;
        }
    } else {
        auto i = find_if(
            participants.begin(),
            participants.end(),
            [&](Participant &p) {
                return p->username == to;
            });
        
        if (i != participants.end()) {
            req.Emplace<ValueStr>("to", to);

            // get a specific connection
            auto c = server.GetConnection(i->id);

            // send
            (*c)()->Push(req);

            // delete
            delete c;
        }
    }
}

Request GenerateErrorResponse(string text) {
    Request res = new Request("error");
    res->Emplace<ValueStr>("text", text);
    return res;
}


bool HandlerDefault(Connection *conn, Request *req, uint32_t id) {
    // log the invalid request
    cout << "Invalid request received:" << endl;
    cout << req->ToString() << endl;

    // invalid request - kill connection
    conn->Push(
        GenerateErrorResponse("Invalid request, closing connection"),
        false);
    conn->Disconnect();

    return true;
}

bool HandlerLogin(Connection *conn, Request *req, uint32_t id) {
    // check if already logged in
    SessionData *s_data = conn->GetSessionData<SessionData>();
    if (s_data->state != ClientStateLogin) {
        conn->Push(
            GenerateErrorResponse("you are already logged in"),
            false);
        return true;
    }
    // get some arg values
    auto username_v = req->Get<ValueStr>("username");

    // check if have args
    if (!username_v) {
        conn->Push(
            GenerateErrorResponse("no username"),
            false);
        return true;
    }

    // get the args
    string username = username_v->Get();

    // check args
    if (username.size() < 2 || username.size() > 32) {
        conn->Push(
            GenerateErrorResponse("username size lies in range [2; 32]"),
            false);
        return true;
    }

    // authorize
    s_data->state = ClientStateOnline;
    s_data->username = username;

    // send ok
    Request ok("authorized");
    ok.Emplace<Value32U>("users", connections.size());
    ok.Emplace<ValueStr>("server", server_name);
    ok.SetId(id);
    conn->Push(ok);

    // message
    IssueMessage(server_name, "", "Let's welcome " + username + " to our server!");

    return true;
}

bool HandlerSend(Connection* conn, Request* req, uint32_t id) {
    // check if not logged in
    SessionData *s_data = conn->GetSessionData<SessionData>();
    if (s_data->state != ClientStateOnline) {
        conn->Push(
            GenerateErrorResponse("you must authorize before sending messages"),
            false);
        return true;
    }
    
    // get the args
    ValueStr* to_v = req->Get<ValueStr>("to");
    ValueStr* text_v = req->Get<ValueStr>("text");

    if (!text_v) {
        conn->Push(
            GenerateErrorResponse("no text specified"),
            false);
        return true;
    }

    // get the values
    string to = "";
    string text = text_v->Get();
    if (to_v) to = to_v->Get();

    // check if text is too short
    if (text.size() < 1) {
        conn->Push(
            GenerateErrorResponse("the text is too short"),
            false);
        return true;
    }

    // send
    IssueMessage(s_data->username, to, text);

    return true;
}

bool HandlerBye(Connection* conn, Request* req, uint32_t id) {
    // response
    Request response("bye");
    conn->Push(response);
    conn->Disconnect();

    SessionData *s_data = conn->GetSessionData<SessionData>();

    // send
    if (s_data) {
        IssueMessage(s_data->username, "", "I'm leaving y'all, bye!");
        IssueMessage(server_name, "", "See you later, " + s_data->username + "!");
    }
    return true;
}

void NewConnection(Server* server, Connection* conn) {
    // generate session data
    SessionData* s_data = new SessionData();
    // assign the session data
    conn->SetSessionData(s_data);

    // send the auth request
    Request auth_req("auth_invite");
    auth_req.Emplace<ValueStr>("text", "Please authorize");
    auth_req.Emplace<ValueStr>("server", server_name);
    conn->Push(auth_req);

    // setup some handlers
    conn->SetHandlerDefault(HandlerDefault);
    conn->SetHandlerNamed("login", HandlerLogin);
    conn->SetHandlerNamed("send", HandlerSend);
    conn->SetHandlerNamed("bye", HandlerBye);
}

// start the server
void StartServer() {
    // used for input
    string temp;

    while (true) {
        // server data
        string ip;
        uint16_t port;

        // ip input
        cout << "IP to use: ";
        cout.flush();
        getline(cin, ip);
        // port input
        cout << "Port to use: ";
        cout.flush();
        getline(cin, temp);
        // server name input
        cout << "How to call the server: ";
        cout.flush();
        getline(cin, server_name);

        port = std::atoi(temp.c_str());

        if (!server.StartTcp(ip, port)) {
            cout << "Failed to start the server on " << ip << ":" << port << "! Try again." << endl;
        } else {
            cout << "Started the server, waiting for connections..." << endl;
            break;
        }
    }

    // make the server nonblocking
    server.SetNonblocking(true);

    // attach the new connection handler
    server.SetNewConnectionHandler(NewConnection);
}

int main() {
    StartServer();

    std::list<Connection*> connections_to_delete;
    while (true) {
        // accept new clients
        server.Accept(connections);

        // poll clients
        for (auto i : connections) {
            // poll
            i->Poll();
            // delete unhandled requests
            delete i->Pull();

            // check if disconnected
            if (!i->IsConnected()) {
                cout << "Disconnected" << endl;
                connections_to_delete.push_back(i);
            }
        }

        // delete disconnected
        for (auto i : connections_to_delete) {
            delete i->GetSessionData<SessionData>();
            delete i;
            connections.remove(i);
            cout << "Deleted, " << connections.size() << endl;
        }
        connections_to_delete.clear();
    }
    return 0;
}
