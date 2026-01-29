/**
 * SimpleGo - smp_ratchet.c
 * Double Ratchet Encryption - CORRECT Version 2 Wire Format (non-PQ)
 * v0.1.21-alpha - Updated 2026-01-23
 * 
 * CRITICAL: Version 2 uses 1-byte length prefixes only (NO Large encoding!)
 * 
 * EncRatchetMessage (v2):
 *   [1B emHeader-len (0x7B = 123)][123B emHeader][16B payload AuthTag][Tail encrypted_payload]
 * 
 * emHeader (EncMessageHeader v2):
 *   [2B ehVersion][16B ehIV direct][16B ehAuthTag direct][1B ehBody-len (0x58)][88B encrypted MsgHeader]
 *   Total: 2 + 16 + 16 + 1 + 88 = 123 bytes
 * 
 * AAD (Associated Data) für BEIDE AES-GCM-Operationen:
 *   rcAD (112 bytes) + plaintext MsgHeader (88 bytes) = 200 bytes
 *   rcAD = our_key1_public_raw (56) || peer_key1_raw (56)
 */

#include "smp_ratchet.h"
#include "smp_x448.h"
#include "smp_crypto.h"
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/sha512.h"

static const char *TAG = "SMP_RATCH";

// Constants
#define RATCHET_VERSION         2
#define MSG_HEADER_CONTENT_LEN  80   // Actual used bytes before padding
#define MSG_HEADER_PADDED_LEN   88   // Padded to 88 bytes
#define GCM_IV_LEN              16
#define GCM_TAG_LEN             16
#define AAD_FULL_LEN            200  // 112 (rcAD) + 88 (plaintext MsgHeader)

// ============== Ratchet State ==============

static ratchet_state_t ratchet_state = {0};

// ============== Helper Functions ==============

static int hkdf_sha512(const uint8_t *salt, size_t salt_len,
                       const uint8_t *ikm, size_t ikm_len,
                       const uint8_t *info, size_t info_len,
                       uint8_t *okm, size_t okm_len) {
    return mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512),
                        salt, salt_len, ikm, ikm_len, info, info_len,
                        okm, okm_len);
}

static int aes_gcm_encrypt(const uint8_t *key,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *plaintext, size_t pt_len,
                           uint8_t *ciphertext,
                           uint8_t *tag) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) goto cleanup;
    
    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                    pt_len, iv, iv_len,
                                    aad, aad_len,
                                    plaintext, ciphertext,
                                    GCM_TAG_LEN, tag);
    
cleanup:
    mbedtls_gcm_free(&gcm);
    return ret;
}

static int aes_gcm_decrypt(const uint8_t *key,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *ciphertext, size_t ct_len,
                           const uint8_t *tag,
                           uint8_t *plaintext) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) goto cleanup;
    
    ret = mbedtls_gcm_auth_decrypt(&gcm, ct_len,
                                   iv, iv_len,
                                   aad, aad_len,
                                   tag, GCM_TAG_LEN,
                                   ciphertext, plaintext);
    
cleanup:
    mbedtls_gcm_free(&gcm);
    return ret;
}

// ============== Key Derivation ==============

static void kdf_root(const uint8_t *root_key, const uint8_t *dh_out,
                     uint8_t *new_root_key, uint8_t *chain_key, uint8_t *next_header_key) {
    uint8_t kdf_output[96];
    hkdf_sha512(root_key, 32, dh_out, 56,
                (const uint8_t *)"SimpleXRootRatchet", 18,
                kdf_output, 96);
    memcpy(new_root_key, kdf_output, 32);       // bytes 0-31 = NEW ROOT KEY
    memcpy(chain_key, kdf_output + 32, 32);     // bytes 32-63 = chain key
    memcpy(next_header_key, kdf_output + 64, 32); // bytes 64-95 = NEXT HEADER KEY
}

static void kdf_chain(const uint8_t *chain_key,
                      uint8_t *next_chain_key, uint8_t *message_key,
                      uint8_t *msg_iv, uint8_t *header_iv) {
    uint8_t kdf_output[96];
    hkdf_sha512(NULL, 0, chain_key, 32,
                (const uint8_t *)"SimpleXChainRatchet", 19,
                kdf_output, 96);
    memcpy(next_chain_key, kdf_output, 32);
    memcpy(message_key, kdf_output + 32, 32);
    memcpy(msg_iv, kdf_output + 64, 16);     // iv1 = bytes 64-79 für MESSAGE
    memcpy(header_iv, kdf_output + 80, 16);  // iv2 = bytes 80-95 für HEADER
}

