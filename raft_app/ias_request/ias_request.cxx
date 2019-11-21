//
// Created by ncl on 21/11/19.
//
#include "intel_ias.hxx"
#include "raft_enclave_u.h"

string verification_report;

uint32_t ocall_ias_request(uint32_t context_id, const char *isvEnclaveQuote_b64) {
    IntelIAS ias("88d490b4d998453b9b4aeeb4d6664cdb", "69f0ec75019749e3b461a8429e5aaf5e", IntelIAS::DEVELOPMENT);
    verification_report = ias.report(isvEnclaveQuote_b64);

    return verification_report.size();
}

void ocall_get_response(uint32_t context_id, char *response, int32_t response_size) {
    memcpy(response, verification_report.c_str(), response_size);
}

