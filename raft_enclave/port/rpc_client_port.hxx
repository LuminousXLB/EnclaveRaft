//
// Created by ncl on 28/10/19.
//

#ifndef ENCLAVERAFT_RPC_CLIENT_PORT_HXX
#define ENCLAVERAFT_RPC_CLIENT_PORT_HXX

#include "raft_enclave_t.h"
#include "../raft/include/cornerstone.hxx"
#include "rpc_listener_port.hxx"
#include "msg_serializer.hxx"

#include <atomic>
#include <map>
#include <mutex>

using std::string;
using std::atomic;
using std::map;
using std::mutex;
using std::lock_guard;
using std::pair;
using std::make_pair;

using cornerstone::rpc_client;
using cornerstone::ptr;
using cornerstone::buffer;
using cornerstone::bufptr;
using cornerstone::req_msg;
using cornerstone::log_entry;
using cornerstone::rpc_handler;
using cornerstone::lstrfmt;


using callback_item = pair<ptr<req_msg>, rpc_handler>;
extern map<uint64_t, callback_item> rpc_client_callback_pool;
extern mutex rpc_client_callback_pool_lock;
extern atomic<uint32_t> last_req_uid_;

static uint32_t rpc_client_create(const string &endpoint) {
    uint32_t ret_val;
    sgx_status_t status = ocall_rpc_client_create(&ret_val, endpoint.c_str());
    return ret_val;
}


class RpcClientPort : public rpc_client {
public:
    explicit RpcClientPort(const string &endpoint) : client_uid_(rpc_client_create(endpoint)) {}

    ~RpcClientPort() override {
        {
            lock_guard<mutex> lock(rpc_client_callback_pool_lock);
            auto it = rpc_client_callback_pool.find(client_uid_);
            if (it != rpc_client_callback_pool.end()) {
                rpc_client_callback_pool.erase(it);
            }
        }

        ocall_rpc_client_close(client_uid_);
    }

    void send(ptr<req_msg> &req, rpc_handler &when_done) override {
        // FIXME: Encrypt & Decrypt
        bufptr message_buffer = serialize_req(req);

        uint32_t uid = 0;
        {
            lock_guard<mutex> lock(rpc_client_callback_pool_lock);
            uid = ++last_req_uid_;
            rpc_client_callback_pool[uid] = make_pair(req, when_done);
        }

        ocall_send_rpc_request(client_uid_, message_buffer->size(), message_buffer->data(), uid);
    }

private:
    uint32_t client_uid_;
};


#endif //ENCLAVERAFT_RPC_CLIENT_PORT_HXX
