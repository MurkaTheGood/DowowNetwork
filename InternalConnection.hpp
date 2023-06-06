#ifndef __DOWOW_NETWORK__INTERNAL_CONNECTION_H_
#define __DOWOW_NETWORK__INTERNAL_CONNECTION_H_

#include "Request.hpp"

#include <cstdint>

#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#define CONN_OT_INTERVAL 10
#define CONN_TT_INTERVAL 60

namespace DowowNetwork {
    class InternalConnection;

    typedef void (*RequestHandler)(InternalConnection *c, Request* r);

    class InternalConnection final {
    private:
        // fds
        int socket_fd = -1;
        int stopped_event = -1;
        int to_stop_event = -1;
        int push_event = -1;

        // free request id
        uint32_t free_rid = 1;
        bool even_rids = false;

        // disconnecting gracefully?
        bool is_disconnecting = false;

        // queues
        std::list<Request*> send_queue;
        std::list<Request*> recv_queue;

        // send buffer
        const char *send_buffer = 0;
        uint32_t send_buffer_size = 0;
        uint32_t send_buffer_offset = 0;

        // receive buffer
        char *recv_buffer = 0;
        uint32_t recv_buffer_size = 0;
        uint32_t recv_buffer_offset = 0;
        std::map<uint32_t, std::pair<int, Request*>> push_subscribe;
        std::map<int, Request*> pull_subscribe;

        // handlers
        std::map<std::string, RequestHandler> handlers_named;
        RequestHandler handler_default = 0;
        // handlers stopped events
        std::map<int, std::thread*> handler_stop_events;

        // the background thread
        std::thread *bg_thread = 0;

        // last errno
        int last_errno = 0;

        // the handler thread logic
        static void HandlerBootstapper(
                InternalConnection *c,
                RequestHandler h,
                Request *r,
                int stopped_event);
        // the logic of the background thread
        static void BackgroundFunction(InternalConnection *c);

        // handle the received request
        void HandleReceived(Request *r);

        // returns true if send queue or buffer is not empty
        bool HasSomethingToSend();

        // pop the send queue and put the result in send buffer
        // returns false if the queue is empty
        bool PopSendQueue();

        // send logic (called in BackgroundFunction only).
        // returns true on success.
        bool Send();

        // receive logic (called in BackgroundFunction only).
        // returns true on success.
        bool Recv();
    public:
        //! Not used inside the class itself.
        uint32_t id = 0;

        InternalConnection() = delete;
        InternalConnection(int fd);

        Request *Push(Request *r, bool change_id = true, bool copy = false, int timeout = 0);
        Request *Push(const Request &r, bool change_id = true, int timeout = 0);

        Request *Pull(int timeout = -1);

        bool WaitForStop(int timeout = -1);
        void Disconnect(bool forced = true);

        int GetStoppedEvent();
        bool IsConnected();

        void SetHandlerNamed(std::string name, RequestHandler h);
        RequestHandler GetHandlerNamed(std::string name);

        void SetHandlerDefault(RequestHandler h);
        RequestHandler GetHandlerDefault();

        int GetErrno();

        ~InternalConnection();
    };
}

#endif
