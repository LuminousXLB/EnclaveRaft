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

extern const char *__msg_type_str[];


ptr<resp_msg> raft_server::handle_append_entries(req_msg &req) {
    if (req.get_term() == state_->get_term()) {
        if (role_ == srv_role::candidate) {
            become_follower();
        } else if (role_ == srv_role::leader) {
            string line = lstrfmt(
                    "Receive AppendEntriesRequest from another leader(%d) with same term, there must be a bug, server exits")
                    .fmt(req.get_src());
            l_->debug(line);
            ctx_->state_mgr_->system_exit(-1);
            throw raft_exception(line);
        } else {
            restart_election_timer();
        }
    }

    // After a snapshot the req.get_last_log_idx() may less than log_store_->next_slot() but equals to log_store_->next_slot() -1
    // In this case, log is Okay if req.get_last_log_idx() == lastSnapshot.get_last_log_idx() && req.get_last_log_term() == lastSnapshot.get_last_log_term()
    // In not accepted case, we will return log_store_->next_slot() for the leader to quick jump to the index that might aligned
    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::append_entries_response, id_, req.get_src(),
                                        log_store_->next_slot()));
    bool log_okay = req.get_last_log_idx() == 0 ||
                    (req.get_last_log_idx() < log_store_->next_slot() &&
                     req.get_last_log_term() == term_for_log(req.get_last_log_idx()));
    if (req.get_term() < state_->get_term() || !log_okay) {
        return resp;
    }

    // follower & log is okay
    if (!req.log_entries().empty()) {
        // write logs to store, start from overlapped logs
        ulong idx = req.get_last_log_idx() + 1;
        size_t log_idx = 0;
        while (idx < log_store_->next_slot() && log_idx < req.log_entries().size()) {
            if (log_store_->term_at(idx) == req.log_entries().at(log_idx)->get_term()) {
                idx++;
                log_idx++;
            } else {
                break;
            }
        }

        // dealing with overwrites
        while (idx < log_store_->next_slot() && log_idx < req.log_entries().size()) {
            ptr<log_entry> old_entry(log_store_->entry_at(idx));
            if (old_entry->get_val_type() == log_val_type::app_log) {
                state_machine_->rollback(idx, old_entry->get_buf());
            } else if (old_entry->get_val_type() == log_val_type::conf) {
                l_->info(sstrfmt("revert from a prev config change to config at %llu").fmt(config_->get_log_idx()));
                config_changing_ = false;
            }

            ptr<log_entry> entry = req.log_entries().at(log_idx);
            log_store_->write_at(idx, entry);
            if (entry->get_val_type() == log_val_type::app_log) {
                state_machine_->pre_commit(idx, entry->get_buf());
            } else if (entry->get_val_type() == log_val_type::conf) {
                l_->info(sstrfmt("receive a config change from leader at %llu").fmt(idx));
                config_changing_ = true;
            }

            idx += 1;
            log_idx += 1;
        }

        // append new log entries
        while (log_idx < req.log_entries().size()) {
            ptr<log_entry> entry = req.log_entries().at(log_idx++);
            ulong idx_for_entry = log_store_->append(entry);
            if (entry->get_val_type() == log_val_type::conf) {
                l_->info(sstrfmt("receive a config change from leader at %llu").fmt(idx_for_entry));
                config_changing_ = true;
            } else if (entry->get_val_type() == log_val_type::app_log) {
                state_machine_->pre_commit(idx_for_entry, entry->get_buf());
            }
        }
    }

    leader_ = req.get_src();
    commit(req.get_commit_idx());
    resp->accept(req.get_last_log_idx() + req.log_entries().size() + 1);
    return resp;
}

