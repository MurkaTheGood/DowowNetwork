#include "../Server.hpp"
#include "../values/All.hpp"

#include <algorithm>
#include <list>
#include <iostream>

using namespace std;
using namespace DowowNetwork;

// Structure for a chat user.
struct Participant {
    // the nickname
    std::string username;
    // amount of sent message
    uint32_t messages_sent;

    // assigned connection id
    uint32_t id;
};

// List of all chat participants.
list<Participant> participants;
// The server in use.
Server server;

Participant* GetParticipant(string username) {
    auto it = find_if(
            participants.begin(),
            participants.end(),
            [&](auto& a){ return a.username == username; });
    if (it != participants.end())
        return &(*it);
    return 0;
}

Participant* GetParticipant(uint32_t id) {
    auto it = find_if(
            participants.begin(),
            participants.end(),
            [&](auto& a){ return a.id == id; });
    if (it != participants.end())
        return &(*it);
    return 0;
}

// Generate a Request for a message.
Request GenerateMessage(string from, string to, string text) {
    Request r("message");
    if (from.size())
        r.Emplace<ValueStr>("from", from);
    if (to.size())
        r.Emplace<ValueStr>("to", to);
    r.Emplace<ValueStr>("text", text);
    return r;
}

// Generate a Request for status.
Request GenerateStatus(uint32_t id) {
    auto issuer = GetParticipant(id);
    Request r("status");
    r.Emplace<ValueStr>("username", issuer->username);
    r.Emplace<Value32U>("amount", participants.size());
    auto them_v = new ValueArr();
    them_v->New(participants.size());
    for (auto p : participants) {
        ValueStr vs = p.username;
        them_v->Push(&vs);
    }
    r.Set("them", them_v, false);
    return r;
}

// Issue a message.
void IssueMessage(string from, string to, string text) {
    if (to.empty()) {
        cout << "[" << from << "] " << text << endl;
        // Send to everyone.
        for (auto i: participants) {
            // Get the associated connection
            // Beware: the type is not Connection*, but SafeConnection*.
            auto c = server.GetConnection(i.id);
            // this participant has been disconnected
            if (!c) continue;
            // issue
            (*c)().Push(GenerateMessage(from, i.username, text));

            delete c;
        }
    } else {
        cout << "[" << from << " -> " + to + "] " << text << endl;
        // look for participant
        auto p = GetParticipant(to);
        // not found
        if (!p) return;

        // get connection
        SafeConnection *c = server.GetConnection(p->id);
        // not found
        if (!c) return;

        (*c)().Push(GenerateMessage(from, to, p->username));

        delete c;
    }
}

void HandlerAuth(Connection *c, Request *r) {
    auto username_v = r->Get<ValueStr>("username");
    if (!username_v) {
        c->Push(GenerateMessage("SERVER", "", "No username specified"));
        return;
    }
    string username = username_v->Get();
    if (username.size() < 2 || username.size() > 16) {
        c->Push(GenerateMessage("SERVER", "", "Username must be at least 2 and at most 16 symbols long"));
        return;
    }
    if (GetParticipant(username)) {
        c->Push(GenerateMessage("SERVER", "", "This username is already taken"));
        return;
    }
    // authorize
    Participant p;
    p.id = c->id;
    p.username = username;
    participants.push_back(p);
    c->Push(Request("auth_success"));
    IssueMessage(
            "SERVER",
            "",
            "Let's welcome " + username + " to our server! "
            "People online: " + to_string(participants.size()));
    IssueMessage(
            "SERVER",
            username,
            "Send '/help' to the global chat to get help about commands");
}

void HandlerMessage(Connection *c, Request *r) {
    auto p = GetParticipant(c->id);
    if (!p) {
        c->Push(GenerateMessage("SERVER", "", "You are not authorized"));
        return;
    }

    auto to_v = r->Get<ValueStr>("to");
    auto text_v = r->Get<ValueStr>("text");
    if (!text_v) {
        c->Push(GenerateMessage("SERVER", p->username, "No text is specified"));
        return;
    }

    string to = to_v ? to_v->Get() : string();
    string text = text_v->Get();

    IssueMessage(p->username, to, text);
}

void HandlerStatus(Connection *c, Request *r) {
    auto p = GetParticipant(c->id);
    if (!p) {
        c->Push(GenerateMessage("SERVER", "", "You are not authorized"));
        return;
    }

    c->Push(GenerateStatus(c->id));
}

void HandlerBye(Connection *c, Request *r) {
    c->Disconnect(true, false);
}

void HandlerConnected(Server *s, Connection *c) {
    // Connect handlers
    c->SetHandlerNamed("auth", HandlerAuth);
    c->SetHandlerNamed("message", HandlerMessage);
    c->SetHandlerNamed("status", HandlerStatus);
    c->SetHandlerNamed("bye", HandlerBye);
    // Authorization invitation.
    Request r("auth");
    c->Push(r);
    // Text.
    c->Push(GenerateMessage(
                 "SERVER",
                 "",
                 "Please authorize. We can not transfer any "
                 "messages unless we know your username."));
}

void HandlerDisconnected(Server *s, Connection *c) {
    // Find the participant
    Participant *p = GetParticipant(c->id);
    if (!p) return;

    // Log
    IssueMessage("SERVER", "", "Cya, " + p->username);

    // Remove from participants
    participants.remove_if([&](auto p) { return c->id == p.id; });
}

int main() {
    // Setup the handler for new connections
    server.SetConnectedHandler(HandlerConnected);
    server.SetDisconnectedHandler(HandlerDisconnected);
    // Try to start the server.
    if (!server.StartTcp("0.0.0.0", 29000)) {
        // Failed to start the server.
        cout << "Failed to start the chat server on 0.0.0.0:29000" << endl;
        return 1;
    }
    // Log.
    cout << "The server is running on 0.0.0.0:29000" << endl;

    // Wait for server stop.
    server.WaitForStop(-1);

    // Log.
    cout << "The server is stopped" << endl;
    return 0;
}
