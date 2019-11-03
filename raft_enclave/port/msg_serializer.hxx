//
// Created by ncl on 3/11/19.
//

#ifndef ENCLAVERAFT_MSG_SERIALIZER_HXX
#define ENCLAVERAFT_MSG_SERIALIZER_HXX

#include "../raft/include/cornerstone.hxx"

// request header, ulong term (8), msg_type type (1), int32 src (4), int32 dst (4), ulong last_log_term (8), ulong last_log_idx (8), ulong commit_idx (8) + one int32 (4) for log data size
#define RPC_REQ_HEADER_SIZE 3 * 4 + 8 * 4 + 1

// response header ulong term (8), msg_type type (1), int32 src (4), int32 dst (4), ulong next_idx (8), bool accepted (1)
#define RPC_RESP_HEADER_SIZE 4 * 2 + 8 * 2 + 2

using cornerstone::ptr;
using cornerstone::bufptr;
using cornerstone::req_msg;
using cornerstone::resp_msg;

bufptr serialize_req(const ptr<req_msg> &req);

ptr<req_msg> deserialize_req(bufptr &header, bufptr &log_data);

bufptr serialize_resp(const ptr<resp_msg> &resp);

ptr<resp_msg> deserialize_resp(bufptr &resp_buf);

#endif //ENCLAVERAFT_MSG_SERIALIZER_HXX
