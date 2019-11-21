//
// Created by ncl on 29/10/19.
//

#include <tlibc/mbusafecrt.h>
#include "rpc_client_port.hxx"
#include "messages.hxx"

using cornerstone::rpc_exception;
using cornerstone::cs_new;
using cornerstone::resp_msg;
using cornerstone::msg_type;

map<uint64_t, raft_callback_item> raft_rpc_client_callback_pool;

mutex rpc_client_callback_pool_lock;
atomic<uint32_t> last_req_uid_;


static void handle_system_resp(er_message_type type, uint32_t req_uid, const uint8_t *data, uint32_t size);

static void handle_raft_message(uint32_t req_uid, const uint8_t *data, uint32_t size);

void ecall_rpc_response(uint32_t req_uid, uint32_t size, const uint8_t *msg, const char *exception) {
    if (exception != nullptr) {
        p_logger->warn("ecall_rpc_response received exception = " + string(exception));
        return;
    }

    auto type = static_cast<er_message_type>(*(uint16_t *) msg);

    switch (type) {
        case system_key_exchange_resp:
        case system_key_setup_resp:
            handle_system_resp(type, req_uid, msg + sizeof(uint16_t), size - sizeof(uint16_t));
            break;
        case raft_message:
            handle_raft_message(req_uid, msg + sizeof(uint16_t), size - sizeof(uint16_t));
            break;
        default:
            return;
    }

}

void handle_system_resp(er_message_type type, uint32_t req_uid, const uint8_t *data, uint32_t size) {
    string payload = string(data, data + size);
    switch (type) {
        case system_key_exchange_resp:
            key_store->handle_key_xchg_resp(payload, "");
            return;
        case system_key_setup_resp:
            return;
        default:
            return;
    }
}

void handle_raft_message(uint32_t req_uid, const uint8_t *data, uint32_t size) {
    ptr<req_msg> req;
    rpc_handler when_done;
    {
        lock_guard<mutex> lock(rpc_client_callback_pool_lock);
        auto item = raft_rpc_client_callback_pool[req_uid];
        req = item.first;
        when_done = item.second;
        raft_rpc_client_callback_pool.erase(req_uid);
    }

    ptr<resp_msg> rsp = nullptr;
    ptr<rpc_exception> except = nullptr;

    // FIXME: Decrypt
    shared_ptr<vector<uint8_t >> message_buffer = raft_decrypt(data, size);

    bufptr resp_buf = buffer::alloc(RPC_RESP_HEADER_SIZE);
    memcpy_s(resp_buf->data(), resp_buf->size(), message_buffer->data(), message_buffer->size());

    rsp = deserialize_resp(resp_buf);

    when_done(rsp, except);
}


