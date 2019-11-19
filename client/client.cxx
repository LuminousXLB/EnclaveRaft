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
        for (auto it = pool_.begin(); it != pool_.end(); it++) {
            if ((*it)->status() == SocketClient::AVAILABLE) {
                return (*it);
            } else if ((*it)->status() == SocketClient::ERROR) {
                pool_.erase(it);
            }
        }

        pool_.emplace_back(std::make_shared<SocketClient>(io_context_ptr_, port_));

        return pool_.back();
    }

private:
    ptr<asio::io_context> io_context_ptr_;
    uint16_t port_;
    list<ptr<SocketClient>> pool_;
};

ptr<asio::io_context> io_context_ptr;


int main(int argc, const char *argv[]) {
    logger = spdlog::stdout_color_mt("client");
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("%^[%H:%M:%S.%f] %n @ %t [%l]%$ %v");

    io_context_ptr = std::make_shared<asio::io_context>();

    uint16_t leader_id = 1;


    double interval = 500;

    if (argc > 1) {
        interval = atof(argv[1]);
    }

#if 0

    string dummy = generate_dummy_string(256);
    auto request = RequestBuilder::append_entries(dummy);

    auto client = std::make_shared<SocketClient>(io_context_ptr, 9000 + leader_id);
    logger->info("sending request {}", dummy);
    client->send(request);
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(interval));


#else

    for (int32_t i = 2; i <= 5; i++) {
        auto request = RequestBuilder::add_server(i);
        auto client = std::make_shared<SocketClient>(io_context_ptr, 9000 + leader_id);
        logger->info("\tadding server {}", i);
        client->send(request);
        std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(500));
    }

    std::thread tt([interval, &leader_id]() {
        ClientPool pool(io_context_ptr, 9000 + leader_id);

        while (true) {
            try {
                string dummy = generate_dummy_string(256);
                auto request = RequestBuilder::append_entries(dummy);

                auto client = pool.get_client();
                logger->info("sending request {}", dummy);
                client->send(request);

            } catch (std::runtime_error &err) {
                logger->error(err.what());
            }

            if (interval >= 1) {
                std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(int(interval)));
            } else if (interval > 0) {
                std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::nano>(int(1000 * interval)));
            }
        }
    });

    tt.detach();

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
