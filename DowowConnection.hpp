#ifndef _DOWOW_NETWORK__DOWOW_CONNECTION_H_
#define _DOWOW_NETWORK__DOWOW_CONNECTION_H_

#include <string>
#include <time.h>
#include <map>
#include <mutex>

#include "Connection.hpp"
#include "Request.hpp"

namespace DowowNetwork {
    // declare for typedef
    class DowowConnection;

    // the first argument is the invoking connection,
    // the second argument is the response of the client
    // the third argument is the id of the request
    typedef int (*RequestHandler)(DowowNetwork::DowowConnection*, DowowNetwork::Request*, uint32_t);

    // the connection
    class DowowConnection {
    private:
        // our keepalive period
        const time_t our_ka_period = 5;
        // their keepalive period limit
        const time_t their_ka_period_limit = 11;
        // the time before considering the response lost
        const time_t time_before_lost = 30;

        // definition of specific handler
        struct SpecificHandler {
            // the time when the response is considered to be lost
            time_t lost_on = 0;
            // the handler
            RequestHandler handler = 0;
        };

        // mutex for exclusive operations
        std::mutex conn_mutex;

        // this is the connection we're driving
        Connection *base;

        // free request id
        uint32_t free_request_id = 1;

        // the map of handlers
        std::map<std::string, RequestHandler> handlers;
        // the default handler
        RequestHandler default_handler = 0;
        // the map of specific handlers
        std::map<uint32_t, SpecificHandler> specific_handlers;

        // the map for multithreading
        std::map<uint32_t, Request*> response_map;

        // the time we need to keep-alive
        time_t our_ka_schedule = 0;
        // the time they are considered lost
        time_t their_ka_schedule = 0;
    public:
        DowowConnection(Connection* base);

        // must be called often to do nonblocking I/O
        void Handle();

        // send the request.
        // returns the id that was assigned to the request.
        // the request is copied if to_copy.
        // the response will be processed by handler function, if it's not 0.
        // if the response wasn't received within 30 seconds, the handler will
        // be called with request being 0.
        // if the handler is not set, then the function will be processed by
        // named handler.
        uint32_t Send(
            Request *req,
            bool to_copy = true,
            RequestHandler handler = 0);

        // sends the request and waits for response (act like blocking function).
        // MUST BE STARTED IN NEW THREAD.
        Request* Execute(Request* req, bool to_copy = true);

        // set the handler for request with name
        void SetHandler(std::string request_name, RequestHandler handler);
        // set the default handler
        void SetDefaultHandler(RequestHandler handler);

        // get the handler
        RequestHandler GetHandler(std::string request_name);
        // get the default handler
        RequestHandler GetDefaultHandler();

        // ask for connection closure
        void Disconnect(bool force = false);

        // true if yes
        bool IsConnected();

        ~DowowConnection();
    };
}

#endif
