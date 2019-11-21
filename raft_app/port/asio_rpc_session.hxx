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

#include "asio_log_utils.hxx"

using std::mutex;
using std::function;
using std::enable_shared_from_this;

#define __SLEEP__ std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(10))

// rpc session
class asio_rpc_session;


typedef function<void(uint32_t)> session_closed_callback;
typedef function<ptr<bytes>(const bytes &)> request_handler;
static constexpr uint32_t request_header_size = sizeof(uint32_t);

extern ptr<spdlog::logger> global_logger;

class asio_rpc_session : public enable_shared_from_this<asio_rpc_session> {
public:
    template<typename SessionCloseCallback>
    asio_rpc_session(uint32_t sid, ptr<asio::io_context> &io, request_handler handler, SessionCloseCallback &&callback)
            :session_id_(sid),
             handler_(std::move(handler)),
             socket_(*io),
             callback_(std::forward<SessionCloseCallback>(callback)),
             payload_size_(0) {}

    ~asio_rpc_session() {
        if (socket_.is_open()) {
            socket_.close();
        }
    }

    void start() {
        read_header();
    }

    void stop() {
        if (socket_.is_open()) {
            socket_.close();
        }

        global_logger->info("socket session {} stopped", session_id_);

        if (callback_) {
            callback_(session_id_);
        }
    }

    asio::ip::tcp::socket &socket() {
        return socket_;
    }

private:
    string local_address() {
        return socket_local_address(socket_);
    }

    string remote_address() {
        return socket_remote_address(socket_);
    }

    void handle_error(const string &description, const asio::error_code &error) {
        global_logger->error("socket session [{}] (R {}, L {}) {} error: {}",
                             session_id_, remote_address(), local_address(), description,
                             error.message());
        stop();
    }

    void read_header() {
        global_logger->trace("socket session {}: {}", session_id_, __FUNCTION__);

        auto self = shared_from_this();
        asio::async_read_until(socket_, request_,
                               asio::transfer_at_least(request_header_size - request_.size()),
                               [self](const asio::error_code &error, size_t bytes_read) {
                                   if (!error) {
                                       self->read_payload();
                                   } else {
                                       self->handle_error("read header", error);
                                   }
                               });
    }

    void read_payload() {
        global_logger->trace("socket session {}: {} payload_size={}, read={}", session_id_, __FUNCTION__, payload_size_,
                             request_.size());


        const uint32_t *msg_size = static_cast<const uint32_t *>(request_.data().data());
        if (request_.size() < *msg_size) {
            auto self = shared_from_this();
            asio::async_read_until(socket_, request_,
                                   asio::transfer_at_least(1),
                                   [self](const std::error_code &error, size_t bytes_transferred) {
                                       if (!error || error == asio::error::eof) {
                                           self->read_payload();
                                       } else {
                                           self->handle_error("read payload", error);
                                       }
                                   });
        } else {
            handle_request();
        }
    }

    void handle_request() {
        global_logger->trace("socket session {}: {}", session_id_, __FUNCTION__);

        try {
            bytes req_buf(payload_size_, 0);
            std::istream in(&request_);
            in.read(reinterpret_cast<char *>(&req_buf[0]), payload_size_);

            global_logger->trace("{} {}: {} <- {}  read {}", __FILE__, __LINE__,
                                 local_address(), remote_address(), spdlog::to_hex(req_buf));

            auto resp_buf = handler_(req_buf);
            if (resp_buf) {
                write_response(resp_buf);
            } else {
                global_logger->critical(
                        "socket session {}: no response is returned from raft message handler, potential system bug",
                        session_id_);
                this->stop();
            }
        } catch (std::exception &ex) {
            global_logger->error("socket session {}: failed to process request message due to error {}",
                                 session_id_, ex.what());
            this->stop();
        }
    }

    void write_response(const ptr<bytes> &resp_buf) {
        global_logger->trace("socket session {}: {} payload_size={}, payload={}", session_id_, __FUNCTION__,
                             resp_buf->size(), spdlog::to_hex(*resp_buf));

        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(*resp_buf),
                          [self](const asio::error_code &error, size_t bytes_written) {
                              if (!error) {
                                  self->payload_size_ = 0;

                                  if (self->request_.size() >= request_header_size) {
                                      self->read_payload();
                                  } else {
                                      self->start();
                                  }
                              } else {
                                  self->handle_error("write response", error);
                              }
                          });
    }

private:
    uint32_t session_id_;

    asio::ip::tcp::socket socket_;
    uint32_t payload_size_;
    asio::streambuf request_;

    request_handler handler_;
    session_closed_callback callback_;
};

#endif //ENCLAVERAFT_ASIO_RPC_SESSION_HXX
