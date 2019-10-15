//
// Created by ncl on 15/10/19.
//

#ifndef SGX_ENCLAVE_TO_ENCLAVE_RA_RAFT_EXCEPTION_HXX
#define SGX_ENCLAVE_TO_ENCLAVE_RA_RAFT_EXCEPTION_HXX

namespace cornerstone {
    class raft_exception : public std::exception {
    public:
        raft_exception(const std::string &err)
                : err_(err) {}

        const char *what() const noexcept {
            return err_.c_str();
        }

    private:
        std::string err_;
    };
}

#endif //SGX_ENCLAVE_TO_ENCLAVE_RA_RAFT_EXCEPTION_HXX
