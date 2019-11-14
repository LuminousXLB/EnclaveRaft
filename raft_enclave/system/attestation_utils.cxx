#include "common.hxx"
#include <tlibc/mbusafecrt.h>
#include <sgx_utils.h>
#include <sgx_trts.h>
#include <sgx_tcrypto.h>
#include <sgx_quote.h>
#include "raft_enclave_t.h"
#include "key_store.hxx"

using json11::Json;

ptr<er_key_store> key_store;


sgx_status_t ecall_get_report(sgx_target_info_t *target_info, sgx_report_t *report) {
    if (key_store == nullptr) {
        sgx_status_t status;

        sgx_ecc_state_handle_t ecc_handle = nullptr;

        status = sgx_ecc256_open_context(&ecc_handle);
        if (status != SGX_SUCCESS) {
            goto cleanup;
        }

        sgx_ec256_private_t ephemeral_private_key;
        sgx_ec256_public_t ephemeral_public_key;
        status = sgx_ecc256_create_key_pair(&ephemeral_private_key, &ephemeral_public_key, ecc_handle);
        if (status != SGX_SUCCESS) {
            goto cleanup;
        }

        key_store = make_shared<er_key_store>(ephemeral_private_key, ephemeral_public_key);

        cleanup:
        sgx_ecc256_close_context(ecc_handle);
        if(status != SGX_SUCCESS) {
            return status;
        }
    }

    sgx_report_data_t report_data;
    memcpy_s(report_data.d, SGX_REPORT_DATA_SIZE, &key_store->public_key(), sizeof(sgx_ec256_public_t));

    return sgx_create_report(target_info, &report_data, report);
}


bool ecall_register_verification_report(const char *http_packet) {
    return key_store->register_report(http_packet);
}
