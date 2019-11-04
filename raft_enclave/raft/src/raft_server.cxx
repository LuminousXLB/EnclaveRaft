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

const int raft_server::default_snapshot_sync_block_size = 4 * 1024;

void raft_server::request_vote() {
    l_->info(sstrfmt("requestVote started with term %llu (%016llx)").fmt(state_->get_term(), state_->get_term()));
    state_->set_voted_for(id_);
    ctx_->state_mgr_->save_state(*state_);
    votes_granted_ += 1;
    voted_servers_.insert(id_);

    // is this the only server?
    if (votes_granted_ > (int32) (peers_.size() + 1) / 2) {
        election_completed_ = true;
        become_leader();
        return;
    }

    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        ptr<req_msg> req = cs_new<req_msg>(state_->get_term(),
                                           msg_type::request_vote_request,
                                           id_,
                                           it->second->get_id(),
                                           term_for_log(log_store_->next_slot() - 1),
                                           log_store_->next_slot() - 1,
                                           quick_commit_idx_);
        l_->debug(sstrfmt("send %s to server %d with term %llu")
                          .fmt(msg_type_string(req->get_type()),
                               it->second->get_id(),
                               state_->get_term()));
        it->second->send_req(req, resp_handler_);
    }
}

void raft_server::request_append_entries() {
    if (peers_.empty()) {
        commit(log_store_->next_slot() - 1);
        return;
    }

    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        request_append_entries(*it->second);
    }
}

bool raft_server::request_append_entries(peer &p) {
    if (p.make_busy()) {
        ptr<req_msg> msg = create_append_entries_req(p);
        p.send_req(msg, resp_handler_);
        return true;
    }

    l_->debug(sstrfmt("Server %d is busy, skip the request").fmt(p.get_id()));
    return false;
}

void raft_server::restart_election_timer() {
    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));

    // don't start the election timer while this server is still catching up the logs
    if (catching_up_) {
        return;
    }

    if (election_task_) {
        scheduler_->cancel(election_task_);
    } else {
        election_task_ = cs_new<timer_task<void>>(election_exec_);
    }
    int32_t timeout = rand_timeout_();

    scheduler_->schedule(election_task_, timeout);
}

void raft_server::stop_election_timer() {
    if (!election_task_) {
        l_->warn("Election Timer is never started but is requested to stop, protential a bug");
        return;
    }

    scheduler_->cancel(election_task_);
}

void raft_server::become_leader() {
    stop_election_timer();
    role_ = srv_role::leader;
    leader_ = id_;
    srv_to_join_.reset();
    ptr<snapshot> nil_snp;
    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        it->second->set_next_log_idx(log_store_->next_slot());
        it->second->set_snapshot_in_sync(nil_snp);
        it->second->set_free();
        enable_hb_for_peer(*(it->second));
    }

    if (config_->get_log_idx() == 0) {
        config_->set_log_idx(log_store_->next_slot());
        bufptr conf_buf = config_->serialize();
        ptr<log_entry> entry(cs_new<log_entry>(state_->get_term(), std::move(conf_buf), log_val_type::conf));
        log_store_->append(entry);
        l_->info("save initial config to log store");
        config_changing_ = true;
    }

    request_append_entries();
}

void raft_server::enable_hb_for_peer(peer &p) {
    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));

    p.enable_hb(true);
    p.resume_hb_speed();
    scheduler_->schedule(p.get_hb_task(), p.get_current_hb_interval());
}

void raft_server::become_follower() {
    l_->debug(lstrfmt("%s %s %d: CurrentLeader=%d. TRACE").fmt(__FILE__, __FUNCTION__, __LINE__, leader_));

    // stop hb for all peers
    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));
        it->second->enable_hb(false);
    }

    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));
    srv_to_join_.reset();
    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));
    role_ = srv_role::follower;

    restart_election_timer();
}

bool raft_server::update_term(ulong term) {
    l_->debug(lstrfmt("%s %s %d: CurrentLeader=%d. TRACE").fmt(__FILE__, __FUNCTION__, __LINE__, leader_));

    if (term > state_->get_term()) {
        state_->set_term(term);
        state_->set_voted_for(-1);
        election_completed_ = false;

        votes_granted_ = 0;
        voted_servers_.clear();
        ctx_->state_mgr_->save_state(*state_);

        become_follower();

        return true;
    }

    return false;
}

