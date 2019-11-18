//
// Created by ncl on 29/10/19.
//

#include <random>
#include "client.hxx"
#include "messages.hxx"
#include "json11.hpp"
#include <cppcodec/base64_default_rfc4648.hpp>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

std::shared_ptr<spdlog::logger> logger;

using namespace json11;

//uint16_t client_request(asio::io_context &io_context, uint16_t leader_id, const std::string &msg) {
//    bufptr buf = buffer::alloc(msg.length() + 1);
//    buf->put(msg);
//    buf->pos(0);
//    auto entry = std::make_shared<log_entry>(0, std::move(buf));
//
//    return send_message(io_context, leader_id, msg_type::client_request, entry);
//}
//
//uint16_t add_server(asio::io_context &io_context, uint16_t leader_id, uint16_t srv_id) {
//    srv_config cfg(srv_id, fmt::format("tcp://127.0.0.1:{}", 9000 + srv_id));
//
//    logger->debug("ADD_SERVER Id={} Endpoint={}", cfg.get_id(), cfg.get_endpoint());
//
//    bufptr buf = cfg.serialize();
//    auto entry = std::make_shared<log_entry>(0, std::move(buf), log_val_type::cluster_server);
//
////    bufptr ser = cfg.serialize();
////    std::shared_ptr<srv_config> p = srv_config::deserialize(*ser);
////    logger->debug("ADD_SERVER Id={} Endpoint={}", p->get_id(), p->get_endpoint());
//
//    return send_message(io_context, leader_id, msg_type::add_server_request, entry);
//}


string add_server(uint16_t server_id) {
    Json body = Json::object{
            {"server_id", server_id},
            {"endpoint",  fmt::format("tcp://127.0.0.1:{}", 9000 + server_id)}
    };

    return body.dump();
}

string remove_server(uint16_t server_id) {
    Json body = Json::object{
            {"server_id", server_id}
    };

    return body.dump();
}

string append_entries(const vector<bytes> &logs) {
    vector<string> body;

    for (const auto &log: logs) {
        body.emplace_back(base64::encode(log));
    }

    return Json(body).dump();
}

ptr<bytes> serialize(er_message_type type, const string &payload) {
    uint16_t type_num = type;
    auto *ptr = reinterpret_cast<uint8_t *>(&type_num);

    auto buffer = make_shared<bytes>();
    buffer->insert(buffer->end(), ptr, ptr + sizeof(uint16_t));
    buffer->insert(buffer->end(), payload.begin(), payload.end());

    return buffer;
}

bool deserialize(const bytes &buffer) {
    const uint16_t &type_num = *(const uint16_t *) buffer.data();
    const char *payload = reinterpret_cast<const char *>(buffer.data() + sizeof(uint16_t));

    string j(payload, payload + buffer.size() - sizeof(uint16_t));

    string json_err;
    Json body = Json::parse(j, json_err);

    if (json_err.empty()) {
        return body["success"].int_value();
    } else {
        throw std::runtime_error(json_err);
    }
}

class SocketClient : public std::enable_shared_from_this<SocketClient> {
public :
    SocketClient(asio::io_context &io_context, uint16_t port) : s_(io_context), resolver_(io_context), port_(port) {}

