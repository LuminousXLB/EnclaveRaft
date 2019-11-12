//
// Created by ncl on 13/11/19.
//

#include "asio_log_utils.hxx"

#include <spdlog/fmt/fmt.h>

string socket_local_address(const asio::ip::tcp::socket &socket) {
    asio::error_code error;
    auto endpoint = socket.local_endpoint(error);

    if (!error) {
        return fmt::format("{}:{}", endpoint.address().to_string(), endpoint.port());
    } else {
        return error.message();
    }
}

string socket_remote_address(const asio::ip::tcp::socket &socket) {
    asio::error_code error;
    auto endpoint = socket.remote_endpoint(error);

    if (!error) {
        return fmt::format("{}:{}", endpoint.address().to_string(), endpoint.port());
    } else {
        return error.message();
    }
}