ptr<resp_msg> raft_server::handle_vote_req(req_msg &req) {
    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::request_vote_response, id_, req.get_src()));
    bool log_okay = req.get_last_log_term() > log_store_->last_entry()->get_term() ||
                    (req.get_last_log_term() == log_store_->last_entry()->get_term() &&
                     log_store_->next_slot() - 1 <= req.get_last_log_idx());
    bool grant = req.get_term() == state_->get_term() && log_okay &&
                 (state_->get_voted_for() == req.get_src() || state_->get_voted_for() == -1);
    if (grant) {
        resp->accept(log_store_->next_slot());
        state_->set_voted_for(req.get_src());
        ctx_->state_mgr_->save_state(*state_);
    }

    return resp;
}

ptr<resp_msg> raft_server::handle_cli_req(req_msg &req) {
    // optimization: check leader expiration
    static volatile int32 time_elapsed_since_quorum_resp(std::numeric_limits<int32>::max());

    l_->debug("====== TRACE START");
    l_->debug(lstrfmt("|>> %s | %s").fmt(__FILE__, __FUNCTION__));
    l_->debug(sstrfmt("| role_ == srv_role::leader -> %d").fmt(role_ == srv_role::leader));
    l_->debug(sstrfmt("| peers_.empty()            -> %d").fmt(peers_.empty()));
    l_->debug("====== TRACE END");

    if (role_ == srv_role::leader && !peers_.empty() &&
        time_elapsed_since_quorum_resp > ctx_->params_->election_timeout_upper_bound_ * 2) {

        std::vector<time_point> peer_resp_times;
        for (auto &peer : peers_) {
            peer_resp_times.push_back(peer.second->get_last_resp());
        }

        std::sort(peer_resp_times.begin(), peer_resp_times.end());
        int64_t timestamp;
        get_time_milliseconds(&timestamp);
        time_elapsed_since_quorum_resp = static_cast<int32>(timestamp - peer_resp_times[peers_.size() / 2]);


        if (time_elapsed_since_quorum_resp > ctx_->params_->election_timeout_upper_bound_ * 2) {
            return cs_new<resp_msg>(state_->get_term(), msg_type::append_entries_response, id_, -1);
        }
    }

    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::append_entries_response, id_, leader_));
    if (role_ != srv_role::leader) {
        return resp;
    }

    std::vector<ptr<log_entry>> &entries = req.log_entries();
    for (auto &entrie : entries) {
        // force the log's term to current term
        entrie->set_term(state_->get_term());

        log_store_->append(entrie);
        state_machine_->pre_commit(log_store_->next_slot() - 1, entrie->get_buf());
    }

    // urgent commit, so that the commit will not depend on hb
    request_append_entries();
    resp->accept(log_store_->next_slot());
    return resp;
}

void raft_server::handle_election_timeout() {
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

    if (catching_up_) {
        // this is a new server for the cluster, will not send out vote req until conf that includes this srv is committed
        l_->info("election timeout while joining the cluster, ignore it.");
        restart_election_timer();
        return;
    }

    if (role_ == srv_role::leader) {
        string line = "A leader should never encounter election timeout, illegal application state, stop the application";
        l_->err(line);
        ctx_->state_mgr_->system_exit(-1);
        throw raft_exception(line);
        return;
    }

    l_->debug("Election timeout, change to Candidate");
    state_->inc_term();
    state_->set_voted_for(-1);
    role_ = srv_role::candidate;
    votes_granted_ = 0;
    voted_servers_.clear();
    election_completed_ = false;
    ctx_->state_mgr_->save_state(*state_);
    request_vote();

    // restart the election timer if this is not yet a leader
    if (role_ != srv_role::leader) {
        restart_election_timer();
    }
}