void raft_server::commit(ulong target_idx) {
    if (target_idx > quick_commit_idx_) {
        quick_commit_idx_ = target_idx;

        // if this is a leader notify peers to commit as well
        // for peers that are free, send the request, otherwise, set pending commit flag for that peer
        if (role_ == srv_role::leader) {
            for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
                if (!request_append_entries(*(it->second))) {
                    it->second->set_pending_commit();
                }
            }
        }
    }

    if (log_store_->next_slot() - 1 > sm_commit_index_ && quick_commit_idx_ > sm_commit_index_) {
        commit_cv_.notify_one();
    }
}

void raft_server::snapshot_and_compact(ulong committed_idx) {
    if (ctx_->params_->snapshot_distance_ == 0 ||
        (committed_idx - log_store_->start_index()) < (ulong) ctx_->params_->snapshot_distance_) {
        // snapshot is disabled or the log store is not long enough
        return;
    }

    bool snapshot_in_action = false;
    try {
        bool f = false;
        if ((!last_snapshot_ ||
             (committed_idx - last_snapshot_->get_last_log_idx()) >= (ulong) ctx_->params_->snapshot_distance_) &&
            snp_in_progress_.compare_exchange_strong(f, true)) {
            snapshot_in_action = true;
            l_->info(sstrfmt("creating a snapshot for index %llu").fmt(committed_idx));

            // get the latest configuration info
            ptr<cluster_config> conf(config_);
            while (conf->get_log_idx() > committed_idx && conf->get_prev_log_idx() >= log_store_->start_index()) {
                ptr<log_entry> conf_log(log_store_->entry_at(conf->get_prev_log_idx()));
                conf = cluster_config::deserialize(conf_log->get_buf());
            }

            if (conf->get_log_idx() > committed_idx &&
                conf->get_prev_log_idx() > 0 &&
                conf->get_prev_log_idx() < log_store_->start_index()) {
                if (!last_snapshot_) {
                    string line = "No snapshot could be found while no configuration cannot be found in current committed logs, this is a system error, exiting";
                    l_->err(line);
                    ctx_->state_mgr_->system_exit(-1);
                    throw raft_exception(line);
                    return;
                }

                conf = last_snapshot_->get_last_config();
            } else if (conf->get_log_idx() > committed_idx && conf->get_prev_log_idx() == 0) {
                string line = "BUG!!! stop the system, there must be a configuration at index one";
                l_->err(line);
                ctx_->state_mgr_->system_exit(-1);
                throw raft_exception(line);
                return;
            }

            ulong log_term_to_compact = log_store_->term_at(committed_idx);
            ptr<snapshot> new_snapshot(cs_new<snapshot>(committed_idx, log_term_to_compact, conf));
            async_result<bool>::handler_type handler = (async_result<bool>::handler_type) std::bind(
                    &raft_server::on_snapshot_completed, this, new_snapshot, std::placeholders::_1,
                    std::placeholders::_2);
            state_machine_->create_snapshot(
                    *new_snapshot,
                    handler);
            snapshot_in_action = false;
        }
    }
    catch (...) {
        l_->err(sstrfmt("failed to compact logs at index %llu due to errors").fmt(committed_idx));
        if (snapshot_in_action) {
            bool val = true;
            snp_in_progress_.compare_exchange_strong(val, false);
        }
    }
}

void raft_server::on_snapshot_completed(ptr<snapshot> &s, bool result, const ptr<std::exception> &err) {
    do {
        if (err != nilptr) {
            l_->err(lstrfmt("failed to create a snapshot due to %s").fmt(err->what()));
            break;
        }

        if (!result) {
            l_->info("the state machine rejects to create the snapshot");
            break;
        }

        {
            recur_lock(lock_);
            l_->debug("snapshot created, compact the log store");

            last_snapshot_ = s;
            if (s->get_last_log_idx() > (ulong) ctx_->params_->reserved_log_items_) {
                log_store_->compact(s->get_last_log_idx() - (ulong) ctx_->params_->reserved_log_items_);
            }
        }
    } while (false);
    snp_in_progress_.store(false);
}

