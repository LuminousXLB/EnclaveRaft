//
// Created by ncl on 14/11/19.
//

#ifndef ENCLAVERAFT_KEY_STORE_HXX
#define ENCLAVERAFT_KEY_STORE_HXX

#include "common.hxx"
#include <map>
#include <cppcodec/base64_default_rfc4648.hpp>
#include <json11.hpp>
#include <utility>
#include <sgx_tcrypto.h>
#include <tlibc/mbusafecrt.h>
#include "../port/rpc_client_port.hxx"

using std::map;
using json11::Json;
using attestation_callback = std::function<void(bool)>;

class er_key_store : public std::enable_shared_from_this<er_key_store> {

public:
    er_key_store(const sgx_ec256_private_t &private_key, const sgx_ec256_public_t &public_key) :
            self_private_(private_key), self_public_(public_key) {}

    bool register_report(const char *http_packet);

    const sgx_ec256_public_t &public_key() const {
        return self_public_;
    }

    void add_server(int32_t server_id, const string &endpoint, attestation_callback &callback) {
        if (store_.find(server_id) != store_.end()) {
            /* attested or attesting */
            callback(true);
            return;
        }

        auto it = temp_connections_.find(server_id);
        if (it != temp_connections_.end()) {
            lock_guard<mutex> lock(temp_connections_lock_);
            ptr<RpcClientPort> client = it->second.first;
            temp_connections_[server_id] = std::make_pair(client, callback);
            return;
        }

        ptr<RpcClientPort> client = make_shared<RpcClientPort>(endpoint);
        int32_t id = raft_rpc_request_handler->server_id();

        auto self = shared_from_this();
        client->send_xchg_request(id, self_report_,
                                  [self, server_id, endpoint](const ptr<Json> &resp, const string &exception) {
                                      int32_t peer_id = (*resp)["server_id"].int_value();
                                      string report = (*resp)["report"].string_value();

                                      self->handle_xchg_resp(server_id, endpoint, report.c_str());
                                  });

    }


    void handle_xchg_resp(int32_t server_id, string endpoint, const char *http_packet) {
        attestation_callback callback;
        {
            lock_guard<mutex> lock(temp_connections_lock_);
            auto it = temp_connections_.find(server_id);
            callback = it->second.second;
            temp_connections_.erase(server_id);
        }

        auto quote_bytes = validate_verification_report(http_packet);
        if (quote_bytes != nullptr) {
            const sgx_quote_t &quote = *reinterpret_cast<const sgx_quote_t *>(quote_bytes->data());
            const sgx_report_body_t &report_body = quote.report_body;
            const sgx_report_data_t &report_data = report_body.report_data;

            sgx_ec256_public_t pk;
            memcpy_s(&pk, sizeof(sgx_ec256_public_t), report_data.d, SGX_REPORT_DATA_SIZE);

            {
                lock_guard<mutex> lock(store_lock_);
                store_[server_id] = make_shared<server_info>(pk, endpoint);
            }

            callback(true);
        } else {
            callback(false);
        }
    }

private:
    static ptr<bytes> validate_verification_report(const char *http_packet);

    string self_report_;
    sgx_ec256_private_t self_private_;
    sgx_ec256_public_t self_public_;

    struct server_info {
        sgx_ec256_public_t public_key;
        string endpoint;

        server_info(sgx_ec256_public_t pk, string ep) : public_key(pk), endpoint(std::move(ep)) {}
    };

    map<int32_t, ptr<server_info>> store_;
    mutex store_lock_;

    map<int32_t, std::pair<ptr<RpcClientPort>, attestation_callback>> temp_connections_;
    mutex temp_connections_lock_;
};

#endif //ENCLAVERAFT_KEY_STORE_HXX
