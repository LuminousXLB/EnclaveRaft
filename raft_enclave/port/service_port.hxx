//
// Created by ncl on 28/10/19.
//

#ifndef ENCLAVERAFT_SERVICE_PORT_HXX
#define ENCLAVERAFT_SERVICE_PORT_HXX

#include "../raft/include/cornerstone.hxx"
#include "rpc_client_port.hxx"
#include <memory>
#include <functional>
#include <map>

using std::atomic;
using std::bind;
using std::mutex;
using std::string;
using std::map;
using std::lock_guard;
using std::function;
using std::error_code;

using cornerstone::delayed_task_scheduler;
using cornerstone::rpc_client_factory;
using cornerstone::ptr;
using cornerstone::rpc_client;
using cornerstone::delayed_task;
using cornerstone::cs_new;

static map<uint64_t, function<void(error_code)>> _task_pool;
static mutex _task_pool_lock;

// TODO: impl this outside enclave
void ocall_schedule_delayed_task(uint64_t task_uid, int32_t milliseconds);

void ocall_cancel_delayed_task(uint64_t task_uid);

// TODO: log this into edl
void ecall_timer_expired_callback(uint64_t task_uid, error_code err) {
    map<uint64_t, function<void(error_code)>>::iterator callback;
    {
        lock_guard<mutex> lock(_task_pool_lock);
        callback = _task_pool.find(task_uid);
    }

    if (callback != _task_pool.end()) {
        callback->second(err);
    }
}

void _free_task_context_(void *ptr) {
    auto *uid = static_cast<uint64_t *> (ptr);
    delete uid;
}

void _timer_handler_(ptr<delayed_task> &task, error_code err) {
    if (!err) {
        task->execute();
    }
}


class ServicePort : public delayed_task_scheduler, public rpc_client_factory {
public:
    ServicePort() : last_task_uid_(0) {}

__nocopy__(ServicePort)

    // From delayed_task_scheduler
    void schedule(ptr<delayed_task> &task, int32 milliseconds) override {
        if (task->get_impl_context() == nilptr) {
            task->set_impl_context(new uint64_t(++last_task_uid_), &_free_task_context_);
        }

        task->reset();

        auto *task_uid = static_cast<uint64_t *> (task->get_impl_context());

        {
            lock_guard<mutex> lock(_task_pool_lock);
            ocall_schedule_delayed_task(*task_uid, milliseconds);
            _task_pool[*task_uid] = bind(&_timer_handler_, task, std::placeholders::_1);
        }
    }

    // From rpc_client_factory
    ptr<rpc_client> create_client(const string &endpoint) override {
        return cs_new<RpcClientPort, const string &>(endpoint);
    }

private:
    // From delayed_task_scheduler
    void cancel_impl(ptr<delayed_task> &task) override {
        if (task->get_impl_context() == nilptr) {
            auto *task_uid = static_cast<uint64_t *> (task->get_impl_context());
            ocall_cancel_delayed_task(*task_uid);
        }
    }

private:
    atomic<uint64_t> last_task_uid_;
};

#endif //ENCLAVERAFT_SERVICE_PORT_HXX
