//
// Created by ncl on 11/11/19.
//

#include "crypto.hxx"
#include <memory>
#include <vector>
#include <sgx_tcrypto.h>
#include "aes_ctr_crypto.hxx"
#include "../raft/include/cornerstone.hxx"

using std::make_shared;


static byte SECRET[S_KEY_SIZE];

byte *get_key() {
    hex::decode(SECRET, S_KEY_SIZE, "828682353d50934b0a672591db570eea693eeb8cf1ad44cf5e50f8ce5a07461d");
    return SECRET;
}


ptr<bytes> raft_encrypt(const uint8_t *data, uint32_t size) {
    p_logger->debug(std::string("Encrypt <- Plaintext: ") + hex::encode(data, size));

    byte iv[S_BLOCK_SIZE];
    rand_fill(iv, S_BLOCK_SIZE);

    auto enc = aes_ctr_encrypt(get_key(), iv, data, size);
    enc->insert(enc->begin(), iv, iv + S_BLOCK_SIZE);

    if (enc) {
        p_logger->debug(std::string("Encrypt -> Ciphertext: ") + hex::encode(*enc));
    } else {
        p_logger->err("ENCRYPTION FAILED");
    }

    return enc;
}

ptr<bytes> raft_decrypt(const uint8_t *data, uint32_t size) {
    p_logger->debug(std::string("Decrypt <- Ciphertext: ") + hex::encode(data, size));

    auto dec = aes_ctr_decrypt(get_key(), data, data + S_BLOCK_SIZE, size - S_BLOCK_SIZE);

    if (dec) {
        p_logger->debug(std::string("Decrypt -> Plaintext: ") + hex::encode(*dec));
    } else {
        p_logger->err("DECRYPTION FAILED");
    }

    return dec;
}