    void send(const bytes &request) {
        try {
            asio::connect(s_, resolver_.resolve("127.0.0.1", std::to_string(port_)));


            auto local = s_.local_endpoint();
            auto remote = s_.remote_endpoint();
            logger->debug("Socket Local {}:{} -> Remote {}:{}",
                          local.address().to_string(), local.port(), remote.address().to_string(), remote.port());

            /* Sending message */
            asio::error_code err;
            uint32_t size = request.size();

            asio::streambuf req_sbuf;
            std::ostream out(&req_sbuf);
            out.write(reinterpret_cast<const char *>(&size), sizeof(uint32_t));
            out.write(reinterpret_cast<const char *>(request.data()), request.size());

            logger->debug("Write: {}", spdlog::to_hex(request));

            auto self = shared_from_this();

            logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);

            asio::async_write(s_, req_sbuf, [self](const asio::error_code &err, size_t bytes) {

                logger->trace("{} {} {}", __FILE__, "async_write callback", __LINE__);
                if (err) {
                    logger->error("Error when writing message: {}", err.message());
                } else {
                    logger->debug("Message sent, reading response");
                }

                self->receive_header();
            });
        } catch (std::system_error &err) {
            logger->critical("SYSTEM ERROR HERE {}{}", __LINE__, err.what());
            throw;
        }
    }

    void receive_header() {
        try {
            logger->trace("{} {} {}", __FILE__, __FUNCTION__, __LINE__);
            /* Reading message */
            asio::error_code err;

            auto self = shared_from_this();
            asio::async_read(s_, resp_sbuf, asio::transfer_at_least(sizeof(uint32_t)),
                             [self](const asio::error_code &err, size_t bytes) {

                                 logger->trace("{} {} {}", __FILE__, "receive_header callback", __LINE__);

                                 std::istream in(&self->resp_sbuf);

                                 if (err && err != asio::error::eof) {
                                     logger->error("Error when reading header: {}", err.message());
                                 }
                                 in.read(reinterpret_cast<char *>(&self->size_), sizeof(uint32_t));

                                 self->receive();
                             });
        } catch (std::system_error &err) {
            logger->critical("SYSTEM ERROR HERE {}{}", __LINE__, err.what());
            throw;
        }
    }

    void receive() {
        try {
            asio::error_code err;
            std::istream in(&resp_sbuf);

            while (resp_sbuf.size() < size_ && !err) {
                asio::read(s_, resp_sbuf, asio::transfer_at_least(1), err);
            }
            if (err && err != asio::error::eof) {
                logger->error("Error when reading message: {}", err.message());
            }

            logger->debug("Read: {}", spdlog::to_hex((char *) resp_sbuf.data().data(),
                                                     (char *) resp_sbuf.data().data() + resp_sbuf.size()));

            auto ret = make_shared<bytes>(size_, 0);
            in.read(reinterpret_cast<char *> (&(*ret)[0]), size_);

            logger->debug("Read: {}", spdlog::to_hex(*ret));
        } catch (std::system_error &err) {
            logger->critical("SYSTEM ERROR HERE {}{}", __LINE__, err.what());
            throw;
        }
    }

    tcp::socket s_;
    tcp::resolver resolver_;
    asio::streambuf resp_sbuf;
    uint16_t port_;
    uint32_t size_;
};

ptr<bytes> send_and_recv(asio::io_context &io_context, uint16_t port, const bytes &request) {
    /* Resolve */
    tcp::socket s(io_context);
    tcp::resolver resolver(io_context);
    asio::connect(s, resolver.resolve("127.0.0.1", std::to_string(port)));

    auto local = s.local_endpoint();
    auto remote = s.remote_endpoint();
    logger->debug("Socket Local {}:{} -> Remote {}:{}",
                  local.address().to_string(), local.port(), remote.address().to_string(), remote.port());

    /* Sending message */
    asio::error_code err;
    uint32_t size = request.size();

    asio::streambuf req_sbuf;
    std::ostream out(&req_sbuf);
    out.write(reinterpret_cast<const char *>(&size), sizeof(uint32_t));
    out.write(reinterpret_cast<const char *>(request.data()), request.size());

    logger->debug("Write: {}", spdlog::to_hex(request));

    asio::write(s, req_sbuf, err);
    if (err) {
        logger->error("Error when writing message: {}", err.message());
    } else {
        logger->debug("Message sent, reading response");
    }

    /* Reading message */
    asio::streambuf resp_sbuf;
    std::istream in(&resp_sbuf);

    asio::read(s, resp_sbuf, asio::transfer_at_least(sizeof(uint32_t)), err);
    if (err && err != asio::error::eof) {
        logger->error("Error when reading header: {}", err.message());
    }
    in.read(reinterpret_cast<char *>(&size), sizeof(uint32_t));

    while (resp_sbuf.size() < size && !err) {
        asio::read(s, resp_sbuf, asio::transfer_at_least(1), err);
    }
    if (err && err != asio::error::eof) {
        logger->error("Error when reading message: {}", err.message());
    }

    logger->debug("Read: {}", spdlog::to_hex((char *) resp_sbuf.data().data(),
                                             (char *) resp_sbuf.data().data() + resp_sbuf.size()));

    auto ret = make_shared<bytes>(size, 0);
    in.read(reinterpret_cast<char *> (&(*ret)[0]), size);

    logger->debug("Read: {}", spdlog::to_hex(*ret));

    return ret;
}

string generate_dummy_string(size_t length) {
    static char charset[] = "0123456789abcdef";

    std::random_device rd;
    std::default_random_engine engine(rd());
    std::uniform_int_distribution<uint8_t> dist(0, strlen(charset) - 1);

    string out;
    while (out.length() < length) {
        out.push_back(charset[dist(engine)]);
    }

    return out;
}