ptr<req_msg> raft_server::create_append_entries_req(peer &p) {
    ulong cur_nxt_idx(0L);
    ulong commit_idx(0L);
    ulong last_log_idx(0L);
    ulong term(0L);
    ulong starting_idx(1L);

    {
        recur_lock(lock_);
        starting_idx = log_store_->start_index();
        cur_nxt_idx = log_store_->next_slot();
        commit_idx = quick_commit_idx_;
        term = state_->get_term();
    }

    {
        std::lock_guard<std::mutex> guard(p.get_lock());
        if (p.get_next_log_idx() == 0L) {
            p.set_next_log_idx(cur_nxt_idx);
        }

        last_log_idx = p.get_next_log_idx() - 1;
    }

    if (last_log_idx >= cur_nxt_idx) {
        string line = sstrfmt("Peer's lastLogIndex is too large %llu v.s. %llu, server exits").fmt(last_log_idx,
                                                                                                   cur_nxt_idx);
        l_->err(line);
        ctx_->state_mgr_->system_exit(-1);
        throw raft_exception(line);
        return ptr<req_msg>();
    }

    // for syncing the snapshots, for starting_idx - 1, we can check with last snapshot
    if (last_log_idx > 0 && last_log_idx < starting_idx - 1) {
        return create_sync_snapshot_req(p, last_log_idx, term, commit_idx);
    }

    ulong last_log_term = term_for_log(last_log_idx);
    ulong end_idx = std::min(cur_nxt_idx, last_log_idx + 1 + ctx_->params_->max_append_size_);
    ptr<std::vector<ptr<log_entry>>> log_entries = (last_log_idx + 1) >= cur_nxt_idx ?
                                                   ptr<std::vector<ptr<log_entry>>>() :
                                                   log_store_->log_entries(last_log_idx + 1, end_idx);
    l_->debug(
            lstrfmt("An AppendEntries Request for %d with LastLogIndex=%llu, LastLogTerm=%llu, EntriesLength=%d, CommitIndex=%llu and Term=%llu")
                    .fmt(p.get_id(),
                         last_log_idx,
                         last_log_term,
                         log_entries ? log_entries->size() : 0,
                         commit_idx,
                         term));

    ptr<req_msg> req = cs_new<req_msg>(term,
                                       msg_type::append_entries_request,
                                       id_,
                                       p.get_id(),
                                       last_log_term,
                                       last_log_idx,
                                       commit_idx);

    std::vector<ptr<log_entry>> &v = req->log_entries();
    if (log_entries) {
        v.insert(v.end(), log_entries->begin(), log_entries->end());
    }

    return req;
}

void raft_server::reconfigure(const ptr<cluster_config> &new_config) {
    // we only allow one server to be added or removed at a time

    l_->debug(lstrfmt("system is reconfigured to have %d servers, last config index: %llu, this config index: %llu")
                      .fmt(new_config->get_servers().size(),
                           new_config->get_prev_log_idx(),
                           new_config->get_log_idx()));

    // `srvs_added` <- find the servers in `new_config` but not in `peers_` and not myself
    std::vector<ptr<srv_config>> srvs_added;
    std::list<ptr<srv_config>> &new_srvs = new_config->get_servers();
    for (std::list<ptr<srv_config>>::const_iterator it = new_srvs.begin(); it != new_srvs.end(); ++it) {
        peer_itor pit = peers_.find((*it)->get_id());
        if (pit == peers_.end() && id_ != (*it)->get_id()) {
            srvs_added.push_back(*it);
        }
    }

    // `srvs_removed` <- find the servers in `new_srvs` but not in `peers_` and not myself
    std::vector<int32> srvs_removed;
    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        if (!new_config->get_server(it->first)) {
            srvs_removed.push_back(it->first);
        }
    }

    // check if myself in new_config
    if (!new_config->get_server(id_)) {
        srvs_removed.push_back(id_);
    }

    // add servers
    for (std::vector<ptr<srv_config>>::const_iterator it = srvs_added.begin(); it != srvs_added.end(); ++it) {
        // construct srv_config
        ptr<srv_config> srv_added(*it);

        // construct timeout_handler
        auto exec = (timer_task<peer &>::executor) std::bind(&raft_server::handle_hb_timeout,
                                                             this,
                                                             std::placeholders::_1);

        // construct peer
        ptr<peer> p = cs_new<peer, ptr<srv_config> &, context &, timer_task<peer &>::executor &>(srv_added,
                                                                                                 *ctx_,
                                                                                                 exec);
        p->set_next_log_idx(log_store_->next_slot());

        // insert the peer
        peers_.insert(std::make_pair(srv_added->get_id(), p));
        l_->info(sstrfmt("server %d is added to cluster").fmt(srv_added->get_id()));

        if (role_ == srv_role::leader) {
            l_->info(sstrfmt("enable heartbeating for server %d").fmt(srv_added->get_id()));
            enable_hb_for_peer(*p);
            if (srv_to_join_ && srv_to_join_->get_id() == p->get_id()) {
                p->set_next_log_idx(srv_to_join_->get_next_log_idx());
                srv_to_join_.reset();
            }
        }
    }

    // remove servers
    for (std::vector<int32>::const_iterator it = srvs_removed.begin(); it != srvs_removed.end(); ++it) {
        int32 srv_removed = *it;
        if (srv_removed == id_ && !catching_up_) {
            // this server is removed from cluster
            ctx_->state_mgr_->save_config(*new_config);
            l_->info("server has been removed, step down");
            ctx_->state_mgr_->system_exit(0);
            return;
        }

        peer_itor pit = peers_.find(srv_removed);
        if (pit != peers_.end()) {
            pit->second->enable_hb(false);
            peers_.erase(pit);
            l_->info(sstrfmt("server %d is removed from cluster").fmt(srv_removed));
        } else {
            l_->info(sstrfmt("peer %d cannot be found, no action for removing").fmt(srv_removed));
        }
    }

    config_ = new_config;
}

