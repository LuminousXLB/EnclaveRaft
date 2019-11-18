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

#include <map>
#include <vector>
#include <mutex>

using std::atomic;
using std::lock_guard;
using std::map;
using std::mutex;
using std::vector;

#include "../raft/include/cornerstone.hxx"

using cornerstone::buffer;
using cornerstone::bufptr;
using cornerstone::cs_new;
using cornerstone::log_entry;
using cornerstone::log_store;

class in_memory_log_store : public log_store {
public:
    in_memory_log_store() : start_idx_(1) {
        logs_[0] = cs_new<log_entry>(0, buffer::alloc(sz_ulong));
    }

    ~in_memory_log_store() override = default;;

__nocopy__(in_memory_log_store);

    /**
    ** The first available slot of the store, starts with 1
    */
    ulong next_slot() const override {
        lock_guard<mutex> lock(logs_lock_);
        // Exclude the dummy entry.
        return start_idx_ + logs_.size() - 1;
    }

    /**
    ** The start index of the log store, at the very beginning, it must be 1
    ** however, after some compact actions, this could be anything greater or equals to one
    */
    ulong start_index() const override {
        return start_idx_;
    }

    /**
    * The last log entry in store
    * @return a dummy constant entry with value set to null and term set to zero if no log entry in store
    */
    ptr<log_entry> last_entry() const override {
        ulong next_idx = next_slot();

        lock_guard<mutex> lock(logs_lock_);
        auto entry = logs_.find(next_idx - 1);
        if (entry == logs_.end()) {
            entry = logs_.find(0);
        }

        return make_clone(entry->second);
    }

    /**
    * Appends a log entry to store
    * @param entry
    */
    ulong append(ptr<log_entry> &entry) override {
        ptr<log_entry> clone = make_clone(entry);

        lock_guard<mutex> lock(logs_lock_);
        size_t idx = start_idx_ + logs_.size() - 1;
        logs_[idx] = clone;
        return idx;
    }

    /**
    * Over writes a log entry at index of {@code index}
    * @param index a value < this->next_slot(), and starts from 1
    * @param entry
    */
    void write_at(ulong index, ptr<log_entry> &entry) override {
        ptr<log_entry> clone = make_clone(entry);

        // Discard all logs equal to or greater than `index.
        lock_guard<mutex> lock(logs_lock_);
        auto it = logs_.lower_bound(index);
        while (it != logs_.end()) {
            it = logs_.erase(it);
        }
        logs_[index] = clone;
    }

    /**
    * Get log entries with index between start and end
    * @param start, the start index of log entries
    * @param end, the end index of log entries (exclusive)
    * @return the log entries between [start, end), nilptr is returned if no entries in that range
    */
    ptr<vector<ptr<log_entry>>> log_entries(ulong start, ulong end) override {
        ptr<vector<ptr<log_entry>>> ret = cs_new<std::vector<ptr<log_entry>>>();
        ret->reserve(end - start);

        for (ulong ii = start; ii < end; ++ii) {
            ptr<log_entry> src = nullptr;
            {
                lock_guard<mutex> lock(logs_lock_);
                auto entry = logs_.find(ii);
                if (entry == logs_.end()) {
                    entry = logs_.find(0);
                    assert(0);
                }
                src = entry->second;
            }
            ret->push_back(make_clone(src));
        }
        return ret;
    }

    /**
    * Gets the log entry at the specified index
    * @param index, starts from 1
    * @return the log entry or null if index >= this->next_slot()
    */
    ptr<log_entry> entry_at(ulong index) override {
        ptr<log_entry> src = nullptr;
        {
            lock_guard<mutex> lock(logs_lock_);
            auto entry = logs_.find(index);
            if (entry == logs_.end()) {
                entry = logs_.find(0);
            }
            src = entry->second;
        }
        return make_clone(src);
    }

    /**
    * Gets the term for the log entry at the specified index
    * Suggest to stop the system if the index >= this->next_slot()
    * @param index, starts from 1
    * @return the term for the specified log entry or 0 if index < this->start_index()
    */
    ulong term_at(ulong index) override {
        ulong term = 0;
        {
            lock_guard<mutex> lock(logs_lock_);
            auto entry = logs_.find(index);
            if (entry == logs_.end()) {
                entry = logs_.find(0);
            }
            term = entry->second->get_term();
        }
        return term;
    }

    /**
    * Pack cnt log items starts from index
    * @param index
    * @param cnt
    * @return log pack
    */
    bufptr pack(ulong index, int32 cnt) override {
        vector<ptr<buffer>> logs;

        size_t total_size = 0;

        for (ulong ii = index; ii < index + cnt; ++ii) {
            ptr<log_entry> entry = nullptr;
            {
                lock_guard<mutex> lock(logs_lock_);
                entry = logs_[ii];
            }
            assert(entry.get());
            ptr<buffer> buf = std::move(entry->serialize());
            total_size += buf->size();
            logs.push_back(buf);
        }

        bufptr buf_out = buffer::alloc(sizeof(int32) + cnt * sizeof(int32) + total_size);
        buf_out->pos(0);
        buf_out->put((int32) cnt);

        for (auto &entry: logs) {
            ptr<buffer> &bb = entry;
            buf_out->put((int32) bb->size());
            buf_out->put(*bb);
        }

        return buf_out;
    }

    /**
    * Apply the log pack to current log store, starting from index
    * @param index, the log index that start applying the pack, index starts from 1
    * @param pack
    */
    void apply_pack(ulong index, buffer &pack) override {
        pack.pos(0);
        int32 num_logs = pack.get_int();

        for (ulong ii = index; ii < index + num_logs; ++ii) {

            bufptr buf_local = buffer::alloc(pack.get_int());
            pack.get(buf_local);

            ptr<log_entry> entry = log_entry::deserialize(*buf_local);
            {
                lock_guard<mutex> lock(logs_lock_);
                logs_[ii] = entry;
            }
        }

        {
            lock_guard<mutex> lock(logs_lock_);
            auto entry = logs_.upper_bound(0);
            if (entry != logs_.end()) {
                start_idx_ = entry->first;
            } else {
                start_idx_ = 1;
            }
        }
    }

    /**
    * Compact the log store by removing all log entries including the log at the last_log_index
    * @param last_log_index
    * @return compact successfully or not
    */
    bool compact(ulong last_log_index) override {
        lock_guard<mutex> lock(logs_lock_);

        for (ulong ii = start_idx_; ii <= last_log_index; ++ii) {
            auto entry = logs_.find(ii);
            if (entry != logs_.end()) {
                logs_.erase(entry);
            }
        }

        // WARNING:
        //   Even though nothing has been erased,
        //   we should set `start_idx_` to new index.
        start_idx_ = last_log_index + 1;
        return true;
    }

private:
    static ptr<log_entry> make_clone(const ptr<log_entry> &entry) {
        return cs_new<log_entry>(
                entry->get_term(),
                buffer::copy(entry->get_buf()),
                entry->get_val_type()
        );
    }

    map<ulong, ptr<log_entry>> logs_;
    mutable mutex logs_lock_;
    atomic<ulong> start_idx_{};
};