ptr<resp_msg> raft_server::handle_extended_msg(req_msg &req) {
    switch (req.get_type()) {
        case msg_type::add_server_request:
            return handle_add_srv_req(req);
        case msg_type::remove_server_request:
            return handle_rm_srv_req(req);
        case msg_type::sync_log_request:
            return handle_log_sync_req(req);
        case msg_type::join_cluster_request:
            return handle_join_cluster_req(req);
        case msg_type::leave_cluster_request:
            return handle_leave_cluster_req(req);
        case msg_type::install_snapshot_request:
            return handle_install_snapshot_req(req);
        default:
            string line = sstrfmt("receive an unknown request %s, for safety, step down.").fmt(
                    __msg_type_str[req.get_type()]);
            l_->err(line);
            ctx_->state_mgr_->system_exit(-1);
            throw raft_exception(line);
            break;
    }

    return ptr<resp_msg>();
}

ptr<resp_msg> raft_server::handle_install_snapshot_req(req_msg &req) {
    if (req.get_term() == state_->get_term() && !catching_up_) {
        if (role_ == srv_role::candidate) {
            become_follower();
        } else if (role_ == srv_role::leader) {
            string line = lstrfmt(
                    "Receive InstallSnapshotRequest from another leader(%d) with same term, there must be a bug, server exits").fmt(
                    req.get_src());
            l_->err(line);
            ctx_->state_mgr_->system_exit(-1);
            throw raft_exception(line);
            return ptr<resp_msg>();
        } else {
            restart_election_timer();
        }
    }

    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::install_snapshot_response, id_, req.get_src()));
    if (!catching_up_ && req.get_term() < state_->get_term()) {
        l_->info("received an install snapshot request which has lower term than this server, decline the request");
        return resp;
    }

    std::vector<ptr<log_entry>> &entries(req.log_entries());
    if (entries.size() != 1 || entries[0]->get_val_type() != log_val_type::snp_sync_req) {
        l_->warn("Receive an invalid InstallSnapshotRequest due to bad log entries or bad log entry value");
        return resp;
    }

    ptr<snapshot_sync_req> sync_req(snapshot_sync_req::deserialize(entries[0]->get_buf()));
    if (sync_req->get_snapshot().get_last_log_idx() <= sm_commit_index_) {
        l_->warn(sstrfmt("received a snapshot (%llu) that is older than current log store").fmt(
                sync_req->get_snapshot().get_last_log_idx()));
        return resp;
    }

    if (handle_snapshot_sync_req(*sync_req)) {
        resp->accept(sync_req->get_offset() + sync_req->get_data().size());
    }

    return resp;
}

bool raft_server::handle_snapshot_sync_req(snapshot_sync_req &req) {
    try {
        state_machine_->save_snapshot_data(req.get_snapshot(), req.get_offset(), req.get_data());
        if (req.is_done()) {
            // Only follower will run this piece of code, but let's check it again
            if (role_ != srv_role::follower) {
                string line = "bad server role for applying a snapshot, exit for debugging";
                l_->err(line);
                ctx_->state_mgr_->system_exit(-1);
                throw raft_exception(line);
            }

            l_->debug("sucessfully receive a snapshot from leader");
            if (log_store_->compact(req.get_snapshot().get_last_log_idx())) {
                // The state machine will not be able to commit anything before the snapshot is applied, so make this synchronously
                // with election timer stopped as usually applying a snapshot may take a very long time
                stop_election_timer();
                l_->info("successfully compact the log store, will now ask the statemachine to apply the snapshot");
                if (!state_machine_->apply_snapshot(req.get_snapshot())) {
                    string line = "failed to apply the snapshot after log compacted, to ensure the safety, will shutdown the system";
                    l_->info(line);
                    ctx_->state_mgr_->system_exit(-1);
                    throw raft_exception(line);
                    return false;
                }

                reconfigure(req.get_snapshot().get_last_config());
                ctx_->state_mgr_->save_config(*config_);
                sm_commit_index_ = req.get_snapshot().get_last_log_idx();
                quick_commit_idx_ = req.get_snapshot().get_last_log_idx();
                ctx_->state_mgr_->save_state(*state_);
                last_snapshot_ = cs_new<snapshot>(
                        req.get_snapshot().get_last_log_idx(),
                        req.get_snapshot().get_last_log_term(),
                        config_,
                        req.get_snapshot().size());
                restart_election_timer();
                l_->info("snapshot is successfully applied");
            } else {
                l_->err("failed to compact the log store after a snapshot is received, will ask the leader to retry");
                return false;
            }
        }
    }
    catch (...) {
        string line = "failed to handle snapshot installation due to system errors";
        l_->err(line);
        ctx_->state_mgr_->system_exit(-1);
        throw raft_exception(line);
        return false;
    }

    return true;
}