// ============== X3DH Key Agreement ==============

bool ratchet_x3dh_sender(const uint8_t *peer_key1,
                         const uint8_t *peer_key2,
                         const x448_keypair_t *our_key1,
                         const x448_keypair_t *our_key2) {
    ESP_LOGI(TAG, "🔐 X3DH Key Agreement (sender)...");

    uint8_t dh1[56], dh2[56], dh3[56];
    if (!x448_dh(peer_key1, our_key2->private_key, dh1)) return false;
    if (!x448_dh(peer_key2, our_key1->private_key, dh2)) return false;
    if (!x448_dh(peer_key2, our_key2->private_key, dh3)) return false;

    // DEBUG: Print FULL keys for Python comparison
    ESP_LOGI(TAG, "📋 FULL KEYS FOR PYTHON TEST:");
    printf("peer_key1_hex = \"");
    for(int i=0; i<56; i++) printf("%02x", peer_key1[i]);
    printf("\"\n");
    printf("peer_key2_hex = \"");
    for(int i=0; i<56; i++) printf("%02x", peer_key2[i]);
    printf("\"\n");
    printf("our_key1_pub_hex = \"");
    for(int i=0; i<56; i++) printf("%02x", our_key1->public_key[i]);
    printf("\"\n");
    printf("our_key1_priv_hex = \"");
    for(int i=0; i<56; i++) printf("%02x", our_key1->private_key[i]);
    printf("\"\n");
    printf("our_key2_pub_hex = \"");
    for(int i=0; i<56; i++) printf("%02x", our_key2->public_key[i]);
    printf("\"\n");
    printf("our_key2_priv_hex = \"");
    for(int i=0; i<56; i++) printf("%02x", our_key2->private_key[i]);
    printf("\"\n");

    uint8_t dh_combined[168];
    memcpy(dh_combined, dh1, 56);
    memcpy(dh_combined + 56, dh2, 56);
    memcpy(dh_combined + 112, dh3, 56);

    uint8_t salt[64] = {0};
    uint8_t kdf_output[96];
    hkdf_sha512(salt, 64, dh_combined, 168,
                (const uint8_t *)"SimpleXX3DH", 11, kdf_output, 96);

    memcpy(ratchet_state.header_key_send, kdf_output, 32);
    memcpy(ratchet_state.header_key_recv, kdf_output + 32, 32);
    memcpy(ratchet_state.root_key, kdf_output + 64, 32);

    // DEBUG: Show X3DH intermediate values (HIER - nach den Berechnungen!)
    ESP_LOGI(TAG, "📋 X3DH Debug:");
    printf("   dh1: "); for(int i=0; i<8; i++) printf("%02x", dh1[i]); printf("...\n");
    printf("   dh2: "); for(int i=0; i<8; i++) printf("%02x", dh2[i]); printf("...\n");
    printf("   dh3: "); for(int i=0; i<8; i++) printf("%02x", dh3[i]); printf("...\n");
    printf("   hk:  "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.header_key_send[i]); printf("...\n");
    printf("   rk:  "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.root_key[i]); printf("...\n");

    // AssocData = rcAD
    memcpy(ratchet_state.assoc_data, our_key1->public_key, 56);
    memcpy(ratchet_state.assoc_data + 56, peer_key1, 56);

    // DEBUG: Show rcAD
    ESP_LOGI(TAG, "📋 rcAD (first 32 bytes):");
    printf("   our_key1: ");
    for (int i = 0; i < 16; i++) printf("%02x ", ratchet_state.assoc_data[i]);
    printf("...\n   peer_key1: ");
    for (int i = 56; i < 72; i++) printf("%02x ", ratchet_state.assoc_data[i]);
    printf("...\n");

    ESP_LOGI(TAG, "✅ X3DH complete - RootKey: %02x%02x...", ratchet_state.root_key[0], ratchet_state.root_key[1]);
    return true;
}

// ============== Ratchet Initialization ==============

