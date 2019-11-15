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
using cornerstone::srv_config;

extern ptr<msg_handler> raft_rpc_request_handler;

class er_key_store : public std::enable_shared_from_this<er_key_store> {

public:
    er_key_store(const sgx_ec256_private_t &private_key, const sgx_ec256_public_t &public_key) :
            self_private_(private_key), self_public_(public_key) {}

    bool register_report(const char *http_packet);

    const sgx_ec256_public_t &public_key() const {
        return self_public_;
    }

    const string &report() const {
        return self_report_;
    }

    void build_key_xchg_req(int32_t server_id, const string &endpoint) {
        p_logger->debug("TRACE er_key_store::" + string(__FUNCTION__) + " " + endpoint);

        if (store_.find(server_id) != store_.end()) {
            /* attested or attesting */
            raft_rpc_request_handler->add_srv(make_shared<srv_config>(server_id, endpoint));
            return;
        }

        ptr<RpcClientPort> client = nullptr;
        int32_t id = raft_rpc_request_handler->server_id();
        {
            lock_guard<mutex> lock(temp_connections_lock_);
            auto it = temp_connections_.find(server_id);
            if (it == temp_connections_.end()) {
                client = make_shared<RpcClientPort>(endpoint);
                temp_connections_[server_id] = std::make_pair(client, endpoint);
            } else {
                client = it->second.first;
                temp_connections_[server_id] = std::make_pair(client, endpoint);
                return;
            }
        }

        auto self = shared_from_this();
        client->send_xchg_request(id, self_report_);
    }

    void handle_key_xchg_resp(const string &payload, const string &exception) {
        if (!exception.empty()) {
            p_logger->err("er_key_store::handle_key_xchg_resp: RPC Exception: " + exception);
            return;
        }

        string json_err;
        Json rsp = Json::parse(payload, json_err);

        if (!json_err.empty()) {
            p_logger->err("er_key_store::handle_key_xchg_resp: JSON Pare Error: " + json_err);
            return;
        }

        int32_t server_id = rsp["server_id"].int_value();
        string report = rsp["report"].string_value();

        bool result = receive_report(server_id, report.c_str());

        string endpoint;
        {
            lock_guard<mutex> lock(temp_connections_lock_);
            auto it = temp_connections_.find(server_id);
            endpoint = it->second.second;
            temp_connections_.erase(server_id);
        }

        p_logger->debug("TRACE er_key_store::" + string(__FUNCTION__) + " " + std::to_string(__LINE__));
        p_logger->debug("TRACE er_key_store::" + string(__FUNCTION__) + " " + endpoint);

        raft_rpc_request_handler->add_srv(make_shared<srv_config>(server_id, endpoint));
    }

    bool receive_report(int32_t server_id, const char *http_packet) {
        p_logger->debug("TRACE er_key_store::" + string(__FUNCTION__) + " " + std::to_string(__LINE__));

        auto quote_bytes = validate_verification_report(http_packet);
        if (quote_bytes != nullptr) {
            const sgx_quote_t &quote = *reinterpret_cast<const sgx_quote_t *>(quote_bytes->data());
            const sgx_report_body_t &report_body = quote.report_body;
            const sgx_report_data_t &report_data = report_body.report_data;

            sgx_ec256_public_t pk;
            memcpy_s(&pk, sizeof(sgx_ec256_public_t), report_data.d, SGX_REPORT_DATA_SIZE);

            {
                lock_guard<mutex> lock(store_lock_);
                store_[server_id] = make_shared<server_info>(pk);
            }

            return true;
        } else {
            return false;
        }
    }

private:
    static ptr<bytes> validate_verification_report(const char *http_packet);

    string self_report_;
    sgx_ec256_private_t self_private_;
    sgx_ec256_public_t self_public_;

    struct server_info {
        sgx_ec256_public_t public_key;

        explicit server_info(sgx_ec256_public_t pk) : public_key(pk) {}
    };

    map<int32_t, ptr<server_info>> store_;
    mutex store_lock_;

    map<int32_t, std::pair<ptr<RpcClientPort>, string>> temp_connections_;
    mutex temp_connections_lock_;
};

#endif //ENCLAVERAFT_KEY_STORE_HXX
