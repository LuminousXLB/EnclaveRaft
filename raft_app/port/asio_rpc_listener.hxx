/**
*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  The ASF licenses
* this file to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#ifndef ENCLAVERAFT_ASIO_RPC_LISTENER_HXX
#define ENCLAVERAFT_ASIO_RPC_LISTENER_HXX

#include "common.hxx"
#include <mutex>
#include <utility>
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include "raft_enclave_u.h"
#include "asio_rpc_session.hxx"

using std::mutex;
using std::lock_guard;
using std::make_shared;
using std::enable_shared_from_this;

extern sgx_enclave_id_t global_enclave_id;


ptr<bytes> message_handler(const bytes &message) {
    spdlog::trace("{} {} {}: {}", __FILE__, __FUNCTION__, __LINE__, message.size());

    uint32_t uid;
    int32_t resp_len;
    ecall_handle_rpc_request(global_enclave_id, &resp_len, message.size(), message.data(), &uid);

    if (resp_len == 0) {
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

// rpc listener implementation
class asio_rpc_listener : public enable_shared_from_this<asio_rpc_listener> {
public:
    asio_rpc_listener(asio::io_service &io, uint16_t port, ptr<logger> p_logger)
            : io_svc_(io), acceptor_(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
              active_sessions_(), session_lock_(), logger_(std::move(p_logger)) {}

public:
    void stop() {
        acceptor_.close();
    }

    void listen() {
        auto local = acceptor_.local_endpoint();
        logger_->info("listening at {}:{}", local.address().to_string(), local.port());

        start();
    }

private:
    void start() {
        if (!acceptor_.is_open()) {
            return;
        }

        auto self = shared_from_this();
        auto session = make_shared<asio_rpc_session>(io_svc_,
                                                     &message_handler,
                                                     logger_,
                                                     std::bind(&asio_rpc_listener::remove_session,
                                                               self,
                                                               std::placeholders::_1));

        acceptor_.async_accept(session->socket(), [self, this, session](const asio::error_code &err) -> void {
            if (!err) {
                this->logger_->debug("receive a incoming rpc connection");
                session->start();
            } else {
                this->logger_->debug("fails to accept a rpc connection due to error {}", err.value());
            }

            this->start();
        });
    }

    void remove_session(const ptr<asio_rpc_session> &session) {
        lock_guard<mutex> lock(session_lock_);

        for (auto it = active_sessions_.begin(); it != active_sessions_.end(); ++it) {
            if (*it == session) {
                active_sessions_.erase(it);
                break;
            }
        }
    }

private:
    asio::io_service &io_svc_;
    asio::ip::tcp::acceptor acceptor_;

    mutex session_lock_;
    vector<ptr<asio_rpc_session>> active_sessions_;

    ptr<logger> logger_;
};


#endif //ENCLAVERAFT_ASIO_RPC_LISTENER_HXX
