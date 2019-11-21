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

#ifndef ENCLAVERAFT_ASIO_RPC_CLIENT_HXX
#define ENCLAVERAFT_ASIO_RPC_CLIENT_HXX

#include "common.hxx"
#include <functional>
#include <asio.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <sgx_eid.h>
#include "asio_log_utils.hxx"
#include "raft_enclave_u.h"

using std::function;

using tcp_resolver = asio::ip::tcp::resolver;

extern sgx_enclave_id_t global_enclave_id;
extern ptr<spdlog::logger> global_logger;


class asio_rpc_client : public std::enable_shared_from_this<asio_rpc_client> {
public:
    asio_rpc_client(asio::io_service &io_svc, std::string &host, std::string &port, uint32_t client_uid)
            : resolver_(io_svc), socket_(io_svc), host_(host), port_(port), client_uid_(client_uid) {}

    ~asio_rpc_client() {
        if (socket_.is_open()) {
            socket_.close();
        }
    }


    void send(uint32_t request_uid, const uint8_t *message, int32_t size) {
        send(request_uid, make_shared<bytes>(message, message + size));
    }

    void send(uint32_t request_uid, const ptr<bytes> &message) {
        request_uid_ = request_uid;
        req_ = message;

        if (!socket_.is_open()) {
            query();
        } else {
            send_message();
        }
    }


private:
    void handle_error(uint32_t request_uid, asio::error_code err, const string &detail) {
        string e = fmt::format("[client #{}] {} due to error [{}] {}", client_uid_, detail, err.value(), err.message());
        ecall_rpc_response(global_enclave_id, request_uid, 0, nullptr, e.c_str());
        if (socket_.is_open()) {
            socket_.close();
        }
    }

    void query() {
        auto self = shared_from_this();
        resolver_.async_resolve(tcp_resolver::query(host_, port_, tcp_resolver::query::all_matching),
                                [self](const asio::error_code &error, const tcp_resolver::iterator &iterator) {
                                    if (!error) {
                                        self->connect(iterator);
                                    } else {
                                        self->handle_error(error, "failed to resolve host");
                                    }
                                });
    }

    void connect(const tcp_resolver::iterator &iter) {
        auto self = shared_from_this();
        asio::async_connect(socket_, iter,
                            [self](const std::error_code &error, const tcp_resolver::iterator &iter) {
                                if (!error) {
                                    self->send_message();
                                } else {
                                    self->handle_error(error, "failed to connect to remote socket");
                                }
                            });
    }

    void send_message() {
        global_logger->trace("{} {} {}: client={} request={} local={} size={} send {}",
                             __FILE__, __FUNCTION__, __LINE__,
                             client_uid_, request_uid_, socket_local_address(socket_), req_->size(),
                             spdlog::to_hex(*req_));

        auto self = shared_from_this();


//        std::bind(&asio_rpc_client::request_sent, shared_from_this(), std::placeholders::_1, std::placeholders::_2)
        asio::async_write(socket_, asio::buffer(*req_),
                          [self](const std::error_code &error, size_t bytes_transferred) {
                              if (error) {
                                  self->handle_error(self->request_uid_, error, "failed to send message");
                              } else {
                                  self->recv_header();
                              }
                          });
    }

    void recv_header() {
        auto self = shared_from_this();
        asio::async_read_until(socket_, resp_, asio::transfer_at_least(sizeof(uint32_t)),
                               [self](const std::error_code &error, size_t bytes_transferred) {
                                   if (error == asio::error::eof) {
                                       self->recv_header();
                                   } else if (error) {
                                       self->handle_error(self->request_uid_, error, "failed to receive header");
                                   } else {
                                       self->recv_message();
                                   }
                               });
    }

    void recv_message() {
        const uint32_t *msg_size = static_cast<const uint32_t *>(resp_.data().data());
        if (resp_.size() < *msg_size) {
            auto self = shared_from_this();
            asio::async_read_until(socket_, resp_, asio::transfer_at_least(1),
                                   [self](const std::error_code &error, size_t bytes_transferred) {
                                       if (!error || error == asio::error::eof) {
                                           self->recv_message();
                                       } else {
                                           self->handle_error(self->request_uid_, error, "failed to receive body");
                                       }
                                   });
        }
    }

    void response_read() {
        global_logger->trace("{} {} {}: client={}, response={}, resp_size {}", __FILE__, __FUNCTION__, __LINE__,
                             client_uid_, request_uid_, resp_.size());
        global_logger->trace("{} {} {}: {} <- {} client={}, response={}. read {}", __FILE__, __FUNCTION__, __LINE__,
                             socket_local_address(socket_), socket_remote_address(socket_), client_uid_, request_uid_,
                             spdlog::to_hex(reinterpret_cast<const uint8_t *>(resp_.data().data()),
                                            reinterpret_cast<const uint8_t *>(resp_.data().data()) + resp_.size()));

        ecall_rpc_response(global_enclave_id, request_uid_, resp_.size(),
                           reinterpret_cast<const uint8_t *>(resp_.data().data()), nullptr);
    }

private:
    tcp_resolver resolver_;
    asio::ip::tcp::socket socket_;
    std::string host_;
    std::string port_;
    uint32_t client_uid_;
    uint32_t request_uid_{};
    ptr<bytes> req_;
    asio::streambuf resp_;
public:
    mutable std::mutex mutex_;
};


#endif //ENCLAVERAFT_ASIO_RPC_CLIENT_HXX
