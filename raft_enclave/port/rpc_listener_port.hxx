//
// Created by ncl on 28/10/19.
//

#ifndef ENCLAVERAFT_RPC_LISTENER_PORT_HXX
#define ENCLAVERAFT_RPC_LISTENER_PORT_HXX

#include "../raft/include/cornerstone.hxx"

using cornerstone::rpc_listener;

// TODO: impl this outside the enclave
void ocall_rpc_listener_stop();


class RpcListenerPort : public rpc_listener {
public:
    void listen(ptr<cornerstone::msg_handler> &handler) override {
        // TODO: impl this. Should be called when launching
    }

    void stop() override {
        ocall_rpc_listener_stop();
    }
};

#endif //ENCLAVERAFT_RPC_LISTENER_PORT_HXX
