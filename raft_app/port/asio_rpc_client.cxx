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
#include <spdlog/fmt/bin_to_hex.h>

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

#define __SLEEP__ std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(10))

uint32_t ocall_rpc_client_create(const char *endpoint) {
    __SLEEP__;

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

    auto client_id = ++rpc_client_id_counter;
    auto client = make_shared<asio_rpc_client>(*global_io_context, hostname, port, client_id);
    {
        lock_guard<mutex> lock(rpc_client_pool_lock);
        rpc_client_pool[client_id] = client;
    }

    global_logger->debug("{} {} {}: client_id={}, endpoint={}", __FILE__, __FUNCTION__, __LINE__,
                         client_id, endpoint);

    return client_id;
}

void ocall_rpc_client_close(uint32_t client_uid) {
    __SLEEP__;

    global_logger->debug("{} {} {}: {}", __FILE__, __FUNCTION__, __LINE__, client_uid);

    lock_guard<mutex> lock(rpc_client_pool_lock);
    auto it = rpc_client_pool.find(client_uid);
    if (it != rpc_client_pool.end()) {
        rpc_client_pool.erase(it);
    }
}

void ocall_send_rpc_request(uint32_t client_uid, uint32_t size, const uint8_t *message, uint32_t request_uid) {
    __SLEEP__;

    global_logger->debug("{} {} {}: client={}, request={}, req_size {}", __FILE__, __FUNCTION__, __LINE__,
                         client_uid, request_uid, size);
    global_logger->debug("{} {} {}: client={}, request={}, send {}", __FILE__, __FUNCTION__, __LINE__,
                         client_uid, request_uid, spdlog::to_hex(message, message + size));

    shared_ptr<asio_rpc_client> client = nullptr;
    {
        lock_guard<mutex> lock(rpc_client_pool_lock);
        auto it = rpc_client_pool.find(client_uid);
        client = it->second;
    }

    if (client) {
        client->send(request_uid, message, size);
    }
}

