// This examples shows how to use nonblocking clients
// ncurses is used as interface

#include "../Client.hpp"
#include "../values/All.hpp"

#include <algorithm>
#include <list>
#include <iostream>
#include <utility>

#include <ncurses.h>

using namespace std;
using namespace DowowNetwork;

int rows, cols;
std::list<pair<string, string>> history;

// the text that user types in
string input_field;

// client to use
Client client;

// is login?
bool is_login = false;

// server name
string server_name = "SERVER";

// add a line to the history
void addToHistory(string from, string text) {
    history.push_back(make_pair(from, text));

    // check if too many messages
    if (history.size() > rows - 1) {
        // delete the oldest message
        history.erase(history.begin());
    }
}

// redraw input
void redrawInput() {
    move(rows - 1, 0);
    for (int i = 0; i < cols; i++)
        addch(' ');

    // draw the input
    mvaddch(rows - 1, 0, '>' | A_BOLD);
    // draw the input
    mvprintw(rows - 1, 2, input_field.c_str());
}

// redraw chat area
void redrawChat() {
    // draw the history
    int begin_row = rows - 2;

    // clear chat area
    for (int i = 0; i < rows - 1; i++) {
        move(i, 0);
        for (int j = 0; j < cols; j++)
            addch(' ');
    }

    // draw
    for (auto i = history.rbegin(); i != history.rend(); ++i) {
        // string with from name
        string from = "[" + i->first + "]";
        // attribute for bold
        // attron(A_BOLD);
        if (from == server_name || from == "CLIENT")
            attron(COLOR_PAIR(2));
        // draw the issuer name
        mvaddstr(begin_row, 0, from.c_str());
        // attribute for bold
        // attroff(A_BOLD);
        if (from == server_name || from == "CLIENT")
            attroff(COLOR_PAIR(2));

        // print the message
        mvaddstr(begin_row, from.size() + 1, i->second.c_str());

        begin_row--;
    }

    // move to input field
    move(rows - 1, 2 + input_field.size());
}

bool handlerAuthInvite(Connection* conn, Request* req, uint32_t id) {
    // login mark
    is_login = true;
    // the value for server name
    auto server_name_v = req->Get<ValueStr>("server");
    // notify
    addToHistory("CLIENT", "This server requires authorization!");
    // no server name
    if (!server_name_v) {
        addToHistory("CLIENT", "Server name is unknown");
    } else {
        server_name = server_name_v->Get();
        addToHistory("CLIENT", "Server name is " + server_name);
    }
    // the text from server
    auto server_text_v = req->Get<ValueStr>("text");
    if (server_text_v) {
        addToHistory(server_name, server_text_v->Get());
    }

    // redraw chat
    redrawChat();

    return true;
}

bool handlerAuthorized(Connection* conn, Request* req, uint32_t id) {
    // authhorized
    is_login = false;
    addToHistory(server_name, "Authorized users: " + to_string(req->Get<Value32U>("users")->Get()));
    redrawChat();
    return true;
}

bool handlerError(Connection* conn, Request* req, uint32_t id) {
    addToHistory(server_name, "Error: " + req->Get<ValueStr>("text")->Get());
    redrawChat();
    return true;
}

bool handlerMessage(Connection* conn, Request* req, uint32_t id) {
    addToHistory(req->Get<ValueStr>("from")->Get(), req->Get<ValueStr>("text")->Get());
    redrawChat();
    return true;
}


// initialize ncurses
void ncursesInit() {
    // init ncurses
    initscr();
    // colored terminal
    start_color();
    // pairs
    init_pair(1, COLOR_RED, COLOR_BLACK);     // regular message
    // disable echo
    noecho();
    // disable buffering
    cbreak();

    // get the rows count
    getmaxyx(stdscr, rows, cols);

    // clear the screen
    clear();

    // make the read ops nonblocking
    timeout(0);

    refresh();
}

int main() {
    // attach handlers
    client.SetNonblocking(true);
    client.SetHandlerNamed("auth_invite", handlerAuthInvite);
    client.SetHandlerNamed("authorized", handlerAuthorized);
    client.SetHandlerNamed("error", handlerError);
    client.SetHandlerNamed("message", handlerMessage);

    // before connecting
    while (!client.IsConnected()) {
        std::string temp;
        std::string ip;
        uint16_t port;
        cout << "Server IP: ";
        cout.flush();
        getline(cin, ip);
        cout << "Server Port: ";
        getline(cin, temp);
        port = atoi(temp.c_str());

        // try to connect
        bool res = client.ConnectTcp(ip, port);
        if (!res) {
            cout << "Couldn't connect to " << ip << ":" << port << endl;
        } else {
            cout << "Connecting..." << endl;
            while (client.IsConnecting()) {
                client.Poll();
            }
        }
    }


    ncursesInit();

    redrawChat();
    redrawInput();
    refresh();

    while (client.IsConnected()) {
        // poll the client
        client.Poll();

        // try to read the input
        int input_ch = getch();

        if (input_ch != -1) {
            if ((input_ch >= ' ' && input_ch <= '~') ||
                (input_ch >= 'A' && input_ch <= 'Z') ||
                (input_ch >= '0' && input_ch <= '9') ||
                input_ch == ' ' || input_ch == '/')
            {
                input_field += char(input_ch);
            } else {
                // backspace
                if (input_ch == 127) {
                    if (input_field.size()) {
                        input_field.erase(input_field.end() - 1);
                    }
                }
                // enter
                else if (input_ch == 10) {
                    if (!is_login) {
                        if (input_field == "/help") {
                            addToHistory("CLIENT", "/bye - disconnect");
                            addToHistory("CLIENT", "/help - show these messages");
                            addToHistory("CLIENT", "/users - show connected users");
                        } else if (input_field == "/bye") {
                            Request bye_request("bye");
                            client.Push(bye_request);
                        } else if (input_field == "/users") {
                            Request users_request("users");
                            client.Push(users_request);
                        } else {
                            Request send_request("send");
                            send_request.Emplace<ValueStr>("text", input_field);
                            client.Push(send_request);
                        }
                    } else {
                        Request login_request("login");
                        login_request.Emplace<ValueStr>("username", input_field);
                        client.Push(login_request);
                    }
                    input_field.clear();
                    redrawInput();
                } else {
                    input_field += std::to_string(input_ch);
                }
            }
            redrawInput();
        }
    }

    endwin();

    return 0;
}
