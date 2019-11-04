//
// Created by ncl on 28/10/19.
//

#ifndef ENCLAVERAFT_SERVICE_PORT_HXX
#define ENCLAVERAFT_SERVICE_PORT_HXX

#include "../raft/include/cornerstone.hxx"
#include "rpc_client_port.hxx"
#include "raft_enclave_t.h"
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

using cornerstone::delayed_task_scheduler;
using cornerstone::rpc_client_factory;
using cornerstone::ptr;
using cornerstone::rpc_client;
using cornerstone::delayed_task;
using cornerstone::cs_new;

extern map<uint64_t, function<void(bool)>> service_task_pool;
extern mutex service_task_pool_lock;


void _free_task_context_(void *ptr);

void _timer_handler_(ptr<delayed_task> &task, bool success);

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
            lock_guard<mutex> lock(service_task_pool_lock);
            service_task_pool[*task_uid] = bind(&_timer_handler_, task, std::placeholders::_1);
        }
        ocall_schedule_delayed_task(*task_uid, milliseconds);
    }

    // From rpc_client_factory
    ptr<rpc_client> create_client(const string &endpoint) override {
        return cs_new<RpcClientPort, const string &>(endpoint);
    }

private:
    // From delayed_task_scheduler
    void cancel_impl(ptr<delayed_task> &task) override {
        if (task->get_impl_context() == nilptr) {
            auto task_uid = static_cast<uint64_t *> (task->get_impl_context());
            ocall_cancel_delayed_task(*task_uid);
        }
    }

private:
    atomic<uint64_t> last_task_uid_;
};

#endif //ENCLAVERAFT_SERVICE_PORT_HXX
