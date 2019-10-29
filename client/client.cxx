//
// Created by ncl on 29/10/19.
//

#include "client.hxx"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include "asio.hpp"
#include "spdlog/spdlog.h"
#include <iostream>
#include <../raft_enclave/raft/include/utils/buffer.hxx>
#include <../raft_enclave/raft/include/rpc/req_msg.hxx>
#include <../raft_enclave/raft/include/rpc/resp_msg.hxx>
#include <../raft_enclave/raft/include/contribution/log_entry.hxx>

using std::cout;
using std::endl;
using cornerstone::buffer;
using cornerstone::bufptr;
using cornerstone::req_msg;
using cornerstone::resp_msg;
using cornerstone::msg_type;
using cornerstone::log_entry;
using asio::ip::tcp;

using byte = uint8_t;
using bytes = std::vector<byte>;

// request header, ulong term (8), msg_type type (1), int32 src (4), int32 dst (4), ulong last_log_term (8), ulong last_log_idx (8), ulong commit_idx (8) + one int32 (4) for log data size
#define RPC_REQ_HEADER_SIZE 3 * 4 + 8 * 4 + 1

// response header ulong term (8), msg_type type (1), int32 src (4), int32 dst (4), ulong next_idx (8), bool accepted (1)
#define RPC_RESP_HEADER_SIZE 4 * 2 + 8 * 2 + 2

enum {
    max_length = 1024
};

bufptr serialize_req(std::shared_ptr<req_msg> &req);

std::shared_ptr<resp_msg> deserialize_resp(const bufptr &resp_buf);

bufptr send(asio::io_context &io_ctx, uint16_t port, const bufptr &req_buf) {
    auto resp_buf = buffer::alloc(RPC_RESP_HEADER_SIZE);
    spdlog::info("Sending message to {}: {}", port, req_buf->size());

    tcp::socket s(io_ctx);
    tcp::resolver resolver(io_ctx);
    asio::connect(s, resolver.resolve("127.0.0.1", std::to_string(port)));
    asio::write(s, asio::buffer(req_buf->data(), req_buf->size()));
    size_t reply_length = asio::read(s, asio::buffer(resp_buf->data(), resp_buf->size()));

    spdlog::info("Fetched reply from {}: {}", port, reply_length);

    return resp_buf;
}

int main() {
    uint16_t port = 9001;
    asio::io_context io_context;

    std::shared_ptr<req_msg> req = std::make_shared<req_msg>(0,
                                                             msg_type::client_request,
                                                             0,
                                                             1,
                                                             0,
                                                             0,
                                                             0);

    bufptr buf = buffer::alloc(100);
    buf->put("hello");
    buf->pos(0);
    req->log_entries().push_back(std::make_shared<log_entry>(0, std::move(buf)));

    auto resp = deserialize_resp(send(io_context, port, serialize_req(req)));

    assert(resp->get_accepted() || resp->get_dst() > 0);

    if (!resp->get_accepted()) {
        port = 9000 + resp->get_dst();

        spdlog::warn("Request rejected, forward to {}", port);

        req = std::make_shared<req_msg>(0,
                                        msg_type::client_request,
                                        0,
                                        resp->get_dst(),
                                        0,
                                        0,
                                        0);
        buf = buffer::alloc(100);
        buf->put("hello");
        buf->pos(0);
        req->log_entries().push_back(std::make_shared<log_entry>(0, std::move(buf)));

        resp = deserialize_resp(send(io_context, port, serialize_req(req)));
    }

    spdlog::info("Request accepted = {}", resp->get_accepted());
}


bufptr serialize_req(std::shared_ptr<req_msg> &req) {
    // serialize req, send and read response
    std::vector<bufptr> log_entry_bufs;
    int32 log_data_size(0);
    for (auto &it : req->log_entries()) {
        bufptr entry_buf(buffer::alloc(8 + 1 + 4 + it->get_buf().size()));
        entry_buf->put(it->get_term());
        entry_buf->put((byte) (it->get_val_type()));
        entry_buf->put((int32) it->get_buf().size());
        it->get_buf().pos(0);
        entry_buf->put(it->get_buf());
        entry_buf->pos(0);
        log_data_size += (int32) entry_buf->size();
        log_entry_bufs.emplace_back(std::move(entry_buf));
    }

    bufptr req_buf(buffer::alloc(RPC_REQ_HEADER_SIZE + log_data_size));
    req_buf->put((byte) req->get_type());
    req_buf->put(req->get_src());
    req_buf->put(req->get_dst());
    req_buf->put(req->get_term());
    req_buf->put(req->get_last_log_term());
    req_buf->put(req->get_last_log_idx());
    req_buf->put(req->get_commit_idx());
    req_buf->put(log_data_size);
    for (auto &item : log_entry_bufs) {
        req_buf->put(*item);
    }

    req_buf->pos(0);

    return req_buf;
}

std::shared_ptr<resp_msg> deserialize_resp(const bufptr &resp_buf) {
    byte msg_type_val = resp_buf->get_byte();
    int32 src = resp_buf->get_int();
    int32 dst = resp_buf->get_int();
    ulong term = resp_buf->get_ulong();
    ulong nxt_idx = resp_buf->get_ulong();
    byte accepted_val = resp_buf->get_byte();

    return std::make_shared<resp_msg>(term, (msg_type) msg_type_val, src, dst, nxt_idx, accepted_val == 1);
}