//
// Created by ncl on 29/10/19.
//

#include "client.hxx"
#include <random>
#include "messages.hxx"
#include <spdlog/sinks/stdout_color_sinks.h>
#include "RequestBuilder.hxx"
#include "SocketClient.hxx"

std::shared_ptr<spdlog::logger> logger;


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

#include <list>
#include <utility>

using std::list;

class ClientPool {
public:
    ClientPool(ptr<asio::io_context> io_context_ptr, uint16_t port)
            : io_context_ptr_(std::move(io_context_ptr)), port_(port) {}

    ptr<SocketClient> get_client() {
//        for (auto it = pool_.begin(); it != pool_.end(); it++) {
//            if ((*it)->status() == SocketClient::AVAILABLE) {
//                return (*it);
//            } else if ((*it)->status() == SocketClient::ERROR) {
//                pool_.erase(it);
//                it = pool_.begin();
//            }
//        }
//
//        pool_.emplace_back(std::make_shared<SocketClient>(io_context_ptr_, port_));

//        return pool_.back();
        return std::make_shared<SocketClient>(io_context_ptr_, port_);
    }

private:
    ptr<asio::io_context> io_context_ptr_;
    uint16_t port_;
    list<ptr<SocketClient>> pool_;
};

ptr<asio::io_context> io_context_ptr;


int main(int argc, const char *argv[]) {
    uint8_t client_id = 1;
    unsigned interval = 500;
    uint32_t payload_size = 256;

    if (argc < 2) {
        // do nothing
    } else if (argc == 2) {
        client_id = strtoul(argv[1], nullptr, 10);
    } else if (argc == 3) {
        client_id = strtoul(argv[1], nullptr, 10);
        interval = strtoul(argv[2], nullptr, 10);
    } else if (argc == 4) {
        client_id = strtoul(argv[1], nullptr, 10);
        interval = strtoul(argv[2], nullptr, 10);
        payload_size = strtoul(argv[3], nullptr, 10);
    } else {
        fprintf(stderr, "Usage: \n");
        fprintf(stderr, "    %s <server_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    logger = spdlog::stdout_color_mt(fmt::format("client_{}", client_id));
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("%^[%H:%M:%S.%f] %n @ %t [%l]%$ %v");

    io_context_ptr = std::make_shared<asio::io_context>();

    int32_t leader_id = 1;

#if 0

    string dummy = generate_dummy_string(256);
    auto request = RequestBuilder::append_entries(dummy);

    auto client = std::make_shared<SocketClient>(io_context_ptr, 9000 + leader_id);
    logger->info("sending request {}", dummy);
    client->send(request);
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(int(interval)));


#else

    if (client_id == 1) {
        for (int32_t i = 2; i <= 5; i++) {
            auto request = RequestBuilder::add_server(i);
            auto client = std::make_shared<SocketClient>(io_context_ptr, 9000 + leader_id);
            logger->info("\tadding server {}", i);
            client->send(request);
            std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(200));
        }
    }

    ClientPool pool(io_context_ptr, 9000 + leader_id);

    while (true) {
        try {
            string dummy = generate_dummy_string(payload_size);
            auto request = RequestBuilder::append_entries(dummy);

            string msg = dummy;
            if (msg.length() > 64) {
                msg.resize(64);
            }

            auto client = pool.get_client();
            logger->info("sending request {}", msg);
            client->send(request);

        } catch (std::runtime_error &err) {
            interval *= 2;
            logger->error(err.what());
        }

        if (interval >= 1) {
            std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(interval));
        }
    }
//    std::thread tt([&interval, &leader_id, payload_size]() {
//
//    });

//    tt.detach();

#endif

#if 0
    unsigned int cpu_cnt = std::thread::hardware_concurrency() - 1;
    vector<std::thread> thread_pool;
    for (unsigned int i = 0; i < cpu_cnt; ++i) {
        thread_pool.emplace_back([] {
            io_context_ptr->run();
        });
    }

    for (auto &thread :thread_pool) {
        thread.join();
    }
#else
    io_context_ptr->run();
#endif
    std::this_thread::sleep_for(std::chrono::seconds(1));

}