void raft_server::on_retryable_req_err(ptr<peer> &p, ptr<req_msg> &req) {
    l_->debug(sstrfmt("retry the request %s for %d").fmt(msg_type_string(req->get_type()), p->get_id()));
    p->send_req(req, ex_resp_handler_);
}

void raft_server::sync_log_to_new_srv(ulong start_idx) {
    // only sync committed logs
    auto gap = (int32) (quick_commit_idx_ - start_idx);
    if (gap < ctx_->params_->log_sync_stop_gap_) {
        l_->info(lstrfmt("LogSync is done for server %d with log gap %d, now put the server into cluster").fmt(
                srv_to_join_->get_id(), gap));
        ptr<cluster_config> new_conf = cs_new<cluster_config>(log_store_->next_slot(), config_->get_log_idx());
        new_conf->get_servers().insert(new_conf->get_servers().end(), config_->get_servers().begin(),
                                       config_->get_servers().end());
        new_conf->get_servers().push_back(conf_to_add_);
        bufptr new_conf_buf(new_conf->serialize());
        ptr<log_entry> entry(cs_new<log_entry>(state_->get_term(), std::move(new_conf_buf), log_val_type::conf));
        log_store_->append(entry);
        config_changing_ = true;
        request_append_entries();
        return;
    }

    ptr<req_msg> req;
    if (start_idx > 0 && start_idx < log_store_->start_index()) {
        req = create_sync_snapshot_req(*srv_to_join_, start_idx, state_->get_term(), quick_commit_idx_);
    } else {
        int32 size_to_sync = std::min(gap, ctx_->params_->log_sync_batch_size_);
        bufptr log_pack = log_store_->pack(start_idx, size_to_sync);
        req = cs_new<req_msg>(state_->get_term(), msg_type::sync_log_request, id_, srv_to_join_->get_id(), 0L,
                              start_idx - 1, quick_commit_idx_);
        req->log_entries().push_back(
                cs_new<log_entry>(state_->get_term(), std::move(log_pack), log_val_type::log_pack));
    }

    srv_to_join_->send_req(req, ex_resp_handler_);
}

void raft_server::invite_srv_to_join_cluster() {
    ptr<req_msg> req = cs_new<req_msg>(state_->get_term(),
                                       msg_type::join_cluster_request,
                                       id_,
                                       srv_to_join_->get_id(),
                                       0L,
                                       log_store_->next_slot() - 1,
                                       quick_commit_idx_);

    ptr<log_entry> entry = cs_new<log_entry>(state_->get_term(), config_->serialize(), log_val_type::conf);
    req->log_entries().push_back(entry);
    srv_to_join_->send_req(req, ex_resp_handler_);
}

