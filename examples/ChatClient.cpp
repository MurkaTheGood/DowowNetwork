/*!
 \file
 This example shows how the client interface works.
*/

#include "../Client.hpp"
#include "../values/All.hpp"

#include <algorithm>
#include <list>
#include <iostream>
#include <utility>

#include <ncurses.h>

using namespace std;
using namespace DowowNetwork;

// Terminal size
int rows, cols;
// History
std::list<pair<string, string>> history;

// the text that user types in
string input_field;

// client to use
Client client;

// is logging in?
bool is_login = false;

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
    // go to the screen bottom
    move(rows - 1, 0);
    // clear the bottom line
    for (int i = 0; i < cols; i++)
        addch(' ');

    // draw the input invitation
    mvaddch(rows - 1, 0, '>' | A_BOLD);
    // draw the input field content
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
        attron(A_BOLD);
        // draw the issuer name
        mvaddstr(begin_row, 0, from.c_str());
        // attribute for bold
        attroff(A_BOLD);

        // print the message
        mvaddstr(begin_row, from.size() + 1, i->second.c_str());

        // prev. line
        begin_row--;
    }

    // move to input field
    move(rows - 1, 2 + input_field.size());
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

void HandlerAuth(Connection *c, Request *r) {
    is_login = true;
}

void HandlerAuthSuccess(Connection *c, Request *r) {
    is_login = false;
}

void HandlerMessage(Connection *c, Request *r) {
    auto from_v = r->Get<ValueStr>("from");
    auto to_v = r->Get<ValueStr>("to");
    auto text_v = r->Get<ValueStr>("text");
    addToHistory(from_v ? from_v->Get() : "THE WIND", text_v->Get());
    redrawChat();
}

void HandlerBye(Connection *c, Request *r) {
    client.Disconnect(true, false);
}

int main() {
    // attach handlers
    client.SetHandlerNamed("auth", HandlerAuth);
    client.SetHandlerNamed("auth_success", HandlerAuthSuccess);
    client.SetHandlerNamed("message", HandlerMessage);
    // TODO: client.SetHandlerNamed("status", HandlerStatus);
    client.SetHandlerNamed("bye", HandlerBye);

    // get the ip
    string ip;
    cout << "ncurses + DowowNetwork CHAT" << endl;
    while (!client.IsConnected()) {
        cout << "Server IP: ";
        cout.flush();
        cin >> ip;
        cout << "Trying to connect to " << ip << ":29000..." << endl;
        if (!client.ConnectTcp(ip, 29000)) {
            cout << "Couldn't connect to the server on " << ip << ", try again" << endl;
        }
    }

    ncursesInit();

    redrawChat();
    redrawInput();
    refresh();

    while (client.IsConnected()) {
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
                        Request send_request("message");
                        send_request.Emplace<ValueStr>("text", input_field);
                        client.Push(send_request);
                    } else {
                        Request login_request("auth");
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

    // wait for disconnection
    client.WaitForStop(-1);
    
    cout << "Disconnected" << endl;

    return 0;
}