bool ratchet_init_sender(const uint8_t *peer_dh_public, const x448_keypair_t *our_key2) {
    // DEBUG: Show inputs
    printf("ratchet_init_sender inputs:\n");
    printf("   peer_dh_public: "); for(int i=0; i<8; i++) printf("%02x", peer_dh_public[i]); printf("...\n");
    printf("   our_key2_pub:   "); for(int i=0; i<8; i++) printf("%02x", our_key2->public_key[i]); printf("...\n");
    printf("   our_key2_priv:  "); for(int i=0; i<8; i++) printf("%02x", our_key2->private_key[i]); printf("...\n");
    
    ESP_LOGI(TAG, "🔄 Initializing send ratchet...");

    memcpy(&ratchet_state.dh_self, our_key2, sizeof(x448_keypair_t));
    memcpy(ratchet_state.dh_peer, peer_dh_public, 56);

    uint8_t dh_out[56];
    if (!x448_dh(peer_dh_public, ratchet_state.dh_self.private_key, dh_out)) {
        return false;
    }

    uint8_t new_root_key[32];
    uint8_t next_header_key[32];
    kdf_root(ratchet_state.root_key, dh_out,
             new_root_key, ratchet_state.chain_key_send, next_header_key);
    memcpy(ratchet_state.root_key, new_root_key, 32);

    // DEBUG: Root KDF output (NACH kdf_root!)
    ESP_LOGI(TAG, "📋 Root KDF output:");
    printf("   dh_out:    "); for(int i=0; i<8; i++) printf("%02x", dh_out[i]); printf("...\n");
    printf("   new_rk:    "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.root_key[i]); printf("...\n");
    printf("   ck:        "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.chain_key_send[i]); printf("...\n");
    printf("   next_hk:   "); for(int i=0; i<8; i++) printf("%02x", next_header_key[i]); printf("...\n");

    ratchet_state.msg_num_send = 0;
    ratchet_state.prev_chain_len = 0;
    ratchet_state.initialized = true;

    ESP_LOGI(TAG, "✅ Ratchet initialized");
    return true;
}

static void build_msg_header(uint8_t *header, const uint8_t *dh_public,
                             uint32_t pn, uint32_t ns) {
    memset(header, 0, MSG_HEADER_PADDED_LEN);
    int p = 0;

    // Word16 BE length prefix (content = 79 bytes)
    // SimpleX Crypto.hs pad(): [2B Word16 len][content][# padding]
    header[p++] = 0x00;
    header[p++] = 79;  // 0x4F

    // msgMaxVersion (Word16 BE)
    header[p++] = 0x00;
    header[p++] = RATCHET_VERSION;

    // msgDHRs - ByteString with 1-BYTE length prefix!
    header[p++] = 68;    // SPKI length = 68 (1 BYTE only!)

    // SPKI header + X448 key
    static const uint8_t X448_SPKI_HEADER[12] = {0x30,0x42,0x30,0x05,0x06,0x03,0x2b,0x65,0x6f,0x03,0x39,0x00};
    memcpy(&header[p], X448_SPKI_HEADER, 12); p += 12;
    memcpy(&header[p], dh_public, 56); p += 56;

    // msgPN (Word32 BE)
    header[p++] = (pn >> 24) & 0xFF;
    header[p++] = (pn >> 16) & 0xFF;
    header[p++] = (pn >> 8)  & 0xFF;
    header[p++] = pn & 0xFF;

    // msgNs (Word32 BE)
    header[p++] = (ns >> 24) & 0xFF;
    header[p++] = (ns >> 16) & 0xFF;
    header[p++] = (ns >> 8)  & 0xFF;
    header[p++] = ns & 0xFF;

    // p = 81 now (2 + 79)
    // '#' padding to reach 88 bytes
    memset(&header[p], '#', 88 - p);
    // Layout: [2B Word16=79][2B version][1B len][68B SPKI][4B msgPN][4B msgNs][7B '#']
    //              2       +     2     +   1   +   68   +    4    +    4    +   7   = 88 ✓
}

// ============== Encrypt Message ==============

