//
// Created by ncl on 21/11/19.
//

#ifndef ENCLAVERAFT_RAFT_ENCLAVE_HXX
#define ENCLAVERAFT_RAFT_ENCLAVE_HXX

#include "raft/include/cornerstone.hxx"
#include "od_attestation_trusted.hxx"

struct app_context_t {
    ptr<cornerstone::raft_server> server;
    ptr<cornerstone::rpc_listener> listener;
    ptr<cornerstone::logger> log;
    ptr<odAttestationManager> keystore;
};

#endif //ENCLAVERAFT_RAFT_ENCLAVE_HXX