void raft_server::handle_ext_resp(ptr<resp_msg> &resp, const ptr<rpc_exception> &err) {
    recur_lock(lock_);
    if (err) {
        handle_ext_resp_err(*err);
        return;
    }

    l_->debug(
            lstrfmt("Receive an extended %s message from peer %d with Result=%d, Term=%llu, NextIndex=%llu")
                    .fmt(
                            __msg_type_str[resp->get_type()],
                            resp->get_src(),
                            resp->get_accepted() ? 1 : 0,
                            resp->get_term(),
                            resp->get_next_idx()));

    switch (resp->get_type()) {
        case msg_type::sync_log_response:
            if (srv_to_join_) {
                // we are reusing heartbeat interval value to indicate when to stop retry
                srv_to_join_->resume_hb_speed();
                srv_to_join_->set_next_log_idx(resp->get_next_idx());
                srv_to_join_->set_matched_idx(resp->get_next_idx() - 1);
                sync_log_to_new_srv(resp->get_next_idx());
            }
            break;
        case msg_type::join_cluster_response:
            if (srv_to_join_) {
                if (resp->get_accepted()) {
                    l_->debug("new server confirms it will join, start syncing logs to it");
                    sync_log_to_new_srv(resp->get_next_idx());
                } else {
                    l_->debug("new server cannot accept the invitation, give up");
                }
            } else {
                l_->debug("no server to join, drop the message");
            }
            break;
        case msg_type::leave_cluster_response:
            if (!resp->get_accepted()) {
                l_->debug("peer doesn't accept to stepping down, stop proceeding");
                return;
            }

            l_->debug("peer accepted to stepping down, removing this server from cluster");
            rm_srv_from_cluster(resp->get_src());
            break;
        case msg_type::install_snapshot_response: {
            if (!srv_to_join_) {
                l_->info("no server to join, the response must be very old.");
                return;
            }

            if (!resp->get_accepted()) {
                l_->info("peer doesn't accept the snapshot installation request");
                return;
            }

            ptr<snapshot_sync_ctx> sync_ctx = srv_to_join_->get_snapshot_sync_ctx();
            if (sync_ctx == nilptr) {
                string line = "Bug! SnapshotSyncContext must not be null";
                l_->err(line);
                ctx_->state_mgr_->system_exit(-1);
                throw raft_exception(line);
                return;
            }

            if (resp->get_next_idx() >= sync_ctx->get_snapshot()->size()) {
                // snapshot is done
                ptr<snapshot> nil_snap;
                l_->debug("snapshot has been copied and applied to new server, continue to sync logs after snapshot");
                srv_to_join_->set_snapshot_in_sync(nil_snap);
                srv_to_join_->set_next_log_idx(sync_ctx->get_snapshot()->get_last_log_idx() + 1);
                srv_to_join_->set_matched_idx(sync_ctx->get_snapshot()->get_last_log_idx());
            } else {
                sync_ctx->set_offset(resp->get_next_idx());
                l_->debug(sstrfmt("continue to send snapshot to new server at offset %llu").fmt(resp->get_next_idx()));
            }

            sync_log_to_new_srv(srv_to_join_->get_next_log_idx());
        }
            break;
        default:
            string line = lstrfmt("received an unexpected response message type %s, for safety, stepping down").fmt(
                    __msg_type_str[resp->get_type()]);
            l_->err(line);
            ctx_->state_mgr_->system_exit(-1);
            throw raft_exception(line);
            break;
    }
}

