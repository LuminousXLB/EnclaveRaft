#include <sgx_utils.h>
#include <tlibc/mbusafecrt.h>
#include <sgx_trts.h>
#include <sgx_tcrypto.h>
#include "raft_enclave_t.h"

sgx_ec256_private_t ephemeral_private_key{{0}};
sgx_ec256_public_t ephemeral_public_key{{0},
                                        {0}};

static sgx_status_t generate_ephemeral_key() {
    sgx_status_t status;

    sgx_ecc_state_handle_t ecc_handle = nullptr;

    status = sgx_ecc256_open_context(&ecc_handle);
    if (status != SGX_SUCCESS) {
        goto cleanup;
    }

    status = sgx_ecc256_create_key_pair(&ephemeral_private_key, &ephemeral_public_key, ecc_handle);
    if (status != SGX_SUCCESS) {
        goto cleanup;
    }

    cleanup:
    sgx_ecc256_close_context(ecc_handle);
    return status;
}

sgx_status_t ecall_get_report(sgx_target_info_t *target_info, sgx_report_t *report) {
    if (memcmp(ephemeral_public_key.gx, ephemeral_public_key.gy, SGX_ECP256_KEY_SIZE) == 0) {
        generate_ephemeral_key();
    }

    sgx_report_data_t report_data;
    memcpy_s(report_data.d, SGX_REPORT_DATA_SIZE, &ephemeral_public_key, sizeof(sgx_ec256_public_t));

    return sgx_create_report(target_info, &report_data, report);
}