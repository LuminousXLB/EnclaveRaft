//
// Created by ncl on 14/11/19.
//

#ifndef ENCLAVERAFT_CERT_UTILS_HXX
#define ENCLAVERAFT_CERT_UTILS_HXX

#include <string>
#include <exception>
#include <openssl/err.h>

class OpenSSLException : public std::exception {
public:
    explicit OpenSSLException(unsigned long err_code) : message(ERR_error_string(err_code, nullptr)) {}

    const char *what() const noexcept override { return message.c_str(); };

private:
    std::string message;

};


bool verify_certs_and_signature(const std::string &certs,
                                const std::vector<uint8_t> &sig,
                                const uint8_t *data,
                                size_t size);

#endif //ENCLAVERAFT_CERT_UTILS_HXX
