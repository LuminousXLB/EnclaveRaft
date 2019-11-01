//
// Created by ncl on 31/10/19.
//

#include "../include/cornerstone.hxx"


namespace cornerstone {
    std::string buffer;

    const char *msg_type_string(msg_type type) {
        switch (type) {
            case request_vote_request:
                return "request_vote_request";
            case request_vote_response:
                return "request_vote_response";
            case append_entries_request:
                return "append_entries_request";
            case append_entries_response:
                return "append_entries_response";
            case client_request:
                return "client_request";
            case add_server_request:
                return "add_server_request";
            case add_server_response:
                return "add_server_response";
            case remove_server_request:
                return "remove_server_request";
            case remove_server_response:
                return "remove_server_response";
            case sync_log_request:
                return "sync_log_request";
            case sync_log_response:
                return "sync_log_response";
            case join_cluster_request:
                return "join_cluster_request";
            case join_cluster_response:
                return "join_cluster_response";
            case leave_cluster_request:
                return "leave_cluster_request";
            case leave_cluster_response:
                return "leave_cluster_response";
            case install_snapshot_request:
                return "install_snapshot_request";
            case install_snapshot_response:
                return "install_snapshot_response";
            default:
                buffer = sstrfmt("unknown_type_%d").fmt(type);
                return buffer.c_str();
        }
    }
}
