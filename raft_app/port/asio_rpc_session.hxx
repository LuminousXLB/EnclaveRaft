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


#ifndef ENCLAVERAFT_ASIO_RPC_SESSION_HXX
#define ENCLAVERAFT_ASIO_RPC_SESSION_HXX

#include "common.hxx"
#include <utility>
#include <functional>
#include <spdlog/spdlog.h>
#include <asio.hpp>
#include <spdlog/fmt/bin_to_hex.h>

using std::mutex;
using std::function;
using std::enable_shared_from_this;

#define __SLEEP__ std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(10))

// rpc session
class asio_rpc_session;

extern ptr<spdlog::logger> global_logger;

typedef function<void(const ptr<asio_rpc_session> &)> session_closed_callback;
typedef function<ptr<bytes>(const bytes &)> request_handler;

class asio_rpc_session : public enable_shared_from_this<asio_rpc_session> {
public:
    template<typename SessionCloseCallback>
    asio_rpc_session(ptr<asio::io_context> &io, request_handler handler, SessionCloseCallback &&callback)
            : handler_(std::move(handler)),
              socket_(*io),
              callback_(std::forward<SessionCloseCallback>(callback)),
              payload_size_(0) {}

    ~asio_rpc_session() {
        if (socket_.is_open()) {
            socket_.close();
        }
    }

    string local_address() {
        return fmt::format("{}:{}", socket_.local_endpoint().address().to_string(), socket_.local_endpoint().port());
    }

    string remote_address() {
        return fmt::format("{}:{}", socket_.remote_endpoint().address().to_string(), socket_.remote_endpoint().port());
    }

    void start() {
        read_header();
    }

    void read_header() {
        auto self = shared_from_this();
        asio::async_read(socket_, request_, asio::transfer_exactly(sizeof(uint32_t)),
                         [self](const asio::error_code &error, size_t bytes_read) {
                             if (!error) {
                                 std::istream in(&self->request_);
                                 in.read(reinterpret_cast<char *>(&self->payload_size_), sizeof(uint32_t));
                                 self->read_payload();
                             } else {
                                 global_logger->error("socket session (R {}, L {}) read error: {}",
                                                      self->remote_address(), self->local_address(), error.message());
                                 self->stop();
                             }
                         });
    }

    void read_payload() {
        message_buffer_.clear();
        message_buffer_.resize((size_t) payload_size_, 0);
        global_logger->trace("{} {} {}: data_size = {}", __FILE__, __FUNCTION__, __LINE__, payload_size_);

        asio::async_read(this->socket_, asio::buffer(message_buffer_),
                         std::bind(&asio_rpc_session::read_log_data, shared_from_this(),
                                   std::placeholders::_1, std::placeholders::_2));
    }

    void stop() {
        global_logger->trace("{} {} {}: TRACE", __FILE__, __FUNCTION__, __LINE__);
        if (socket_.is_open()) {
            socket_.close();
        }
        if (callback_) {
            callback_(this->shared_from_this());
        }
    }

    asio::ip::tcp::socket &socket() {
        return socket_;
    }

private:
    void read_log_data(const asio::error_code &err, size_t bytes_read) {
        if (!err) {
            this->read_complete();
        } else {
            global_logger->error("failed to read rpc log data from socket due to error [{}]{}", err.value(),
                                 err.message());
            this->stop();
        }
    }

    void read_complete() {
        ptr<asio_rpc_session> self = this->shared_from_this();
        global_logger->trace("{} {}: {} <- {}  read {}", __FILE__, __LINE__,
                             local_address(), remote_address(), spdlog::to_hex(message_buffer_));

        try {
            auto resp_buf = handler_(message_buffer_);
            uint32_t length = resp_buf->size();

            if (resp_buf) {
                global_logger->trace("{} {} {}: resp_size {}", __FILE__, __FUNCTION__, __LINE__,
                                     resp_buf->size());
                global_logger->trace("{} {} {}: {} -> {} send {}", __FILE__, __FUNCTION__, __LINE__,
                                     local_address(), remote_address(), spdlog::to_hex(*resp_buf));

                asio::write(socket_, asio::buffer(&length, sizeof(length)));
                asio::async_write(
                        socket_,
                        asio::buffer(*resp_buf),
                        [this, self](asio::error_code err, size_t) -> void {
                            if (!err) {
                                payload_size_ = 0;
                                start();
                            } else {
                                global_logger->error("failed to send response to peer due to error {}", err.value());
                                stop();
                            }
                        }
                );
            } else {
                global_logger->error("no response is returned from raft message handler, potential system bug");
                this->stop();
            }
        } catch (std::exception &ex) {
            global_logger->error("failed to process request message due to error: {}", ex.what());
            this->stop();
        }
    }

private:
    request_handler handler_;
    asio::ip::tcp::socket socket_;
    asio::streambuf request_;
    uint32_t payload_size_;
    bytes message_buffer_;
    session_closed_callback callback_;
};

#endif //ENCLAVERAFT_ASIO_RPC_SESSION_HXX
