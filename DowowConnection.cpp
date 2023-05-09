#include "DowowConnection.hpp"
#include "values/All.hpp"

#include <ctime>

DowowNetwork::DowowConnection::DowowConnection(Connection* base) : base(base) {
    // set to be nonblocking
    base->SetBlocking(false);
}

void DowowNetwork::DowowConnection::Handle() {
    // lock
    conn_mutex.lock();

    // not connected
    if (!IsConnected()) return;

    // handle
    base->Handle();

    // request to parse (will be got later)
    Request *req = 0;
    // no more than 5 requests per Handle() call.
    // this is done to give some time for other
    // connections, if this one has sent too much data.
    for (int i = 0; i < 5; i++) {
        // delete the old request
        delete req;

        // try to pull the request
        req = base->Pull();
        // no request
        if (req) break;

        // check if the id is more than our free request id
        if (req->GetId() >= free_request_id)
            free_request_id = req->GetId() + 1;

        // check if reserved
        if (req->GetName() == "keep_alive") {
            // they are alive
            their_ka_schedule = time(0) + their_ka_period_limit;
        } else if (req->GetName() == "disconnect") {
            // check if forced
            Value8S *v_forced = req->Get<Value8S>("forced");
            if (v_forced && v_forced->Get())
                Disconnect(true);
            else
                Disconnect(false);
        }

        // check if multithreaded
        auto rm_i = response_map.find(req->GetId());
        if (rm_i != response_map.end()) {
            // copy
            response_map[req->GetId()] = new Request();
            response_map[req->GetId()]->CopyFrom(req);
            // next request
            continue;
        }

        // check if we have specific handler
        auto s_h = specific_handlers.find(req->GetId());
        if (s_h != specific_handlers.end()) {
            // handling
            s_h->second.handler(this, req, s_h->first);
            // delete the specific handler
            specific_handlers.erase(s_h->first);

            // the request is processed - next request
            continue;
        }

        // check if we have named handler
        auto n_h = handlers.find(req->GetName());
        if (n_h != handlers.end()) {
            // handling
            (*n_h->second)(this, req, req->GetId());

            // next request
            continue;
        }

        // default handler (if set)
        if (default_handler) {
            (*default_handler)(this, req, req->GetId());
        }
    }
    delete req;

    // find lost specific requests
    std::list<uint32_t> specific_to_delete;
    for (auto& i: specific_handlers) {
        // check if outdated
        if (time(0) >= i.second.lost_on) {
            // call the handler with no request
            (*i.second.handler)(this, 0, i.first);
            // add to the deletion list
            specific_to_delete.push_back(i.first);
        }
    }
    // delete outdated requests
    for (auto& i: specific_to_delete) {
        specific_handlers.erase(i);
    }

    // keep-alive processing
    if (time(0) >= our_ka_schedule) {
        our_ka_schedule = time(0) + our_ka_period;

        Request *our_ka_req = new Request("keep_alive");
        base->Push(our_ka_req, false);
    }
    // they must be already dead
    if (time(0) >= their_ka_schedule) {
        Disconnect(false);
    }

    // unlock the handler mutex
    conn_mutex.unlock();
}

uint32_t DowowNetwork::DowowConnection::Send(
    DowowNetwork::Request *req, bool to_copy, DowowNetwork::RequestHandler handler)
{
    // not connected
    if (!IsConnected()) return 0;

    // lock
    conn_mutex.lock();

    // the id we're using
    uint32_t req_id = free_request_id++;

    // set the id
    req->SetId(req_id);

    // push the request
    base->Push(req, to_copy);

    // add the specific handler
    if (handler) {
        auto s_h = SpecificHandler();
        s_h.lost_on = time(0) + time_before_lost;
        s_h.handler = handler;
        specific_handlers[req_id] = s_h;
    }

    // unlock
    conn_mutex.unlock();

    return req_id;
}

DowowNetwork::Request *DowowNetwork::DowowConnection::Execute(DowowNetwork::Request *req, bool to_copy) {
    // making the request
    uint32_t id = Send(req, to_copy);

    // lock
    conn_mutex.lock();

    // subscribe
    response_map[id] = 0;

    // unlock
    conn_mutex.unlock();

    // timeout
    time_t time_limit = time(0) + 30;
    
    // result
    Request* res = 0;

    // waiting for response
    while (true) {
        // lock
        conn_mutex.lock();

        res = response_map[id];

        // unlock
        conn_mutex.unlock();

        if (res || time(0) > time_limit) break;
    }

    // lock
    conn_mutex.lock();

    // delete from subscribed
    response_map.erase(id);

    // unlock
    conn_mutex.unlock();

    return res;
}

void DowowNetwork::DowowConnection::SetHandler(std::string req_name, DowowNetwork::RequestHandler handler) {
    // remove
    if (handler == 0) {
        auto it = handlers.find(req_name);
        if (it != handlers.end())
            handlers.erase(req_name);
    }
    // set
    else {
        handlers[req_name] = handler;
    }
}

void DowowNetwork::DowowConnection::SetDefaultHandler(DowowNetwork::RequestHandler handler) {
    default_handler = handler;
}

DowowNetwork::RequestHandler DowowNetwork::DowowConnection::GetHandler(std::string req_name) {
    auto it = handlers.find(req_name);
    if (it == handlers.end()) return 0;
    return it->second;
}

DowowNetwork::RequestHandler DowowNetwork::DowowConnection::GetDefaultHandler() {
    return default_handler;
}

void DowowNetwork::DowowConnection::Disconnect(bool force) {
    if (!IsConnected()) return;

    base->Close(force);
}

bool DowowNetwork::DowowConnection::IsConnected() {
    return !base->IsClosed();
}

DowowNetwork::DowowConnection::~DowowConnection() {
    Disconnect(true);
}
