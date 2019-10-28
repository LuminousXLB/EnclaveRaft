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

#include <asio.hpp>
#include <spdlog/fmt/fmt.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

using std::vector;
using std::string;
using std::function;
using std::shared_ptr;
using std::make_shared;

using tcp_resolver = asio::ip::tcp::resolver;

using rpc_handler = function<void(char, char)>;


class asio_rpc_client : public std::enable_shared_from_this<asio_rpc_client> {
public:
    asio_rpc_client(asio::io_service &io_svc, std::string &host, std::string &port)
            : resolver_(io_svc), socket_(io_svc), host_(host), port_(port) {}

    ~asio_rpc_client() {
        if (socket_.is_open()) {
            socket_.close();
        }
    }

public:
    void send(uint32_t request_uid, const uint8_t *message, int32_t size) {
        shared_ptr<asio_rpc_client> self = shared_from_this();

        if (!socket_.is_open()) {
            tcp_resolver::query q(host_, port_, tcp_resolver::query::all_matching);
            resolver_.async_resolve(
                    q,
                    [self, this, request_uid, message, size](asio::error_code err,
                                                             tcp_resolver::iterator iter) -> void {
                        if (!err) {
                            asio::async_connect(socket_, std::move(iter),
                                                std::bind(&asio_rpc_client::connected, self,
                                                          request_uid,
                                                          message,
                                                          size,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2));
                        } else {
                            string e = fmt::format("failed to resolve host {} due to error {}", host_, err.value());
                            ecall_rpc_response(request_uid, 0, nullptr, e.c_str());
                        }
                    }
            );
        } else {
            asio::write(socket_, asio::buffer(&size, sizeof(int32_t)));
            asio::async_write(
                    socket_,
                    asio::buffer(message, size),
                    [self, request_uid](std::error_code err, size_t bytes_transferred) mutable -> void {
                        self->sent(request_uid, err, bytes_transferred);
                    }
            );
        }
    }

private:
    void connected(uint32_t request_uid, const uint8_t *message, uint32_t size, std::error_code err,
                   const tcp_resolver::iterator &iter) {
        if (!err) {
            this->send(request_uid, message, size);
        } else {
            string e = fmt::format("failed to connect to remote socket due to error %d", err.value());
            ecall_rpc_response(request_uid, 0, nullptr, e.c_str());
        }
    }

    void sent(uint32_t request_uid, asio::error_code err, size_t bytes_transferred) {
        if (!err) {
            // read a response

            int32_t data_size;
            asio::read(socket_, asio::buffer(&data_size, sizeof(uint32_t)));

            vector<uint8_t> message_buffer((size_t) data_size);

            asio::async_read(this->socket_,
                             asio::buffer(message_buffer),
                             std::bind(&asio_rpc_client::response_read, this, request_uid, message_buffer,
                                       std::placeholders::_1, std::placeholders::_2)
            );
        } else {
            string e = fmt::format("failed to send request to remote socket due to error %d", err.value());
            ecall_rpc_response(request_uid, 0, nullptr, e.c_str());
            socket_.close();
        }
    }

    void response_read(uint32_t req_uid, const vector<uint8_t> &resp_buffer, asio::error_code err, size_t bytes_read) {
        if (!err) {
            ecall_rpc_response(req_uid, resp_buffer.size(), resp_buffer.data(), nullptr);
        } else {
            string e = fmt::format("failed to read response from remote socket due to error %d", err.value());
            ecall_rpc_response(req_uid, 0, nullptr, e.c_str());
            socket_.close();
        }
    }

private:
    tcp_resolver resolver_;
    asio::ip::tcp::socket socket_;
    std::string host_;
    std::string port_;
};

}


#endif //ENCLAVERAFT_ASIO_RPC_CLIENT_HXX
