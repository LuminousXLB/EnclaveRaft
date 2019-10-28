//
// Created by ncl on 29/10/19.
//

#include "service_port.hxx"

map<uint64_t, function<void(error_code)>> _task_pool;
mutex _task_pool_lock;


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

