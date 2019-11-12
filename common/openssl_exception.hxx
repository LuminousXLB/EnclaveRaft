//
// Created by ncl on 11/11/19.
//

#ifndef ENCLAVERAFT_OPENSSL_EXCEPTION_HXX
#define ENCLAVERAFT_OPENSSL_EXCEPTION_HXX

#include <openssl/err.h>
#include <exception>
#include <string>

class OpenSSLException : public std::exception {
    std::string message;
public:
    explicit OpenSSLException(unsigned long err_code)
            : message(ERR_error_string(err_code, nullptr)) {}

    const char *what() const noexcept override {
        return message.c_str();
    };
};

#if 0
OpenSSLException get_openssl_error() {
    return OpenSSLException(ERR_get_error());
}
#endif

#endif //ENCLAVERAFT_OPENSSL_EXCEPTION_HXX
