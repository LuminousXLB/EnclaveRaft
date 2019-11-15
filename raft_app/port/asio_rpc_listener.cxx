//
// Created by ncl on 29/10/19.
//


#include "asio_rpc_listener.hxx"
#include "raft_enclave_u.h"

extern ptr<asio::io_context> global_io_context;

extern ptr<asio_rpc_listener> global_rpc_listener;

//void ocall_rpc_listener_create(uint16_t port) {
//    global_logger->trace("{} {} {}: {}", __FILE__, __FUNCTION__, __LINE__, port);
//    global_rpc_listener = make_shared<asio_rpc_listener>(global_io_context, port);
//    global_logger->trace("{} {} {}: {}", __FILE__, __FUNCTION__, __LINE__, port);
//    listener->listen();
//}

//void ocall_rpc_listener_stop() {
//    global_logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);
//    global_rpc_listener->stop();
//}


ptr<bytes> message_handler(const bytes &message) {
    spdlog::trace("{} {} {}: {}", __FILE__, __FUNCTION__, __LINE__, message.size());

    uint32_t uid;
    int32_t resp_len;
    ecall_handle_rpc_request(global_enclave_id, &resp_len, message.size(), message.data(), &uid);

    if (resp_len < 1) {
        return nullptr;
    }

    auto buffer = make_shared<bytes>(resp_len, 0);
    bool ret;
    ecall_fetch_rpc_response(global_enclave_id, &ret, uid, buffer->size(), &(*buffer)[0]);

    if (ret) {
        return buffer;
    } else {
        return nullptr;
    }
}