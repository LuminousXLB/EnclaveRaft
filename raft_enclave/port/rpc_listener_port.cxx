//
// Created by ncl on 29/10/19.
//

#include "common.hxx"
#include <map>
#include <mutex>
#include <tlibc/mbusafecrt.h>
#include "rpc_listener_port.hxx"
#include "crypto.hxx"
#include "messages.hxx"
#include "json11.hpp"
#include <cppcodec/base64_default_rfc4648.hpp>

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


ptr<msg_handler> raft_rpc_request_handler;

static mutex message_buffer_lock;
static atomic<uint32_t> id_counter;
static map<uint32_t, shared_ptr<vector<uint8_t>>> response_buffer_map;
extern string attestation_verification_report;


ptr<bytes> handle_raft_message(const uint8_t *data, uint32_t size);

ptr<bytes> handle_client_message(er_message_type type, const string &payload);

//#include <cppcodec/hex_default_lower.hpp>

int32_t ecall_handle_rpc_request(uint32_t size, const uint8_t *message, uint32_t *msg_id) {
    p_logger->debug(lstrfmt("ecall_handle_rpc_request -> %d %s").fmt(size, hex::encode(message, size).c_str()));

    auto type = static_cast<er_message_type>(*(uint16_t *) message);

    ptr<bytes> response = nullptr;

    switch (type) {
        case client_add_srv_req:
        case client_remove_srv_req:
        case client_append_entries_req:;
            response = handle_client_message(type, string(message + sizeof(uint16_t), message + size));
            break;
        case system_key_exchange_req:
            break;
        case system_key_setup_req:
            break;
        case raft_message:
            response = handle_raft_message(message + sizeof(uint16_t), size - sizeof(uint16_t));
            response->insert(response->begin(), message, message + sizeof(uint16_t));
            break;
        default:
            p_logger->err(lstrfmt("Unexpected message type [%d]").fmt(type));
            return -1;
    }

    if (response) {
        {
            lock_guard<mutex> lock(message_buffer_lock);
            *msg_id = ++id_counter;
            response_buffer_map.emplace(*msg_id, response);
        }
        return response->size();
    } else {
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
        memcpy_s(buffer, buffer_size, it->second->data(), it->second->size());
        return true;
    }
}

///////////////////////////////////////////////////////////////////////////////


ptr<bytes> handle_client_message(er_message_type type, const string &payload) {
    string json_err;
    Json body = Json::parse(payload, json_err);

    if (!json_err.empty()) {
        p_logger->err(lstrfmt("JSON Parse Error: %s").fmt(payload.c_str()));
        p_logger->err(lstrfmt("JSON Parse Error: %s").fmt(json_err.c_str()));
    }

    auto resp_type = make_shared<uint16_t>(0);

    bool result;

    switch (type) {
        case client_add_srv_req:
            *resp_type = client_add_srv_resp;
            {
                uint16_t server_id = body["server_id"].int_value();
                string endpoint = body["endpoint"].string_value();
                p_logger->debug(lstrfmt("%s -> %s %d").fmt(__FUNCTION__, endpoint.c_str(), server_id));

                srv_config srv_cfg(server_id, endpoint);
                ptr<async_result<bool>> a_result = raft_rpc_request_handler->add_srv(srv_cfg);
                result = a_result->get();
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
                    bufptr p_log = buffer::alloc(log.size());
                    memcpy_s(p_log->data(), p_log->size(), log.data(), log.size());
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
    resp_buf->insert(resp_buf->end(), response.data(), response.data() + response.size());

    return resp_buf;
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

    return message_buffer;
}