int ratchet_encrypt(const uint8_t *plaintext, size_t pt_len,
                    uint8_t *output, size_t *out_len,
                    size_t padded_msg_len) {
    
    if (!ratchet_state.initialized) return -1;

    // 1. Derive keys & IVs
    uint8_t message_key[32], next_chain_key[32], msg_iv[16], header_iv[16];
    kdf_chain(ratchet_state.chain_key_send, next_chain_key, message_key, msg_iv, header_iv);
    
    // DEBUG: Show chainKdf outputs
    ESP_LOGI(TAG, "📋 chainKdf Debug:");
    printf("   chain_key_in:  "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.chain_key_send[i]); printf("...\n");
    printf("   message_key:   "); for(int i=0; i<8; i++) printf("%02x", message_key[i]); printf("...\n");
    printf("   msg_iv:        "); for(int i=0; i<8; i++) printf("%02x", msg_iv[i]); printf("...\n");
    printf("   header_iv:     "); for(int i=0; i<8; i++) printf("%02x", header_iv[i]); printf("...\n");
    printf("   header_key:    "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.header_key_send[i]); printf("...\n");
    
    memcpy(ratchet_state.chain_key_send, next_chain_key, 32);

    // 2. Build plaintext MsgHeader
    uint8_t msg_header[MSG_HEADER_PADDED_LEN];
    build_msg_header(msg_header, ratchet_state.dh_self.public_key,
                     ratchet_state.prev_chain_len, ratchet_state.msg_num_send);
    
    // DEBUG: Show MsgHeader structure
    // Layout: [8B Int64=79][2B version][1B len][68B SPKI][4B msgPN][4B msgNs][1B '#'] = 88
    ESP_LOGI(TAG, "📋 MsgHeader debug:");
    ESP_LOGI(TAG, "   Int64 len: %02x %02x %02x %02x %02x %02x %02x %02x", msg_header[0], msg_header[1], msg_header[2], msg_header[3], msg_header[4], msg_header[5], msg_header[6], msg_header[7]);
    ESP_LOGI(TAG, "   msgMaxVersion: %02x %02x", msg_header[8], msg_header[9]);
    ESP_LOGI(TAG, "   msgDHRs len: %02x (=%d)", msg_header[10], msg_header[10]);
    ESP_LOGI(TAG, "   SPKI header: %02x %02x %02x %02x...", msg_header[11], msg_header[12], msg_header[13], msg_header[14]);
    ESP_LOGI(TAG, "   X448 key: %02x %02x %02x %02x...", msg_header[23], msg_header[24], msg_header[25], msg_header[26]);
    ESP_LOGI(TAG, "   msgPN: %02x%02x%02x%02x (offset 79)", msg_header[79], msg_header[80], msg_header[81], msg_header[82]);
    ESP_LOGI(TAG, "   msgNs: %02x%02x%02x%02x (offset 83)", msg_header[83], msg_header[84], msg_header[85], msg_header[86]);
    ESP_LOGI(TAG, "   padding: %02x%02x%02x%02x%02x%02x%02x%02x%02x",
            msg_header[79], msg_header[80], msg_header[81], msg_header[82],
            msg_header[83], msg_header[84], msg_header[85], msg_header[86], msg_header[87]);
    
    // FULL HEX DUMP
    ESP_LOGI(TAG, "📋 MsgHeader FULL (88 bytes):");
    printf("   ");
    for (int i = 0; i < 88; i++) {
        printf("%02x", msg_header[i]);
        if ((i+1) % 32 == 0) printf("\n   ");
    }
    printf("\n");

    // 3. Build full AAD (rcAD + plaintext MsgHeader) – für BEIDE encryptions
    uint8_t aad_full[AAD_FULL_LEN];
    memcpy(aad_full, ratchet_state.assoc_data, 112);
    memcpy(aad_full + 112, msg_header, MSG_HEADER_PADDED_LEN);

    // 4. Encrypt Header
    uint8_t encrypted_header[MSG_HEADER_PADDED_LEN];
    uint8_t header_tag[GCM_TAG_LEN];
    if (aes_gcm_encrypt(ratchet_state.header_key_send, header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,
                        msg_header, MSG_HEADER_PADDED_LEN,
                        encrypted_header, header_tag) != 0) {
        return -1;
    }
    // ===== AES-GCM DEBUG OUTPUT FOR PYTHON VERIFICATION =====
    ESP_LOGI(TAG, "📋 AES-GCM TEST DATA (msg %u):", ratchet_state.msg_num_send);
    printf("header_key = \"");
    for (int i = 0; i < 32; i++) printf("%02x", ratchet_state.header_key_send[i]);
    printf("\"\n");
    
    printf("header_iv = \"");
    for (int i = 0; i < 16; i++) printf("%02x", header_iv[i]);
    printf("\"\n");
    
    printf("rcAD = \"");
    for (int i = 0; i < 112; i++) printf("%02x", ratchet_state.assoc_data[i]);
    printf("\"\n");
    
    printf("msg_header_plain = \"");
    for (int i = 0; i < 88; i++) printf("%02x", msg_header[i]);
    printf("\"\n");
    
    printf("header_tag = \"");
    for (int i = 0; i < 16; i++) printf("%02x", header_tag[i]);
    printf("\"\n");
    
    printf("encrypted_header = \"");
    for (int i = 0; i < 88; i++) printf("%02x", encrypted_header[i]);
    printf("\"\n");
    // ===== END DEBUG =====
    // 5. Build emHeader (123 bytes for Version 2!)
    // Version 2 uses 1-byte length prefixes (standard ByteString encoding)
    // Version 3+ would use 2-byte (Large encoding)
    uint8_t em_header[123];
    int hp = 0;
    em_header[hp++] = 0x00; em_header[hp++] = RATCHET_VERSION;          // ehVersion (Word16 BE)
    memcpy(&em_header[hp], header_iv, 16); hp += 16;                    // ehIV direct (no length prefix)
    memcpy(&em_header[hp], header_tag, 16); hp += 16;                   // ehAuthTag direct (no length prefix)
    em_header[hp++] = 0x58;                                             // ehBody-len = 88 (1 BYTE for v2!)
    memcpy(&em_header[hp], encrypted_header, 88); hp += 88;

    // DEBUG: Show emHeader structure
    ESP_LOGI(TAG, "📋 emHeader debug:");
    ESP_LOGI(TAG, "   ehVersion: %02x %02x", em_header[0], em_header[1]);
    ESP_LOGI(TAG, "   ehIV: %02x%02x%02x%02x...", em_header[2], em_header[3], em_header[4], em_header[5]);
    ESP_LOGI(TAG, "   ehAuthTag: %02x%02x%02x%02x...", em_header[18], em_header[19], em_header[20], em_header[21]);
    ESP_LOGI(TAG, "   ehBody len: %02x (=%d)", em_header[34], em_header[34]);

    // 6. Build AAD for payload: rcAD + emHeader (OHNE length prefix!)
    uint8_t payload_aad[235];  // 112 + 123 = 235 (nicht 236!)
    memcpy(payload_aad, ratchet_state.assoc_data, 112);
    memcpy(payload_aad + 112, em_header, 123);  // KEIN length prefix!
    
    // 6.5 PAD PLAINTEXT TO padded_msg_len BYTES
    uint8_t *padded_payload = malloc(padded_msg_len);
    if (!padded_payload) return -1;
    
    // Padding format: [2-byte Word16 BE length][plaintext][###...###]
    // SimpleX Crypto.hs pad() uses Word16 (2 bytes) for AES-GCM!
    padded_payload[0] = (pt_len >> 8) & 0xFF;
    padded_payload[1] = pt_len & 0xFF;
    memcpy(&padded_payload[2], plaintext, pt_len);
    memset(&padded_payload[2 + pt_len], '#', padded_msg_len - 2 - pt_len);
    
    // 7. Encrypt PADDED Payload
    uint8_t *encrypted_payload = malloc(padded_msg_len);
    if (!encrypted_payload) { free(padded_payload); return -1; }
    
    uint8_t payload_tag[GCM_TAG_LEN];
    if (aes_gcm_encrypt(message_key, msg_iv, GCM_IV_LEN,
                        payload_aad, 235,  // ← 236 bytes AAD!
                        padded_payload, padded_msg_len,  // ← PADDED!
                        encrypted_payload, payload_tag) != 0) {
        free(padded_payload);
        free(encrypted_payload);
        return -1;
    }
    
    free(padded_payload);
    
    // 8. Assemble final output
    int p = 0;
    output[p++] = 0x7B;                         // emHeader len = 123 (1 BYTE for v2!)
    memcpy(&output[p], em_header, 123); p += 123;
    memcpy(&output[p], payload_tag, 16); p += 16;
    memcpy(&output[p], encrypted_payload, padded_msg_len); p += padded_msg_len;
    
    free(encrypted_payload);
    *out_len = p;

    // DEBUG: Show EncRatchetMessage structure
    ESP_LOGI(TAG, "📋 EncRatchetMessage (first 30 bytes):");
    printf("   ");
    for (int i = 0; i < 30 && i < p; i++) printf("%02x ", output[i]);
    printf("\n");

    ratchet_state.msg_num_send++;

    ESP_LOGI(TAG, "✅ Encrypted: %zu bytes (msg %u)", *out_len, ratchet_state.msg_num_send - 1);
    return 0;
}

