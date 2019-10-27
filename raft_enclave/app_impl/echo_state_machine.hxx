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
using std::lock_guard;
using std::mutex;
using std::string;

using cornerstone::async_result;
using cornerstone::buffer;
using cornerstone::bufptr;
using cornerstone::state_machine;
using cornerstone::snapshot;
using cornerstone::ptr;
using cornerstone::strfmt;

class echo_state_machine : public state_machine {
public:
    void pre_commit(const ulong log_idx, buffer &data) override {
        string put = strfmt<256>("PRE_COMMIT %ul %s").fmt(log_idx, data.get_str());
        ocall_puts(put.c_str());
    }

    void commit(const ulong log_idx, buffer &data) override {
        string put = strfmt<256>("COMMIT %ul %s").fmt(log_idx, data.get_str());
        ocall_puts(put.c_str());

        last_committed_idx_ = log_idx;
    }

    void rollback(const ulong log_idx, buffer &data) override {
        string put = strfmt<256>("ROLLBACK %ul %s").fmt(log_idx, data.get_str());
        ocall_puts(put.c_str());
    }

    void save_snapshot_data(snapshot &s, const ulong offset, buffer &data) override {
//        TODO:

    }

    bool apply_snapshot(snapshot &s) override {
        string put = strfmt<256>("APPLY_SNAPSHOT @log=%ul @term=%ul").fmt(
                s.get_last_log_term(),
                s.get_last_log_term()
        );
        ocall_puts(put.c_str());

        // Clone snapshot from `s`.
        {
            lock_guard<mutex> lock(last_snapshot_lock_);
            bufptr snp_buf = s.serialize();
            last_snapshot_ = snapshot::deserialize(*snp_buf);
        }

        return true;
    }

    int read_snapshot_data(snapshot &s, const ulong offset, buffer &data) override {
//        TODO:
        return 0;
    }

    ptr<snapshot> last_snapshot() override {
        lock_guard<mutex> lock(last_snapshot_lock_);
        return last_snapshot_;
    }

    ulong last_commit_index() override {
        return last_committed_idx_;
    }

    void create_snapshot(snapshot &s, async_result<bool>::handler_type &when_done) override {
        string put = strfmt<256>("CREATE_SNAPSHOT @log=%ul @term=%ul").fmt(
                s.get_last_log_term(),
                s.get_last_log_term()
        );
        ocall_puts(put.c_str());

        // Clone snapshot from `s`.
        {
            lock_guard<mutex> lock(last_snapshot_lock_);
            bufptr snp_buf = s.serialize();
            last_snapshot_ = snapshot::deserialize(*snp_buf);
        }

        ptr<std::exception> except(nullptr);
        bool ret = true;
        when_done(ret, except);
    }

private:
    // Last committed Raft log number.
    atomic<uint64_t> last_committed_idx_;

    // Last snapshot.
    ptr<snapshot> last_snapshot_;

    // Mutex for last snapshot.
    mutex last_snapshot_lock_;
};