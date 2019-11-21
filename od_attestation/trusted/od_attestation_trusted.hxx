//
// Created by ncl on 20/11/19.
//

#ifndef RA_GET_REPORT_OD_ATTESTATION_TRUSTED_HXX
#define RA_GET_REPORT_OD_ATTESTATION_TRUSTED_HXX

#include <sgx_quote.h>
#include <memory>
#include "attestation_manager.hxx"

sgx_status_t od_init_attestation_context(const sgx_spid_t &spid,
                                         sgx_quote_sign_type_t type,
                                         int32_t &context_id,
                                         uint32_t &quote_size,
                                         std::shared_ptr<sgx_quote_t> &quote);

std::shared_ptr<odAttestationManager> od_get_attestation_manager(uint32_t context_id);

#endif //RA_GET_REPORT_OD_ATTESTATION_TRUSTED_HXX
