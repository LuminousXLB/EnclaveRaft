//
// Created by ncl on 28/10/19.
//

#ifndef ENCLAVERAFT_RPC_CLIENT_PORT_HXX
#define ENCLAVERAFT_RPC_CLIENT_PORT_HXX

#include <utility>

#include "../raft/include/cornerstone.hxx"

using std::string;

using cornerstone::rpc_client;
using cornerstone::ptr;
using cornerstone::req_msg;
using cornerstone::rpc_handler;

class RpcClientPort : public rpc_client {
public:
    explicit RpcClientPort(string endpoint) : endpoint_(std::move(endpoint)) {}

    void send(ptr<req_msg> &req, rpc_handler &when_done) override {
//        TODO: IMPL THIS
    }

private:
    string endpoint_;
};


#endif //ENCLAVERAFT_RPC_CLIENT_PORT_HXX
