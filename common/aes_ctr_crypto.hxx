//
// Created by ncl on 12/11/19.
//

#ifndef ENCLAVERAFT_AES_CBC_CRYPTO_HXX
#define ENCLAVERAFT_AES_CBC_CRYPTO_HXX

#include "common.hxx"
#include <string>
#include <memory>
#include <limits>
#include <openssl/crypto.h>

template<typename T>
struct zallocator {
public:
    typedef T value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;
    typedef value_type &reference;
    typedef const value_type &const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    pointer address(reference v) const { return &v; }

    const_pointer address(const_reference v) const { return &v; }

    pointer allocate(size_type n, const void *hint = 0) {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T))
            throw std::bad_alloc();
        return static_cast<pointer>(::operator new(n * sizeof(value_type)));
    }

    void deallocate(pointer p, size_type n) {
        OPENSSL_cleanse(p, n * sizeof(T));
        ::operator delete(p);
    }

    size_type max_size() const {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    template<typename U>
    struct rebind {
        typedef zallocator<U> other;
    };

    void construct(pointer ptr, const T &val) {
        new(static_cast<T *>(ptr)) T(val);
    }

    void destroy(pointer ptr) {
        static_cast<T *>(ptr)->~T();
    }
};

void rand_fill(byte *buffer, int size);

ptr<bytes> aes_ctr_encrypt(const byte key[S_KEY_SIZE], const byte iv[S_BLOCK_SIZE], const byte *ptext, uint32_t psize);

ptr<bytes> aes_ctr_decrypt(const byte key[S_KEY_SIZE], const byte iv[S_BLOCK_SIZE], const byte *ctext, uint32_t csize);

#endif //ENCLAVERAFT_AES_CBC_CRYPTO_HXX
