//
// Created by ncl on 24/10/19.
//

#ifndef ENCLAVERAFT_LOGGER_PORT_HXX
#define ENCLAVERAFT_LOGGER_PORT_HXX

#include "../raft/include/cornerstone.hxx"
#include "log_level.hxx"
#include <string>

using std::string;

void ocall_print_log(uint level, const char *log);


class LoggerPort : public cornerstone::logger {
    static void log(uint level, const string &message) {
        ocall_print_log(level, message.c_str());
    }

public:
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
};

#endif //ENCLAVERAFT_LOGGER_PORT_HXX
