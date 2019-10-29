//
// Created by ncl on 29/10/19.
//

#include "service_port.hxx"

map<uint64_t, function<void(bool)>> service_task_pool;
mutex service_task_pool_lock;


// TODO: log this into edl
void ecall_timer_expired_callback(uint64_t task_uid, bool success) {
    map<uint64_t, function<void(bool)>>::iterator callback;

    {
        lock_guard<mutex> lock(service_task_pool_lock);
        callback = service_task_pool.find(task_uid);
    }

    if (callback != service_task_pool.end()) {
        callback->second(success);
    }
}

void _free_task_context_(void *ptr) {
    auto *uid = static_cast<uint64_t *> (ptr);
    delete uid;
}

void _timer_handler_(ptr<delayed_task> &task, bool success) {
    if (success) {
        task->execute();
    }
}
