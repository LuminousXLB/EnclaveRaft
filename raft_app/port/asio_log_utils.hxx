//
// Created by ncl on 13/11/19.
//

#ifndef ENCLAVERAFT_ASIO_LOG_UTILS_HXX
#define ENCLAVERAFT_ASIO_LOG_UTILS_HXX

#include "common.hxx"
#include <asio.hpp>

string socket_local_address(const asio::ip::tcp::socket &socket);

string socket_remote_address(const asio::ip::tcp::socket &socket);

#endif //ENCLAVERAFT_ASIO_LOG_UTILS_HXX
