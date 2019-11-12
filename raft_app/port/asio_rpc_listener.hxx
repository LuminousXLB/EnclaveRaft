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
using std::enable_shared_from_this;

extern sgx_enclave_id_t global_enclave_id;

ptr<bytes> message_handler(const bytes &message);

// rpc listener implementation
class asio_rpc_listener : public enable_shared_from_this<asio_rpc_listener> {
public:
    asio_rpc_listener(ptr<asio::io_context> &io, uint16_t port)
            : io_svc_(io), acceptor_(*io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
              active_sessions_(), session_lock_() {}

public:
    void stop() {
        acceptor_.close();
    }

    void listen() {
        auto local = acceptor_.local_endpoint();
        global_logger->info("listening at {}:{}", local.address().to_string(), local.port());

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
                                                     std::bind(&asio_rpc_listener::remove_session,
                                                               self,
                                                               std::placeholders::_1));

        acceptor_.async_accept(session->socket(), [self, this, session](const asio::error_code &err) -> void {
            if (!err) {
                global_logger->debug("receive a incoming rpc connection");
                session->start();
            } else {
                global_logger->debug("fails to accept a rpc connection due to error {}", err.value());
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
    ptr<asio::io_context> &io_svc_;
    asio::ip::tcp::acceptor acceptor_;

    mutex session_lock_;
    vector<ptr<asio_rpc_session>> active_sessions_;
};


#endif //ENCLAVERAFT_ASIO_RPC_LISTENER_HXX
