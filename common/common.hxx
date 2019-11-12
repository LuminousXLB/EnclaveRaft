//
// Created by ncl on 12/11/19.
//

#ifndef ENCLAVERAFT_COMMON_HXX
#define ENCLAVERAFT_COMMON_HXX

#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <memory>

using std::vector;
using std::array;
using std::string;
using std::make_shared;

static constexpr unsigned int S_KEY_SIZE = 32;
static constexpr unsigned int S_BLOCK_SIZE = 16;

using byte = uint8_t;
using bytes = std::vector<uint8_t>;

template<class T>
using ptr = std::shared_ptr<T>;

#endif //ENCLAVERAFT_COMMON_HXX
