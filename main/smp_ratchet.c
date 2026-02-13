/**
 * SimpleGo - smp_ratchet.c
 * Double Ratchet Encryption - Version 3 Wire Format (non-PQ)
 * v0.1.23-alpha - Updated 2026-02-07
 * 
 * SESSION 22 FIXES:
 * - Fix 1: X3DH nhk → next_header_key_recv (NOT header_key_recv!)
 * - Fix 2: ratchet_init_sender saves NHKs to next_header_key_send
 * - Fix 3: ratchet_decrypt_body does proper HK←NHK promotion per Signal spec
 * 
 * Signal Double Ratchet with Header Encryption spec:
 *   DHRatchetHE(): state.HKs = state.NHKs; state.HKr = state.NHKr;
 *   Then NHKs/NHKr ← KDF output
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
#define RATCHET_VERSION         3
#define MSG_HEADER_CONTENT_LEN  80
#define MSG_HEADER_PADDED_LEN   88
#define GCM_IV_LEN              16
#define GCM_TAG_LEN             16
#define AAD_FULL_LEN            200

// ============== Ratchet State ==============

static ratchet_state_t ratchet_state = {0};

// Saved X3DH keys (before ratchet_init_sender modifies them)
static uint8_t saved_x3dh_hk[32] = {0};
static uint8_t saved_x3dh_nhk[32] = {0};
static bool saved_x3dh_valid = false;

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
    memcpy(new_root_key, kdf_output, 32);
    memcpy(chain_key, kdf_output + 32, 32);
    memcpy(next_header_key, kdf_output + 64, 32);
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
    memcpy(msg_iv, kdf_output + 64, 16);
    memcpy(header_iv, kdf_output + 80, 16);
}

// ============== X3DH Key Agreement ==============

bool ratchet_x3dh_sender(const uint8_t *peer_key1,
                         const uint8_t *peer_key2,
                         const x448_keypair_t *our_key1,
                         const x448_keypair_t *our_key2) {
    ESP_LOGI(TAG, "🔐 X3DH Key Agreement (sender)...");
    ESP_LOGI(TAG, "   Note: We are RESPONDER, peer is INITIATOR");

    uint8_t dh1[56], dh2[56], dh3[56];
    if (!x448_dh(peer_key1, our_key2->private_key, dh1)) return false;
    if (!x448_dh(peer_key2, our_key1->private_key, dh2)) return false;
    if (!x448_dh(peer_key2, our_key2->private_key, dh3)) return false;

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

    // ================================================================
    // FIX 1: X3DH Output Assignment per Signal spec RatchetInitAliceHE()
    // bytes 0-31:  hk → HKs (header_key_send) — active send header key
    // bytes 32-63: nhk → NHKr (next_header_key_recv) — NOT active HKr!
    // bytes 64-95: root_key
    // 
    // Signal: "state.HKr = None; state.NHKr = shared_nhkb"
    // header_key_recv stays 0x00 until first AdvanceRatchet
    // ================================================================
    memcpy(ratchet_state.header_key_send, kdf_output, 32);           // HKs = hk
    memcpy(ratchet_state.next_header_key_recv, kdf_output + 32, 32); // NHKr = nhk (FIX 1!)
    // header_key_recv stays 0x00 — no active HKr before first AdvanceRatchet
    memcpy(ratchet_state.root_key, kdf_output + 64, 32);

    ESP_LOGI(TAG, "📋 X3DH Debug:");
    printf("   dh1: "); for(int i=0; i<8; i++) printf("%02x", dh1[i]); printf("...\n");
    printf("   dh2: "); for(int i=0; i<8; i++) printf("%02x", dh2[i]); printf("...\n");
    printf("   dh3: "); for(int i=0; i<8; i++) printf("%02x", dh3[i]); printf("...\n");
    printf("   hk (HKs):   "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.header_key_send[i]); printf("...\n");
    printf("   nhk (NHKr): "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.next_header_key_recv[i]); printf("...\n");
    printf("   rk:         "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.root_key[i]); printf("...\n");

    memcpy(ratchet_state.assoc_data, our_key1->public_key, 56);
    memcpy(ratchet_state.assoc_data + 56, peer_key1, 56);

    ESP_LOGI(TAG, "📋 rcAD (our || peer):");
    printf("   our_key1:  ");
    for (int i = 0; i < 16; i++) printf("%02x ", ratchet_state.assoc_data[i]);
    printf("...\n   peer_key1: ");
    for (int i = 56; i < 72; i++) printf("%02x ", ratchet_state.assoc_data[i]);
    printf("...\n");

    ESP_LOGI(TAG, "✅ X3DH complete - RootKey: %02x%02x...", ratchet_state.root_key[0], ratchet_state.root_key[1]);
    
    memcpy(saved_x3dh_hk, ratchet_state.header_key_send, 32);
    memcpy(saved_x3dh_nhk, ratchet_state.next_header_key_recv, 32);
    saved_x3dh_valid = true;
    ESP_LOGI(TAG, "📌 Saved X3DH keys: hk=%02x%02x..., nhk=%02x%02x...",
             saved_x3dh_hk[0], saved_x3dh_hk[1], saved_x3dh_nhk[0], saved_x3dh_nhk[1]);
    
    return true;
}

// ============== Ratchet Initialization ==============

bool ratchet_init_sender(const uint8_t *peer_dh_public, const x448_keypair_t *our_key2) {
    printf("ratchet_init_sender inputs:\n");
    printf("   peer_dh_public: "); for(int i=0; i<8; i++) printf("%02x", peer_dh_public[i]); printf("...\n");
    printf("   our_key2_pub:   "); for(int i=0; i<8; i++) printf("%02x", our_key2->public_key[i]); printf("...\n");
    printf("   our_key2_priv:  "); for(int i=0; i<8; i++) printf("%02x", our_key2->private_key[i]); printf("...\n");
    
    ESP_LOGI(TAG, "🔄 Initializing initial send ratchet...");

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
    
    // ================================================================
    // FIX 2: Save NHKs per Signal spec RatchetInitAliceHE()
    // "state.RK, state.CKs, state.NHKs = KDF_RK_HE(...)"
    // NHKs will be promoted to HKs on first AdvanceRatchet
    // ================================================================
    memcpy(ratchet_state.next_header_key_send, next_header_key, 32);  // FIX 2!

    ESP_LOGI(TAG, "📋 Root KDF output:");
    printf("   dh_out:         "); for(int i=0; i<8; i++) printf("%02x", dh_out[i]); printf("...\n");
    printf("   new_rk:         "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.root_key[i]); printf("...\n");
    printf("   ck:             "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.chain_key_send[i]); printf("...\n");
    printf("   next_hk (NHKs): "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.next_header_key_send[i]); printf("...\n");

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

    header[p++] = 0x00;
    header[p++] = 80;

    header[p++] = 0x00;
    header[p++] = RATCHET_VERSION;

    header[p++] = 68;

    static const uint8_t X448_SPKI_HEADER[12] = {0x30,0x42,0x30,0x05,0x06,0x03,0x2b,0x65,0x6f,0x03,0x39,0x00};
    memcpy(&header[p], X448_SPKI_HEADER, 12); p += 12;
    memcpy(&header[p], dh_public, 56); p += 56;

    header[p++] = 0x30;

    header[p++] = (pn >> 24) & 0xFF;
    header[p++] = (pn >> 16) & 0xFF;
    header[p++] = (pn >> 8)  & 0xFF;
    header[p++] = pn & 0xFF;

    header[p++] = (ns >> 24) & 0xFF;
    header[p++] = (ns >> 16) & 0xFF;
    header[p++] = (ns >> 8)  & 0xFF;
    header[p++] = ns & 0xFF;

    memset(&header[p], '#', 88 - p);
}

// ============== Encrypt Message ==============

int ratchet_encrypt(const uint8_t *plaintext, size_t pt_len,
                    uint8_t *output, size_t *out_len,
                    size_t padded_msg_len) {
    
    if (!ratchet_state.initialized) return -1;

    // === Auftrag 35b: Pre-encrypt state dump ===
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📤 === ENCRYPT STATE (msg_num_send=%u) ===", ratchet_state.msg_num_send);
    printf("   HKs  (header_key_send):      "); for(int i=0; i<32; i++) printf("%02x", ratchet_state.header_key_send[i]); printf("\n");
    printf("   NHKs (next_header_key_send):  "); for(int i=0; i<32; i++) printf("%02x", ratchet_state.next_header_key_send[i]); printf("\n");
    printf("   CKs  (chain_key_send):        "); for(int i=0; i<32; i++) printf("%02x", ratchet_state.chain_key_send[i]); printf("\n");
    printf("   RK   (root_key):              "); for(int i=0; i<32; i++) printf("%02x", ratchet_state.root_key[i]); printf("\n");
    printf("   DH self pub:                  "); for(int i=0; i<16; i++) printf("%02x", ratchet_state.dh_self.public_key[i]); printf("...\n");
    printf("   DH peer:                      "); for(int i=0; i<16; i++) printf("%02x", ratchet_state.dh_peer[i]); printf("...\n");
    ESP_LOGI(TAG, "   msg_num_send=%u, prev_chain_len=%u",
             ratchet_state.msg_num_send, ratchet_state.prev_chain_len);
    ESP_LOGI(TAG, "📤 ================================");

    // 1. Derive keys & IVs

    uint8_t message_key[32], next_chain_key[32], msg_iv[16], header_iv[16];
    kdf_chain(ratchet_state.chain_key_send, next_chain_key, message_key, msg_iv, header_iv);
    
    ESP_LOGI(TAG, "📋 chainKdf Debug:");
    printf("   chain_key_in:  "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.chain_key_send[i]); printf("...\n");
    printf("   message_key:   "); for(int i=0; i<8; i++) printf("%02x", message_key[i]); printf("...\n");
    printf("   msg_iv:        "); for(int i=0; i<8; i++) printf("%02x", msg_iv[i]); printf("...\n");
    printf("   header_iv:     "); for(int i=0; i<8; i++) printf("%02x", header_iv[i]); printf("...\n");
    printf("   header_key:    "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.header_key_send[i]); printf("...\n");
    
    memcpy(ratchet_state.chain_key_send, next_chain_key, 32);

    uint8_t msg_header[MSG_HEADER_PADDED_LEN];
    build_msg_header(msg_header, ratchet_state.dh_self.public_key,
                     ratchet_state.prev_chain_len, ratchet_state.msg_num_send);
    
    ESP_LOGI(TAG, "📋 MsgHeader debug:");
    ESP_LOGI(TAG, "   Word16 len: %02x %02x (=%d)", msg_header[0], msg_header[1], (msg_header[0]<<8)|msg_header[1]);
    ESP_LOGI(TAG, "   msgMaxVersion: %02x %02x", msg_header[2], msg_header[3]);
    ESP_LOGI(TAG, "   msgDHRs len: %02x (=%d)", msg_header[4], msg_header[4]);
    ESP_LOGI(TAG, "   SPKI header: %02x %02x %02x %02x...", msg_header[5], msg_header[6], msg_header[7], msg_header[8]);
    ESP_LOGI(TAG, "   X448 key: %02x %02x %02x %02x...", msg_header[17], msg_header[18], msg_header[19], msg_header[20]);
    ESP_LOGI(TAG, "   KEM: %02x '%c' (offset 73)", msg_header[73], msg_header[73]);
    ESP_LOGI(TAG, "   msgPN: %02x%02x%02x%02x (offset 74)", msg_header[74], msg_header[75], msg_header[76], msg_header[77]);
    ESP_LOGI(TAG, "   msgNs: %02x%02x%02x%02x (offset 78)", msg_header[78], msg_header[79], msg_header[80], msg_header[81]);
    ESP_LOGI(TAG, "   padding: %02x (offset 82)", msg_header[82]);
    
    ESP_LOGI(TAG, "📋 MsgHeader FULL (88 bytes):");
    printf("   ");
    for (int i = 0; i < 88; i++) {
        printf("%02x", msg_header[i]);
        if ((i+1) % 32 == 0) printf("\n   ");
    }
    printf("\n");

    uint8_t aad_full[AAD_FULL_LEN];
    memcpy(aad_full, ratchet_state.assoc_data, 112);
    memcpy(aad_full + 112, msg_header, MSG_HEADER_PADDED_LEN);

    uint8_t encrypted_header[MSG_HEADER_PADDED_LEN];
    uint8_t header_tag[GCM_TAG_LEN];
    if (aes_gcm_encrypt(ratchet_state.header_key_send, header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,
                        msg_header, MSG_HEADER_PADDED_LEN,
                        encrypted_header, header_tag) != 0) {
        return -1;
    }

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

    uint8_t em_header[124];
    int hp = 0;
    em_header[hp++] = 0x00; em_header[hp++] = RATCHET_VERSION;
    memcpy(&em_header[hp], header_iv, 16); hp += 16;
    memcpy(&em_header[hp], header_tag, 16); hp += 16;
    em_header[hp++] = 0x00; em_header[hp++] = 0x58;
    memcpy(&em_header[hp], encrypted_header, 88); hp += 88;

    ESP_LOGI(TAG, "📋 emHeader debug (v3, 124 bytes):");
    ESP_LOGI(TAG, "   ehVersion: %02x %02x (=%d)", em_header[0], em_header[1], (em_header[0]<<8)|em_header[1]);
    ESP_LOGI(TAG, "   ehIV: %02x%02x%02x%02x...", em_header[2], em_header[3], em_header[4], em_header[5]);
    ESP_LOGI(TAG, "   ehAuthTag: %02x%02x%02x%02x...", em_header[18], em_header[19], em_header[20], em_header[21]);
    ESP_LOGI(TAG, "   ehBody len: %02x %02x (=%d, Large encoding)", em_header[34], em_header[35], (em_header[34]<<8)|em_header[35]);

    uint8_t payload_aad[236];
    memcpy(payload_aad, ratchet_state.assoc_data, 112);
    memcpy(payload_aad + 112, em_header, 124);
    
    uint8_t *padded_payload = malloc(padded_msg_len);
    if (!padded_payload) return -1;
    
    padded_payload[0] = (pt_len >> 8) & 0xFF;
    padded_payload[1] = pt_len & 0xFF;
    memcpy(&padded_payload[2], plaintext, pt_len);
    memset(&padded_payload[2 + pt_len], '#', padded_msg_len - 2 - pt_len);

    ESP_LOGI(TAG, "🔬 [L5] Inner Padded Payload: %zu bytes", padded_msg_len);
    printf("   L5 first 16: ");
    for (int i = 0; i < 16; i++) printf("%02x ", padded_payload[i]);
    printf("\n");
    printf("   L5 last 4:   ");
    for (size_t i = padded_msg_len - 4; i < padded_msg_len; i++) printf("%02x ", padded_payload[i]);
    printf("\n");

    uint8_t *encrypted_payload = malloc(padded_msg_len);
    if (!encrypted_payload) {
        free(padded_payload);
        return -1;
    }
    uint8_t payload_tag[GCM_TAG_LEN];
    if (aes_gcm_encrypt(message_key, msg_iv, GCM_IV_LEN,
                        payload_aad, 236,
                        padded_payload, padded_msg_len,
                        encrypted_payload, payload_tag) != 0) {
        free(padded_payload);
        free(encrypted_payload);
        return -1;
    }

    int op = 0;
    output[op++] = 0x00;
    output[op++] = 124;
    memcpy(&output[op], em_header, 124); op += 124;
    memcpy(&output[op], payload_tag, 16); op += 16;
    memcpy(&output[op], encrypted_payload, padded_msg_len); op += padded_msg_len;
    *out_len = op;

    ESP_LOGI(TAG, "🔬 [L4] EncRatchetMessage: %zu bytes", *out_len);
    printf("   L4 first 32: ");
    for (size_t i = 0; i < 32 && i < *out_len; i++) printf("%02x ", output[i]);
    printf("\n");

    ratchet_state.msg_num_send++;
    free(padded_payload);
    free(encrypted_payload);

    ESP_LOGI(TAG, "✅ Encrypted %zu bytes (padded to %zu) -> %zu bytes", pt_len, padded_msg_len, *out_len);
    return 0;
}

// ============== Self-Decrypt Test ==============

int ratchet_self_decrypt_test(const uint8_t *ciphertext, size_t ct_len,
                              uint8_t *plaintext, size_t *pt_len) {
    ESP_LOGI(TAG, "🔬 Self-decrypt test (header only)...");
    
    int p = 0;
    uint16_t em_hdr_len = (ciphertext[0] << 8) | ciphertext[1];
    if (em_hdr_len != 124) {
        ESP_LOGE(TAG, "   ❌ Expected emHeader len 124 (0x007C), got %u (0x%04x)", em_hdr_len, em_hdr_len);
        return -1;
    }
    p = 2;
    
    uint16_t version = (ciphertext[p] << 8) | ciphertext[p + 1]; p += 2;
    uint8_t header_iv[16];
    memcpy(header_iv, &ciphertext[p], 16); p += 16;
    uint8_t header_tag[16];
    memcpy(header_tag, &ciphertext[p], 16); p += 16;
    uint16_t eh_body_len = (ciphertext[p] << 8) | ciphertext[p + 1]; p += 2;
    const uint8_t *encrypted_header = &ciphertext[p];
    
    ESP_LOGI(TAG, "   Version: %d, ehBody len: %d", version, eh_body_len);
    
    uint8_t decrypted_header[MSG_HEADER_PADDED_LEN];
    
    if (aes_gcm_decrypt(ratchet_state.header_key_send, header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,
                        encrypted_header, eh_body_len,
                        header_tag, decrypted_header) != 0) {
        ESP_LOGE(TAG, "❌ Self-decrypt FAILED!");
        ESP_LOGI(TAG, "   (This is expected - sender can't decrypt own message)");
        ESP_LOGI(TAG, "   Header encrypted with HKs, but for receiver it needs HKr");
        return -1;
    }
    
    ESP_LOGI(TAG, "✅ Self-decrypt SUCCESS!");
    ESP_LOGI(TAG, "   Header bytes: %02x %02x %02x %02x...",
             decrypted_header[0], decrypted_header[1], 
             decrypted_header[2], decrypted_header[3]);
    
    return 0;
}

// ============== Decrypt Message ==============

int ratchet_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    uint8_t *plaintext, size_t *pt_len) {
    ESP_LOGI(TAG, "🔓 Decrypting message (%zu bytes)...", ct_len);
    
    int p = 0;
    uint16_t em_header_len;

    em_header_len = (ciphertext[0] << 8) | ciphertext[1];
    p = 2;
    
    if (em_header_len == 124) {
        ESP_LOGI(TAG, "   ✓ v3 format (2-byte prefix, emHeader=124)");
    } else if (em_header_len == 123) {
        ESP_LOGI(TAG, "   ⚠️ Detected v2 format (0x7B prefix) — reparsing");
        em_header_len = 123;
        p = 1;
    } else {
        ESP_LOGW(TAG, "   ⚠️ Unexpected emHeader len: %u (0x%04x) — trying as v3", em_header_len, em_header_len);
    }

    ESP_LOGI(TAG, "   emHeader length: %u, starting at offset %d", em_header_len, p);
    
    const uint8_t *em_header = &ciphertext[p];
    p += em_header_len;
    
    int hp = 0;
    uint16_t version = (em_header[hp] << 8) | em_header[hp + 1]; hp += 2;
    ESP_LOGI(TAG, "   Version: %d", version);
    
    uint8_t header_iv[16];
    memcpy(header_iv, &em_header[hp], 16); hp += 16;
    
    uint8_t header_tag[16];
    memcpy(header_tag, &em_header[hp], 16); hp += 16;
    
    uint16_t eh_body_len;
    if (version >= 3) {
        eh_body_len = (em_header[hp] << 8) | em_header[hp + 1]; hp += 2;
    } else {
        eh_body_len = em_header[hp++];
    }
    ESP_LOGI(TAG, "   ehBody length: %d (v%d format)", eh_body_len, version);
    
    const uint8_t *encrypted_header = &em_header[hp];
    
    const uint8_t *payload_tag = &ciphertext[p]; p += 16;
    const uint8_t *encrypted_payload = &ciphertext[p];
    size_t payload_len = ct_len - p;
    
    ESP_LOGI(TAG, "   Payload length: %zu", payload_len);
    
    uint8_t decrypted_header[MSG_HEADER_PADDED_LEN];
    
    ESP_LOGI(TAG, "   rcAD (same for both directions): %02x%02x...||%02x%02x...",
             ratchet_state.assoc_data[0], ratchet_state.assoc_data[1],
             ratchet_state.assoc_data[56], ratchet_state.assoc_data[57]);

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
    
    uint8_t peer_new_dh[56];
    memcpy(peer_new_dh, &decrypted_header[mhp + 12], 56);
    mhp += 68;
    
    if (msg_version >= 3) {
        uint8_t kem_tag = decrypted_header[mhp];
        ESP_LOGI(TAG, "   KEM tag: 0x%02x '%c' %s", kem_tag, kem_tag,
                 kem_tag == 0x30 ? "(Nothing)" : "(Just - PQ!)");
        if (kem_tag == 0x30) {
            mhp += 1;
        } else if (kem_tag == 0x31) {
            ESP_LOGW(TAG, "   ⚠️ KEM = Just (PQ mode) — not implemented!");
            return -1;
        }
    }
    
    uint32_t msg_pn = (decrypted_header[mhp] << 24) | (decrypted_header[mhp+1] << 16) |
                      (decrypted_header[mhp+2] << 8) | decrypted_header[mhp+3];
    mhp += 4;
    uint32_t msg_ns = (decrypted_header[mhp] << 24) | (decrypted_header[mhp+1] << 16) |
                      (decrypted_header[mhp+2] << 8) | decrypted_header[mhp+3];
    
    ESP_LOGI(TAG, "   PN: %u, Ns: %u", msg_pn, msg_ns);
    ESP_LOGI(TAG, "   Peer new DH: %02x%02x%02x%02x...",
             peer_new_dh[0], peer_new_dh[1], peer_new_dh[2], peer_new_dh[3]);
    
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
    
    uint16_t actual_len = (plaintext[0] << 8) | plaintext[1];
    ESP_LOGI(TAG, "   Padded: %zu bytes, actual: %d bytes", payload_len, actual_len);
    
    memmove(plaintext, plaintext + 2, actual_len);
    *pt_len = actual_len;
    
    ESP_LOGI(TAG, "   ✅ Decrypted message: %zu bytes", *pt_len);
    ESP_LOGI(TAG, "   First bytes: %02x %02x %02x %02x",
             plaintext[0], plaintext[1], plaintext[2], plaintext[3]);
    
    return 0;
}

// ============== Decrypt Incoming Message ==============

int ratchet_decrypt_incoming(const uint8_t *ciphertext, size_t ct_len,
                             uint8_t *plaintext, size_t *pt_len) {
    ESP_LOGI(TAG, "🔓 Decrypting INCOMING message from peer (%zu bytes)...", ct_len);
    return ratchet_decrypt(ciphertext, ct_len, plaintext, pt_len);
}

// ============== Decrypt Body (Phase 2b) ==============

int ratchet_decrypt_body(ratchet_decrypt_mode_t mode,
                         const uint8_t *peer_new_pub,
                         uint32_t msg_pn, uint32_t msg_ns,
                         const uint8_t *em_header_raw, size_t em_header_len,
                         const uint8_t *em_auth_tag,
                         const uint8_t *em_body, size_t em_body_len,
                         uint8_t *plaintext, size_t *pt_len) {

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  🐰 PHASE 2b: Ratchet Body Decrypt                    ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════╝");

    ESP_LOGI(TAG, "   Inputs:");
    ESP_LOGI(TAG, "   peer_new_pub: %02x%02x%02x%02x%02x%02x%02x%02x...",
             peer_new_pub[0], peer_new_pub[1], peer_new_pub[2], peer_new_pub[3],
             peer_new_pub[4], peer_new_pub[5], peer_new_pub[6], peer_new_pub[7]);
    ESP_LOGI(TAG, "   msg_pn=%u, msg_ns=%u", msg_pn, msg_ns);
    ESP_LOGI(TAG, "   em_header_len=%zu, em_body_len=%zu", em_header_len, em_body_len);
    ESP_LOGI(TAG, "   emAuthTag: %02x%02x%02x%02x%02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x",
             em_auth_tag[0], em_auth_tag[1], em_auth_tag[2], em_auth_tag[3],
             em_auth_tag[4], em_auth_tag[5], em_auth_tag[6], em_auth_tag[7],
             em_auth_tag[8], em_auth_tag[9], em_auth_tag[10], em_auth_tag[11],
             em_auth_tag[12], em_auth_tag[13], em_auth_tag[14], em_auth_tag[15]);
    ESP_LOGI(TAG, "   emBody[0-15]: %02x%02x%02x%02x%02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x",
             em_body[0], em_body[1], em_body[2], em_body[3],
             em_body[4], em_body[5], em_body[6], em_body[7],
             em_body[8], em_body[9], em_body[10], em_body[11],
             em_body[12], em_body[13], em_body[14], em_body[15]);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   === Current Ratchet State (BEFORE body decrypt) ===");
    printf("   root_key:      "); for(int i=0; i<32; i++) printf("%02x", ratchet_state.root_key[i]); printf("\n");
    printf("   dh_self.priv:  "); for(int i=0; i<56; i++) printf("%02x", ratchet_state.dh_self.private_key[i]); printf("\n");
    printf("   dh_self.pub:   "); for(int i=0; i<56; i++) printf("%02x", ratchet_state.dh_self.public_key[i]); printf("\n");
    printf("   dh_peer:       "); for(int i=0; i<56; i++) printf("%02x", ratchet_state.dh_peer[i]); printf("\n");

    // Variables needed by both modes
    uint8_t recv_chain_key[32];
    uint8_t new_root_key_2[32] = {0};
    uint8_t send_chain_key[32] = {0};
    uint8_t new_nhk_recv[32] = {0};
    uint8_t new_nhk_send[32] = {0};
    x448_keypair_t new_dh_self = {0};

    if (mode == RATCHET_MODE_ADVANCE) {
    // SCHRITT 1: DH Ratchet Step — Receiving Chain
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   === SCHRITT 1: DH Ratchet (Receiving Chain) ===");

    uint8_t dh_secret_recv[56];
    if (!x448_dh(peer_new_pub, ratchet_state.dh_self.private_key, dh_secret_recv)) {
        ESP_LOGE(TAG, "   ❌ X448 DH (recv) failed!");
        return -1;
    }
    printf("   dh_secret_recv: "); for(int i=0; i<56; i++) printf("%02x", dh_secret_recv[i]); printf("\n");

    uint8_t new_root_key_1[32];
    kdf_root(ratchet_state.root_key, dh_secret_recv,
             new_root_key_1, recv_chain_key, new_nhk_recv);

    printf("   new_root_key_1: "); for(int i=0; i<32; i++) printf("%02x", new_root_key_1[i]); printf("\n");
    printf("   recv_chain_key: "); for(int i=0; i<32; i++) printf("%02x", recv_chain_key[i]); printf("\n");
    printf("   new_nhk_recv:   "); for(int i=0; i<32; i++) printf("%02x", new_nhk_recv[i]); printf("\n");

    // SCHRITT 2: DH Ratchet Step — Sending Chain
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   === SCHRITT 2: DH Ratchet (Sending Chain) ===");

    if (!x448_generate_keypair(&new_dh_self)) {
        ESP_LOGE(TAG, "   ❌ X448 keygen failed!");
        return -2;
    }
    printf("   new_dh_self.pub: "); for(int i=0; i<56; i++) printf("%02x", new_dh_self.public_key[i]); printf("\n");

    uint8_t dh_secret_send[56];
    if (!x448_dh(peer_new_pub, new_dh_self.private_key, dh_secret_send)) {
        ESP_LOGE(TAG, "   ❌ X448 DH (send) failed!");
        return -3;
    }
    printf("   dh_secret_send: "); for(int i=0; i<56; i++) printf("%02x", dh_secret_send[i]); printf("\n");

    kdf_root(new_root_key_1, dh_secret_send,
             new_root_key_2, send_chain_key, new_nhk_send);

    printf("   new_root_key_2: "); for(int i=0; i<32; i++) printf("%02x", new_root_key_2[i]); printf("\n");
    printf("   send_chain_key: "); for(int i=0; i<32; i++) printf("%02x", send_chain_key[i]); printf("\n");
    printf("   new_nhk_send:   "); for(int i=0; i<32; i++) printf("%02x", new_nhk_send[i]); printf("\n");

    } else {
        // RATCHET_MODE_SAME: Skip DH ratchet, use existing chain_key_recv
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "   === SAME RATCHET: Using existing recv chain (no DH) ===");
        memcpy(recv_chain_key, ratchet_state.chain_key_recv, 32);
        ESP_LOGI(TAG, "   chain_key_recv: %02x%02x%02x%02x%02x%02x%02x%02x...",
                 recv_chain_key[0], recv_chain_key[1], recv_chain_key[2], recv_chain_key[3],
                 recv_chain_key[4], recv_chain_key[5], recv_chain_key[6], recv_chain_key[7]);
    }

    // SCHRITT 3: Chain KDF
    // ADVANCE: chain starts at position 0, skip 0..msg_ns-1 (absolute)
    // SAME:    chain starts at position msg_num_recv, skip msg_num_recv..msg_ns-1 (relative)
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   === SCHRITT 3: Chain KDF ===");

    uint8_t temp_ck[32];
    memcpy(temp_ck, recv_chain_key, 32);

    uint8_t message_key[32], next_chain_key[32], iv_body[16], iv_header[16];

    uint32_t skip_from = (mode == RATCHET_MODE_ADVANCE) ? 0 : ratchet_state.msg_num_recv;
    ESP_LOGI(TAG, "   Chain position: %u, target Ns: %u, steps to skip: %u",
             skip_from, msg_ns, (msg_ns > skip_from) ? msg_ns - skip_from : 0);

    for (uint32_t i = skip_from; i < msg_ns; i++) {
        ESP_LOGI(TAG, "   Skipping chain step %u...", i);
        kdf_chain(temp_ck, next_chain_key, message_key, iv_body, iv_header);
        memcpy(temp_ck, next_chain_key, 32);
    }

    kdf_chain(temp_ck, next_chain_key, message_key, iv_body, iv_header);

    ESP_LOGI(TAG, "   chainKdf output (for Ns=%u):", msg_ns);
    printf("   message_key:     "); for(int i=0; i<32; i++) printf("%02x", message_key[i]); printf("\n");
    printf("   iv_body (16B):   "); for(int i=0; i<16; i++) printf("%02x", iv_body[i]); printf("\n");
    printf("   iv_header (16B): "); for(int i=0; i<16; i++) printf("%02x", iv_header[i]); printf("\n");
    printf("   next_chain_key:  "); for(int i=0; i<32; i++) printf("%02x", next_chain_key[i]); printf("\n");

    // SCHRITT 4: AES-256-GCM Body Decrypt
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   === SCHRITT 4: AES-GCM Body Decrypt ===");

    size_t aad_len = 112 + em_header_len;
    uint8_t *aad = malloc(aad_len);
    if (!aad) {
        ESP_LOGE(TAG, "   ❌ malloc AAD failed!");
        return -4;
    }
    memcpy(aad, ratchet_state.assoc_data, 112);
    memcpy(aad + 112, em_header_raw, em_header_len);

    ESP_LOGI(TAG, "   AAD: %zu bytes (112 rcAD + %zu emHeader)", aad_len, em_header_len);
    ESP_LOGI(TAG, "   rcAD[0-7]:     %02x%02x%02x%02x%02x%02x%02x%02x",
             aad[0], aad[1], aad[2], aad[3], aad[4], aad[5], aad[6], aad[7]);
    ESP_LOGI(TAG, "   emHeader[0-7]: %02x%02x%02x%02x%02x%02x%02x%02x",
             aad[112], aad[113], aad[114], aad[115], aad[116], aad[117], aad[118], aad[119]);

    int ret = aes_gcm_decrypt(message_key, iv_body, GCM_IV_LEN,
                               aad, aad_len,
                               em_body, em_body_len,
                               em_auth_tag, plaintext);
    free(aad);

    if (ret != 0) {
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "   ❌ AES-GCM Body Decrypt FAILED! (ret=%d)", ret);
        return -5;
    }

    // SCHRITT 5: unPad
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   === SCHRITT 5: unPad ===");

    uint16_t actual_len = (plaintext[0] << 8) | plaintext[1];
    ESP_LOGI(TAG, "   Padded size: %zu, actual content: %u, padding: %zu",
             em_body_len, actual_len, em_body_len - 2 - actual_len);

    if (actual_len > em_body_len - 2) {
        ESP_LOGE(TAG, "   ❌ unPad length invalid! %u > %zu", actual_len, em_body_len - 2);
        return -6;
    }

    memmove(plaintext, plaintext + 2, actual_len);
    *pt_len = actual_len;

    ESP_LOGI(TAG, "   Plaintext (%zu bytes):", *pt_len);
    printf("      ");
    for (size_t i = 0; i < 64 && i < *pt_len; i++) {
        printf("%02x ", plaintext[i]);
        if ((i + 1) % 16 == 0) printf("\n      ");
    }
    printf("\n");
    printf("      ASCII: ");
    for (size_t i = 0; i < 64 && i < *pt_len; i++) {
        char c = (char)plaintext[i];
        printf("%c", (c >= 32 && c < 127) ? c : '.');
    }
    printf("\n");

    if (*pt_len > 0) {
        ESP_LOGI(TAG, "   First byte: 0x%02X '%c'", plaintext[0],
                 (plaintext[0] >= 0x20 && plaintext[0] < 0x7f) ? (char)plaintext[0] : '?');
        if (plaintext[0] == 'D') {
            ESP_LOGI(TAG, "   ✅ Tag 'D' = AgentConnInfoReply — EXPECTED!");
        }
    }

    // ================================================================
    // SCHRITT 6: State Update — Mode-dependent
    // ADVANCE: Full DH ratchet state + HK←NHK Promotion
    // SAME: Only chain_key_recv + msg_num_recv
    // ================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   === SCHRITT 6: State Update (mode=%s) ===",
             mode == RATCHET_MODE_SAME ? "SameRatchet" : "AdvanceRatchet");

    if (mode == RATCHET_MODE_ADVANCE) {
        ESP_LOGI(TAG, "   Updating ratchet state per Signal DHRatchetHE():");

        // Log old values
        printf("   OLD root_key:      "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.root_key[i]); printf("...\n");
        printf("   OLD dh_self.pub:   "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.dh_self.public_key[i]); printf("...\n");
        printf("   OLD hk_recv:       "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.header_key_recv[i]); printf("...\n");
        printf("   OLD hk_send:       "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.header_key_send[i]); printf("...\n");
        printf("   OLD nhk_recv:      "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.next_header_key_recv[i]); printf("...\n");
        printf("   OLD nhk_send:      "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.next_header_key_send[i]); printf("...\n");

        // Apply state updates
        memcpy(ratchet_state.root_key, new_root_key_2, 32);
        memcpy(ratchet_state.chain_key_recv, next_chain_key, 32);
        memcpy(ratchet_state.chain_key_send, send_chain_key, 32);

        // HK←NHK Promotion per Signal spec
        memcpy(ratchet_state.header_key_send, ratchet_state.next_header_key_send, 32);
        memcpy(ratchet_state.header_key_recv, ratchet_state.next_header_key_recv, 32);
        memcpy(ratchet_state.next_header_key_send, new_nhk_send, 32);
        memcpy(ratchet_state.next_header_key_recv, new_nhk_recv, 32);

        // DH keys
        memcpy(ratchet_state.dh_self.private_key, new_dh_self.private_key, 56);
        memcpy(ratchet_state.dh_self.public_key, new_dh_self.public_key, 56);
        memcpy(ratchet_state.dh_peer, peer_new_pub, 56);

        // Counters
        ratchet_state.prev_chain_len = ratchet_state.msg_num_send;
        ratchet_state.msg_num_recv = msg_ns + 1;
        ratchet_state.msg_num_send = 0;

        // Log new values
        printf("   NEW root_key:      "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.root_key[i]); printf("...\n");
        printf("   NEW dh_self.pub:   "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.dh_self.public_key[i]); printf("...\n");
        printf("   NEW hk_recv:       "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.header_key_recv[i]); printf("...\n");
        printf("   NEW hk_send:       "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.header_key_send[i]); printf("...\n");
        printf("   NEW nhk_recv:      "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.next_header_key_recv[i]); printf("...\n");
        printf("   NEW nhk_send:      "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.next_header_key_send[i]); printf("...\n");
        printf("   NEW ck_recv:       "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.chain_key_recv[i]); printf("...\n");
        printf("   NEW ck_send:       "); for(int i=0; i<8; i++) printf("%02x", ratchet_state.chain_key_send[i]); printf("...\n");
        ESP_LOGI(TAG, "   ✅ State updated! msg_num_recv=%u, msg_num_send=%u, prev_chain_len=%u",
                 ratchet_state.msg_num_recv, ratchet_state.msg_num_send, ratchet_state.prev_chain_len);
    } else {
        // SAME: Only update chain key and recv counter
        memcpy(ratchet_state.chain_key_recv, next_chain_key, 32);
        ratchet_state.msg_num_recv = msg_ns + 1;

        ESP_LOGI(TAG, "   ✅ SAME state: msg_num_recv=%u, ck_recv=%02x%02x%02x%02x%02x%02x%02x%02x...",
                 ratchet_state.msg_num_recv,
                 ratchet_state.chain_key_recv[0], ratchet_state.chain_key_recv[1],
                 ratchet_state.chain_key_recv[2], ratchet_state.chain_key_recv[3],
                 ratchet_state.chain_key_recv[4], ratchet_state.chain_key_recv[5],
                 ratchet_state.chain_key_recv[6], ratchet_state.chain_key_recv[7]);
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  🎉 PHASE 2b BODY DECRYPT SUCCESS!                    ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════╝");

    return 0;
}

// ============== Getters ==============

ratchet_state_t *ratchet_get_state(void) { return &ratchet_state; }
bool ratchet_is_initialized(void) { return ratchet_state.initialized; }

const uint8_t *ratchet_get_saved_hk(void) { return saved_x3dh_valid ? saved_x3dh_hk : NULL; }
const uint8_t *ratchet_get_saved_nhk(void) { return saved_x3dh_valid ? saved_x3dh_nhk : NULL; }