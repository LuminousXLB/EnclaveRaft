//
// Created by ncl on 19/11/19.
//

#ifndef ENCLAVERAFT_REQUESTBUILDER_HXX
#define ENCLAVERAFT_REQUESTBUILDER_HXX

#include "client.hxx"
#include "json11.hpp"

using namespace json11;

class RequestBuilder {
public:
    static ptr<bytes> add_server(uint16_t server_id) {
        string req = build_add_server_request(server_id);
        return serialize(client_add_srv_req, req);
    }

    static ptr<bytes> append_entries(const string &message) {
        const bytes log(message.begin(), message.end());
        const string req = build_append_entries_request(vector<bytes>{log});
        return serialize(client_append_entries_req, req);
    }

    static bool deserialize(const bytes &buffer) {
        const uint16_t &type_num = *(const uint16_t *) buffer.data();
        const char *payload = reinterpret_cast<const char *>(buffer.data() + sizeof(uint16_t));

        string j(payload, payload + buffer.size() - sizeof(uint16_t));

        string json_err;
        Json body = Json::parse(j, json_err);

        if (json_err.empty()) {
            return body["success"].int_value();
        } else {
            throw std::runtime_error(json_err);
        }
    }

private:
    static string build_add_server_request(uint16_t server_id) {
        Json body = Json::object{
                {"server_id", server_id},
                {"endpoint",  fmt::format("tcp://127.0.0.1:{}", 9000 + server_id)}
        };

        return body.dump();
    }

    static string build_remove_server_request(uint16_t server_id) {
        Json body = Json::object{
                {"server_id", server_id}
        };

        return body.dump();
    }

    static string build_append_entries_request(const vector<bytes> &logs) {
        vector<string> body;

        for (const auto &log: logs) {
            body.emplace_back(base64::encode(log));
        }

        return Json(body).dump();
    }

    static ptr<bytes> serialize(er_message_type type, const string &payload) {
        uint16_t type_num = type;
        auto *ptr = reinterpret_cast<uint8_t *>(&type_num);

        auto buffer = make_shared<bytes>();
        buffer->insert(buffer->end(), ptr, ptr + sizeof(uint16_t));
        buffer->insert(buffer->end(), payload.begin(), payload.end());

        return buffer;
    }
};


#endif //ENCLAVERAFT_REQUESTBUILDER_HXX
