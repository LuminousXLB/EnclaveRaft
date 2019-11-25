#include "raft_enclave_t.h"

#include "common.hxx"
#include <cppcodec/base64_default_rfc4648.hpp>
#include <tlibc/mbusafecrt.h>
#include <sgx_utils.h>
#include <sgx_trts.h>
#include <sgx_tcrypto.h>
#include <sgx_quote.h>

#include "raft_enclave.hxx"
#include "port/logger_port.hxx"
#include "port/service_port.hxx"
#include "port/rpc_listener_port.hxx"
#include "app_impl/in_memory_state_mgr.hxx"
#include "app_impl/echo_state_machine.hxx"

using cornerstone::logger;
using cornerstone::rpc_listener;
using cornerstone::state_mgr;
using cornerstone::state_machine;
using cornerstone::raft_params;
using cornerstone::delayed_task_scheduler;
using cornerstone::rpc_client_factory;
using cornerstone::context;
using cornerstone::raft_server;
using cornerstone::sstrfmt;

using json11::Json;

app_context_t g;

void run_raft_instance(app_context_t &a_ctx, int srv_id, const char *address, uint16_t port) {
    string endpoint = sstrfmt("tcp://%s:%d").fmt(address, port);

    ptr<logger> p_logger = make_shared<LoggerPort>();
    ptr<rpc_listener> p_listener = make_shared<RpcListenerPort>(port);
    ptr<state_mgr> p_manager = make_shared<in_memory_state_mgr>(srv_id, endpoint);
    ptr<state_machine> p_machine = make_shared<echo_state_machine>();

    auto *p_params = new raft_params;
    (*p_params).with_election_timeout_lower(200);
    (*p_params).with_election_timeout_upper(400);
    (*p_params).with_hb_interval(100);
    (*p_params).with_max_append_size(100);
    (*p_params).with_rpc_failure_backoff(50);

    auto p_service = make_shared<ServicePort>();
    ptr<delayed_task_scheduler> p_scheduler = p_service;
    ptr<rpc_client_factory> p_client_factory = p_service;

    auto *p_context = new context(
            p_manager,
            p_machine,
            p_listener,
            p_logger,
            p_client_factory,
            p_scheduler,
            p_params
    );

    shared_ptr<raft_server> p_server = make_shared<raft_server>(p_context);

    p_listener->listen(p_server);

    a_ctx.log = p_logger;
    a_ctx.listener = p_listener;
    a_ctx.server = p_server;
}

void ecall_init_application(int srv_id, uint16_t port, const sgx_spid_t *spid, sgx_quote_sign_type_t type) {
    int32_t context_id;
    /* get_quote  */
    uint32_t quote_size;
    std::shared_ptr<sgx_quote_t> quote = nullptr;
    od_init_attestation_context(*spid, SGX_UNLINKABLE_SIGNATURE, context_id, quote_size, quote);

    /* request report */
    uint32_t size;
    std::string isvEnclaveQuote_b64 = base64::encode((uint8_t *) quote.get(), quote_size);
    ocall_ias_request(&size, context_id, isvEnclaveQuote_b64.c_str());

    /* retrieve report */
    std::string response(size, '\0');
    ocall_get_response(context_id, &response[0], response.size());

    /* verify report */
    g.keystore = od_get_attestation_manager(context_id);
    if (g.keystore->register_verification_report(response.c_str()) != 1) {
        throw std::runtime_error("Failed to verify verification report");
    }

    run_raft_instance(g, srv_id, "127.0.0.1", port);
}


void ecall_raft_instance_commit_bg() {
    g.server->commit_in_bg();
}

void ecall_raft_instance_stop() {
    g.listener->stop();
}