void app_add_server(asio::io_context &io_context, int32_t leader_id, int32_t srv_id) {
    logger->info("\tadding server {}", srv_id);
    auto request = serialize(client_add_srv_req, add_server(srv_id));
    auto response = send_and_recv(io_context, 9000 + leader_id, *request);
    if (deserialize(*response)) {
        logger->info("\tSUCCESS");
    } else {
        logger->info("\tFAILED");
    }
}

std::mutex stop_mutex;
std::condition_variable stop_cv;
bool want_to_stop;

//std::mutex thread_mutex;
//std::condition_variable thread_cv;
//int32_t thread_count;
//constexpr int32_t thread_max = 512;

int main(int argc, const char *argv[]) {
    logger = spdlog::stdout_color_mt("client");
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("%^[%H:%M:%S.%f] %n @ %t [%l]%$ %v");

//    asio::io_context io_context;

    auto io_context_ptr = std::make_shared<asio::io_context>();

    uint16_t leader_id = 1;


#if 0
    string dummy = generate_dummy_string(256);
    logger->info("\tsending request {:spn}", spdlog::to_hex(dummy));

    const bytes log(dummy.begin(), dummy.end());

    auto request = serialize(client_append_entries_req, append_entries(vector<bytes>{log}));
    auto response = send_and_recv(io_context, 9000 + leader_id, *request);
    if (deserialize(*response)) {
        logger->info("\tSUCCESS");
    } else {
        logger->info("\tFAILED");
    }

    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(200));

#else
    int32_t interval = 500;

    if (argc > 1) {
        interval = atoi(argv[1]);
    }

    want_to_stop = false;
//    thread_count = 0;


    for (int32_t i = 2; i <= 5; i++) {
        auto request = serialize(client_add_srv_req, add_server(i));
        logger->info("\tadding server {}", i);
        auto client = std::make_shared<SocketClient>(*io_context_ptr, 9000 + leader_id);
        client->send(*request);
        std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(500));
    }

    while (true) {
        try {
            string dummy = generate_dummy_string(256);
            const bytes log(dummy.begin(), dummy.end());
            auto request = serialize(client_append_entries_req, append_entries(vector<bytes>{log}));
            logger->info("\tsending request {}", dummy);

            auto client = std::make_shared<SocketClient>(*io_context_ptr, 9000 + leader_id);
            client->send(*request);
            std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(interval));

        } catch (std::runtime_error &err) {
            logger->error(err.what());
        }

        if (interval > 0) {
            std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(interval));
        }
//        break;
    }

    vector<std::thread> thread_pool;
    for (unsigned int i = 0; i < 8; ++i) {
        thread_pool.emplace_back([io_context_ptr] {
            io_context_ptr->run();
        });
    }


    for (auto &thread :thread_pool) {
        thread.join();
    }


    return 0;

//    std::thread t([&io_context, leader_id, &interval]() {
//        for (int32_t i = 2; i <= 5; i++) {
//            app_add_server(io_context, leader_id, i);
//            std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(200));
//        }
//
//        while (true) {
//
//            try {
//                string dummy = generate_dummy_string(256);
//                logger->info("\tsending request {}", dummy);
//
//                const bytes log(dummy.begin(), dummy.end());
//
//                auto request = serialize(client_append_entries_req, append_entries(vector<bytes>{log}));
//                auto response = send_and_recv(io_context, 9000 + leader_id, *request);
//                if (deserialize(*response)) {
//                    logger->info("\tSUCCESS");
//                } else {
//                    logger->info("\tFAILED");
//                }
//
//                {
//                    std::unique_lock<std::mutex> lock(stop_mutex);
//                    if (want_to_stop) {
//                        break;
//                    }
//                }
//
//                if (interval > 1) {
//                    interval--;
//                }
//            } catch (std::runtime_error &err) {
//                logger->error(err.what());
//                if (strcmp(err.what(), "connect: Connection refused") == 0) {
//                    logger->info("interval doubled");
//                    interval *= 2;
//                }
//            }
//
//            if (interval > 0) {
//                std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(interval));
//            }
//        }
//
//        stop_cv.notify_all();
//    });
//
//    t.detach();
//
//    asio::steady_timer timer(io_context, std::chrono::seconds(timeout));
//
//    timer.wait();
//
//    want_to_stop = true;
//    std::unique_lock<std::mutex> lock(stop_mutex);
//    stop_cv.wait(lock);
#endif

}
