#include "raft_enclave_t.h"

#include "raft/include/cornerstone.hxx"
#include "port/logger_port.hxx"
#include "port/service_port.hxx"
#include "port/rpc_listener_port.hxx"
#include "app_impl/in_memory_state_mgr.hxx"
#include "app_impl/echo_state_machine.hxx"

using std::make_shared;
using std::shared_ptr;
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


pair<shared_ptr<raft_server>, shared_ptr<rpc_listener>> run_raft_instance(int srv_id, const string &endpoint);



void ecall_init(int srv_id, const char * endpoint) {

}

pair<shared_ptr<raft_server>, shared_ptr<rpc_listener>> run_raft_instance(int srv_id, const string &endpoint) {
    shared_ptr<logger> p_logger = make_shared<LoggerPort>();

    shared_ptr<rpc_listener> p_listener = make_shared<RpcListenerPort>();
    shared_ptr<state_mgr> p_manager = make_shared<in_memory_state_mgr>(srv_id, endpoint);
    shared_ptr<state_machine> p_machine = make_shared<echo_state_machine>();

    auto *p_params = new raft_params;
    p_params->with_election_timeout_lower(200)
            .with_election_timeout_upper(400)
            .with_hb_interval(100)
            .with_max_append_size(100)
            .with_rpc_failure_backoff(50);

    auto p_service = make_shared<ServicePort>();
    shared_ptr<delayed_task_scheduler> p_scheduler = p_service;
    shared_ptr<rpc_client_factory> p_client_factory = p_service;

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

#if 0
void run_raft_instance_with_asio(int srv_id) {

    context *ctx(new context(smgr, smachine, listener, l, rpc_cli_factory, scheduler, params));
    ptr<raft_server> server(cs_new<raft_server>(ctx));
    listener->listen(server);

    {
        std::unique_lock<std::mutex> ulock(lock1);
        stop_cv1.wait(ulock);
        listener->stop();
    }
}
#endif
