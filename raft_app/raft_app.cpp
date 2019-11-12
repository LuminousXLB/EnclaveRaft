#include <cstdio>
#include "raft_enclave_u.h"
#include "sgx_utils/sgx_utils.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>
#include "system/enclave_quote.hxx"
#include "system/intel_ias.hxx"
#include "secret.h"
#include "cppcodec/base64_default_rfc4648.hpp"
#include "port/asio_rpc_listener.hxx"

using std::thread;

/* Global Enclave ID */
sgx_enclave_id_t global_enclave_id;
ptr<asio::io_context> global_io_context;
ptr<spdlog::logger> global_logger;
ptr<asio_rpc_listener> global_rpc_listener;
string attestation_verification_report;

int main(int argc, char const *argv[]) {
    /* parse command line parameter and get srv_id */
    uint8_t srv_id = 1;
    if (argc < 2) {
        // do nothing
    } else if (argc == 2) {
        srv_id = strtoul(argv[1], nullptr, 10);
    } else {
        fprintf(stderr, "Usage: \n");
        fprintf(stderr, "    %s <server_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* initialise global variables */
    global_io_context = make_shared<asio::io_context>();
    global_logger = spdlog::stdout_color_mt("raft");
    global_logger->set_level(spdlog::level::trace);
    global_logger->set_pattern("%^[%H:%M:%S.%f] @ %t [%l]%$ %v");
    global_rpc_listener = make_shared<asio_rpc_listener>(global_io_context, 9000 + srv_id);

    /* initialise enclave */
    if (initialize_enclave(&global_enclave_id, "raft_enclave.token", "Enclave_raft.signed.so") < 0) {
        printf("Fail to initialize enclave.\n");
        return 1;
    }
    sgx_status_t status;

    /* get and verify attestation verification report */
    auto quote = get_enclave_quote();
    IntelIAS ias(intel_api_primary, intel_api_secondary, IntelIAS::DEVELOPMENT);
    attestation_verification_report = ias.report(base64::encode(*quote));

    std::cout << attestation_verification_report << std::endl;

//    exit(-1);

    /* initialise raft service */
    status = ecall_raft_instance_run(global_enclave_id, srv_id, "127.0.0.1", 9000 + srv_id);
    if (status != SGX_SUCCESS) {
        print_error_message(status);
        exit(EXIT_FAILURE);
    }

    /* start listener */
    global_rpc_listener->listen();

    unsigned int cpu_cnt = std::thread::hardware_concurrency();

    thread t(std::bind(ecall_raft_instance_commit_bg, global_enclave_id));
    t.detach();
    cpu_cnt -= 1;

    if (cpu_cnt < 1) {
        cpu_cnt = 1;
    }

//    cpu_cnt = 8;
    vector<thread> thread_pool;
    for (unsigned int i = 0; i < cpu_cnt; ++i) {
        thread_pool.emplace_back([] {
            global_io_context->run();
        });
    }

    global_logger->info("{} threads generated", cpu_cnt);

    for (auto &thread :thread_pool) {
        thread.join();
    }

    global_logger->info("Exiting ...");
    global_rpc_listener->stop();

    return 0;
}
