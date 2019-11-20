//
// Created by ncl on 19/11/19.
//
#include "common.hxx"
#include "client.hxx"

extern std::shared_ptr<spdlog::logger> logger;


//uint16_t client_request(asio::io_context &io_context, uint16_t leader_id, const std::string &msg) {
//    bufptr buf = buffer::alloc(msg.length() + 1);
//    buf->put(msg);
//    buf->pos(0);
//    auto entry = std::make_shared<log_entry>(0, std::move(buf));
//
//    return send_message(io_context, leader_id, msg_type::client_request, entry);
//}
//
//uint16_t add_server(asio::io_context &io_context, uint16_t leader_id, uint16_t srv_id) {
//    srv_config cfg(srv_id, fmt::format("tcp://127.0.0.1:{}", 9000 + srv_id));
//
//    logger->debug("ADD_SERVER Id={} Endpoint={}", cfg.get_id(), cfg.get_endpoint());
//
//    bufptr buf = cfg.serialize();
//    auto entry = std::make_shared<log_entry>(0, std::move(buf), log_val_type::cluster_server);
//
////    bufptr ser = cfg.serialize();
////    std::shared_ptr<srv_config> p = srv_config::deserialize(*ser);
////    logger->debug("ADD_SERVER Id={} Endpoint={}", p->get_id(), p->get_endpoint());
//
//    return send_message(io_context, leader_id, msg_type::add_server_request, entry);
//}



ptr<bytes> send_and_recv(asio::io_context &io_context, uint16_t port, const bytes &request) {
    /* Resolve */
    tcp::socket s(io_context);
    tcp::resolver resolver(io_context);
    asio::connect(s, resolver.resolve("127.0.0.1", std::to_string(port)));

    auto local = s.local_endpoint();
    auto remote = s.remote_endpoint();
    logger->debug("Socket Local {}:{} -> Remote {}:{}",
                  local.address().to_string(), local.port(), remote.address().to_string(), remote.port());

    /* Sending message */
    asio::error_code err;
    uint32_t size = request.size();

    asio::streambuf req_sbuf;
    std::ostream out(&req_sbuf);
    out.write(reinterpret_cast<const char *>(&size), sizeof(uint32_t));
    out.write(reinterpret_cast<const char *>(request.data()), request.size());

    logger->debug("Write: {}", spdlog::to_hex(request));

    asio::write(s, req_sbuf, err);
    if (err) {
        logger->error("Error when writing message: {}", err.message());
    } else {
        logger->debug("Message sent, reading response");
    }

    /* Reading message */
    asio::streambuf resp_sbuf;
    std::istream in(&resp_sbuf);

    asio::read(s, resp_sbuf, asio::transfer_at_least(sizeof(uint32_t)), err);
    if (err && err != asio::error::eof) {
        logger->error("Error when reading header: {}", err.message());
    }
    in.read(reinterpret_cast<char *>(&size), sizeof(uint32_t));

    while (resp_sbuf.size() < size && !err) {
        asio::read(s, resp_sbuf, asio::transfer_at_least(1), err);
    }
    if (err && err != asio::error::eof) {
        logger->error("Error when reading message: {}", err.message());
    }

    logger->debug("Read: {}", spdlog::to_hex((char *) resp_sbuf.data().data(),
                                             (char *) resp_sbuf.data().data() + resp_sbuf.size()));

    auto ret = make_shared<bytes>(size, 0);
    in.read(reinterpret_cast<char *> (&(*ret)[0]), size);

    logger->debug("Read: {}", spdlog::to_hex(*ret));

    return ret;
}
