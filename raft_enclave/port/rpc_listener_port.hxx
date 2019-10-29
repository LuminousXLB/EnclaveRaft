//
// Created by ncl on 28/10/19.
//

#ifndef ENCLAVERAFT_RPC_LISTENER_PORT_HXX
#define ENCLAVERAFT_RPC_LISTENER_PORT_HXX

#include "raft_enclave_t.h"
#include "../raft/include/cornerstone.hxx"

using cornerstone::ptr;
using cornerstone::msg_handler;
using cornerstone::rpc_listener;

extern ptr<msg_handler> rpc_listener_req_handler;


// request header, ulong term (8), msg_type type (1), int32 src (4), int32 dst (4), ulong last_log_term (8), ulong last_log_idx (8), ulong commit_idx (8) + one int32 (4) for log data size
#define RPC_REQ_HEADER_SIZE 3 * 4 + 8 * 4 + 1

// response header ulong term (8), msg_type type (1), int32 src (4), int32 dst (4), ulong next_idx (8), bool accepted (1)
#define RPC_RESP_HEADER_SIZE 4 * 2 + 8 * 2 + 2





class RpcListenerPort : public rpc_listener {
public:
    explicit RpcListenerPort(uint16_t port) : port_(port) {}

    void listen(ptr<msg_handler> &handler) override {
        rpc_listener_req_handler = handler;
        ocall_rpc_listener_create(port_);
    }

    void stop() override {
        ocall_rpc_listener_stop();
    }

private:
    uint16_t port_;
};

#endif //ENCLAVERAFT_RPC_LISTENER_PORT_HXX