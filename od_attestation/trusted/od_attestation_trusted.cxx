//
// Created by ncl on 20/11/19.
//

#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <tlibc/mbusafecrt.h>
#include <sgx_key_exchange.h>
#include <sgx_utils.h>

#include "od_attestation_t.h"
#include "attestation_manager.cxx"

using std::make_shared;
using std::string;
using std::map;
using std::mutex;
using std::lock_guard;
using std::atomic_uint32_t;
template<class T>
using sptr = std::shared_ptr<T>;

static struct _manager_context_t {
    mutex obj_mutex;
    map<int32_t, sptr<odAttestationManager>> store;
    atomic_uint32_t id_counter{0};

    uint32_t insert(sptr<odAttestationManager> mgr) {
        uint32_t id = ++id_counter;
        {
            lock_guard<mutex> lock(obj_mutex);
            store.insert({id, mgr});
        }
        return id;
    }

    sptr<odAttestationManager> query(uint32_t context_id) {
        {
            lock_guard<mutex> lock(obj_mutex);
            auto it = store.find(context_id);
            if (it != store.end()) {
                return it->second;
            } else {
                return nullptr;
            }
        }
    }

} _manager_ctx_;

sgx_status_t od_init_attestation_context(const sgx_spid_t &spid, sgx_quote_sign_type_t type,
                                         int32_t &context_id, uint32_t &quote_size, sptr<sgx_quote_t> &quote) {
    sgx_status_t call_status, ret_status;
    /* initialize quote */
    sgx_target_info_t target_info;
    sgx_epid_group_id_t epid_gid;
    call_status = od_init_quote_ocall(&ret_status, &target_info, &epid_gid);
    if (call_status != SGX_SUCCESS || ret_status != SGX_SUCCESS) {
        return SGX_ERROR_UNEXPECTED;
    }

    /* initialize attestation manager */
    sptr<odAttestationManager> mgr = make_shared<odAttestationManager>();
    context_id = _manager_ctx_.insert(mgr);

    /* get report */
    sgx_report_data_t report_data;
    memcpy_s(report_data.d, SGX_REPORT_DATA_SIZE, &mgr->public_key(), sizeof(sgx_ec256_public_t));

    sgx_report_t report;
    call_status = sgx_create_report(&target_info, &report_data, &report);
    if (call_status != SGX_SUCCESS) {
        return SGX_ERROR_UNEXPECTED;
    }

    /* calculate quote size */
    call_status = od_calc_quote_size_ocall(&ret_status, &quote_size);
    if (call_status != SGX_SUCCESS || ret_status != SGX_SUCCESS) {
        return SGX_ERROR_UNEXPECTED;
    }

    /* get quote */
    quote = std::shared_ptr<sgx_quote_t>((sgx_quote_t *) new uint8_t[quote_size], [](sgx_quote_t *ptr) {
        auto *p = reinterpret_cast<uint8_t *>(ptr);
        delete[](ptr);
    });

    call_status = od_get_quote_ocall(&ret_status, &report, type, &spid,
                                     quote_size, reinterpret_cast<uint8_t *>(quote.get()));
    if (call_status != SGX_SUCCESS || ret_status != SGX_SUCCESS) {
        return SGX_ERROR_UNEXPECTED;
    }

    return SGX_SUCCESS;
}


sptr<odAttestationManager> od_get_attestation_manager(uint32_t context_id) {
    return _manager_ctx_.query(context_id);
}
