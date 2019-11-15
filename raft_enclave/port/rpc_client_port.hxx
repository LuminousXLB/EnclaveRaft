//
// Created by ncl on 28/10/19.
//

#ifndef ENCLAVERAFT_RPC_CLIENT_PORT_HXX
#define ENCLAVERAFT_RPC_CLIENT_PORT_HXX

#include "common.hxx"
#include <map>
#include <atomic>
#include <mutex>

#include "raft_enclave_t.h"
#include "../raft/include/cornerstone.hxx"
#include "rpc_listener_port.hxx"
#include "msg_serializer.hxx"
#include "crypto.hxx"
#include "messages.hxx"
#include "json11.hpp"

using json11::Json;

using std::map;
using std::pair;
using std::make_pair;
using std::atomic;
using std::mutex;
using std::lock_guard;

using cornerstone::rpc_client;
using cornerstone::buffer;
using cornerstone::bufptr;
using cornerstone::req_msg;
using cornerstone::log_entry;
using cornerstone::rpc_handler;
using cornerstone::lstrfmt;


using raft_callback_item = pair<ptr<req_msg>, rpc_handler>;

//using system_callback = std::function<void(const string &, string)>;
//using system_callback_item = pair<ptr<string>, system_callback>;
//extern map<uint64_t, system_callback_item> system_rpc_client_callback_pool;

extern map<uint64_t, raft_callback_item> raft_rpc_client_callback_pool;
extern mutex rpc_client_callback_pool_lock;
extern atomic<uint32_t> last_req_uid_;

static uint32_t rpc_client_create(const string &endpoint) {
    uint32_t ret_val;
    sgx_status_t status = ocall_rpc_client_create(&ret_val, endpoint.c_str());
    if (status != SGX_SUCCESS) {
        p_logger->err(lstrfmt("ocall_rpc_client_create failed: %04x").fmt(status));
    }
    return ret_val;
}


class RpcClientPort : public rpc_client {
public:
    explicit RpcClientPort(const string &endpoint) : client_uid_(rpc_client_create(endpoint)) {}

    ~RpcClientPort() override {
        {
            lock_guard<mutex> lock(rpc_client_callback_pool_lock);
            auto it = raft_rpc_client_callback_pool.find(client_uid_);
            if (it != raft_rpc_client_callback_pool.end()) {
                raft_rpc_client_callback_pool.erase(it);
            }
        }

        ocall_rpc_client_close(client_uid_);
    }

    void send(ptr<req_msg> &req, rpc_handler &when_done) override {
        p_logger->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));

        bufptr req_buffer = serialize_req(req);

        // FIXME: Encrypt
        auto message_buffer = raft_encrypt(req_buffer->data(), req_buffer->size());

        uint32_t uid = ++last_req_uid_;;
        {
            lock_guard<mutex> lock(rpc_client_callback_pool_lock);
            raft_rpc_client_callback_pool[uid] = make_pair(req, when_done);
        }

        raw_send(raft_message, message_buffer, uid);
    }

    void send_xchg_request(int32_t my_id, const string &my_report) {
        Json body = Json::object{
                {"server_id", my_id},
                {"report",    my_report}
        };

        string body_str = body.dump();
        ptr<bytes> buffer = make_shared<bytes>(body_str.begin(), body_str.end());

        uint32_t uid = ++last_req_uid_;

        raw_send(system_key_exchange_req, buffer, uid);
    }

    void raw_send(er_message_type type, const ptr<bytes> &message, uint32_t unique_id) {
        auto *ptr = reinterpret_cast<uint8_t *>(&type);
        message->insert(message->begin(), ptr, ptr + sizeof(uint16_t));
        ocall_send_rpc_request(client_uid_, message->size(), message->data(), unique_id);
    }

private:
    uint32_t client_uid_;
};


#endif //ENCLAVERAFT_RPC_CLIENT_PORT_HXX
