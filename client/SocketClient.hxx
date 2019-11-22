//
// Created by ncl on 19/11/19.
//

#ifndef ENCLAVERAFT_SOCKETCLIENT_HXX
#define ENCLAVERAFT_SOCKETCLIENT_HXX

#include "client.hxx"

extern std::shared_ptr<spdlog::logger> logger;


class SocketClient : public std::enable_shared_from_this<SocketClient> {
public :
    enum status_t {
        CONNECTING,
        AVAILABLE,
        BUSY,
        ERROR
    };

    SocketClient(const ptr<asio::io_context> &ctx_ptr, uint16_t port)
            : ctx_(ctx_ptr),
              socket_(*ctx_ptr) {

        asio::error_code err;
        auto resolve_result = tcp::resolver(*ctx_ptr).resolve("127.0.0.1", std::to_string(port));
        asio::connect(socket_, resolve_result, err);

        if (err) {
            handle_error("connect", err);
        } else {
            status_ = AVAILABLE;
        }
    }

    ~SocketClient() {
        if (socket_.is_open()) {
            socket_.close();
        }
    }

    bool is_open() {
        return socket_.is_open();
    }

    status_t status() {
        return status_;
    }

    void send(const ptr<bytes> &request) {
        logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);

        if (status_ != AVAILABLE) {
            throw std::runtime_error("Using a socket client not available");
        }

        status_ = BUSY;

        try {
            logger->debug("Socket Local {} -> Remote {}", local_address(), remote_address());

            /* Sending message */
            asio::error_code err;

            size_ = request->size();
            std::ostream out(&req_sbuf);
            out.write(reinterpret_cast<const char *>(&size_), sizeof(uint32_t));
            out.write(reinterpret_cast<const char *>(request->data()), request->size());

            logger->debug("Write: {}", spdlog::to_hex(*request));

            auto self = shared_from_this();

            logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);

            asio::async_write(socket_, req_sbuf, [self](const asio::error_code &err, size_t bytes) {

                logger->trace("{} {} {}", __FILE__, "async_write callback", __LINE__);
                if (err) {
                    logger->error("Error when writing message: {}", err.message());
                    self->handle_error("writing message", err);
                } else {
                    logger->debug("Message sent, reading response");
                    self->receive_header();
                }
            });
        } catch (std::system_error &err) {
            logger->critical("SYSTEM ERROR HERE {}{}", __LINE__, err.what());
//            throw;
        }
    }

    string local_address() {
        asio::error_code error;
        auto endpoint = socket_.local_endpoint(error);

        if (!error) {
            return fmt::format("{}:{}", endpoint.address().to_string(), endpoint.port());
        } else {
            return error.message();
        }
    }

    string remote_address() {
        asio::error_code error;
        auto endpoint = socket_.remote_endpoint(error);

        if (!error) {
            return fmt::format("{}:{}", endpoint.address().to_string(), endpoint.port());
        } else {
            return error.message();
        }
    }


private:
    bool handle_error(const std::string &when, const asio::error_code &err) {
        if (!err) {
            throw std::invalid_argument(__FUNCTION__);
        }

        if (err == asio::error::eof) {
            logger->warn("Error when {}: {}", when, err.message());
            return true;
        } else {
            logger->warn("Error when {}: {}", when, err.message());
            if (socket_.is_open()) {
                socket_.close();
            }
            status_ = ERROR;
            return false;
        }
    }

    void receive_header() {
        logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);

        try {
            /* Reading message */
            asio::error_code err;

            auto self = shared_from_this();

            logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);

            asio::async_read(socket_, resp_sbuf, asio::transfer_at_least(sizeof(uint32_t)),
                             [self](const asio::error_code &err, size_t bytes) {

                                 logger->trace("{} {} {}", __FILE__, "receive_header callback", __LINE__);

                                 std::istream in(&self->resp_sbuf);

                                 if (err && self->handle_error("reading header", err)) {
                                     self->receive_header();
                                 } else {
                                     in.read(reinterpret_cast<char *>(&self->size_), sizeof(uint32_t));
                                     self->receive_body();
                                 }
                             });
        } catch (std::system_error &err) {
            logger->critical("SYSTEM ERROR HERE {}{}", __LINE__, err.what());
            throw;
        }
    }

    void receive_body() {
        logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);

        auto self = shared_from_this();
        if (resp_sbuf.size() < size_) {
            asio::async_read(socket_, resp_sbuf, asio::transfer_at_least(1),
                             [self](const asio::error_code &err, size_t bytes) {
                                 if (err && self->handle_error("reading message", err)) {
                                     self->receive_body();
                                 } else {
                                     self->receive_body();
                                 }
                             });
        } else {
            receive_done();
        }
    }

    void receive_done() {
        logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);

        try {
            std::istream in(&resp_sbuf);

            logger->debug("Read: {}", spdlog::to_hex((char *) resp_sbuf.data().data(),
                                                     (char *) resp_sbuf.data().data() + resp_sbuf.size()));

            auto ret = make_shared<bytes>(size_, 0);
            in.read(reinterpret_cast<char *> (&(*ret)[0]), size_);

            bool result = RequestBuilder::deserialize(*ret);

            logger->info("Result: {}", result);
        } catch (std::system_error &err) {
            logger->critical("SYSTEM ERROR HERE {}{}", __LINE__, err.what());
            throw;
        }

        status_ = AVAILABLE;
    }

    ptr<asio::io_context> ctx_;
    tcp::resolver::results_type resolve_result_;
    tcp::socket socket_;
    uint32_t size_ = 0;
    asio::streambuf req_sbuf;
    asio::streambuf resp_sbuf;
    status_t status_ = CONNECTING;
    ptr<bytes> waiting_req_ = nullptr;
};

#endif //ENCLAVERAFT_SOCKETCLIENT_HXX
