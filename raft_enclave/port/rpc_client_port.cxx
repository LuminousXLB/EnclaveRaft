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
atomic<uint32_t> last_req_uid_;

void ecall_rpc_response(uint32_t req_uid, uint32_t size, const uint8_t *msg, const char *exception) {
    ocall_puts(lstrfmt("\t%s %s %d: resp_size=%u").fmt(__FILE__, __FUNCTION__, __LINE__, size));

    ptr<req_msg> req;
    rpc_handler when_done;
    {
        lock_guard<mutex> lock(rpc_client_callback_pool_lock);
        auto item = rpc_client_callback_pool[req_uid];
        req = item.first;
        when_done = item.second;
        rpc_client_callback_pool.erase(req_uid);
    }

    ptr<resp_msg> rsp;
    ptr<rpc_exception> except;

    if (!exception) {
        bufptr resp_buf(buffer::alloc(RPC_RESP_HEADER_SIZE));
        // FIXME: Encrypt & Decrypt
        memcpy_s(resp_buf->data(), resp_buf->size(), msg, size);

        resp_buf->pos(0);
        byte msg_type_val = resp_buf->get_byte();
        int32 src = resp_buf->get_int();
        int32 dst = resp_buf->get_int();
        ulong term = resp_buf->get_ulong();
        ulong nxt_idx = resp_buf->get_ulong();
        byte accepted_val = resp_buf->get_byte();

        rsp = cs_new<resp_msg>(term, (msg_type) msg_type_val, src, dst, nxt_idx, accepted_val == 1);
    } else {
        ocall_puts(lstrfmt("\t%s %s %d: exception=%s").fmt(__FILE__, __FUNCTION__, __LINE__, exception));
        except = cs_new<rpc_exception>(exception, req);
    }

    when_done(rsp, except);

    ocall_puts(lstrfmt("\t%s %s %d: resp_size=%u").fmt(__FILE__, __FUNCTION__, __LINE__, size));
}
