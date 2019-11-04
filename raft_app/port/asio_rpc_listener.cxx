//
// Created by ncl on 29/10/19.
//


#include "asio_rpc_listener.hxx"
#include "raft_enclave_u.h"

extern shared_ptr<asio::io_context> global_io_context;
extern shared_ptr<spdlog::logger> global_logger;

static shared_ptr<asio_rpc_listener> listener;

void ocall_rpc_listener_create(uint16_t port) {
    spdlog::trace("{} {} {}: {}", __FILE__, __FUNCTION__, __LINE__, port);

    listener = make_shared<asio_rpc_listener>(*global_io_context, port, global_logger);
    listener->listen();
}

void ocall_rpc_listener_stop() {
    spdlog::trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);
    listener->stop();
}
