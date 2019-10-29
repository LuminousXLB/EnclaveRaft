//
// Created by ncl on 29/10/19.
//

#ifndef ENCLAVERAFT_UTILS_HXX
#define ENCLAVERAFT_UTILS_HXX


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


bufptr serialize_req(std::shared_ptr<req_msg> &req);

std::shared_ptr<resp_msg> deserialize_resp(const bufptr &resp_buf);

bufptr send(asio::io_context &io_ctx, uint16_t port, const bufptr &req_buf);

uint16_t send_message(asio::io_context &io_ctx, uint16_t dst, msg_type type, const std::shared_ptr<log_entry>& entry);


#endif //ENCLAVERAFT_UTILS_HXX
