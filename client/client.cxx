//
// Created by ncl on 29/10/19.
//

#include "client.hxx"


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


int main() {
    spdlog::set_level(spdlog::level::debug);

    asio::io_context io_context;

    uint16_t leader_id = 1;

    spdlog::info("\tadding server 2");
    leader_id = add_server(io_context, leader_id, 2);
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(1000));

    spdlog::info("\tadding server 3");
    leader_id = add_server(io_context, leader_id, 3);
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(1000));

    spdlog::info("\tadding server 4");
    leader_id = add_server(io_context, leader_id, 4);
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(1000));

    spdlog::info("\tadding server 5");
    leader_id = add_server(io_context, leader_id, 5);
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(1000));

    spdlog::info("\tcommitting message");
    leader_id = client_request(io_context, leader_id, "Hello");
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(1000));
}
