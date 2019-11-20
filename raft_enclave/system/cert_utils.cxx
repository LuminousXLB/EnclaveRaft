//
// Created by ncl on 14/11/19.
//

#include "cert_utils.hxx"
#include <functional>

#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509.h>

typedef STACK_OF(X509) sk_X509;

/*==========================================================================
 * Load Certificates
 *========================================================================== */

X509 *load_cert_unsafe(const char *data, size_t size) {
    ptr<BIO> bio_mem = ptr<BIO>(BIO_new(BIO_s_mem()), BIO_free);

    if (bio_mem == nullptr || bio_mem.get() == nullptr) {
        throw OpenSSLException(ERR_get_error());
    }

    if (BIO_write(bio_mem.get(), data, size) != size) {
        throw OpenSSLException(ERR_get_error());
    }

    /* ret */ X509 *cert = PEM_read_bio_X509(bio_mem.get(), nullptr, nullptr, nullptr);
    if (cert == nullptr) {
        throw OpenSSLException(ERR_get_error());
    } else {
        return cert;
    }
}


ptr<sk_X509> load_cert_chain(const string &cert_chain, ptr<X509> &sign_cert) {
    /* ret */ ptr<sk_X509> stack = ptr<sk_X509>(sk_X509_new_null(),
                                                std::bind(sk_X509_pop_free, std::placeholders::_1, X509_free));
    if (stack == nullptr || stack.get() == nullptr) {
        throw OpenSSLException(ERR_get_error());
    }

    /* Build the cert stack */
    size_t idx_l = 0, idx_r = 0;
    while (idx_r != string::npos) {
        idx_r = cert_chain.find("-----BEGIN", idx_l + 1);
        size_t len = ((idx_r == string::npos) ? cert_chain.length() : idx_r) - idx_l;

        X509 *cert = load_cert_unsafe(cert_chain.c_str() + idx_l, len);
        sk_X509_push(stack.get(), cert);

        if (idx_l == 0) {
            sign_cert = ptr<X509>(cert, X509_free);
        }

        idx_l = idx_r;
    }

    return stack;
}

/*==========================================================================
 * Certificate verification
 *========================================================================== */

ptr<X509_STORE> build_ca(X509 *cert) {
    /* ret */ ptr<X509_STORE> store = ptr<X509_STORE>(X509_STORE_new(), X509_STORE_free);
    if (store == nullptr || store.get() == nullptr) {
        throw OpenSSLException(ERR_get_error());
    }

    if (X509_STORE_add_cert(store.get(), cert) != 1) {
        throw OpenSSLException(ERR_get_error());
    }

    return store;
}

/*
 * Verify cert chain against our CA in store. Assume the first cert in
 * the chain is the one to validate. Note that a store context can only
 * be used for a single verification so we need to do this every time
 * we want to validate a cert.
 */

bool cert_verify(X509_STORE *store, sk_X509 *chain) {
    ptr<X509_STORE_CTX> ctx = ptr<X509_STORE_CTX>(X509_STORE_CTX_new(), X509_STORE_CTX_free);
    if (ctx == nullptr || ctx.get() == nullptr) {
        throw OpenSSLException(ERR_get_error());
    }

    if (X509_STORE_CTX_init(ctx.get(), store, sk_X509_value(chain, 0), chain) != 1) {
        throw OpenSSLException(ERR_get_error());
    }

    int rv = X509_verify_cert(ctx.get());
    if (rv != 1 && rv != 0) {
        throw OpenSSLException(ERR_get_error());
    }

    return rv;
}

bool sha256_verify(const uint8_t *msg, size_t msgsz, const uint8_t *sig, size_t sigsz, EVP_PKEY *pkey) {
    ptr<EVP_MD_CTX> ctx = ptr<EVP_MD_CTX>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (ctx == nullptr || ctx.get() == nullptr) {
        throw OpenSSLException(ERR_get_error());
    }

    if (EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, pkey) != 1) {
        throw OpenSSLException(ERR_get_error());
    }

    if (EVP_DigestVerifyUpdate(ctx.get(), msg, msgsz) != 1) {
        throw OpenSSLException(ERR_get_error());
    }

    int rv = EVP_DigestVerifyFinal(ctx.get(), sig, sigsz);
    if (rv == 0 || rv == 1) {
        return rv;
    } else {
        throw OpenSSLException(ERR_get_error());
    }
}


