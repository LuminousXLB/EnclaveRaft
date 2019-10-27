/************************************************************************
Copyright 2017-2019 eBay Inc.
Author/Developer(s): Jung-Sang Ahn

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#pragma once

#include "../raft/include/cornerstone.hxx"
#include "raft_enclave_t.h"

#include <mutex>

using std::atomic;
using std::mutex;

using cornerstone::async_result;
using cornerstone::buffer;
using cornerstone::state_machine;
using cornerstone::snapshot;
using cornerstone::ptr;

class echo_state_machine : public state_machine {
public:
    void commit(const ulong log_idx, buffer &data) override {
        
    }

    void pre_commit(const ulong log_idx, buffer &data) override {

    }

    void rollback(const ulong log_idx, buffer &data) override {

    }

    void save_snapshot_data(snapshot &s, const ulong offset, buffer &data) override {

    }

    bool apply_snapshot(snapshot &s) override {
        return false;
    }

    int read_snapshot_data(snapshot &s, const ulong offset, buffer &data) override {
        return 0;
    }

    ptr<snapshot> last_snapshot() override {
        return ptr<snapshot>();
    }

    ulong last_commit_index() override {
        return 0;
    }

    void create_snapshot(snapshot &s, async_result<bool>::handler_type &when_done) override {

    }

private:
    // Last committed Raft log number.
    atomic<uint64_t> last_committed_idx_;

    // Last snapshot.
    ptr <snapshot> last_snapshot_;

    // Mutex for last snapshot.
    mutex last_snapshot_lock_;
};