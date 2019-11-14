//
// Created by ncl on 12/11/19.
//

#include "enclave_quote.hxx"

#include <memory>
#include <iostream>
#include <thread>
#include "cppcodec/hex_default_lower.hpp"
#include "sgx_uae_service.h"
#include "sgx_exception.hpp"
#include "secret.h"
#include "raft_enclave_u.h"

//#define VERBOSE

extern sgx_enclave_id_t global_enclave_id;

#define enc(a, b) hex::encode((a), (b)).c_str()

void print_sgx_report_body(const char *prefix, const sgx_report_body_t &r_body) {
    printf("%scpu_svn         : %s\n", prefix, enc(r_body.cpu_svn.svn, SGX_CPUSVN_SIZE));
    printf("%smisc_select     : %08x\n", prefix, r_body.misc_select);
    printf("%sisv_ext_prod_id : %s\n", prefix, enc(r_body.isv_ext_prod_id, SGX_ISVEXT_PROD_ID_SIZE));
    printf("%sattributes: {\n", prefix);
    printf("%s    flags       : %016lx\n", prefix, r_body.attributes.flags);
    printf("%s    xfrm        : %016lx\n", prefix, r_body.attributes.xfrm);
    printf("%s}\n", prefix);
    printf("%smr_enclave      : %s\n", prefix, enc(r_body.mr_enclave.m, SGX_HASH_SIZE));
    printf("%smr_signer       : %s\n", prefix, enc(r_body.mr_signer.m, SGX_HASH_SIZE));
    printf("%sconfig_id       : %s\n", prefix, enc(r_body.config_id, SGX_CONFIGID_SIZE));
    printf("%sisv_prod_id     : %04x\n", prefix, r_body.isv_prod_id);
    printf("%sisv_svn         : %04x\n", prefix, r_body.isv_svn);
    printf("%sconfig_svn      : %04x\n", prefix, r_body.config_svn);
    printf("%sisv_family_id   : %s\n", prefix, enc(r_body.isv_family_id, SGX_ISV_FAMILY_ID_SIZE));
    printf("%sreport_data     : %s\n", prefix, enc(r_body.report_data.d, SGX_REPORT_DATA_SIZE));
}

void print_sgx_report(const char *prefix, const sgx_report_t &report) {
    printf("%sreport_body: {\n", prefix);
    std::string indent = prefix;
    indent += indent;
    print_sgx_report_body(indent.c_str(), report.body);
    printf("%s}\n", prefix);

    printf("%skey_id          : %s\n", prefix, enc(report.key_id.id, SGX_KEYID_SIZE));
    printf("%smac             : %s\n", prefix, enc(report.mac, SGX_MAC_SIZE));
}

void print_sgx_quote(const char *prefix, const sgx_quote_t *quote) {
    printf("%sversion         : %04x\n", prefix, quote->version);
    printf("%ssign_type       : %04x\n", prefix, quote->sign_type);
    printf("%sepid_group_id   : %08x\n", prefix, *reinterpret_cast<const uint32_t *>(quote->epid_group_id));
    printf("%sqe_svn          : %04x\n", prefix, quote->qe_svn);
    printf("%spce_svn         : %04x\n", prefix, quote->pce_svn);
    printf("%sxeid            : %08x\n", prefix, quote->xeid);
    printf("%sbasename        : %s\n", prefix, enc(quote->basename.name, sizeof(sgx_basename_t)));

    printf("%sreport_body: {\n", prefix);
    std::string indent = prefix;
    indent += indent;
    print_sgx_report_body(indent.c_str(), quote->report_body);
    printf("%s}\n", prefix);

    printf("%ssignature_len   : %08x\n", prefix, quote->signature_len);
    printf("%ssignature       : %s\n", prefix, enc(quote->signature, quote->signature_len));
}

#undef enc

ptr<bytes> get_enclave_quote() {
    sgx_status_t status;
    sgx_status_t sgx_ret = SGX_SUCCESS;

    /* sgx_init_quote */
    sgx_target_info_t target_info;
    sgx_epid_group_id_t epid_gid;
    int retry_counter = 0;
    do {
        status = sgx_init_quote(&target_info, &epid_gid);
        if (status != SGX_ERROR_BUSY) {
            break;
        }

        retry_counter++;
        if (retry_counter < 3) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } else {
            break;
        }
    } while (true);

    if (status != SGX_SUCCESS) {
        throw sgx_error("call sgx_init_quote", status);
    }

    /* sgx_create_report */
    sgx_report_t report;
    status = ecall_get_report(global_enclave_id, &sgx_ret, &target_info, &report);
    if (status != SGX_SUCCESS) {
        throw sgx_error("call get_report", status);
    }
    if (sgx_ret != SGX_SUCCESS) {
        throw sgx_error("get_report return", sgx_ret);
    }

    /* sgx_calc_quote_size */
    uint32_t quote_size;
    status = sgx_calc_quote_size(nullptr, 0, &quote_size);
    if (status != SGX_SUCCESS) {
        throw sgx_error("call sgx_init_quote", status);
    }

    /* decode SPID */
    sgx_spid_t spid;
    hex::decode(spid.id, sizeof(spid.id), intel_api_spid, strlen(intel_api_spid));

    /* sgx_get_quote */
    auto quote_buffer = std::make_shared<bytes>(quote_size, 0);

    status = sgx_get_quote(
            &report,
            SGX_UNLINKABLE_SIGNATURE,
            &spid,
            nullptr,
            nullptr,
            0,
            nullptr,
            reinterpret_cast<sgx_quote_t *>(&(*quote_buffer)[0]),
            quote_buffer->size()
    );
    if (status != SGX_SUCCESS) {
        throw sgx_error("call sgx_get_quote", status);
    }
#ifdef VERBOSE
#include "cppcodec/base64_default_rfc4648.hpp"

    puts("SGX quote: {");
    print_sgx_quote("    ", reinterpret_cast<const sgx_quote_t *>(quote_buffer.get()));
    puts("}");

    std::cout << "SGX quote [base64] = " << base64::encode(quote_buffer.get(), quote_size) << std::endl;
#endif

    return quote_buffer;
}