void raft_server::handle_ext_resp_err(rpc_exception &err) {
    l_->debug(lstrfmt("receive an rpc error response from peer server, %s").fmt(err.what()));
    ptr<req_msg> req = err.req();
    if (req->get_type() == msg_type::sync_log_request ||
        req->get_type() == msg_type::join_cluster_request ||
        req->get_type() == msg_type::leave_cluster_request) {
        ptr<peer> p;
        if (req->get_type() == msg_type::leave_cluster_request) {
            peer_itor pit = peers_.find(req->get_dst());
            if (pit != peers_.end()) {
                p = pit->second;
            }
        } else {
            p = srv_to_join_;
        }

        if (p != nilptr) {
            if (p->get_current_hb_interval() >= ctx_->params_->max_hb_interval()) {
                if (req->get_type() == msg_type::leave_cluster_request) {
                    l_->info(
                            lstrfmt("rpc failed again for the removing server (%d), will remove this server directly").fmt(
                                    p->get_id()));

                    /**
                    * In case of there are only two servers in the cluster, it safe to remove the server directly from peers
                    * as at most one config change could happen at a time
                    *  prove:
                    *      assume there could be two config changes at a time
                    *      this means there must be a leader after previous leader offline, which is impossible
                    *      (no leader could be elected after one server goes offline in case of only two servers in a cluster)
                    * so the bug https://groups.google.com/forum/#!topic/raft-dev/t4xj6dJTP6E
                    * does not apply to cluster which only has two members
                    */
                    if (peers_.size() == 1) {
                        peer_itor pit = peers_.find(p->get_id());
                        if (pit != peers_.end()) {
                            pit->second->enable_hb(false);
                            peers_.erase(pit);
                            l_->info(sstrfmt("server %d is removed from cluster").fmt(p->get_id()));
                        } else {
                            l_->info(sstrfmt("peer %d cannot be found, no action for removing").fmt(p->get_id()));
                        }
                    }

                    rm_srv_from_cluster(p->get_id());
                } else {
                    l_->info(
                            lstrfmt("rpc failed again for the new coming server (%d), will stop retry for this server").fmt(
                                    p->get_id()));
                    config_changing_ = false;
                    srv_to_join_.reset();
                }
            } else {
                // reuse the heartbeat interval value to indicate when to stop retrying, as rpc backoff is the same
                l_->debug("retry the request");
                p->slow_down_hb();
                timer_task<void>::executor exec = (timer_task<void>::executor) std::bind(
                        &raft_server::on_retryable_req_err, this, p, req);
                ptr<delayed_task> task(cs_new<timer_task<void>>(exec));
                scheduler_->schedule(task, p->get_current_hb_interval());
            }
        }
    }
}


ptr<resp_msg> raft_server::handle_rm_srv_req(req_msg &req) {
    std::vector<ptr<log_entry>> &entries(req.log_entries());
    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::remove_server_response, id_, leader_));
    if (entries.size() != 1 || entries[0]->get_buf().size() != sz_int) {
        l_->info("bad remove server request as we are expecting one log entry with value type of int");
        return resp;
    }

    if (role_ != srv_role::leader) {
        l_->info("this is not a leader, cannot handle RemoveServerRequest");
        return resp;
    }

    if (config_changing_) {
        // the previous config has not committed yet
        l_->info("previous config has not committed yet");
        return resp;
    }

    int32 srv_id = entries[0]->get_buf().get_int();
    if (srv_id == id_) {
        l_->info("cannot request to remove leader");
        return resp;
    }

    peer_itor pit = peers_.find(srv_id);
    if (pit == peers_.end()) {
        l_->info(sstrfmt("server %d does not exist").fmt(srv_id));
        return resp;
    }

    ptr<peer> p = pit->second;
    ptr<req_msg> leave_req(cs_new<req_msg>(state_->get_term(), msg_type::leave_cluster_request, id_, srv_id, 0,
                                           log_store_->next_slot() - 1, quick_commit_idx_));
    p->send_req(leave_req, ex_resp_handler_);
    resp->accept(log_store_->next_slot());
    return resp;
}

