//
// Created by ncl on 24/10/19.
//

#include "log_level.hxx"
#include "spdlog/spdlog.h"

void ocall_print_log(uint level, const char *log) {
    spdlog::log((spdlog::level::level_enum) level, log);
}
