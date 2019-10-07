#ifndef SGX_UTILS_H_
#define SGX_UTILS_H_

#include <sgx_urts.h>
#include <string>

void print_error_message(sgx_status_t ret);

std::string search_shared_library(const std::string &filename, const std::string &path);

int initialize_enclave(sgx_enclave_id_t *eid, const std::string &launch_token_path, const std::string &enclave_name);

#endif // SGX_UTILS_H_
