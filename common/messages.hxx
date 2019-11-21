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

enum erMessageType {
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

enum erKeyType {
    None,
    SharedKey,
    GroupKey
};

struct erMessageHeader {
    uint32_t size = 16;
    uint16_t m_type;
    uint16_t k_type;
    union {
        uint64_t group;
        struct {
            uint32_t src;
            uint32_t dst;
        } shared;
    } key_info;

    explicit erMessageHeader(erMessageType msg_type) : m_type(msg_type) {
        k_type = None;
        key_info.shared.src = -1;
        key_info.shared.dst = -1;
    }

    erMessageHeader(erMessageType msg_type, uint64_t group) : m_type(msg_type) {
        k_type = GroupKey;
        key_info.group = group;
    }

    erMessageHeader(erMessageType msg_type, uint32_t src, uint32_t dst) : m_type(msg_type) {
        k_type = SharedKey;
        key_info.shared.src = src;
        key_info.shared.dst = dst;
    }

    erMessageHeader(const erMessageHeader &) = default;
};

template<class PayloadContainer>
class erMessage {
public:
    erMessageHeader header_;
    ptr<PayloadContainer> payload_;

    erMessage(const erMessageHeader &header, ptr<PayloadContainer> payload) : header_(header), payload_(payload) {}

    erMessage(ptr<PayloadContainer> payload, erMessageType msg_type)
            : header_(msg_type), payload_(payload) {}

    erMessage(ptr<PayloadContainer> payload, erMessageType msg_type, uint64_t group)
            : header_(msg_type, group), payload_(payload) {}

    erMessage(ptr<PayloadContainer> payload, erMessageType msg_type, uint32_t src, uint32_t dst)
            : header_(msg_type, src, dst), payload_(payload) {}

    ptr<bytes> serialize() {
        uint32_t size = sizeof(erMessageHeader) + payload_->size();
        header_.size = size;

        auto buffer = std::make_shared<bytes>();
        buffer->reserve(size);

        const auto *ptr_h = reinterpret_cast<const uint8_t *> (&header_);
        buffer->insert(buffer->end(), ptr_h, ptr_h + sizeof(erMessageHeader));

        const auto *ptr_p = reinterpret_cast<const uint8_t *>(payload_->data());
        buffer->insert(buffer->end(), ptr_p, ptr_p + payload_->data());

        return buffer;
    }
};

struct erMessageMold {
    erMessageHeader header;
    uint8_t payload[];
};


#endif //ENCLAVERAFT_MESSAGES_HXX