// ============== Decrypt Message ==============

int ratchet_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    uint8_t *plaintext, size_t *pt_len) {
    
    if (ct_len < 1 + 123 + 16) return -1;

    int p = 0;
    uint8_t em_header_len = ciphertext[p++];
    if (em_header_len != 123) {
        ESP_LOGE(TAG, "Invalid emHeader length: %d (expected 123)", em_header_len);
        return -1;
    }

    const uint8_t *em_header = &ciphertext[p];
    p += 123;

    int hp = 0;
    uint16_t version = (em_header[hp] << 8) | em_header[hp + 1]; hp += 2;
    if (version != RATCHET_VERSION) {
        ESP_LOGE(TAG, "Unsupported version: %d", version);
        return -1;
    }

    uint8_t header_iv[16];
    memcpy(header_iv, &em_header[hp], 16); hp += 16;

    uint8_t header_tag[16];
    memcpy(header_tag, &em_header[hp], 16); hp += 16;

    uint8_t eh_body_len = em_header[hp++];
    if (eh_body_len != 88) {
        ESP_LOGE(TAG, "Invalid ehBody length: %d", eh_body_len);
        return -1;
    }

    const uint8_t *encrypted_header = &em_header[hp];

    const uint8_t *payload_tag = &ciphertext[p]; p += 16;
    const uint8_t *encrypted_payload = &ciphertext[p];
    size_t payload_len = ct_len - p;

    // Prepare AAD (rcAD + plaintext MsgHeader – aber wir kennen MsgHeader erst nach Decrypt → chicken-egg)
    // Für Decrypt: AAD muss vorab bekannt sein → in SimpleX/Signal ist AAD meist rcAD + MsgHeader plaintext
    // → Wir decrypten Header erst, dann können wir AAD bauen, aber das passt nicht.
    // Lösung: Für erste Nachricht oft nur rcAD als AAD (da MsgHeader public ist, aber encrypted)
    // → Testweise erst nur rcAD (112 bytes) für Header-Decrypt versuchen

    uint8_t decrypted_header[MSG_HEADER_PADDED_LEN];
    if (aes_gcm_decrypt(ratchet_state.header_key_recv, header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,  // ← nur rcAD erstmal!
                        encrypted_header, eh_body_len,
                        header_tag, decrypted_header) != 0) {
        ESP_LOGE(TAG, "Header decryption failed (try with full AAD?)");
        return -1;
    }

    // Parse decrypted_header to get PN, Ns, DH key (für Ratchet fortsetzen)

    // Derive message key + IVs
    uint8_t message_key[32], next_chain_key[32], msg_iv[16], unused_iv[16];
    kdf_chain(ratchet_state.chain_key_recv, next_chain_key, message_key, msg_iv, unused_iv);
    memcpy(ratchet_state.chain_key_recv, next_chain_key, 32);

    if (aes_gcm_decrypt(message_key, msg_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,  // ← erstmal nur rcAD
                        encrypted_payload, payload_len,
                        payload_tag, plaintext) != 0) {
        ESP_LOGE(TAG, "Payload decryption failed");
        return -1;
    }

    *pt_len = payload_len;
    ESP_LOGI(TAG, "✅ Decrypted: %zu bytes", *pt_len);
    return 0;
}

