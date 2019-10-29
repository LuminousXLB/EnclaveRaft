#include <cstdio>
#include "raft_enclave_u.h"
#include "sgx_utils/sgx_utils.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

using std::shared_ptr;
using std::make_shared;
using std::vector;
using std::thread;

/* Global Enclave ID */
sgx_enclave_id_t global_enclave_id;
shared_ptr<asio::io_context> global_io_context;
shared_ptr<spdlog::logger> global_logger;


int main(int argc, char const *argv[]) {
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


    global_io_context = make_shared<asio::io_context>();
    global_logger = spdlog::stdout_color_mt("raft");
    global_logger->set_level(spdlog::level::trace);

    /* Enclave Initialization */
    if (initialize_enclave(&global_enclave_id, "raft_enclave.token", "Enclave_raft.signed.so") < 0) {
        printf("Fail to initialize enclave.\n");
        return 1;
    }

    sgx_status_t status;

    status = ecall_raft_instance_run(global_enclave_id, srv_id, "127.0.0.1", 9000 + srv_id);
    if (status != SGX_SUCCESS) {
        print_error_message(status);
        exit(EXIT_FAILURE);
    }

    unsigned int cpu_cnt = std::thread::hardware_concurrency();
    if (cpu_cnt == 0) {
        cpu_cnt = 1;
    }

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

    return 0;
}
