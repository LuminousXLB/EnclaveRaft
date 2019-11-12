//
// Created by ncl on 29/10/19.
//

#include "utils.hxx"
#include "client_crypto.hxx"

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
    uint32_t size = req_buf->size();

    spdlog::info("Sending message to {}: {}", port, size);

    /* Encrypt request */
    auto req = client_encrypt(req_buf->data(), req_buf->size());
    size = req->size();

    tcp::socket s(io_ctx);
    tcp::resolver resolver(io_ctx);
    asio::connect(s, resolver.resolve("127.0.0.1", std::to_string(port)));

    auto local = s.local_endpoint();
    auto remote = s.remote_endpoint();
    spdlog::info("Socket Local {}:{} -> Remote {}:{}",
                 local.address().to_string(), local.port(), remote.address().to_string(), remote.port());

    /* Sending message */
    asio::streambuf req_sbuf;
    std::ostream out(&req_sbuf);

    out.write(reinterpret_cast<const char *>(&size), sizeof(uint32_t));
    out.write(reinterpret_cast<const char *>(req->data()), req->size());

    asio::error_code err;
    asio::write(s, req_sbuf, err);
    if (err) {
        spdlog::error("Error when writing message: {}", err.message());
    } else {
        spdlog::info("Message sent, reading response");
    }

    /* Reading message */
    asio::streambuf resp_sbuf;
    std::istream in(&resp_sbuf);

    asio::read(s, resp_sbuf, asio::transfer_at_least(sizeof(uint32_t)), err);
    if (err && err != asio::error::eof) {
        spdlog::error("Error when reading header: {}", err.message());
    }
    in.read(reinterpret_cast<char *>(&size), sizeof(uint32_t));

    while (resp_sbuf.size() < size && !err) {
        asio::read(s, resp_sbuf, asio::transfer_at_least(1), err);
    }
    if (err && err != asio::error::eof) {
        spdlog::error("Error when reading message: {}", err.message());
    }

    /* Decrypting message */
    auto resp = client_decrypt(reinterpret_cast<const uint8_t *>(resp_sbuf.data().data()), size);

    auto resp_buf = buffer::alloc(RPC_RESP_HEADER_SIZE);
    memcpy(resp_buf->data(), resp->data(), resp->size());

    spdlog::info("Fetched reply from {}: {}", port, resp->size());

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

    spdlog::info("Current Leader = {}, Request accepted = {}", dst, resp->get_accepted());
    return dst;
}
