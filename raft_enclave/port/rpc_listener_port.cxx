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

static mutex message_buffer_lock;
static atomic<uint32_t> id_counter;
static map<uint32_t, bufptr> message_buffer;

int32_t ecall_handle_rpc_request(uint32_t size, const uint8_t *message, uint32_t *msg_id) {
    // FIXME: Encrypt & Decrypt
    bufptr header = buffer::alloc(RPC_REQ_HEADER_SIZE);
    memcpy_s(header->data(), header->size(), message, RPC_REQ_HEADER_SIZE);

    header->pos(RPC_REQ_HEADER_SIZE - 4);
    int32 data_size = header->get_int();

    bufptr log_data = buffer::alloc(data_size);
    memcpy_s(log_data->data(), log_data->size(), message + RPC_REQ_HEADER_SIZE, data_size);

    try {
        header->pos(0);
        auto t = (msg_type) header->get_byte();
        int32 src = header->get_int();
        int32 dst = header->get_int();
        ulong term = header->get_ulong();
        ulong last_term = header->get_ulong();
        ulong last_idx = header->get_ulong();
        ulong commit_idx = header->get_ulong();

        ptr<req_msg> req(cs_new<req_msg>(term, t, src, dst, last_term, last_idx, commit_idx));

        if (header->get_int() > 0 && log_data) {
            log_data->pos(0);
            while (log_data->size() > log_data->pos()) {
                ulong log_term = log_data->get_ulong();
                auto val_type = (log_val_type) log_data->get_byte();
                int32 val_size = log_data->get_int();
                bufptr buf(buffer::alloc((size_t) val_size));
                log_data->get(buf);

                ptr<log_entry> entry(cs_new<log_entry>(log_term, std::move(buf), val_type));
                req->log_entries().push_back(entry);
            }
        }

        ptr<resp_msg> resp = rpc_listener_req_handler->process_req(*req);

        if (!resp) {
            rpc_listener_req_handler->get_logger()->err(
                    "no response is returned from raft message handler, potential system bug");
            *msg_id = 0;
            return 0;
        } else {
            bufptr resp_buf(buffer::alloc(RPC_RESP_HEADER_SIZE));
            resp_buf->put((byte) resp->get_type());
            resp_buf->put(resp->get_src());
            resp_buf->put(resp->get_dst());
            resp_buf->put(resp->get_term());
            resp_buf->put(resp->get_next_idx());
            resp_buf->put((byte) resp->get_accepted());
            resp_buf->pos(0);

            {
                lock_guard<mutex> lock(message_buffer_lock);
                *msg_id = ++id_counter;
                message_buffer[*msg_id] = std::move(resp_buf);
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
    {
        lock_guard<mutex> lock(message_buffer_lock);
        auto it = message_buffer.find(msg_id);
        if (it == message_buffer.end()) {
            return false;
        } else {
            memcpy_s(buffer, buffer_size, it->second->data(), it->second->size());
            return true;
        }
    }
}
