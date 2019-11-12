//
// Created by ncl on 12/11/19.
//

#ifndef ENCLAVERAFT_INTEL_IAS_HXX
#define ENCLAVERAFT_INTEL_IAS_HXX

#include "ssl_client.hxx"
#include "common.hxx"
#include <memory>
#include "json11.hpp"

using std::string;
using std::array;
using std::vector;
using std::make_shared;

using json11::Json;

static const char *host = "api.trustedservices.intel.com";
static const char *protocol = "https";

class IntelIAS {
public:

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
        const string body = Json(Json::object{
                {"isvEnclaveQuote", isvEnclaveQuote_b64}
        }).dump();
        uint8_t key_index = 0;

        string request;
        request += fmt::format("POST {} HTTP/1.1", path) + "\r\n";
        request += fmt::format("Host: {}", host) + "\r\n";
        request += fmt::format("Content-Type: {}", "application/json") + "\r\n";
        request += fmt::format("Ocp-Apim-Subscription-Key: {}", key[key_index]) + "\r\n";
        request += fmt::format("Content-Length: {}", body.length()) + "\r\n";
        request += "\r\n";
        request += body;
        request += "\r\n";

        connection_ = make_shared<ssl_client>(io_context_, ssl_context_, host, protocol);
        return connection_->request(reinterpret_cast<const uint8_t *>(request.data()), request.size());
    }

    const map<string, string> &response_headers() const {
        return connection_->response_headers();
    }

    size_t content_length() const {
        return connection_->content_length();
    }

    const void *response_body() const {
        return connection_->response_body();
    }

private:
    ENVIRONMENT env_;
    asio::io_context io_context_;
    asio::ssl::context ssl_context_;
    ptr<ssl_client> connection_;
    array<string, 2> key;
};

#endif //ENCLAVERAFT_INTEL_IAS_HXX
