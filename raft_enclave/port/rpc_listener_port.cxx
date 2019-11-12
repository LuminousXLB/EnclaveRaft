//
// Created by ncl on 29/10/19.
//

#include <map>
#include <mutex>
#include <tlibc/mbusafecrt.h>
#include "rpc_listener_port.hxx"
#include "crypto.hxx"

using std::mutex;
using std::lock_guard;
using std::map;
using std::atomic;
using std::shared_ptr;
using std::vector;

using cornerstone::buffer;
using cornerstone::bufptr;
using cornerstone::msg_type;
using cornerstone::req_msg;
using cornerstone::cs_new;
using cornerstone::log_entry;
using cornerstone::log_val_type;
using cornerstone::resp_msg;
using cornerstone::lstrfmt;


ptr<msg_handler> rpc_listener_req_handler;

static mutex message_buffer_lock;
static atomic<uint32_t> id_counter;
static map<uint32_t, shared_ptr<vector<uint8_t>>> response_buffer_map;

int32_t ecall_handle_rpc_request(uint32_t size, const uint8_t *message, uint32_t *msg_id) {
    // FIXME: Encrypt & Decrypt
    auto message_buffer = raft_decrypt(message, size);

    bufptr header = buffer::alloc(RPC_REQ_HEADER_SIZE);
    memcpy_s(header->data(), header->size(), message_buffer->data(), RPC_REQ_HEADER_SIZE);
    header->pos(RPC_REQ_HEADER_SIZE - 4);
    int32 data_size = header->get_int();

    bufptr log_data = buffer::alloc(data_size);
    memcpy_s(log_data->data(), log_data->size(), message_buffer->data() + RPC_REQ_HEADER_SIZE, data_size);

    try {
        ptr<req_msg> req = deserialize_req(header, log_data);
        ptr<resp_msg> resp = rpc_listener_req_handler->process_req(*req);

        if (!resp) {
            rpc_listener_req_handler->get_logger()->err(
                    "no response is returned from raft message handler, potential system bug");
            *msg_id = 0;
            return 0;
        } else {
            bufptr resp_buf = serialize_resp(resp);
            // FIXME: Encrypt & Decrypt
            message_buffer = raft_encrypt(resp_buf->data(), resp_buf->size());

            {
                lock_guard<mutex> lock(message_buffer_lock);
                *msg_id = ++id_counter;
                response_buffer_map.emplace(*msg_id, message_buffer);
            }

            return message_buffer->size();
        }
    }
    catch (std::exception &ex) {
        rpc_listener_req_handler->get_logger()->err(
                lstrfmt("failed to process request message due to error: %s").fmt(ex.what()));
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
