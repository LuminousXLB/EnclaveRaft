//
// Created by ncl on 29/10/19.
//

#include "common.hxx"
#include <map>
#include <mutex>
#include <tlibc/mbusafecrt.h>
#include <cppcodec/base64_default_rfc4648.hpp>
#include "json11.hpp"
#include "rpc_listener_port.hxx"
#include "crypto.hxx"
#include "messages.hxx"
#include "../raft_enclave.hxx"

using std::map;
using std::mutex;
using std::lock_guard;
using std::atomic;

using json11::Json;

using cornerstone::async_result;
using cornerstone::buffer;
using cornerstone::bufptr;
using cornerstone::msg_type;
using cornerstone::req_msg;
using cornerstone::srv_config;
using cornerstone::cs_new;
using cornerstone::log_entry;
using cornerstone::log_val_type;
using cornerstone::resp_msg;
using cornerstone::lstrfmt;

extern app_context_t g;

static mutex message_buffer_lock;
static atomic<uint32_t> id_counter;
static map<uint32_t, shared_ptr<vector<uint8_t>>> response_buffer_map;
extern string attestation_verification_report;

ptr<bytes> handle_client_request(const erMessageMold *message);

ptr<bytes> handle_system_request(const erMessageMold *message);

ptr<bytes> handle_raft_message(const erMessageMold *message);

int32_t ecall_handle_rpc_request(uint32_t size, const uint8_t *message, uint32_t *msg_id) {
    p_logger->debug(lstrfmt("ecall_handle_rpc_request -> %d %s").fmt(size, hex::encode(message, size).c_str()));

    auto msg = reinterpret_cast<const erMessageMold * >(message);

    ptr<bytes> response = nullptr;

    switch (msg->header.m_type) {
        case client_add_srv_req:
        case client_remove_srv_req:
        case client_append_entries_req:
            response = handle_client_request(msg);
            break;
        case system_key_exchange_req:
        case system_key_setup_req:
            response = handle_system_request(msg);
            break;
        case raft_message:
            response = handle_raft_message(msg);
            break;
        default:
            p_logger->err(lstrfmt("Unexpected message type [%d]").fmt(msg->header.m_type));
            return -1;
    }

    if (response) {
        p_logger->debug("ecall_handle_rpc_request: RESPONSE " + hex::encode(*response));
        {
            lock_guard<mutex> lock(message_buffer_lock);
            *msg_id = ++id_counter;
            response_buffer_map.emplace(*msg_id, response);
        }
        return response->size();
    } else {
        p_logger->warn("ecall_handle_rpc_request: RESPONSE <NONE>");

        *msg_id = 0;
        return -1;
    }
}

bool ecall_fetch_rpc_response(uint32_t msg_id, uint32_t buffer_size, uint8_t *buffer) {
    lock_guard<mutex> lock(message_buffer_lock);
    auto it = response_buffer_map.find(msg_id);
    if (it == response_buffer_map.end()) {
        return false;
    } else {
        p_logger->debug("ecall_fetch_rpc_response: RESPONSE " + hex::encode(*it->second));
        memcpy_s(buffer, buffer_size, it->second->data(), it->second->size());
        return true;
    }
}

///////////////////////////////////////////////////////////////////////////////

//ptr<bytes> handle_client_request(const erMessageMold *message);
//
//ptr<bytes> handle_system_request(const erMessageMold *message);
//
//ptr<bytes> handle_raft_message(const erMessageMold *message);

