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


#ifndef ENCLAVERAFT_RPC_SESSION_HXX
#define ENCLAVERAFT_RPC_SESSION_HXX

#include <spdlog/spdlog.h>
#include <asio.hpp>
#include <memory>
#include <utility>
#include <spdlog/fmt/bin_to_hex.h>

using std::shared_ptr;
using std::mutex;
using std::vector;
using std::make_shared;
using std::enable_shared_from_this;
using std::function;

using spdlog::logger;


// rpc session
class rpc_session;

typedef function<void(const shared_ptr<rpc_session> &)> session_closed_callback;
typedef function<shared_ptr<vector<uint8_t >>(const vector<uint8_t> &)> msg_handler;

class rpc_session : public enable_shared_from_this<rpc_session> {
public:
    template<typename SessionCloseCallback>
    rpc_session(asio::io_service &io, msg_handler handler, shared_ptr<logger> &logger,
                SessionCloseCallback &&callback)
            : handler_(std::move(handler)),
              socket_(io),
              logger_(logger),
              callback_(std::forward<SessionCloseCallback>(callback)) {}


public:
    ~rpc_session() {
        if (socket_.is_open()) {
            socket_.close();
        }
    }

public:
    void start() {
        auto remote = socket_.remote_endpoint();
        auto local = socket_.local_endpoint();

        logger_->info("connect to {}:{} from {}:{}",
                      remote.address().to_string(), remote.port(), local.address().to_string(), local.port());

        shared_ptr<rpc_session> self = shared_from_this(); // this is safe since we only expose ctor to cs_new

        asio::read(socket_, asio::buffer(&data_size, sizeof(uint32_t)));
        if (data_size < 0 || data_size > 0x1000000) {
            logger_->warn(
                    "bad log data size in the header {}, stop this session to protect further corruption",
                    data_size);
            this->stop();
            return;
        }

        if (data_size == 0) {
            this->read_complete();
            return;
        }

        message_buffer.resize((size_t) data_size);
        logger_->debug("{}: data_size = {}", __FILE__, data_size);

        asio::async_read(this->socket_,
                         asio::buffer(message_buffer),
                         std::bind(&rpc_session::read_log_data, self, std::placeholders::_1, std::placeholders::_2));
    }

    void stop() {
        socket_.close();
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
            logger_->error("failed to read rpc log data from socket due to error {}", err.value());
            this->stop();
        }
    }

    void read_complete() {
        shared_ptr<rpc_session> self = this->shared_from_this();

        logger_->debug("{}: req read = {}", __FILE__, spdlog::to_hex(message_buffer));

        auto resp_buf = handler_(message_buffer);
        uint32_t length = resp_buf->size();

        if (resp_buf) {
            asio::write(socket_, asio::buffer(&length, sizeof(length)));
            asio::async_write(
                    socket_,
                    asio::buffer(*resp_buf),
                    [this, self](asio::error_code err, size_t) -> void {
                        if (!err) {
                            data_size = 0;
                            start();
                        } else {
                            logger_->error("failed to send response to peer due to error {}", err.value());
                            stop();
                        }
                    }
            );
        }
    }

private:
    msg_handler handler_;
    asio::ip::tcp::socket socket_;
    int32_t data_size{};
    vector<uint8_t> message_buffer;
    shared_ptr<logger> logger_;
    session_closed_callback callback_;
};

#endif //ENCLAVERAFT_RPC_SESSION_HXX
