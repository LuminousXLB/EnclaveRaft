//
// Created by ncl on 29/10/19.
//

#include "client.hxx"
#include "messages.hxx"
#include "json11.hpp"
#include <cppcodec/base64_default_rfc4648.hpp>
#include <spdlog/fmt/bin_to_hex.h>

using namespace json11;

uint16_t client_request(asio::io_context &io_context, uint16_t leader_id, const std::string &msg) {
    bufptr buf = buffer::alloc(msg.length() + 1);
    buf->put(msg);
    buf->pos(0);
    auto entry = std::make_shared<log_entry>(0, std::move(buf));

    return send_message(io_context, leader_id, msg_type::client_request, entry);
}

uint16_t add_server(asio::io_context &io_context, uint16_t leader_id, uint16_t srv_id) {
    srv_config cfg(srv_id, fmt::format("tcp://127.0.0.1:{}", 9000 + srv_id));

    spdlog::debug("ADD_SERVER Id={} Endpoint={}", cfg.get_id(), cfg.get_endpoint());

    bufptr buf = cfg.serialize();
    auto entry = std::make_shared<log_entry>(0, std::move(buf), log_val_type::cluster_server);

//    bufptr ser = cfg.serialize();
//    std::shared_ptr<srv_config> p = srv_config::deserialize(*ser);
//    spdlog::debug("ADD_SERVER Id={} Endpoint={}", p->get_id(), p->get_endpoint());

    return send_message(io_context, leader_id, msg_type::add_server_request, entry);
}


string add_server(uint16_t server_id) {
    Json body = Json::object{
            {"server_id", server_id},
            {"endpoint",  fmt::format("tcp://127.0.0.1:{}", 9000 + server_id)}
    };

    return body.dump();
}

string remove_server(uint16_t server_id) {
    Json body = Json::object{
            {"server_id", server_id}
    };

    return body.dump();
}

string append_entries(const vector<bytes> &logs) {
    vector<string> body;

    for (const auto &log: logs) {
        body.emplace_back(base64::encode(log));
    }

    return Json(body).dump();
}

ptr<bytes> serialize(er_message_type type, const string &payload) {
    uint16_t type_num = type;
    auto *ptr = reinterpret_cast<uint8_t *>(&type_num);

    auto buffer = make_shared<bytes>();
    buffer->insert(buffer->end(), ptr, ptr + sizeof(uint16_t));
    buffer->insert(buffer->end(), payload.begin(), payload.end());

    return buffer;
}

bool deserialize(const bytes &buffer) {
    const uint16_t &type_num = *(const uint16_t *) buffer.data();
    const char *payload = reinterpret_cast<const char *>(buffer.data() + sizeof(uint16_t));

    string j(payload, payload + buffer.size() - sizeof(uint16_t));

    spdlog::trace("Deserialize: {}", j);

    string json_err;
    Json body = Json::parse(j, json_err);

    if (json_err.empty()) {
        return body["success"].int_value();
    } else {
        throw std::runtime_error(json_err);
    }
}

ptr<bytes> send_and_recv(asio::io_context &io_context, uint16_t port, const bytes &request) {
    /* Resolve */
    tcp::socket s(io_context);
    tcp::resolver resolver(io_context);
    asio::connect(s, resolver.resolve("127.0.0.1", std::to_string(port)));

    auto local = s.local_endpoint();
    auto remote = s.remote_endpoint();
    spdlog::info("Socket Local {}:{} -> Remote {}:{}",
                 local.address().to_string(), local.port(), remote.address().to_string(), remote.port());

    /* Sending message */
    asio::error_code err;
    uint32_t size = request.size();

    asio::streambuf req_sbuf;
    std::ostream out(&req_sbuf);
    out.write(reinterpret_cast<const char *>(&size), sizeof(uint32_t));
    out.write(reinterpret_cast<const char *>(request.data()), request.size());

    spdlog::info("Write: {}", spdlog::to_hex(request));

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

    spdlog::info("Read: {}", spdlog::to_hex((char *) resp_sbuf.data().data(),
                                            (char *) resp_sbuf.data().data() + resp_sbuf.size()));

    auto ret = make_shared<bytes>(size, 0);
    in.read(reinterpret_cast<char *> (&(*ret)[0]), size);

    spdlog::info("Read: {}", spdlog::to_hex(*ret));

    return ret;
}


int main() {
    spdlog::set_level(spdlog::level::debug);

    asio::io_context io_context;

    uint16_t leader_id = 1;

    spdlog::info("\tadding server 2");
    auto request = serialize(client_add_srv_req, add_server(2));
    auto response = send_and_recv(io_context, 9000 + leader_id, *request);
    if (deserialize(*response)) {
        spdlog::info("\tSUCCESS");
    } else {
        spdlog::info("\tFAILED");
    }

//    spdlog::info("\tadding server 2");
//    leader_id = add_server(io_context, leader_id, 2);
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(400));
//
//    spdlog::info("\tadding server 3");
//    leader_id = add_server(io_context, leader_id, 3);
//    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(400));
//
//    spdlog::info("\tadding server 4");
//    leader_id = add_server(io_context, leader_id, 4);
//    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(400));
//
//    spdlog::info("\tadding server 5");
//    leader_id = add_server(io_context, leader_id, 5);
//    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(400));
//
//    spdlog::info("\tcommitting message");
//    leader_id = client_request(io_context, leader_id, "Hello");
//    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(400));
}
