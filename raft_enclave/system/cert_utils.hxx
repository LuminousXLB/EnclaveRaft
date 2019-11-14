//
// Created by ncl on 14/11/19.
//

#ifndef ENCLAVERAFT_CERT_UTILS_HXX
#define ENCLAVERAFT_CERT_UTILS_HXX

#include "common.hxx"
#include <exception>
#include "openssl_exception.hxx"

bool verify_certs_and_signature(const string &certs, const bytes &sig, const uint8_t *data, size_t size);

#endif //ENCLAVERAFT_CERT_UTILS_HXX
