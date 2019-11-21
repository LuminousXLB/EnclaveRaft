//
// Created by ncl on 12/11/19.
//

#ifndef ENCLAVERAFT_INTEL_IAS_HXX
#define ENCLAVERAFT_INTEL_IAS_HXX

#include "ssl_client.hxx"

#include <memory>
#include <cstring>

using std::make_shared;

static const char *host = "api.trustedservices.intel.com";
static const char *protocol = "https";

class IntelIAS {
public:
    template<class T> using sptr = std::shared_ptr<T>;

    enum ENVIRONMENT {
        DEVELOPMENT,
        PRODUCTION
    };

    IntelIAS(const string &primary_key, const string &secondary_key, ENVIRONMENT env = DEVELOPMENT)
            : env_(env),
              key({primary_key, secondary_key}),
              io_context_(),
              ssl_context_(asio::ssl::context::sslv23) {

        ssl_context_.load_verify_file("/etc/ssl/certs/ca-certificates.crt");

    }

    string report(const string &isvEnclaveQuote_b64) {
        const char *path = (env_ == PRODUCTION) ? "/sgx/attestation/v3/report" : "/sgx/dev/attestation/v3/report";
        const string body = R"({"isvEnclaveQuote": ")" + isvEnclaveQuote_b64 + R"("})";

        uint8_t key_index = 0;

        string request;
        request.append("POST ").append(path).append(" HTTP/1.1");
        request += "\r\n";
        request.append("Host: ").append(host);
        request += "\r\n";
        request.append("Content-Type: application/json");
        request += "\r\n";
        request.append("Ocp-Apim-Subscription-Key: ").append(key[key_index]);
        request += "\r\n";
        request.append("Content-Length: ").append(std::to_string(body.length()));
        request += "\r\n\r\n";
        request.append(body);
        request += "\r\n";

        connection_ = make_shared<ssl_client>(io_context_, ssl_context_, host, protocol);
        return connection_->request(reinterpret_cast<const uint8_t *>(request.data()), request.size());
    }

    string str() const {
        return string(data(), data() + size());
    }

    size_t size() const {
        return connection_->size();
    }

    const char *data() const {
        return static_cast<const char *>(connection_->data());
    }

private:
    ENVIRONMENT env_;
    asio::io_context io_context_;
    asio::ssl::context ssl_context_;
    sptr<ssl_client> connection_;
    array<string, 2> key;
};

#endif //ENCLAVERAFT_INTEL_IAS_HXX
