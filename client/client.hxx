//
// Created by ncl on 29/10/19.
//

#ifndef ENCLAVERAFT_CLIENT_HXX
#define ENCLAVERAFT_CLIENT_HXX

//#include "utils.hxx"

#include "asio.hpp"
#include "spdlog/spdlog.h"
#include <spdlog/fmt/bin_to_hex.h>
#include <iostream>
#include <../raft_enclave/raft/include/utils/buffer.hxx>
#include <../raft_enclave/raft/include/rpc/req_msg.hxx>
#include <../raft_enclave/raft/include/rpc/resp_msg.hxx>
#include <../raft_enclave/raft/include/srv_config.hxx>
#include <cppcodec/base64_default_rfc4648.hpp>
#include <cppcodec/hex_default_lower.hpp>

using std::cout;
using std::endl;
using cornerstone::buffer;
using cornerstone::bufptr;
using cornerstone::req_msg;
using cornerstone::resp_msg;
using cornerstone::msg_type;
using cornerstone::log_entry;
using cornerstone::log_val_type;
using cornerstone::srv_config;
using asio::ip::tcp;

using byte = uint8_t;
using bytes = std::vector<byte>;


#endif //ENCLAVERAFT_CLIENT_HXX
