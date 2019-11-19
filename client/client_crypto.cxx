////
//// Created by ncl on 11/11/19.
////
//
//#include <spdlog/spdlog.h>
//#include "client_crypto.hxx"
//#include "cppcodec/hex_default_lower.hpp"
//#include "aes_ctr_crypto.hxx"
//
//using std::make_shared;
//
//byte SECRET[S_KEY_SIZE];
//
//byte *get_key() {
//    hex::decode(SECRET, S_KEY_SIZE, "828682353d50934b0a672591db570eea693eeb8cf1ad44cf5e50f8ce5a07461d");
//    return SECRET;
//}
//
//
//ptr<bytes> client_encrypt(const byte *data, uint32_t size) {
//    spdlog::debug(std::string("Encrypt <- Plaintext: ") + hex::encode(data, size));
//
//    byte iv[S_BLOCK_SIZE];
//    rand_fill(iv, S_BLOCK_SIZE);
//
//    auto enc = aes_ctr_encrypt(get_key(), iv, data, size);
//    enc->insert(enc->begin(), iv, iv + S_BLOCK_SIZE);
//
//    if (enc) {
//        spdlog::debug(std::string("Encrypt -> Ciphertext: ") + hex::encode(*enc));
//    } else {
//        spdlog::error("ENCRYPTION FAILED");
//    }
//
//    return enc;
//}
//
//ptr<bytes> client_decrypt(const byte *data, uint32_t size) {
//    spdlog::debug(std::string("Decrypt <- Ciphertext: ") + hex::encode(data, size));
//
//    auto dec = aes_ctr_decrypt(get_key(), data, data + S_BLOCK_SIZE, size - S_BLOCK_SIZE);
//
//    if (dec) {
//        spdlog::debug(std::string("Decrypt -> Plaintext: ") + hex::encode(*dec));
//    } else {
//        spdlog::error("DECRYPTION FAILED");
//    }
//
//    return dec;
//}