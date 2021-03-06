//
// Created by ncl on 29/10/19.
//


#include "raft_enclave_u.h"
#include <asio.hpp>

#include <memory>
#include <map>
#include <mutex>
#include <chrono>
#include <spdlog/logger.h>

using std::shared_ptr;

using std::map;
using std::mutex;
using std::lock_guard;
using std::make_shared;

extern sgx_enclave_id_t global_enclave_id;
extern shared_ptr<asio::io_context> global_io_context;
extern shared_ptr<spdlog::logger> global_logger;

static mutex asio_task_scheduler_pool_lock;
static map<uint64_t, shared_ptr<asio::steady_timer>> asio_task_scheduler_pool;

void ocall_schedule_delayed_task(uint64_t task_uid, int32_t milliseconds) {
    lock_guard<mutex> lock(asio_task_scheduler_pool_lock);

    auto timer = make_shared<asio::steady_timer>(*global_io_context);

    timer->expires_after(std::chrono::duration<int32_t, std::milli>(milliseconds));
    timer->async_wait([task_uid](asio::error_code err) -> void {
        if (!err) {
            ecall_timer_expired_callback(global_enclave_id, task_uid, true);
        } else {
            ecall_timer_expired_callback(global_enclave_id, task_uid, false);
        }
    });

    asio_task_scheduler_pool[task_uid] = timer;
}

void ocall_cancel_delayed_task(uint64_t task_uid) {
    global_logger->trace("{} {} {}: task_uid={}", __FILE__, __FUNCTION__, __LINE__, task_uid);

    lock_guard<mutex> lock(asio_task_scheduler_pool_lock);

    auto it = asio_task_scheduler_pool.find(task_uid);
    if (it != asio_task_scheduler_pool.end()) {
        it->second->cancel();
    }
}