void raft_server::rm_srv_from_cluster(int32 srv_id) {
    ptr<cluster_config> new_conf = cs_new<cluster_config>(log_store_->next_slot(), config_->get_log_idx());
    for (cluster_config::const_srv_itor it = config_->get_servers().begin(); it != config_->get_servers().end(); ++it) {
        if ((*it)->get_id() != srv_id) {
            new_conf->get_servers().push_back(*it);
        }
    }

    l_->info(lstrfmt("removed a server from configuration and save the configuration to log store at %llu").fmt(
            new_conf->get_log_idx()));
    config_changing_ = true;
    bufptr new_conf_buf(new_conf->serialize());
    ptr<log_entry> entry(cs_new<log_entry>(state_->get_term(), std::move(new_conf_buf), log_val_type::conf));
    log_store_->append(entry);
    request_append_entries();
}

int32 raft_server::get_snapshot_sync_block_size() const {
    int32 block_size = ctx_->params_->snapshot_block_size_;
    return block_size == 0 ? default_snapshot_sync_block_size : block_size;
}

ptr<req_msg> raft_server::create_sync_snapshot_req(peer &p, ulong last_log_idx, ulong term, ulong commit_idx) {
    std::lock_guard<std::mutex> guard(p.get_lock());
    ptr<snapshot_sync_ctx> sync_ctx = p.get_snapshot_sync_ctx();
    ptr<snapshot> snp;
    if (sync_ctx != nilptr) {
        snp = sync_ctx->get_snapshot();
    }

    if (!snp || (last_snapshot_ && last_snapshot_->get_last_log_idx() > snp->get_last_log_idx())) {
        snp = last_snapshot_;
        if (snp == nilptr || last_log_idx > snp->get_last_log_idx()) {
            string line = lstrfmt(
                    "system is running into fatal errors, failed to find a snapshot for peer %d(snapshot null: %d, snapshot doesn't contais lastLogIndex: %d")
                    .fmt(p.get_id(), snp == nilptr ? 1 : 0, last_log_idx > snp->get_last_log_idx() ? 1 : 0);
            l_->err(
                    line);
            ctx_->state_mgr_->system_exit(-1);
            throw raft_exception(line);
            return ptr<req_msg>();
        }

        if (snp->size() < 1L) {
            string line = "invalid snapshot, this usually means a bug from state machine implementation, stop the system to prevent further errors";
            l_->err(line);
            ctx_->state_mgr_->system_exit(-1);
            throw raft_exception(line);
            return ptr<req_msg>();
        }

        l_->info(sstrfmt("trying to sync snapshot with last index %llu to peer %d").fmt(snp->get_last_log_idx(),
                                                                                        p.get_id()));
        p.set_snapshot_in_sync(snp);
    }

    ulong offset = p.get_snapshot_sync_ctx()->get_offset();
    auto sz_left = (int32) (snp->size() - offset);
    int32 blk_sz = get_snapshot_sync_block_size();
    bufptr data = buffer::alloc((size_t) (std::min(blk_sz, sz_left)));
    int32 sz_rd = state_machine_->read_snapshot_data(*snp, offset, *data);
    if ((size_t) sz_rd < data->size()) {
        string line = lstrfmt(
                "only %d bytes could be read from snapshot while %d bytes are expected, must be something wrong, exit.").fmt(
                sz_rd, data->size());
        l_->err(line);
        ctx_->state_mgr_->system_exit(-1);
        throw raft_exception(line);
        return ptr<req_msg>();
    }

    bool done = (offset + (ulong) data->size()) >= snp->size();
    std::unique_ptr<snapshot_sync_req> sync_req(new snapshot_sync_req(snp, offset, std::move(data), done));
    ptr<req_msg> req(
            cs_new<req_msg>(term, msg_type::install_snapshot_request, id_, p.get_id(), snp->get_last_log_term(),
                            snp->get_last_log_idx(), commit_idx));
    req->log_entries().push_back(cs_new<log_entry>(term, sync_req->serialize(), log_val_type::snp_sync_req));
    return req;
}

