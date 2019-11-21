//
// Created by ncl on 20/11/19.
//

#include <sgx.h>
#include <sgx_quote.h>
#include <sgx_uae_service.h>
#include "od_attestation_u.h"

sgx_status_t od_init_quote_ocall(sgx_target_info_t *target_info, sgx_epid_group_id_t *epid_gid) {
    return sgx_init_quote(target_info, epid_gid);
}

sgx_status_t od_calc_quote_size_ocall(uint32_t *quote_size) {
    return sgx_calc_quote_size(nullptr, 0, quote_size);
}

sgx_status_t od_get_quote_ocall(const sgx_report_t *report, sgx_quote_sign_type_t type, const sgx_spid_t *spid,
                                uint32_t quote_size, uint8_t *quote) {
    return sgx_get_quote(report, type, spid,
                         nullptr, nullptr, 0, nullptr,
                         reinterpret_cast<sgx_quote_t *>(quote), quote_size);
}

