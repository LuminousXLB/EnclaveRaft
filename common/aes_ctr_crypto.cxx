//
// Created by ncl on 12/11/19.
// Thanks to https://wiki.openssl.org/images/5/5d/Evp-encrypt-cxx.tar.gz
//

#include "aes_ctr_crypto.hxx"

#include <memory>
#include <stdexcept>
#include <openssl/evp.h>
#include <openssl/rand.h>

using EVP_CIPHER_CTX_free_ptr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&::EVP_CIPHER_CTX_free)>;

void rand_fill(byte *buffer, int size) {
    if (RAND_bytes(buffer, size) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
}

ptr<bytes> aes_ctr_encrypt(const byte key[S_KEY_SIZE], const byte iv[S_BLOCK_SIZE], const byte *ptext, uint32_t psize) {
    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    auto out = std::make_shared<bytes>();
    bytes &ctext = *out;

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_ctr(), nullptr, key, iv) != 1) {
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    // Recovered text expands upto S_BLOCK_SIZE
    ctext.resize(psize + S_BLOCK_SIZE);
    int out_len1 = (int) ctext.size();

    if (EVP_EncryptUpdate(ctx.get(), &ctext[0], &out_len1, &ptext[0], (int) psize) != 1) {
        throw std::runtime_error("EVP_EncryptUpdate failed");
    }

    int out_len2 = (int) ctext.size() - out_len1;
    if (EVP_EncryptFinal_ex(ctx.get(), (byte *) &ctext[0] + out_len1, &out_len2) != 1) {
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }

    // Set cipher text size now that we know it
    ctext.resize(out_len1 + out_len2);

    return out;
}

ptr<bytes> aes_ctr_decrypt(const byte key[S_KEY_SIZE], const byte iv[S_BLOCK_SIZE], const byte *ctext, uint32_t csize) {
    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    auto out = std::make_shared<bytes>();
    bytes &rtext = *out;

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_ctr(), nullptr, key, iv) != 1) {
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }

    // Recovered text contracts upto S_BLOCK_SIZE
    rtext.resize(csize);
    int out_len1 = (int) rtext.size();

    if (EVP_DecryptUpdate(ctx.get(), &rtext[0], &out_len1, &ctext[0], (int) csize) != 1) {
        throw std::runtime_error("EVP_DecryptUpdate failed");
    }

    int out_len2 = (int) rtext.size() - out_len1;
    if (EVP_DecryptFinal_ex(ctx.get(), (byte *) &rtext[0] + out_len1, &out_len2) != 1) {
        throw std::runtime_error("EVP_DecryptFinal_ex failed");
    }

    // Set recovered text size now that we know it
    rtext.resize(out_len1 + out_len2);

    return out;
}