// ============== Decrypt Incoming (Receiver Side) ==============
// For first message from App after we sent AgentConfirmation:
// - App did ratchet step, uses NEW DH key
// - We decrypt header with header_key_recv (from X3DH = nhk)
// - Extract App's new DH key from header
// - Do rootKdf to derive chain_key_recv
// - Use chainKdf to get message key
// - Decrypt payload

int ratchet_decrypt_incoming(const uint8_t *ciphertext, size_t ct_len,
                             uint8_t *plaintext, size_t *pt_len) {
    if (!ratchet_state.initialized) {
        ESP_LOGE(TAG, "Ratchet not initialized!");
        return -1;
    }
    
    if (ct_len < 1 + 123 + 16) {
        ESP_LOGE(TAG, "Ciphertext too short: %zu", ct_len);
        return -1;
    }
    
    ESP_LOGI(TAG, "🔓 Decrypting incoming message (%zu bytes)...", ct_len);
    
    int p = 0;
    
    // Parse EncRatchetMessage structure
    uint8_t em_header_len = ciphertext[p++];
    ESP_LOGI(TAG, "   emHeader length: %d", em_header_len);
    
    if (em_header_len != 123 && em_header_len != 0x7B) {
        // Could be Large encoding (2-byte length for v3+)
        if (ciphertext[0] < 32) {
            uint16_t len = (ciphertext[0] << 8) | ciphertext[1];
            ESP_LOGI(TAG, "   Large encoding, emHeader length: %d", len);
            p = 2;
            em_header_len = (uint8_t)len;
        } else {
            ESP_LOGE(TAG, "Invalid emHeader length: %d", em_header_len);
            return -1;
        }
    }
    
    const uint8_t *em_header = &ciphertext[p];
    p += em_header_len;
    
    // Parse EncMessageHeader
    int hp = 0;
    uint16_t version = (em_header[hp] << 8) | em_header[hp + 1]; hp += 2;
    ESP_LOGI(TAG, "   Version: %d", version);
    
    uint8_t header_iv[16];
    memcpy(header_iv, &em_header[hp], 16); hp += 16;
    
    uint8_t header_tag[16];
    memcpy(header_tag, &em_header[hp], 16); hp += 16;
    
    uint8_t eh_body_len = em_header[hp++];
    ESP_LOGI(TAG, "   ehBody length: %d", eh_body_len);
    
    const uint8_t *encrypted_header = &em_header[hp];
    
    // Payload follows emHeader
    const uint8_t *payload_tag = &ciphertext[p]; p += 16;
    const uint8_t *encrypted_payload = &ciphertext[p];
    size_t payload_len = ct_len - p;
    
    ESP_LOGI(TAG, "   Payload length: %zu", payload_len);
    
    // === Step 1: Decrypt Header ===
    uint8_t decrypted_header[MSG_HEADER_PADDED_LEN];
    
    ESP_LOGI(TAG, "   Trying header decrypt with header_key_recv...");
    ESP_LOGI(TAG, "   header_key_recv: %02x%02x%02x%02x...",
             ratchet_state.header_key_recv[0], ratchet_state.header_key_recv[1],
             ratchet_state.header_key_recv[2], ratchet_state.header_key_recv[3]);
    
    if (aes_gcm_decrypt(ratchet_state.header_key_recv, header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,
                        encrypted_header, eh_body_len,
                        header_tag, decrypted_header) != 0) {
        ESP_LOGE(TAG, "   ❌ Header decryption failed with header_key_recv!");
        
        ESP_LOGI(TAG, "   Trying with header_key_send...");
        if (aes_gcm_decrypt(ratchet_state.header_key_send, header_iv, GCM_IV_LEN,
                            ratchet_state.assoc_data, 112,
                            encrypted_header, eh_body_len,
                            header_tag, decrypted_header) != 0) {
            ESP_LOGE(TAG, "   ❌ Header decryption also failed with header_key_send!");
            return -1;
        }
        ESP_LOGI(TAG, "   ✅ Header decrypted with header_key_send");
    } else {
        ESP_LOGI(TAG, "   ✅ Header decrypted with header_key_recv");
    }
    
    // === Step 2: Parse MsgHeader ===
    ESP_LOGI(TAG, "   📋 Decrypted MsgHeader (first 20 bytes):");
    printf("      ");
    for (int i = 0; i < 20; i++) printf("%02x ", decrypted_header[i]);
    printf("\n");
    
    int mhp = 0;
    uint16_t content_len = (decrypted_header[mhp] << 8) | decrypted_header[mhp + 1]; mhp += 2;
    uint16_t msg_version = (decrypted_header[mhp] << 8) | decrypted_header[mhp + 1]; mhp += 2;
    uint8_t key_len = decrypted_header[mhp++];
    
    ESP_LOGI(TAG, "   Content len: %d, Version: %d, Key len: %d", content_len, msg_version, key_len);
    
    if (key_len != 68) {
        ESP_LOGE(TAG, "   ❌ Unexpected key length: %d", key_len);
        return -1;
    }
    
    // Extract peer's new DH public key (skip 12-byte SPKI header)
    uint8_t peer_new_dh[56];
    memcpy(peer_new_dh, &decrypted_header[mhp + 12], 56);
    mhp += 68;
    
    uint32_t msg_pn = (decrypted_header[mhp] << 24) | (decrypted_header[mhp+1] << 16) |
                      (decrypted_header[mhp+2] << 8) | decrypted_header[mhp+3];
    mhp += 4;
    uint32_t msg_ns = (decrypted_header[mhp] << 24) | (decrypted_header[mhp+1] << 16) |
                      (decrypted_header[mhp+2] << 8) | decrypted_header[mhp+3];
    
    ESP_LOGI(TAG, "   PN: %u, Ns: %u", msg_pn, msg_ns);
    ESP_LOGI(TAG, "   Peer new DH: %02x%02x%02x%02x...",
             peer_new_dh[0], peer_new_dh[1], peer_new_dh[2], peer_new_dh[3]);
    
    // === Step 3: DH Ratchet Step (if new DH key) ===
    bool dh_changed = (memcmp(peer_new_dh, ratchet_state.dh_peer, 56) != 0);
    
    if (dh_changed) {
        ESP_LOGI(TAG, "   🔄 New DH key detected - doing ratchet step...");
        
        uint8_t dh_out[56];
        if (!x448_dh(peer_new_dh, ratchet_state.dh_self.private_key, dh_out)) {
            ESP_LOGE(TAG, "   ❌ DH failed!");
            return -1;
        }
        
        ESP_LOGI(TAG, "   DH output: %02x%02x%02x%02x...",
                 dh_out[0], dh_out[1], dh_out[2], dh_out[3]);
        
        uint8_t new_root_key[32], new_chain_key[32], new_header_key[32];
        kdf_root(ratchet_state.root_key, dh_out, new_root_key, new_chain_key, new_header_key);
        
        memcpy(ratchet_state.root_key, new_root_key, 32);
        memcpy(ratchet_state.chain_key_recv, new_chain_key, 32);
        memcpy(ratchet_state.header_key_recv, new_header_key, 32);
        memcpy(ratchet_state.dh_peer, peer_new_dh, 56);
        ratchet_state.msg_num_recv = 0;
        
        ESP_LOGI(TAG, "   New chain_key_recv: %02x%02x%02x%02x...",
                 ratchet_state.chain_key_recv[0], ratchet_state.chain_key_recv[1],
                 ratchet_state.chain_key_recv[2], ratchet_state.chain_key_recv[3]);
    }
    
    // === Step 4: chainKdf to get message key ===
    uint8_t message_key[32], next_chain_key[32], msg_iv[16], unused_iv[16];
    uint8_t temp_ck[32];
    memcpy(temp_ck, ratchet_state.chain_key_recv, 32);
    
    for (uint32_t i = ratchet_state.msg_num_recv; i < msg_ns; i++) {
        kdf_chain(temp_ck, next_chain_key, message_key, msg_iv, unused_iv);
        memcpy(temp_ck, next_chain_key, 32);
        ESP_LOGI(TAG, "   Skipped to msg %u", i + 1);
    }
    
    kdf_chain(temp_ck, next_chain_key, message_key, msg_iv, unused_iv);
    memcpy(ratchet_state.chain_key_recv, next_chain_key, 32);
    ratchet_state.msg_num_recv = msg_ns + 1;
    
    ESP_LOGI(TAG, "   message_key: %02x%02x%02x%02x...",
             message_key[0], message_key[1], message_key[2], message_key[3]);
    
    // === Step 5: Decrypt Payload ===
    // AAD = rcAD + emHeader
    uint8_t *payload_aad = malloc(112 + em_header_len);
    if (!payload_aad) {
        ESP_LOGE(TAG, "   ❌ malloc failed");
        return -1;
    }
    memcpy(payload_aad, ratchet_state.assoc_data, 112);
    memcpy(payload_aad + 112, em_header, em_header_len);
    
    if (aes_gcm_decrypt(message_key, msg_iv, GCM_IV_LEN,
                        payload_aad, 112 + em_header_len,
                        encrypted_payload, payload_len,
                        payload_tag, plaintext) != 0) {
        ESP_LOGE(TAG, "   ❌ Payload decryption failed!");
        free(payload_aad);
        return -1;
    }
    
    free(payload_aad);
    
    // Unpad: first 2 bytes = actual length
    uint16_t actual_len = (plaintext[0] << 8) | plaintext[1];
    ESP_LOGI(TAG, "   Padded: %zu bytes, actual: %d bytes", payload_len, actual_len);
    
    memmove(plaintext, plaintext + 2, actual_len);
    *pt_len = actual_len;
    
    ESP_LOGI(TAG, "   ✅ Decrypted message: %zu bytes", *pt_len);
    ESP_LOGI(TAG, "   First bytes: %02x %02x %02x %02x",
             plaintext[0], plaintext[1], plaintext[2], plaintext[3]);
    
    return 0;
}

// ============== Getters ==============

ratchet_state_t *ratchet_get_state(void) { return &ratchet_state; }
bool ratchet_is_initialized(void) { return ratchet_state.initialized; }