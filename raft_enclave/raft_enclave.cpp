#include "raft_enclave_t.h"

#include "common.hxx"
#include <tlibc/mbusafecrt.h>
#include <sgx_utils.h>
#include <sgx_trts.h>
#include <sgx_tcrypto.h>
#include <sgx_quote.h>

#include "raft/include/cornerstone.hxx"
#include "port/logger_port.hxx"
#include "port/service_port.hxx"
#include "port/rpc_listener_port.hxx"
#include "app_impl/in_memory_state_mgr.hxx"
#include "app_impl/echo_state_machine.hxx"
#include "system/key_store.hxx"

using std::pair;
using std::make_pair;

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

using raft_app_context = pair<ptr<raft_server>, ptr<rpc_listener>>;
using json11::Json;

static ptr<raft_server> g_server = nullptr;
static ptr<rpc_listener> g_listener = nullptr;
ptr<logger> p_logger;
ptr<er_key_store> key_store;

sgx_status_t ecall_get_report(sgx_target_info_t *target_info, sgx_report_t *report) {
    if (key_store == nullptr) {
        sgx_status_t status;

        sgx_ecc_state_handle_t ecc_handle = nullptr;

        status = sgx_ecc256_open_context(&ecc_handle);
        if (status != SGX_SUCCESS) {
            goto cleanup;
        }

        sgx_ec256_private_t ephemeral_private_key;
        sgx_ec256_public_t ephemeral_public_key;
        status = sgx_ecc256_create_key_pair(&ephemeral_private_key, &ephemeral_public_key, ecc_handle);
        if (status != SGX_SUCCESS) {
            goto cleanup;
        }

        key_store = make_shared<er_key_store>(ephemeral_private_key, ephemeral_public_key);

        cleanup:
        sgx_ecc256_close_context(ecc_handle);
        if (status != SGX_SUCCESS) {
            return status;
        }
    }

    sgx_report_data_t report_data;
    memcpy_s(report_data.d, SGX_REPORT_DATA_SIZE, &key_store->public_key(), sizeof(sgx_ec256_public_t));

    return sgx_create_report(target_info, &report_data, report);
}


bool ecall_register_verification_report(const char *http_packet) {
    return key_store->register_report(http_packet);
}


raft_app_context run_raft_instance(int srv_id, const string &endpoint, uint16_t port) {
    p_logger = make_shared<LoggerPort>();

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

    return make_pair(p_server, p_listener);
}

void ecall_raft_instance_run(int srv_id, const char *address, uint16_t port) {
    string endpoint = sstrfmt("tcp://%s:%d").fmt(address, port);
    auto p = run_raft_instance(srv_id, endpoint, port);
    g_server = p.first;
    g_listener = p.second;
}

void ecall_raft_instance_commit_bg() {
    g_server->commit_in_bg();
}

void ecall_raft_instance_stop() {
    g_listener->stop();
}