ptr<bytes> handle_client_request(const erMessageMold *message) {
    const char *payload = reinterpret_cast<const char *>(message->payload);

    string json_err;
    Json body = Json::parse(payload, json_err);

    if (!json_err.empty()) {
        p_logger->err(lstrfmt("JSON Parse Error: %s").fmt(payload));
        p_logger->err(lstrfmt("JSON Parse Error: %s").fmt(json_err.c_str()));
    }

    auto resp_type = make_shared<uint16_t>(0);

    bool result;

    switch (message->header.m_type) {
        case client_add_srv_req:
            *resp_type = client_add_srv_resp;
            {
                int32_t server_id = body["server_id"].int_value();
                string endpoint = body["endpoint"].string_value();

                p_logger->debug(lstrfmt("%s -> %s %d").fmt(__FUNCTION__, endpoint.c_str(), server_id));

                // FIXME:
//                key_store->build_key_xchg_req(server_id, endpoint);

                result = true;
            }
            break;
        case client_remove_srv_req:
            *resp_type = client_remove_srv_resp;
            {
                uint16_t server_id = body["server_id"].int_value();
                ptr<async_result<bool>> a_result = raft_rpc_request_handler->remove_srv(server_id);
                result = a_result->get();
            }
            break;
        case client_append_entries_req:
            *resp_type = client_append_entries_resp;
            {
                std::vector<bufptr> logs;
                for (const auto &item: body.array_items()) {
                    bytes log = base64::decode(item.string_value());
                    string s_log(log.begin(), log.end());
                    bufptr p_log = buffer::alloc(s_log.size() + 8);
                    memset_s(p_log->data(), p_log->size(), 0, p_log->size());
                    memcpy_s(p_log->data(), p_log->size(), s_log.data(), s_log.length());

                    logs.push_back(p_log);
                }
                ptr<async_result<bool>> a_result = raft_rpc_request_handler->append_entries(logs);
                result = a_result->get();
            }
            break;
        default:
            return nullptr;
    }

    Json resp_body = Json::object{
            {"success", int(result)}
    };

    string response = resp_body.dump();
    auto *type_ptr = reinterpret_cast<uint8_t *>(resp_type.get());

    auto resp_buf = make_shared<bytes>();
    resp_buf->reserve(sizeof(uint16_t) + response.length());
    resp_buf->insert(resp_buf->end(), type_ptr, type_ptr + sizeof(uint16_t));
    resp_buf->insert(resp_buf->end(), response.begin(), response.end());

    return resp_buf;
}

ptr<bytes> handle_system_request(erMessageType type, const string &payload) {
    string json_err;
    Json body = Json::parse(payload, json_err);

    if (!json_err.empty()) {
        p_logger->err(lstrfmt("JSON Parse Error: %s").fmt(payload.c_str()));
        p_logger->err(lstrfmt("JSON Parse Error: %s").fmt(json_err.c_str()));
    }

    ptr<bytes> buffer = nullptr;
    uint16_t resp_type;

    switch (type) {
        case system_key_exchange_req: {
            int32_t peer_id = body["server_id"].int_value();
            string report = body["report"].string_value();

            bool result = key_store->receive_report(peer_id, report.c_str());

            if (result) {
                Json resp = Json::object{
                        {"server_id", raft_rpc_request_handler->server_id()},
                        {"report",    key_store->report()}
                };

                string resp_str = resp.dump();
                p_logger->info("response: " + resp_str);
                buffer = make_shared<bytes>(resp_str.begin(), resp_str.end());
            }
        }
            resp_type = system_key_exchange_resp;
            break;
        case system_key_setup_req:
            resp_type = system_key_setup_resp;
            return nullptr;
        default:
            return nullptr;
    }

    auto *ptr = reinterpret_cast<uint8_t *>(&resp_type);
    buffer->insert(buffer->begin(), ptr, ptr + sizeof(uint16_t));

    return buffer;
}

ptr<bytes> handle_raft_message(const uint8_t *data, uint32_t size) {
    // FIXME: Decrypt
    auto message_buffer = raft_decrypt(data, size);

    bufptr header = buffer::alloc(RPC_REQ_HEADER_SIZE);
    memcpy_s(header->data(), header->size(), message_buffer->data(), RPC_REQ_HEADER_SIZE);
    header->pos(RPC_REQ_HEADER_SIZE - 4);
    int32 data_size = header->get_int();

    bufptr log_data = buffer::alloc(data_size);
    memcpy_s(log_data->data(), log_data->size(), message_buffer->data() + RPC_REQ_HEADER_SIZE, data_size);

    try {
        ptr<req_msg> req = deserialize_req(header, log_data);
        ptr<resp_msg> resp = raft_rpc_request_handler->process_req(*req);

        if (!resp) {
            p_logger->err("no response is returned from raft message handler, potential system bug");
            return nullptr;
        } else {
            bufptr resp_buf = serialize_resp(resp);
            // FIXME: Encrypt
            message_buffer = raft_encrypt(resp_buf->data(), resp_buf->size());
        }
    }
    catch (std::exception &ex) {
        p_logger->err(lstrfmt("failed to process request message due to error: %s").fmt(ex.what()));
        return nullptr;
    }

    static uint16_t type = raft_message;
    static auto *ptr = reinterpret_cast<uint8_t *>(&type);

    message_buffer->insert(message_buffer->begin(), ptr, ptr + sizeof(uint16_t));

    return message_buffer;
}
