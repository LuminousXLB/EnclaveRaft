#include "raft_enclave_t.h"
#include <cstdio>
#include <cstdarg>
#include <sgx_tae_service.h>

static uint64_t SECRET = 0x12345678abcdef;

void printf(const char *fmt, ...) {
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
            va_end(ap);
    ocall_puts(buf);
}

uint64_t get_secret() {
    int64_t current_time;

    sgx_status_t status = get_time_milliseconds(&current_time);

    return current_time;
}

class RPC_Request {
    // properties
    //   meta data
    //   rpc type

    // methods
    //   serialize(to bytes)
    //   deserialize(from bytes)
};

sgx_status_t ecall_raft_rpc_handle_request(char *base64_request) {
    // decode
    // decrypt
    // dispatch
}