#include <cstdio>
#include "raft_enclave_u.h"
#include "sgx_utils/sgx_utils.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

using std::shared_ptr;
using std::make_shared;

/* Global Enclave ID */
sgx_enclave_id_t global_enclave_id;
shared_ptr<asio::io_context> global_io_context;
shared_ptr<spdlog::logger> global_logger;


int main(int argc, char const *argv[]) {

    global_io_context = make_shared<asio::io_context>();
    global_logger = spdlog::stderr_color_mt("raft");
    global_logger->info("init");

    /* Enclave Initialization */
    if (initialize_enclave(&global_enclave_id, "raft_enclave.token", "Enclave_raft.signed.so") < 0) {
        printf("Fail to initialize enclave.\n");
        return 1;
    }

    sgx_status_t status;

    status = ecall_raft_instance_run(global_enclave_id, 1, "127.0.0.1", 9001);
    if (status != SGX_SUCCESS) {
        print_error_message(status);
        exit(EXIT_FAILURE);
    }

    unsigned int cpu_cnt = std::thread::hardware_concurrency();
    if (cpu_cnt == 0) {
        cpu_cnt = 1;
    }

    for (unsigned int i = 0; i < cpu_cnt; ++i) {
        std::thread t([] {
            global_io_context->run();
        });
        t.detach();
    }

    return 0;
}