ptr<resp_msg> raft_server::handle_add_srv_req(req_msg &req) {

    std::vector<ptr<log_entry>> &entries(req.log_entries());
    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::add_server_response, id_, leader_));
    if (entries.size() != 1 || entries[0]->get_val_type() != log_val_type::cluster_server) {
        l_->debug("bad add server request as we are expecting one log entry with value type of ClusterServer");
        return resp;
    }

    if (role_ != srv_role::leader) {
        l_->info("this is not a leader, cannot handle AddServerRequest");
        return resp;
    }

    ptr<srv_config> srv_conf(srv_config::deserialize(entries[0]->get_buf()));
    if (peers_.find(srv_conf->get_id()) != peers_.end() || id_ == srv_conf->get_id()) {
        l_->warn(lstrfmt("the server to be added has a duplicated id with existing server %d").fmt(srv_conf->get_id()));
        return resp;
    }

    if (config_changing_) {
        // the previous config has not committed yet
        l_->info("previous config has not committed yet");
        return resp;
    }

    conf_to_add_ = std::move(srv_conf);
    timer_task<peer &>::executor exec = (timer_task<peer &>::executor) std::bind(&raft_server::handle_hb_timeout, this,
                                                                                 std::placeholders::_1);
    srv_to_join_ = cs_new<peer, ptr<srv_config> &, context &, timer_task<peer &>::executor &>(conf_to_add_, *ctx_,
                                                                                              exec);
    invite_srv_to_join_cluster();
    resp->accept(log_store_->next_slot());
    return resp;
}

ptr<resp_msg> raft_server::handle_log_sync_req(req_msg &req) {
    std::vector<ptr<log_entry>> &entries = req.log_entries();
    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::sync_log_response, id_, req.get_src()));
    if (entries.size() != 1 || entries[0]->get_val_type() != log_val_type::log_pack) {
        l_->info("receive an invalid LogSyncRequest as the log entry value doesn't meet the requirements");
        return resp;
    }

    if (!catching_up_) {
        l_->info("This server is ready for cluster, ignore the request");
        return resp;
    }

    log_store_->apply_pack(req.get_last_log_idx() + 1, entries[0]->get_buf());
    commit(log_store_->next_slot() - 1);
    resp->accept(log_store_->next_slot());
    return resp;
}


ptr<resp_msg> raft_server::handle_join_cluster_req(req_msg &req) {
    std::vector<ptr<log_entry>> &entries = req.log_entries();
    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::join_cluster_response, id_, req.get_src()));
    if (entries.size() != 1 || entries[0]->get_val_type() != log_val_type::conf) {
        l_->info("receive an invalid JoinClusterRequest as the log entry value doesn't meet the requirements");
        return resp;
    }

    if (catching_up_) {
        l_->info("this server is already in log syncing mode");
        return resp;
    }

    catching_up_ = true;
    role_ = srv_role::follower;
    leader_ = req.get_src();
    sm_commit_index_ = 0;
    quick_commit_idx_ = 0;
    state_->set_voted_for(-1);
    state_->set_term(req.get_term());
    ctx_->state_mgr_->save_state(*state_);
    reconfigure(cluster_config::deserialize(entries[0]->get_buf()));
    resp->accept(log_store_->next_slot());
    return resp;
}

ptr<resp_msg> raft_server::handle_leave_cluster_req(req_msg &req) {
    ptr<resp_msg> resp(cs_new<resp_msg>(state_->get_term(), msg_type::leave_cluster_response, id_, req.get_src()));
    if (!config_changing_) {
        steps_to_down_ = 2;
        resp->accept(log_store_->next_slot());
    }

    return resp;
}

