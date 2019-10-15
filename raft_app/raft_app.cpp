#include <cstdio>
#include "raft_enclave_u.h"
#include "sgx_utils/sgx_utils.h"
#include <ctime>

/* Global Enclave ID */
sgx_enclave_id_t global_eid;

/* OCall implementations */
void ocall_puts(const char *str) {
    printf("%s\n", str);
}

int64_t get_time_milliseconds() {
    timespec tp{};
    clock_gettime(CLOCK_REALTIME, &tp);

    return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

int main(int argc, char const *argv[]) {
    puts("============================================================");

    /* Enclave Initialization */
    if (initialize_enclave(&global_eid, "raft_enclave.token", "Enclave_raft.signed.so") < 0) {
        printf("Fail to initialize enclave.\n");
        return 1;
    }

    sgx_status_t status;
    uint64_t secret;

    status = get_secret(global_eid, &secret);
    if (status != SGX_SUCCESS) {
        printf("ECall failed.\n");
        return 1;
    }

    printf("SECRET: %lx\n", secret);

    time_t timestamp = time(nullptr);
    printf("TIMESTAMP: %lx\n", timestamp);


    return 0;
}
