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

extern ptr<msg_handler> rpc_listener_req_handler;


class RpcListenerPort : public rpc_listener {
public:
    explicit RpcListenerPort(uint16_t port) : port_(port) {}

    void listen(ptr<msg_handler> &handler) override {
        rpc_listener_req_handler = handler;
//        ocall_rpc_listener_create(port_);
    }

    void stop() override {
//        ocall_rpc_listener_stop();
    }

private:
    uint16_t port_;
};

#endif //ENCLAVERAFT_RPC_LISTENER_PORT_HXX
