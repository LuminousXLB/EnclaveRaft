//
// Created by ncl on 11/11/19.
//

#include "aec_gcm.hxx"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <spdlog/spdlog.h>

void handleErrors() {
    spdlog::critical(ERR_get_error());
}

int gcm_encrypt(const unsigned char *plaintext, int plaintext_len,
                const unsigned char *aad, int aad_len,
                const unsigned char *key,
                const unsigned char *iv, int iv_len,
                unsigned char *ciphertext,
                unsigned char *tag) {
    EVP_CIPHER_CTX *ctx;

    int len;

    int ciphertext_len;


    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new()))
        handleErrors();

    /* Initialise the encryption operation. */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr))
        handleErrors();

    /*
     * Set IV length if default 12 bytes (96 bits) is not appropriate
     */
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, nullptr))
        handleErrors();

    /* Initialise key and IV */
    if (1 != EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, iv))
        handleErrors();

    /*
     * Provide any AAD data. This can be called zero or more times as
     * required
     */
    if (1 != EVP_EncryptUpdate(ctx, nullptr, &len, aad, aad_len))
        handleErrors();

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
        handleErrors();
    ciphertext_len = len;

    /*
     * Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
        handleErrors();
    ciphertext_len += len;

    /* Get the tag */
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag))
        handleErrors();

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;
}

int gcm_decrypt(const unsigned char *ciphertext, int ciphertext_len,
                const unsigned char *aad, int aad_len,
                const unsigned char *tag,
                const unsigned char *key,
                const unsigned char *iv, int iv_len,
                unsigned char *plaintext) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    int ret;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new()))
        handleErrors();

    /* Initialise the decryption operation. */
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr))
        handleErrors();

    /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, nullptr))
        handleErrors();

    /* Initialise key and IV */
    if (!EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, iv))
        handleErrors();

    /*
     * Provide any AAD data. This can be called zero or more times as
     * required
     */
    if (!EVP_DecryptUpdate(ctx, nullptr, &len, aad, aad_len))
        handleErrors();

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if (!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
        handleErrors();
    plaintext_len = len;

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (char *) tag))
        handleErrors();

    /*
     * Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    if (ret > 0) {
        /* Success */
        plaintext_len += len;
        return plaintext_len;
    } else {
        /* Verify failed */
        return -1;
    }
}

const uint32_t AESGCM_IV_SIZE = 12;
const uint32_t AESGCM_MAC_SIZE = 16;

#include "cppcodec/hex_default_lower.hpp"
#include <random>
#include <openssl/rand.h>


using std::make_shared;

uint8_t SECRET[16];

uint8_t *get_key() {
    hex::decode(SECRET, 16, "828682353d50934b0a672591db570eea", 32);
    return SECRET;
}

shared_ptr<vector<uint8_t>> raft_encrypt(const uint8_t *data, uint32_t size) {
    auto out = make_shared<vector<uint8_t >>(AESGCM_IV_SIZE + AESGCM_MAC_SIZE + size, 0);
    uint8_t *buffer_base = &(*out)[0];

    /* build key */
    const uint8_t *p_key = get_key();

    /* build IV */
    uint8_t *p_iv = buffer_base;
    std::random_device rd;
    std::uniform_int_distribution<uint8_t> dist;

    RAND_bytes(buffer_base, AESGCM_IV_SIZE);

    /* Positioning tag and ciphertext */
    uint8_t *p_out_mac = buffer_base + AESGCM_IV_SIZE;
    uint8_t *p_cipher = buffer_base + AESGCM_IV_SIZE + AESGCM_MAC_SIZE;

    gcm_encrypt(data, size, nullptr, 0, p_key, p_iv, AESGCM_IV_SIZE, p_cipher, p_out_mac);

    return out;
}

shared_ptr<vector<uint8_t >> raft_decrypt(const uint8_t *data, uint32_t size) {
    const uint32_t plaintext_size = size - (AESGCM_IV_SIZE + AESGCM_MAC_SIZE);
    auto out = make_shared<vector<uint8_t >>(plaintext_size, 0);
    uint8_t *buffer_base = &(*out)[0];

    /* build key */
    const uint8_t *p_key = get_key();

    /* Positioning IV, MAC and Ciphertext */
    const uint8_t *p_iv = data;
    const uint8_t *p_mac = data + AESGCM_IV_SIZE;
    const uint8_t *p_cipher = data + AESGCM_IV_SIZE + AESGCM_MAC_SIZE;
    const uint32_t cipher_len = plaintext_size;

    /* plaintext space */
    gcm_decrypt(p_cipher, cipher_len, nullptr, 0, p_mac, p_key, p_iv, AESGCM_IV_SIZE, buffer_base);

    return out;
}