//
// Created by ncl on 24/10/19.
//

#include "raft_enclave_u.h"
#include "spdlog/spdlog.h"
#include <memory>

using std::shared_ptr;

extern shared_ptr<spdlog::logger> global_logger;

void ocall_puts(const char *str) {
    puts(str);
}

void ocall_print_log(unsigned level, const char *log) {
    global_logger->log((spdlog::level::level_enum) level, log);
}
