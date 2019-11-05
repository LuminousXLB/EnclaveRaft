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

#include "../include/cornerstone.hxx"

using namespace cornerstone;


ptr<logger> raft_server::get_logger() {
    return l_;
}

ptr<resp_msg> raft_server::process_req(req_msg &req) {
    recur_lock(lock_);
    l_->debug(
            lstrfmt("[%s] from %d -> REQUEST with LastLogIndex=%llu, LastLogTerm=%llu, EntriesLength=%d, CommitIndex=%llu and Term=%llu [CurrentLeader=%d]")
                    .fmt(msg_type_string(req.get_type()),
                         req.get_src(),
                         req.get_last_log_idx(),
                         req.get_last_log_term(),
                         req.log_entries().size(),
                         req.get_commit_idx(),
                         req.get_term(),
                         leader_));

    if (req.get_type() == msg_type::append_entries_request ||
        req.get_type() == msg_type::request_vote_request ||
        req.get_type() == msg_type::install_snapshot_request) {
        // we allow the server to be continue after term updated to save a round message
        update_term(req.get_term());

        // Reset stepping down value to prevent this server goes down when leader crashes after sending a LeaveClusterRequest
        if (steps_to_down_ > 0) {
            steps_to_down_ = 2;
        }
    }

    l_->debug(lstrfmt("%s %s %d: TRACE").fmt(__FILE__, __FUNCTION__, __LINE__));

    ptr<resp_msg> resp;
    if (req.get_type() == msg_type::append_entries_request) {
        resp = handle_append_entries(req);
    } else if (req.get_type() == msg_type::request_vote_request) {
        resp = handle_vote_req(req);
    } else if (req.get_type() == msg_type::client_request) {
        resp = handle_cli_req(req);
    } else {
        // extended requests
        resp = handle_extended_msg(req);
    }

    if (resp) {
        l_->debug(lstrfmt("[%s] to %d <- RESPONSE with Accepted=%d, Term=%llu, NextIndex=%llu [CurrentLeader=%d]")
                          .fmt(msg_type_string(resp->get_type()),
                               resp->get_dst(),
                               resp->get_accepted() ? 1 : 0,
                               resp->get_term(),
                               resp->get_next_idx(),
                               leader_));
    }

    return resp;
}

ptr<async_result<bool>> raft_server::add_srv(const srv_config &srv) {
    bufptr buf = srv.serialize();
    ptr<log_entry> log = cs_new<log_entry>(0, std::move(buf), log_val_type::cluster_server);
    ptr<req_msg> req = cs_new<req_msg>((ulong) 0, msg_type::add_server_request, 0, 0, (ulong) 0, (ulong) 0, (ulong) 0);
    req->log_entries().push_back(log);
    return send_msg_to_leader(req);
}

ptr<async_result<bool>> raft_server::append_entries(std::vector<bufptr> &logs) {
    if (logs.empty()) {
        bool result(false);
        return cs_new<async_result<bool>>(result);
    }

    ptr<req_msg> req(cs_new<req_msg>((ulong) 0, msg_type::client_request, 0, 0, (ulong) 0, (ulong) 0, (ulong) 0));
    for (auto &item : logs) {
        ptr<log_entry> log(cs_new<log_entry>(0, std::move(item), log_val_type::app_log));
        req->log_entries().push_back(log);
    }

    return send_msg_to_leader(req);
}

ptr<async_result<bool>> raft_server::remove_srv(const int srv_id) {
    bufptr buf(buffer::alloc(sz_int));
    buf->put(srv_id);
    buf->pos(0);
    ptr<log_entry> log = cs_new<log_entry>(0, std::move(buf), log_val_type::cluster_server);
    ptr<req_msg> req = cs_new<req_msg>((ulong) 0,
                                       msg_type::remove_server_request,
                                       0,
                                       0,
                                       (ulong) 0,
                                       (ulong) 0,
                                       (ulong) 0);
    req->log_entries().push_back(log);
    return send_msg_to_leader(req);
}
