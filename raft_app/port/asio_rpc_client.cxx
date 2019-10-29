//
// Created by ncl on 29/10/19.
//

#include "raft_enclave_u.h"
#include "asio_rpc_client.hxx"

#include <regex>
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <spdlog/logger.h>

using std::regex;
using std::smatch;
using std::regex_match;
using std::string;
using std::map;
using std::atomic;
using std::mutex;
using std::lock_guard;

extern shared_ptr<asio::io_context> global_io_context;

static mutex rpc_client_pool_lock;
static atomic<uint32_t> rpc_client_id_counter;
static map<uint32_t, shared_ptr<asio_rpc_client>> rpc_client_pool;


uint32_t ocall_rpc_client_create(const char *endpoint) {
    // the endpoint is expecting to be protocol://host:port, and we only support tcp for this factory
    // which is endpoint must be tcp://hostname:port

    string ep = endpoint;
    static regex reg("^tcp://(([a-zA-Z0-9\\-]+\\.)*([a-zA-Z0-9]+)):([0-9]+)$");
    smatch match_results;
    if (!regex_match(ep, match_results, reg) || match_results.size() != 5) {
        return 0;
    }

    std::string hostname = match_results[1].str();
    std::string port = match_results[4].str();

    auto client = make_shared<asio_rpc_client>(*global_io_context, hostname, port);
    auto client_id = ++rpc_client_id_counter;
    {
        lock_guard<mutex> lock(rpc_client_pool_lock);
        rpc_client_pool[client_id] = client;
    }

    return client_id;
}

void ocall_rpc_client_close(uint32_t client_uid) {
    lock_guard<mutex> lock(rpc_client_pool_lock);
    auto it = rpc_client_pool.find(client_uid);
    if (it != rpc_client_pool.end()) {
        rpc_client_pool.erase(it);
    }
}

void ocall_send_rpc_request(uint32_t client_uid, uint32_t size, const uint8_t *message, uint32_t request_uid) {
    shared_ptr<asio_rpc_client> client = nullptr;
    {
        lock_guard<mutex> lock(rpc_client_pool_lock);
        auto it = rpc_client_pool.find(client_uid);
        if (it != rpc_client_pool.end()) {
            rpc_client_pool.erase(it);
        }
    }

    if (client) {
        client->send(request_uid, message, size);
    }
}

