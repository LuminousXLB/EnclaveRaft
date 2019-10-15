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

#ifndef _CLUSTER_CONFIG_HXX_
#define _CLUSTER_CONFIG_HXX_

#include "srv_config.hxx"

namespace cornerstone {
    class cluster_config {
    public:
        explicit cluster_config(ulong log_idx = 0L, ulong prev_log_idx = 0L)
                : log_idx_(log_idx), prev_log_idx_(prev_log_idx), servers_() {}

        ~cluster_config() = default;

    __nocopy__(cluster_config)

    public:
        typedef std::list<ptr<srv_config>>::iterator srv_itor;
        typedef std::list<ptr<srv_config>>::const_iterator const_srv_itor;

        ulong get_log_idx() const {
            return log_idx_;
        }

        void set_log_idx(ulong log_idx) {
            prev_log_idx_ = log_idx_;
            log_idx_ = log_idx;
        }

        ulong get_prev_log_idx() const {
            return prev_log_idx_;
        }

        std::list<ptr<srv_config>> &

        get_servers() {
            return servers_;
        }

        ptr<srv_config> get_server(int id) const {
            for (const auto &server : servers_) {
                if (server->get_id() == id) {
                    return server;
                }
            }

            return ptr<srv_config>();
        }

        bufptr serialize() {
            size_t sz = 2 * sz_ulong + sz_int;
            std::vector<bufptr> srv_buffs;
            for (cluster_config::const_srv_itor it = servers_.begin(); it != servers_.end(); ++it) {
                bufptr buf = (*it)->serialize();
                sz += buf->size();
                srv_buffs.emplace_back(std::move(buf));
            }

            bufptr result = buffer::alloc(sz);
            result->put(log_idx_);
            result->put(prev_log_idx_);
            result->put((int32) servers_.size());
            for (auto &item : srv_buffs) {
                result->put(*item);
            }

            result->pos(0);
            return result;
        }

        static ptr<cluster_config> deserialize(buffer &buf) {
            ulong log_idx = buf.get_ulong();
            ulong prev_log_idx = buf.get_ulong();
            int32 cnt = buf.get_int();
            ptr<cluster_config> conf = cs_new<cluster_config>(log_idx, prev_log_idx);
            while (cnt-- > 0) {
                conf->get_servers().push_back(srv_config::deserialize(buf));
            }

            return conf;
        }

    private:
        ulong log_idx_;
        ulong prev_log_idx_;
        std::list<ptr<srv_config>> servers_;
    };
}

#endif //_CLUSTER_CONFIG_HXX_