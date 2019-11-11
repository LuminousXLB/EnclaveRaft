//
// Created by ncl on 11/11/19.
//

#ifndef ENCLAVERAFT_AEC_GCM_HXX
#define ENCLAVERAFT_AEC_GCM_HXX

#include <vector>
#include <memory>

using std::shared_ptr;
using std::vector;

shared_ptr<vector<uint8_t>> raft_encrypt(const uint8_t *data, uint32_t size);
shared_ptr<vector<uint8_t >> raft_decrypt(const uint8_t *data, uint32_t size);

#endif //ENCLAVERAFT_AEC_GCM_HXX