ulong raft_server::term_for_log(ulong log_idx) {
    if (log_idx == 0) {
        return 0L;
    }

    if (log_idx >= log_store_->start_index()) {
        return log_store_->term_at(log_idx);
    }

    ptr<snapshot> last_snapshot(state_machine_->last_snapshot());
    if (!last_snapshot || log_idx != last_snapshot->get_last_log_idx()) {
        string line = sstrfmt(
                "bad log_idx %llu for retrieving the term value, kill the system to protect the system").fmt(
                log_idx);
        l_->err(line);
        ctx_->state_mgr_->system_exit(-1);
        throw raft_exception(line);
    }

    return last_snapshot->get_last_log_term();
}

ptr<async_result<bool>> raft_server::send_msg_to_leader(ptr<req_msg> &req) {
    typedef std::unordered_map<int32, ptr<rpc_client>>::const_iterator rpc_client_itor;
    int32 leader_id = leader_;
    ptr<cluster_config> cluster = config_;
    bool result(false);
    if (leader_id == -1) {
        return cs_new<async_result<bool>>(result);
    }

    if (leader_id == id_) {
        ptr<resp_msg> resp = process_req(*req);
        result = resp->get_accepted();
        return cs_new<async_result<bool>>(result);
    }

    ptr<rpc_client> rpc_cli;
    {
        auto_lock(rpc_clients_lock_);
        rpc_client_itor itor = rpc_clients_.find(leader_id);
        if (itor == rpc_clients_.end()) {
            ptr<srv_config> srv_conf = config_->get_server(leader_id);
            if (!srv_conf) {
                return cs_new<async_result<bool>>(result);
            }

            rpc_cli = ctx_->rpc_cli_factory_->create_client(srv_conf->get_endpoint());
            rpc_clients_.insert(std::make_pair(leader_id, rpc_cli));
        } else {
            rpc_cli = itor->second;
        }
    }

    if (!rpc_cli) {
        return cs_new<async_result<bool>>(result);
    }

    ptr<async_result<bool>> presult(cs_new<async_result<bool>>());
    rpc_handler handler = [presult](ptr<resp_msg> &resp, const ptr<rpc_exception> &err) -> void {
        bool rpc_success(false);
        ptr<std::exception> perr;
        if (err) {
            perr = err;
        } else {
            rpc_success = resp && resp->get_accepted();
        }

        presult->set_result(rpc_success, perr);
    };
    rpc_cli->send(req, handler);
    return presult;
}


void raft_server::commit_in_bg() {
    while (true) {
        try {
            while (quick_commit_idx_ <= sm_commit_index_ || sm_commit_index_ >= log_store_->next_slot() - 1) {
                std::unique_lock<std::mutex> lock(commit_lock_);
                commit_cv_.wait(lock);
                if (stopping_) {
                    lock.unlock();
                    lock.release();
                    {
                        auto_lock(stopping_lock_);
                        ready_to_stop_cv_.notify_all();
                    }

                    return;
                }
            }

            while (sm_commit_index_ < quick_commit_idx_ && sm_commit_index_ < log_store_->next_slot() - 1) {
                sm_commit_index_ += 1;
                ptr<log_entry> log_entry(log_store_->entry_at(sm_commit_index_));
                if (log_entry->get_val_type() == log_val_type::app_log) {
                    state_machine_->commit(sm_commit_index_, log_entry->get_buf());
                } else if (log_entry->get_val_type() == log_val_type::conf) {
                    recur_lock(lock_);
                    log_entry->get_buf().pos(0);
                    ptr<cluster_config> new_conf = cluster_config::deserialize(log_entry->get_buf());
                    l_->info(sstrfmt("config at index %llu is committed").fmt(new_conf->get_log_idx()));
                    ctx_->state_mgr_->save_config(*new_conf);
                    config_changing_ = false;
                    if (config_->get_log_idx() < new_conf->get_log_idx()) {
                        reconfigure(new_conf);
                    }

                    if (catching_up_ && new_conf->get_server(id_) != nilptr) {
                        l_->info("this server is committed as one of cluster members");
                        catching_up_ = false;
                    }
                }

                snapshot_and_compact(sm_commit_index_);
            }
        }
        catch (std::exception &err) {
            string line = lstrfmt("background committing thread encounter err %s, exiting to protect the system").fmt(
                    err.what());
            l_->err(line);
            ctx_->state_mgr_->system_exit(-1);
            throw raft_exception(line);
        }
    }
}
