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

#if 0
    void sent(ptr<req_msg> &req, bufptr &buf, rpc_handler &when_done, std::error_code err, size_t bytes_transferred) {
        ptr<asio_rpc_client> self(this->shared_from_this());
        if (!err) {
            // read a response
            bufptr resp_buf(buffer::alloc(RPC_RESP_HEADER_SIZE));
            auto buffer = asio::buffer(resp_buf->data(), resp_buf->size());
            asio::async_read(socket_, buffer,
                             [self, req_msg = req, when_done, rbuf = std::move(resp_buf)](std::error_code err,
                                                                                          size_t bytes_transferred) mutable -> void {
                                 self->response_read(req_msg, when_done, rbuf, err, bytes_transferred);
                             });
        } else {
//            ptr<resp_msg> rsp;
//            ptr<rpc_exception> except(
                    cs_new<rpc_exception>(sstrfmt("failed to send request to remote socket %d").fmt(err.value()),
                                          req));
            socket_.close();
//            when_done(rsp, except);
        }
    }

    void response_read(ptr<req_msg> &req, rpc_handler &when_done, bufptr &resp_buf, std::error_code err,
                       size_t bytes_transferred) {
        if (!err) {
//            byte msg_type_val = resp_buf->get_byte();
//            int32 src = resp_buf->get_int();
//            int32 dst = resp_buf->get_int();
//            ulong term = resp_buf->get_ulong();
//            ulong nxt_idx = resp_buf->get_ulong();
//            byte accepted_val = resp_buf->get_byte();
//
//            ptr<resp_msg> rsp(
//                    cs_new<resp_msg>(term, (msg_type) msg_type_val, src, dst, nxt_idx, accepted_val == 1));
//            ptr<rpc_exception> except;
//            when_done(rsp, except);
        } else {
//            ptr<resp_msg> rsp;
//            ptr<rpc_exception> except(
                    cs_new<rpc_exception>(sstrfmt("failed to read response to remote socket %d").fmt(err.value()),
                                          req));
            socket_.close();
//            when_done(rsp, except);
        }
    }

#endif

}
