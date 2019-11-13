//
// Created by ncl on 28/10/19.
//

#ifndef ENCLAVERAFT_RPC_LISTENER_PORT_HXX
#define ENCLAVERAFT_RPC_LISTENER_PORT_HXX

#include "raft_enclave_t.h"
#include "../raft/include/cornerstone.hxx"
#include "msg_serializer.hxx"

using cornerstone::ptr;
using cornerstone::msg_handler;
using cornerstone::rpc_listener;

extern ptr<msg_handler> raft_rpc_request_handler;


class RpcListenerPort : public rpc_listener {
public:
    explicit RpcListenerPort(uint16_t port) : port_(port) {}

    void listen(ptr<msg_handler> &handler) override {
        raft_rpc_request_handler = handler;
//        ocall_rpc_listener_create(port_);
    }

    void stop() override {
//        ocall_rpc_listener_stop();
    }

private:
    uint16_t port_;
};

#endif //ENCLAVERAFT_RPC_LISTENER_PORT_HXX
