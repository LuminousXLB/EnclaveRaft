/**
*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  The ASF licenses
* this file to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef _SNAPSHOT_HXX_
#define _SNAPSHOT_HXX_

#include "../cluster_config.hxx"

namespace cornerstone {
    class snapshot {
    public:
        snapshot(ulong last_log_idx, ulong last_log_term, const ptr<cluster_config> &last_config, ulong size = 0)
                : last_log_idx_(last_log_idx), last_log_term_(last_log_term), size_(size), last_config_(last_config) {}

    __nocopy__(snapshot)

    public:
        ulong get_last_log_idx() const {
            return last_log_idx_;
        }

        ulong get_last_log_term() const {
            return last_log_term_;
        }

        ulong size() const {
            return size_;
        }

        const ptr<cluster_config> &get_last_config() const {
            return last_config_;
        }

        bufptr serialize() {
            bufptr conf_buf = last_config_->serialize();
            bufptr buf = buffer::alloc(conf_buf->size() + sz_ulong * 3);
            buf->put(last_log_idx_);
            buf->put(last_log_term_);
            buf->put(size_);
            buf->put(*conf_buf);
            buf->pos(0);
            return buf;
        }

        static ptr<snapshot> deserialize(buffer &buf) {
            ulong last_log_idx = buf.get_ulong();
            ulong last_log_term = buf.get_ulong();
            ulong size = buf.get_ulong();
            ptr<cluster_config> conf(cluster_config::deserialize(buf));
            return cs_new<snapshot>(last_log_idx, last_log_term, conf, size);
        }

    private:
        ulong last_log_idx_;
        ulong last_log_term_;
        ulong size_;
        ptr<cluster_config> last_config_;
    };
}

#endif