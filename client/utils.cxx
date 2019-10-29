//
// Created by ncl on 29/10/19.
//

#include "utils.hxx"


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

bufptr send(asio::io_context &io_ctx, uint16_t port, const bufptr &req_buf) {
    auto resp_buf = buffer::alloc(RPC_RESP_HEADER_SIZE);
    uint32_t size = req_buf->size();

    spdlog::info("Sending message to {}: {}", port, size);


    tcp::socket s(io_ctx);
    tcp::resolver resolver(io_ctx);
    asio::connect(s, resolver.resolve("127.0.0.1", std::to_string(port)));

    asio::write(s, asio::buffer(&size, sizeof(uint32_t)));
    asio::write(s, asio::buffer(req_buf->data(), req_buf->size()));

    asio::read(s, asio::buffer(&size, sizeof(uint32_t)));
    size_t reply_length = asio::read(s, asio::buffer(resp_buf->data(), resp_buf->size()));

    spdlog::info("Fetched reply from {}: {}", port, reply_length);

    s.close();

    return resp_buf;
}

uint16_t send_message(asio::io_context &io_ctx, uint16_t dst, msg_type type, const std::shared_ptr<log_entry> &entry) {
    std::shared_ptr<req_msg> req = std::make_shared<req_msg>(0, type, 0, dst, 0, 0, 0);
    req->log_entries().push_back(entry);

    auto resp = deserialize_resp(send(io_ctx, 9000 + dst, serialize_req(req)));

    assert(resp->get_accepted() || resp->get_dst() > 0);

    if (!resp->get_accepted()) {
        dst = resp->get_dst();

        spdlog::warn("Request rejected, forward to {}", 9000 + dst);

        req = std::make_shared<req_msg>(0, type, 0, dst, 0, 0, 0);
        req->log_entries().push_back(entry);

        resp = deserialize_resp(send(io_ctx, 9000 + dst, serialize_req(req)));
    }

    spdlog::info("Request accepted = {}", resp->get_accepted());
    return dst;
}
