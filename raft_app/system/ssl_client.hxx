//
// Created by ncl on 12/11/19.
//

#ifndef ENCLAVERAFT_SSLCLIENT_HXX
#define ENCLAVERAFT_SSLCLIENT_HXX

#include <regex>
#include <iostream>
#include <string>
#include <spdlog/logger.h>
#include "asio.hpp"
#include "asio/ssl.hpp"

using tcp_resolver = asio::ip::tcp::resolver;
using std::string;
using std::map;
using std::cerr;
using std::endl;
using std::shared_ptr;

extern shared_ptr<spdlog::logger> global_logger;


class ssl_client {
public:
    ssl_client(asio::io_context &io_context, asio::ssl::context &ssl_context, const string &host,
               const string &protocol)
            : endpoints_(tcp_resolver(io_context).resolve(host, protocol)),
              socket_(io_context, ssl_context) {

        socket_.set_verify_mode(asio::ssl::verify_peer);

        socket_.set_verify_callback(std::bind(&ssl_client::verify_certificate,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
    }

    static bool verify_certificate(bool pre_verified, asio::ssl::verify_context &ctx) {
        char subject_name[256];
        X509 *cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);

        return pre_verified;
    }

    string request(const uint8_t *data, uint32_t size) {
        asio::error_code error;

        connect();
        handshake();
        send_request(data, size);
        recv_response_header();
        recv_response_body();

        const char *resp = static_cast<const char *>(response_.data().data());
        return string(resp, resp + size_);
    }

    size_t size() const {
        return size_;
    }

    const void *data() const {
        return response_.data().data();
    }

private:

    void handle_error(const string &description) {
        if (error_) {
            global_logger->critical("IAS {} failed: {}", description, error_.message());
            throw asio::system_error(error_);
        }
    }

    void connect() {
        asio::connect(socket_.lowest_layer(), endpoints_, error_);
        handle_error(__FUNCTION__);
    }

    void handshake() {
        socket_.handshake(asio::ssl::stream_base::client, error_);
        handle_error(__FUNCTION__);
    }

    void send_request(const uint8_t *data, uint32_t size) {
        asio::write(socket_, asio::buffer(data, size), error_);
        handle_error(__FUNCTION__);
    }

    void recv_response_header() {
        const char *delimiter = "\r\n\r\n";

        asio::read_until(socket_, response_, delimiter, error_);
        handle_error(__FUNCTION__);

        const char *data = static_cast<const char *>(response_.data().data());
        const char *ptr = strstr(data, "Content-Length");
        sscanf(ptr, "Content-Length: %lu\r\n", &size_);

        ptr = strstr(data, delimiter);
        size_ += (ptr - data) + strlen(delimiter);
    }

    void recv_response_body() {
        while (response_.size() < size_ && !error_) {
            asio::read(socket_, response_, asio::transfer_at_least(1), error_);
        }
        handle_error(__FUNCTION__);
    }

private:
    tcp_resolver::results_type endpoints_;
    asio::ssl::stream<asio::ip::tcp::socket> socket_;

protected:
    asio::error_code error_;
    asio::streambuf response_;
    size_t size_;
};
//
//int main(int argc, char *argv[]) {
//    try {
//        asio::io_context io_context;
//
//        tcp_resolver resolver(io_context);
//        tcp_resolver::results_type endpoints = resolver.resolve(host, protocol);
//
//        asio::ssl::context ctx(asio::ssl::context::sslv23);
//        ctx.load_verify_file("/etc/ssl/certs/ca-certificates.crt");
//
//        client c(io_context, ctx, endpoints);
//
//        io_context.run();
//    }
//    catch (std::exception &e) {
//        std::cerr << "Exception @ <" << __LINE__ << ">: " << e.what() << "\n";
//    }
//
//    return 0;
//}
//

#endif //ENCLAVERAFT_SSLCLIENT_HXX
