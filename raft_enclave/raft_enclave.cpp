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

sgx_status_t sgx_get_trusted_time_hook(sgx_time_t *current_time, sgx_time_source_nonce_t *time_source_nounce) {
    // sgx_create_pse_session();
    sgx_status_t status = get_time(current_time);
    // sgx_close_pse_session();
    return status;
}

uint64_t get_secret() {
    sgx_time_t timestamp;
    sgx_time_source_nonce_t nonce;

    sgx_get_trusted_time_hook(&timestamp, &nonce);

    return timestamp;
}
