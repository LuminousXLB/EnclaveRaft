//
// Created by ncl on 24/10/19.
//

#ifndef ENCLAVERAFT_LOGGER_PORT_HXX
#define ENCLAVERAFT_LOGGER_PORT_HXX

#include "../raft/include/cornerstone.hxx"
#include "log_level.hxx"
#include <string>
#include "raft_enclave_t.h"

using std::string;


class LoggerPort : public cornerstone::logger {
    static void log(unsigned level, const string &message) {
#ifdef DEBUG
        ocall_print_log(level, message.c_str());
#else
        if (level >= LOG_LEVEL_INFO) {
            ocall_print_log(level, message.c_str());
        }
#endif
    }

public:

    void trace(const string &log_line) {
        log(LOG_LEVEL_TRACE, log_line);
    }

    void debug(const string &log_line) override {
        log(LOG_LEVEL_DEBUG, log_line);
    }

    void info(const string &log_line) override {
        log(LOG_LEVEL_INFO, log_line);
    }

    void warn(const string &log_line) override {
        log(LOG_LEVEL_WARN, log_line);
    }

    void err(const string &log_line) override {
        log(LOG_LEVEL_ERROR, log_line);
    }

    void critical(const string &log_line) {
        log(LOG_LEVEL_CRITICAL, log_line);
    }
};

#endif //ENCLAVERAFT_LOGGER_PORT_HXX
