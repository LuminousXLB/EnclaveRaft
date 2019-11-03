//
// Created by ncl on 29/10/19.
//

#include <map>
#include <mutex>
#include <tlibc/mbusafecrt.h>
#include "rpc_listener_port.hxx"

using std::mutex;
using std::lock_guard;
using std::map;
using std::atomic;

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
extern ptr<cornerstone::logger> p_logger;

static mutex message_buffer_lock;
static atomic<uint32_t> id_counter;
static map<uint32_t, bufptr> message_buffer;

int32_t ecall_handle_rpc_request(uint32_t size, const uint8_t *message, uint32_t *msg_id) {
    p_logger->debug(lstrfmt("%s %s %d: req_size=%u").fmt(__FILE__, __FUNCTION__, __LINE__, size));

    // FIXME: Encrypt & Decrypt
    bufptr header = buffer::alloc(RPC_REQ_HEADER_SIZE);
    memcpy_s(header->data(), header->size(), message, RPC_REQ_HEADER_SIZE);
    header->pos(RPC_REQ_HEADER_SIZE - 4);
    int32 data_size = header->get_int();

    bufptr log_data = buffer::alloc(data_size);
    memcpy_s(log_data->data(), log_data->size(), message + RPC_REQ_HEADER_SIZE, data_size);

    try {
        ptr<req_msg> req = deserialize_req(header, log_data);
        p_logger->debug(lstrfmt("%s %s %d: req_type=%d").fmt(__FILE__, __FUNCTION__, __LINE__, req->get_type()));

        ptr<resp_msg> resp = rpc_listener_req_handler->process_req(*req);

        if (!resp) {
            rpc_listener_req_handler->get_logger()->err(
                    "no response is returned from raft message handler, potential system bug");
            *msg_id = 0;
            return 0;
        } else {
            p_logger->debug(
                    lstrfmt("%s: MessageId=%d REQUEST.type=[%s, %d].term=%016llx RESPONSE.type=[%s, %d].term=%016llx")
                            .fmt(__FUNCTION__,
                                 *msg_id,
                                 msg_type_string(req->get_type()),
                                 req->get_type(),
                                 req->get_term(),
                                 msg_type_string(resp->get_type()),
                                 resp->get_type(),
                                 resp->get_term()));

            bufptr resp_buf = serialize_resp(resp);

            {
                lock_guard<mutex> lock(message_buffer_lock);
                *msg_id = ++id_counter;
                message_buffer.emplace(*msg_id, buffer::copy(*resp_buf));
            }

            return RPC_RESP_HEADER_SIZE;
        }
    }
    catch (std::exception &ex) {
        rpc_listener_req_handler->get_logger()->err(
                lstrfmt("failed to process request message due to error: %s").fmt(ex.what()));
        return -1;
    }
}

bool ecall_fetch_rpc_response(uint32_t msg_id, uint32_t buffer_size, uint8_t *buffer) {
    p_logger->debug(lstrfmt("%s %s %d: %u").fmt(__FILE__, __FUNCTION__, __LINE__, msg_id));

    lock_guard<mutex> lock(message_buffer_lock);
    auto it = message_buffer.find(msg_id);
    if (it == message_buffer.end()) {
        return false;
    } else {
        memcpy_s(buffer, buffer_size, it->second->data(), it->second->size());
        return true;
    }
}
