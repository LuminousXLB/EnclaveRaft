//
// Created by ncl on 14/11/19.
//

#include "key_store.hxx"
#include <sgx_quote.h>
#include "cert_utils.hxx"
#include "url_decode.hxx"

bool er_key_store::register_report(const char *http_packet) {
    if(!self_report_.empty()) {
        throw std::runtime_error("Key Store has been registered");
    }

    auto quote_bytes = validate_verification_report(http_packet);
    if (quote_bytes == nullptr) {
        return false;
    }

    const sgx_quote_t &quote = *reinterpret_cast<const sgx_quote_t *>(quote_bytes->data());
    const sgx_report_body_t &report_body = quote.report_body;
    const sgx_report_data_t &report_data = report_body.report_data;

    if (memcmp(report_data.d, &self_public_, sizeof(sgx_ec256_public_t)) == 0) {
        self_report_ = http_packet;
        return true;
    } else {
        return false;
    }
}

ptr<bytes> er_key_store::validate_verification_report(const char *http_packet) {
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

    return make_shared<bytes>(base64::decode(body["isvEnclaveQuoteBody"].string_value()));
}
