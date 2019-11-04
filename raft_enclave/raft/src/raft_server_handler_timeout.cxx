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

#include <raft_enclave_t.h>
#include "../include/cornerstone.hxx"

using namespace cornerstone;


void raft_server::handle_election_timeout() {
    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));
    recur_lock(lock_);
    if (steps_to_down_ > 0) {
        if (--steps_to_down_ == 0) {
            string line = "no hearing further news from leader, remove this server from cluster and step down";
            l_->info(line);
            for (auto it = config_->get_servers().begin(); it != config_->get_servers().end(); ++it) {
                if ((*it)->get_id() == id_) {
                    config_->get_servers().erase(it);
                    ctx_->state_mgr_->save_config(*config_);
                    break;
                }
            }

            ctx_->state_mgr_->system_exit(-1);
            throw raft_exception(line);
            return;
        }

        l_->info(sstrfmt("stepping down (cycles left: %d), skip this election timeout event").fmt(steps_to_down_));
        restart_election_timer();
        return;
    }

    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));

    if (catching_up_) {
        // this is a new server for the cluster, will not send out vote req until conf that includes this srv is committed
        l_->info("election timeout while joining the cluster, ignore it.");
        restart_election_timer();
        return;
    }

    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));

    if (role_ == srv_role::leader) {
        string line = "A leader should never encounter election timeout, illegal application state, stop the application";
        l_->err(line);
        ctx_->state_mgr_->system_exit(-1);
        throw raft_exception(line);
        return;
    }

    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));

    l_->debug("Election timeout, change to Candidate");
    state_->inc_term();
    state_->set_voted_for(-1);
    role_ = srv_role::candidate;
    votes_granted_ = 0;
    voted_servers_.clear();
    election_completed_ = false;
    ctx_->state_mgr_->save_state(*state_);
    request_vote();

    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));

    // restart the election timer if this is not yet a leader
    if (role_ != srv_role::leader) {
        restart_election_timer();
    }

    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));
}

void raft_server::handle_hb_timeout(peer &p) {
    recur_lock(lock_);
    l_->debug(sstrfmt("Heartbeat timeout for %d").fmt(p.get_id()));
    if (role_ == srv_role::leader) {
        request_append_entries(p);
        {
            std::lock_guard<std::mutex> guard(p.get_lock());
            if (p.is_hb_enabled()) {
                // Schedule another heartbeat if heartbeat is still enabled
                scheduler_->schedule(p.get_hb_task(), p.get_current_hb_interval());
            } else {
                l_->debug(sstrfmt("heartbeat is disabled for peer %d").fmt(p.get_id()));
            }
        }
    } else {
        l_->info(sstrfmt("Receive a heartbeat event for %d while no longer as a leader").fmt(p.get_id()));
    }
}
