//
// Created by ncl on 20/11/19.
//

#include "attestation_manager.hxx"

using std::string;
using bytes = std::vector<uint8_t>;
template<class T> using sptr = std::shared_ptr<T>;

#include <json11.hpp>
#include <cppcodec/base64_default_rfc4648.hpp>

using json11::Json;

#include "cert_utils.hxx"

static string url_decode(const char *start, const char *end) {
    string output;
    output.reserve((end - start + 1) * sizeof(char));

    const char *ptr = start;
    while (*ptr && ptr < end) {
        if (*ptr == '%') {
            char buffer[3] = {ptr[1], ptr[2], 0};
            output.push_back((char) strtol(buffer, nullptr, 16));
            ptr += 3;
        } else {
            output.push_back(*ptr);
            ptr++;
        }
    }

    return output;
}

static sptr<bytes> validate_verification_report(const char *http_packet) {
    const char *header_length = "Content-Length: ";
    const char *header_signature = "X-IASReport-Signature: ";
    const char *header_certificates = "X-IASReport-Signing-Certificate: ";
    const char *double_delimiter = "\r\n\r\n";
    const char *delimiter = "\r\n";

    char *ptr_l, *ptr_r;

    /* Parse Content-Length */
    ptr_l = strstr(http_packet, header_length) + strlen(header_length);
    size_t content_length = strtoul(ptr_l, nullptr, 10);

    /* Parse IASReport-Signature */
    ptr_l = strstr(http_packet, header_signature) + strlen(header_signature);
    ptr_r = strstr(ptr_l, delimiter);
    bytes signature = base64::decode(ptr_l, ptr_r - ptr_l);

    /* Parse IASReport-Signing-Certificate */
    ptr_l = strstr(http_packet, header_certificates) + strlen(header_certificates);
    ptr_r = strstr(ptr_l, delimiter);
    string certificates = url_decode(ptr_l, ptr_r);

    /* Parse response body */
    ptr_l = strstr(http_packet, double_delimiter) + strlen(double_delimiter);
    string parse_error;
    Json body = Json::parse(ptr_l, parse_error);

    /* Verify Certificates and Signature */
    bool result;
    try {
        result = verify_certs_and_signature(certificates,
                                            signature,
                                            reinterpret_cast<const uint8_t *>(ptr_l),
                                            content_length);
    } catch (OpenSSLException &ex) {
        return nullptr;
    }
    if (!result || !parse_error.empty()) {
        return nullptr;
    }

#if 0
    {
        "id": "106582804461982983826361650412833082065",
        "timestamp": "2019-11-13T15:37:33.882837",
        "version": 3,
        "isvEnclaveQuoteStatus": "OK",
        "isvEnclaveQuoteBody": "AgAAAFQLAAAIAAcAAAAAAG5efDZ1P3juuVBseEtuC4eOQ2ZklBH+BDSnXK+REv4iBgYCBAGAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABwAAAAAAAAAHAAAAAAAAALFP1Bby2HTBUNgjVO0JylCBQo+FCqLdX3fhLIEpuuQMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB+QRJPiJjE8HTUxZyvS+TfHWpfAyoA3gRijfU5KA7dBwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADf6oCPPd/bA48OU6gGmYtwJoacI+M3HEQXM91TZHbZQ8TRkZOtEH1JDlhfKZ1RAFpZVLN1So+BJmMX4xwk/AmH"
    }
#endif

    if (body["version"].int_value() != 3) {
        return nullptr;
    }

    if (body["isvEnclaveQuoteStatus"].string_value() != string("OK")) {
        return nullptr;
    }

    return std::make_shared<bytes>(base64::decode(body["isvEnclaveQuoteBody"].string_value()));
}

odAttestationManager::odAttestationManager() {
    sgx_ecc_state_handle_t ecc_handle = nullptr;

    sgx_status_ = sgx_ecc256_open_context(&ecc_handle);
    if (sgx_status_ != SGX_SUCCESS) {
        goto cleanup;
    }

    sgx_status_ = sgx_ecc256_create_key_pair(&self_private_key_, &self_public_key_, ecc_handle);
    if (sgx_status_ != SGX_SUCCESS) {
        goto cleanup;
    }

    cleanup:
    sgx_ecc256_close_context(ecc_handle);
}

