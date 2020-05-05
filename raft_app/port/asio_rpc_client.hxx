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
#include "asio_log_utils.hxx"
#include <functional>

#include <asio.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

using std::function;

using tcp_resolver = asio::ip::tcp::resolver;

extern sgx_enclave_id_t global_enclave_id;
extern ptr<spdlog::logger> global_logger;

uint8_t dummy;

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
        req_map_[request_uid] = make_shared<bytes>(message, message + size);

        if (!socket_.is_open()) {
            query(request_uid);
        } else {
            send_message(request_uid);
        }
    }

private:
    void handle_error(uint32_t request_uid, asio::error_code err, const string &detail) {
        string e = fmt::format("[client #{}] {} due to error [{}] {}", client_uid_, detail, err.value(), err.message());
        ecall_rpc_response(global_enclave_id, request_uid, 0, &dummy, e.c_str());
        if (socket_.is_open()) {
            socket_.close();
        }
    }

    void query(uint32_t request_uid) {
        auto self = shared_from_this();
        resolver_.async_resolve(tcp_resolver::query(host_, port_, tcp_resolver::query::all_matching),
                                [self, request_uid](const asio::error_code &error,
                                                    const tcp_resolver::iterator &iterator) {
                                    if (!error) {
                                        self->connect(request_uid, iterator);
                                    } else {
                                        self->handle_error(request_uid, error, "failed to resolve host");
                                    }
                                });
    }

    void connect(uint32_t request_uid, const tcp_resolver::iterator &iter) {
        auto self = shared_from_this();
        asio::async_connect(socket_, iter,
                            [self, request_uid](const std::error_code &error, const tcp_resolver::iterator &iter) {
                                if (!error) {
                                    self->send_message(request_uid);
                                } else {
                                    self->handle_error(request_uid, error, "failed to connect to remote socket");
                                }
                            });
    }

    void send_message(uint32_t request_uid) {
        ptr<bytes> req_buf = req_map_[request_uid];

        // this part is for log
        global_logger->trace("{} {} {}: client={} request={} local={} size={} send {}",
                             __FILE__, __FUNCTION__, __LINE__,
                             client_uid_, request_uid, socket_local_address(socket_), req_buf->size(),
                             spdlog::to_hex(*req_buf));
        // this part is for log



        asio::error_code err;
        int32_t size = req_buf->size();
        asio::write(socket_, asio::buffer(&size, sizeof(int32_t)), err);
        if (err) {
            handle_error(request_uid, err, "failed to send request to remote socket");
            return;
        }

        asio::async_write(socket_, asio::buffer(*req_buf),
                          std::bind(&asio_rpc_client::request_sent, shared_from_this(),
                                    request_uid,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
    }

    void request_sent(uint32_t request_uid, asio::error_code err, size_t bytes_transferred) {
        global_logger->trace("{} {} {}: client={}, TRACE", __FILE__, __FUNCTION__, __LINE__, client_uid_);

        if (!err) {
            recv_message(request_uid);
        } else {
            handle_error(request_uid, err, "failed to send request to remote socket");
            socket_.close();
        }
    }

    void recv_message(uint32_t request_uid) {
        // read a response
        int32_t data_size = 0;
        size_t bytes_read = 0;
        asio::error_code err;
        do {
            bytes_read = asio::read(socket_, asio::buffer(&data_size, sizeof(int32_t)), err);
        } while (err == asio::error::eof);

        if (err) {
            handle_error(request_uid, err, "failed to read response size from remote socket");
            return;
        }

        global_logger->trace("{} {} {}: client={}, response={}, header_size={}, data_size={} TRACE", __FILE__,
                             __FUNCTION__, __LINE__,
                             client_uid_, request_uid, bytes_read, data_size);

        resp_map_[request_uid] = make_shared<bytes>(data_size, 0);

        asio::async_read(socket_, asio::buffer(*resp_map_[request_uid]),
                         std::bind(&asio_rpc_client::response_read, shared_from_this(),
                                   request_uid,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
    }

    void response_read(uint32_t req_uid, asio::error_code err, size_t bytes_read) {
        auto resp_buffer = resp_map_[req_uid];

        global_logger->trace("{} {} {}: client={}, response={}, buffer_size={}, bytes_read={}, TRACE", __FILE__,
                             __FUNCTION__, __LINE__,
                             client_uid_, req_uid, resp_buffer->size(), bytes_read);

        if (!err) {
            auto remote_addr = fmt::format("{}:{}",
                                           socket_.remote_endpoint().address().to_string(),
                                           socket_.remote_endpoint().port());
            auto local_addr = fmt::format("{}:{}",
                                          socket_.local_endpoint().address().to_string(),
                                          socket_.local_endpoint().port());

            global_logger->trace("{} {} {}: client={}, response={}, resp_size {}", __FILE__, __FUNCTION__, __LINE__,
                                 client_uid_, req_uid, resp_buffer->size());
            global_logger->trace("{} {} {}: {} <- {} client={}, response={}. read {}", __FILE__, __FUNCTION__, __LINE__,
                                 local_addr, remote_addr, client_uid_, req_uid,
                                 spdlog::to_hex(resp_buffer->data(), resp_buffer->data() + bytes_read));

            ecall_rpc_response(global_enclave_id, req_uid, resp_buffer->size(), resp_buffer->data(), nullptr);
        } else {
            handle_error(req_uid, err, "failed to read response from remote socket");
        }

        resp_map_.erase(req_uid);
    }

private:
    tcp_resolver resolver_;
    asio::ip::tcp::socket socket_;
    string host_;
    string port_;
    uint32_t client_uid_;
    map<uint32_t, ptr<bytes>> req_map_;
    map<uint32_t, ptr<bytes>> resp_map_;
};


#endif //ENCLAVERAFT_ASIO_RPC_CLIENT_HXX
