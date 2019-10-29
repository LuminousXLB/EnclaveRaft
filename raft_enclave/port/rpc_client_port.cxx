//
// Created by ncl on 29/10/19.
//

#include <tlibc/mbusafecrt.h>
#include "rpc_client_port.hxx"

using cornerstone::rpc_exception;
using cornerstone::cs_new;
using cornerstone::resp_msg;
using cornerstone::msg_type;

map<uint64_t, callback_item> rpc_client_callback_pool;
mutex rpc_client_callback_pool_lock;

void ecall_rpc_response(uint32_t req_uid, uint32_t size, const uint8_t *msg, const char *exception) {
    ptr<req_msg> req;
    rpc_handler when_done;
    {
        lock_guard<mutex> lock(rpc_client_callback_pool_lock);
        auto val = rpc_client_callback_pool[req_uid];
        req = val.first;
        when_done = val.second;
        rpc_client_callback_pool.erase(req_uid);
    }

    ptr<resp_msg> rsp;
    ptr<rpc_exception> except;

    if (!exception) {
        bufptr resp_buf(buffer::alloc(RPC_RESP_HEADER_SIZE));
        // FIXME: Encrypt & Decrypt
        memcpy_s(resp_buf->data(), resp_buf->size(), msg, size);

        byte msg_type_val = resp_buf->get_byte();
        int32 src = resp_buf->get_int();
        int32 dst = resp_buf->get_int();
        ulong term = resp_buf->get_ulong();
        ulong nxt_idx = resp_buf->get_ulong();
        byte accepted_val = resp_buf->get_byte();

        rsp = cs_new<resp_msg>(term, (msg_type) msg_type_val, src, dst, nxt_idx, accepted_val == 1);
    } else {
        except = cs_new<rpc_exception>(exception, req);
    }

    when_done(rsp, except);
}