bool verify_certs_and_signature(const string &certs, const bytes &sig, const uint8_t *data, size_t size) {
    static const string ias_root_ca_cert =
            "-----BEGIN CERTIFICATE-----\n"
            "MIIFSzCCA7OgAwIBAgIJANEHdl0yo7CUMA0GCSqGSIb3DQEBCwUAMH4xCzAJBgNV\n"
            "BAYTAlVTMQswCQYDVQQIDAJDQTEUMBIGA1UEBwwLU2FudGEgQ2xhcmExGjAYBgNV\n"
            "BAoMEUludGVsIENvcnBvcmF0aW9uMTAwLgYDVQQDDCdJbnRlbCBTR1ggQXR0ZXN0\n"
            "YXRpb24gUmVwb3J0IFNpZ25pbmcgQ0EwIBcNMTYxMTE0MTUzNzMxWhgPMjA0OTEy\n"
            "MzEyMzU5NTlaMH4xCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJDQTEUMBIGA1UEBwwL\n"
            "U2FudGEgQ2xhcmExGjAYBgNVBAoMEUludGVsIENvcnBvcmF0aW9uMTAwLgYDVQQD\n"
            "DCdJbnRlbCBTR1ggQXR0ZXN0YXRpb24gUmVwb3J0IFNpZ25pbmcgQ0EwggGiMA0G\n"
            "CSqGSIb3DQEBAQUAA4IBjwAwggGKAoIBgQCfPGR+tXc8u1EtJzLA10Feu1Wg+p7e\n"
            "LmSRmeaCHbkQ1TF3Nwl3RmpqXkeGzNLd69QUnWovYyVSndEMyYc3sHecGgfinEeh\n"
            "rgBJSEdsSJ9FpaFdesjsxqzGRa20PYdnnfWcCTvFoulpbFR4VBuXnnVLVzkUvlXT\n"
            "L/TAnd8nIZk0zZkFJ7P5LtePvykkar7LcSQO85wtcQe0R1Raf/sQ6wYKaKmFgCGe\n"
            "NpEJUmg4ktal4qgIAxk+QHUxQE42sxViN5mqglB0QJdUot/o9a/V/mMeH8KvOAiQ\n"
            "byinkNndn+Bgk5sSV5DFgF0DffVqmVMblt5p3jPtImzBIH0QQrXJq39AT8cRwP5H\n"
            "afuVeLHcDsRp6hol4P+ZFIhu8mmbI1u0hH3W/0C2BuYXB5PC+5izFFh/nP0lc2Lf\n"
            "6rELO9LZdnOhpL1ExFOq9H/B8tPQ84T3Sgb4nAifDabNt/zu6MmCGo5U8lwEFtGM\n"
            "RoOaX4AS+909x00lYnmtwsDVWv9vBiJCXRsCAwEAAaOByTCBxjBgBgNVHR8EWTBX\n"
            "MFWgU6BRhk9odHRwOi8vdHJ1c3RlZHNlcnZpY2VzLmludGVsLmNvbS9jb250ZW50\n"
            "L0NSTC9TR1gvQXR0ZXN0YXRpb25SZXBvcnRTaWduaW5nQ0EuY3JsMB0GA1UdDgQW\n"
            "BBR4Q3t2pn680K9+QjfrNXw7hwFRPDAfBgNVHSMEGDAWgBR4Q3t2pn680K9+Qjfr\n"
            "NXw7hwFRPDAOBgNVHQ8BAf8EBAMCAQYwEgYDVR0TAQH/BAgwBgEB/wIBADANBgkq\n"
            "hkiG9w0BAQsFAAOCAYEAeF8tYMXICvQqeXYQITkV2oLJsp6J4JAqJabHWxYJHGir\n"
            "IEqucRiJSSx+HjIJEUVaj8E0QjEud6Y5lNmXlcjqRXaCPOqK0eGRz6hi+ripMtPZ\n"
            "sFNaBwLQVV905SDjAzDzNIDnrcnXyB4gcDFCvwDFKKgLRjOB/WAqgscDUoGq5ZVi\n"
            "zLUzTqiQPmULAQaB9c6Oti6snEFJiCQ67JLyW/E83/frzCmO5Ru6WjU4tmsmy8Ra\n"
            "Ud4APK0wZTGtfPXU7w+IBdG5Ez0kE1qzxGQaL4gINJ1zMyleDnbuS8UicjJijvqA\n"
            "152Sq049ESDz+1rRGc2NVEqh1KaGXmtXvqxXcTB+Ljy5Bw2ke0v8iGngFBPqCTVB\n"
            "3op5KBG3RjbF6RRSzwzuWfL7QErNC8WEy5yDVARzTA5+xmBc388v9Dm21HGfcC8O\n"
            "DD+gT9sSpssq0ascmvH49MOgjt1yoysLtdCtJW/9FZpoOypaHx0R+mJTLwPXVMrv\n"
            "DaVzWh5aiEx+idkSGMnX\n"
            "-----END CERTIFICATE-----\n";

    /*
     * The response body has the attestation report. The headers have
     * a signature of the report, and the public signing certificate.
     * We need to:
     *
     * 1) Verify the certificate chain, to ensure it's issued by the
     *    Intel CA (passed with the -A option).
     *
     * 2) Extract the public key from the signing cert, and verify
     *    the signature.
     */

    if (certs.empty()) {
        throw std::invalid_argument("Certificate Chain is empty");
    } else if (sig.empty()) {
        throw std::invalid_argument("Signature is empty");
    } else if (data == nullptr || size == 0) {
        throw std::invalid_argument("Body is empty");
    }

    /* Load Intel Root CA */
    ptr<X509> root_ca = ptr<X509>(load_cert_unsafe(ias_root_ca_cert.c_str(), ias_root_ca_cert.size()), X509_free);

    /* Build certificate stack from the certificate chain string */
    ptr<X509> sign_cert;
    ptr<sk_X509> stack = load_cert_chain(certs, sign_cert);

    /* Verify the certificate */
    ptr<X509_STORE> store = build_ca(root_ca.get());
    if (!cert_verify(store.get(), stack.get())) {
        return false;
    }

    /* The signing cert is valid, so extract and verify the signature */
    /*
     * The report body is SHA256 signed with the private key of the
     * signing cert.  Extract the public key from the certificate and
     * verify the signature.
     */
    ptr<EVP_PKEY> pkey = ptr<EVP_PKEY>(X509_get_pubkey(sign_cert.get()), EVP_PKEY_free);
    if (pkey == nullptr || pkey.get() == nullptr) {
        throw OpenSSLException(ERR_get_error());
    }

    return sha256_verify(data, size, sig.data(), sig.size(), pkey.get());
}
