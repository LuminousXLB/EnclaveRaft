//
// Created by ncl on 15/10/19.
// Thanks to https://github.com/kaimast/linux-sgx/blob/master/sdk/tlibcxx/src/random.cpp
//

#ifndef SGX_ENCLAVE_TO_ENCLAVE_RA_RANDOM_HPP
#define SGX_ENCLAVE_TO_ENCLAVE_RA_RANDOM_HPP


#include <sgx_trts.h>
#include <string>

using std::string;
using std::numeric_limits;

class random_device {
public:
    // types
    typedef unsigned int result_type;

    // constructors
    explicit random_device(const string &token = "/dev/urandom") {
        (void) token;
    }

    // generating functions
    result_type operator()() {
        unsigned result;

        sgx_read_rand(reinterpret_cast<unsigned char *>(&result), sizeof(result));
        return result;
    }

    // generator characteristics
    static constexpr result_type min() {
        return numeric_limits<result_type>::min();
    }

    static constexpr result_type max() {
        return numeric_limits<result_type>::max();
    }

    // property functions
    double entropy() const noexcept {
        return 0;
    }

    // no copy functions
//    random_device(const random_device &) = delete;

//    void operator=(const random_device &) = delete;
};


#endif //SGX_ENCLAVE_TO_ENCLAVE_RA_RANDOM_HPP