int odAttestationManager::register_verification_report(const char *vreport) {
    /* verify the verification report */
    auto isvEnclaveQuoteBody = validate_verification_report(vreport);
    if (isvEnclaveQuoteBody == nullptr) {
        return __LINE__;
    }

    /* extract and check the public key */
    const auto &quote = *reinterpret_cast<const sgx_quote_t *>(isvEnclaveQuoteBody->data());
    const auto &peer_public_key = *reinterpret_cast<const sgx_ec256_public_t *>(&quote.report_body.report_data);

    if (memcmp(&self_public_key_, &peer_public_key, sizeof(sgx_ec256_public_t)) == 0) {
        self_vreport_ = std::string(vreport);
        return 1;
    } else {
        return __LINE__;
    }
}

int odAttestationManager::verify_peer_report(int32_t peer_id, const char *vreport,
                                             const sptr<sgx_ec256_dh_shared_t> &shared_secret) {
    int ret = 0;

    /* verify the verification report */
    auto isvEnclaveQuoteBody = validate_verification_report(vreport);
    if (isvEnclaveQuoteBody == nullptr) {
        return __LINE__;
    }

    /* extract the public key */
    const auto &quote = *reinterpret_cast<const sgx_quote_t *>(isvEnclaveQuoteBody->data());
    const auto &peer_public_key = *reinterpret_cast<const sgx_ec256_public_t *>(&quote.report_body.report_data);
    {
        std::lock_guard<std::mutex> lock(key_store_mutex_);
        key_store_.insert({peer_id, std::make_shared<sgx_ec256_public_t>(peer_public_key)});
    }

    /* compute the dh shared secret */
    sgx_ecc_state_handle_t ecc_handle = nullptr;
    sgx_status_ = sgx_ecc256_open_context(&ecc_handle);
    if (sgx_status_ != SGX_SUCCESS) {
        ret = __LINE__;
        goto cleanup;
    }

    sgx_status_ = sgx_ecc256_compute_shared_dhkey(&self_private_key_,
                                                  const_cast<sgx_ec256_public_t *>( &peer_public_key),
                                                  shared_secret.get(),
                                                  ecc_handle);
    if (sgx_status_ != SGX_SUCCESS) {
        ret = __LINE__;
        goto cleanup;
    }

    cleanup:
    sgx_ecc256_close_context(ecc_handle);

    if (sgx_status_ != SGX_SUCCESS) {
        return ret;
    } else {
        return 1;
    }
}

sptr<sgx_ec256_signature_t> odAttestationManager::sign(const uint8_t *data, uint32_t data_size) const {
    auto signature = std::make_shared<sgx_ec256_signature_t>();

    sgx_ecc_state_handle_t ecc_handle = nullptr;
    sgx_status_ = sgx_ecc256_open_context(&ecc_handle);
    if (sgx_status_ != SGX_SUCCESS) {
        goto cleanup;
    }

    sgx_status_ = sgx_ecdsa_sign(data,
                                 data_size,
                                 const_cast<sgx_ec256_private_t *>(&self_private_key_),
                                 signature.get(),
                                 ecc_handle);
    if (sgx_status_ != SGX_SUCCESS) {
        goto cleanup;
    }

    cleanup:
    sgx_ecc256_close_context(ecc_handle);

    if (sgx_status_ != SGX_SUCCESS) {
        return nullptr;
    } else {
        return signature;
    }
}

int odAttestationManager::verify_signature(uint32_t peer_id, const uint8_t *data, uint32_t data_size,
                                           const sgx_ec256_signature_t &signature) const {
    sptr<sgx_ec256_public_t> peer_public_key = nullptr;
    {
        std::lock_guard<std::mutex> lock(key_store_mutex_);
        auto it = key_store_.find(peer_id);
        if (it != key_store_.end()) {
            peer_public_key = it->second;
        } else {
            return __LINE__;
        }
    }

    uint8_t result = -1;

    sgx_ecc_state_handle_t ecc_handle = nullptr;
    sgx_status_ = sgx_ecc256_open_context(&ecc_handle);
    if (sgx_status_ != SGX_SUCCESS) {
        goto cleanup;
    }

    sgx_status_ = sgx_ecdsa_verify(data, data_size, const_cast<sgx_ec256_public_t *>(peer_public_key.get()),
                                   const_cast<sgx_ec256_signature_t *>(&signature),
                                   &result, ecc_handle);
    if (sgx_status_ != SGX_SUCCESS) {
        goto cleanup;
    }

    cleanup:
    sgx_ecc256_close_context(ecc_handle);

    if (sgx_status_ != SGX_SUCCESS) {
        return -1;
    } else if (result == SGX_EC_VALID) {
        return 1;
    } else {
        return 0;
    }
}
