//
// Created by ncl on 13/11/19.
//

#ifndef ENCLAVERAFT_MESSAGES_HXX
#define ENCLAVERAFT_MESSAGES_HXX

#include "common.hxx"

static constexpr uint32_t er_length_field = sizeof(uint32_t);
static constexpr uint32_t er_type_field = sizeof(uint16_t);


//ptr<async_result<bool>> add_srv(const srv_config &srv);
//ptr<async_result<bool>> remove_srv(const int srv_id);
//ptr<async_result<bool>> append_entries(std::vector<bufptr> &logs);

enum er_message_type {
    client_add_srv_req = 1,
    client_add_srv_resp,
    client_remove_srv_req,
    client_remove_srv_resp,
    client_append_entries_req,
    client_append_entries_resp,
    system_key_exchange_req,
    system_key_exchange_resp,
    system_key_setup_req,
    system_key_setup_resp,
    raft_message = 255,
};

#endif //ENCLAVERAFT_MESSAGES_HXX
