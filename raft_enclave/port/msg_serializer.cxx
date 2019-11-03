//
// Created by ncl on 3/11/19.
//

#include "msg_serializer.hxx"


using cornerstone::buffer;
using cornerstone::msg_type;
using cornerstone::cs_new;
using cornerstone::log_entry;
using cornerstone::log_val_type;
using cornerstone::lstrfmt;

bufptr serialize_req(const ptr<req_msg> &req) {
    // serialize req, send and read response
    std::vector<bufptr> log_entry_bufs;
    int32 log_data_size(0);
    for (auto &it : req->log_entries()) {
        bufptr entry_buf(buffer::alloc(8 + 1 + 4 + it->get_buf().size()));
        entry_buf->put(it->get_term());
        entry_buf->put((byte) (it->get_val_type()));
        entry_buf->put((int32) it->get_buf().size());
        it->get_buf().pos(0);
        entry_buf->put(it->get_buf());
        entry_buf->pos(0);
        log_data_size += (int32) entry_buf->size();
        log_entry_bufs.emplace_back(std::move(entry_buf));
    }

    bufptr req_buf(buffer::alloc(RPC_REQ_HEADER_SIZE + log_data_size));
    req_buf->put((byte) req->get_type());
    req_buf->put(req->get_src());
    req_buf->put(req->get_dst());
    req_buf->put(req->get_term());
    req_buf->put(req->get_last_log_term());
    req_buf->put(req->get_last_log_idx());
    req_buf->put(req->get_commit_idx());
    req_buf->put(log_data_size);
    for (auto &item : log_entry_bufs) {
        req_buf->put(*item);
    }

    req_buf->pos(0);

    return req_buf;
}

ptr<req_msg> deserialize_req(bufptr &header, bufptr &log_data) {
    header->pos(0);

    auto t = (msg_type) header->get_byte();
    int32 src = header->get_int();
    int32 dst = header->get_int();
    ulong term = header->get_ulong();
    ulong last_term = header->get_ulong();
    ulong last_idx = header->get_ulong();
    ulong commit_idx = header->get_ulong();


    ptr<req_msg> req = cs_new<req_msg>(term, t, src, dst, last_term, last_idx, commit_idx);

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

    return req;
}

bufptr serialize_resp(const ptr<resp_msg> &resp) {
    bufptr resp_buf(buffer::alloc(RPC_RESP_HEADER_SIZE));

    resp_buf->put((byte) resp->get_type());
    resp_buf->put(resp->get_src());
    resp_buf->put(resp->get_dst());
    resp_buf->put(resp->get_term());
    resp_buf->put(resp->get_next_idx());
    resp_buf->put((byte) resp->get_accepted());
    resp_buf->pos(0);

    return resp_buf;
}

ptr<resp_msg> deserialize_resp(bufptr &resp_buf) {
    resp_buf->pos(0);
    byte msg_type_val = resp_buf->get_byte();
    int32 src = resp_buf->get_int();
    int32 dst = resp_buf->get_int();
    ulong term = resp_buf->get_ulong();
    ulong nxt_idx = resp_buf->get_ulong();
    byte accepted_val = resp_buf->get_byte();

    return cs_new<resp_msg>(term, (msg_type) msg_type_val, src, dst, nxt_idx, accepted_val == 1);
}