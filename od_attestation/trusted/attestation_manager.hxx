//
// Created by ncl on 20/11/19.
//

#ifndef RA_GET_REPORT_ATTESTATION_MANAGER_HXX
#define RA_GET_REPORT_ATTESTATION_MANAGER_HXX

#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <memory>
#include <mutex>

#include <sgx_tcrypto.h>
#include <sgx_quote.h>

class odAttestationManager {
    template<class T>
    using sptr = std::shared_ptr<T>;

public:
    odAttestationManager();

    int register_verification_report(const char *vreport);

    int verify_peer_report(int32_t peer_id, const char *vreport, const sptr<sgx_ec256_dh_shared_t> &shared_secret);

    const sgx_ec256_public_t &public_key() const { return self_public_key_; }

    const std::string &report() const { return self_vreport_; }

    sptr<sgx_ec256_signature_t> sign(const uint8_t *data, uint32_t data_size) const;

    int verify_signature(uint32_t peer_id, const uint8_t *data, uint32_t data_size,
                         const sgx_ec256_signature_t &signature) const;

private:
    mutable sgx_status_t sgx_status_ = SGX_SUCCESS;
    sgx_ec256_private_t self_private_key_{};
    sgx_ec256_public_t self_public_key_{};
    std::string self_vreport_;
    std::map<uint32_t, sptr<sgx_ec256_public_t>> key_store_;
    mutable std::mutex key_store_mutex_;
};

#endif //RA_GET_REPORT_ATTESTATION_MANAGER_HXX
