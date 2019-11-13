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

map<uint64_t, callback_item> rpc_client_callback_pool;
mutex rpc_client_callback_pool_lock;
atomic<uint32_t> last_req_uid_;

void ecall_rpc_response(uint32_t req_uid, uint32_t size, const uint8_t *msg, const char *exception) {
    ptr<req_msg> req;
    rpc_handler when_done;
    {
        lock_guard<mutex> lock(rpc_client_callback_pool_lock);
        auto item = rpc_client_callback_pool[req_uid];
        req = item.first;
        when_done = item.second;
        rpc_client_callback_pool.erase(req_uid);
    }


    ptr<resp_msg> rsp = nullptr;
    ptr<rpc_exception> except = nullptr;


    auto type = static_cast<er_message_type>(*(uint16_t *) msg);
    if (type != raft_message) {
        except = cs_new<rpc_exception>("Unexpected Message Type", req);
    } else if (!exception) {
        // FIXME: Decrypt
        shared_ptr<vector<uint8_t >> message_buffer = raft_decrypt(msg + sizeof(uint16_t), size - sizeof(uint16_t));

        bufptr resp_buf = buffer::alloc(RPC_RESP_HEADER_SIZE);
        memcpy_s(resp_buf->data(), resp_buf->size(), message_buffer->data(), message_buffer->size());

        rsp = deserialize_resp(resp_buf);
    } else {
        except = cs_new<rpc_exception>(exception, req);
    }

    when_done(rsp, except);
}