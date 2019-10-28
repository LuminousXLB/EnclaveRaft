//
// Created by ncl on 24/10/19.
//

#include "raft_enclave_u.h"
#include "spdlog/spdlog.h"

void ocall_puts(const char *str) {
    puts(str);
}

void ocall_print_log(unsigned level, const char *log) {
    spdlog::log((spdlog::level::level_enum) level, log);
}
