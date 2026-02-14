/**
 * SimpleGo - Reply Queue E2E Decryption
 * Extracted from main.c (Auftrag 46c)
 *
 * Consolidates the Reply Queue decrypt pipeline that existed twice:
 * 1. Main message loop Reply Queue handler
 * 2. 42d secondary Reply Queue read after KEY/HELLO
 */

#include "smp_e2e.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "sodium.h"
#include "smp_queue.h"        // our_queue, queue_save_credentials
#include "smp_types.h"        // reply_queue_e2e_peer_public/valid
#include "simplex_crypto.h"   // simplex_secretbox_open_debug

// Declared in simplex_crypto.c but not in header
extern int decrypt_client_msg(const uint8_t *data, int data_len,
                               const uint8_t *sender_pub, const uint8_t *rcv_priv,
                               uint8_t *out);

static const char *TAG = "SMP_E2E";

// X25519 SPKI header (12 bytes before 32-byte raw key)
static const uint8_t X25519_SPKI_HDR[] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03,
    0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00
};

/**
 * Parse ClientMsgEnvelope and extract sender's E2E public key.
 * Handles all corrId/e2e combinations and SPKI fallback search.
 *
 * @param envelope      Envelope data (after length prefix)
 * @param envelope_len  Envelope length
 * @param sender_pub    OUT: 32-byte sender public key
 * @param offset        IN/OUT: parse offset (starts at 12, advances past key)
 * @return true if key was extracted
 */
static bool extract_sender_key(const uint8_t *envelope, size_t envelope_len,
                                uint8_t *sender_pub, int *offset)
{
    int off = *offset;

    uint8_t maybe_corrId = envelope[off++];
    uint8_t maybe_e2e = envelope[off++];

    ESP_LOGD(TAG, "maybe_corrId=0x%02x, maybe_e2e=0x%02x", maybe_corrId, maybe_e2e);

    if (maybe_corrId == '1' && (maybe_e2e == ',' || maybe_e2e == '0')) {
        // corrId SPKI doubles as E2E key
        if (memcmp(&envelope[off], X25519_SPKI_HDR, 12) == 0) {
            memcpy(sender_pub, &envelope[off + 12], 32);
            // 47f: Cache Q_B E2E key for future messages
            memcpy(reply_queue_e2e_peer_public, sender_pub, 32);
            reply_queue_e2e_peer_valid = true;
            ESP_LOGW(TAG, "47f: Updated reply_queue_e2e_peer_public from Q_B corrId SPKI!");
            ESP_LOGW(TAG, "     New key: %02x%02x%02x%02x %02x%02x%02x%02x",
                     sender_pub[0], sender_pub[1], sender_pub[2], sender_pub[3],
                     sender_pub[4], sender_pub[5], sender_pub[6], sender_pub[7]);
            off += 44;
            *offset = off;
            return true;
        }
        ESP_LOGE(TAG, "SPKI header mismatch at offset %d", off);
        off += 44;

    } else if (maybe_corrId == '1' && maybe_e2e == '1') {
        // Separate E2E key after corrId SPKI
        off += 44;  // skip corrId SPKI
        uint8_t e2e_len = envelope[off++];
        if (e2e_len == 44 && memcmp(&envelope[off], X25519_SPKI_HDR, 12) == 0) {
            memcpy(sender_pub, &envelope[off + 12], 32);
            off += 44;
            *offset = off;
            // 47f: Cache Q_B E2E key for future messages with corrId='0'/','
            memcpy(reply_queue_e2e_peer_public, sender_pub, 32);
            reply_queue_e2e_peer_valid = true;
            ESP_LOGW(TAG, "47f: Updated reply_queue_e2e_peer_public from Q_B inline key!");
            ESP_LOGW(TAG, "     New key: %02x%02x%02x%02x %02x%02x%02x%02x",
                     sender_pub[0], sender_pub[1], sender_pub[2], sender_pub[3],
                     sender_pub[4], sender_pub[5], sender_pub[6], sender_pub[7]);
            return true;
        }
        off += 44;

    } else if (maybe_corrId == ',' || maybe_corrId == '0') {
        // 48a: No inline key → cached peer key, nonce right after corrId byte
        // Format: [12B header][corrId byte][24B nonce][ciphertext+MAC]
        // maybe_e2e was actually nonce[0], so back up 1 byte
        if (reply_queue_e2e_peer_valid) {
            memcpy(sender_pub, reply_queue_e2e_peer_public, 32);
            *offset = off - 1;  // Nonce starts at byte 13, not 14
            ESP_LOGD(TAG, "E2E: corrId='%c', nonce_offset=%d, using cached key", maybe_corrId, off - 1);
            return true;
        }
        ESP_LOGE(TAG, "No pre-shared E2E key available");

    } else {
        // Unknown format → search for SPKI at nearby offsets
        ESP_LOGW(TAG, "Unknown corrId/e2e format, searching for SPKI...");
        for (int try_off = 10; try_off <= 20; try_off++) {
            if ((size_t)(try_off + 44) <= envelope_len &&
                memcmp(&envelope[try_off], X25519_SPKI_HDR, 12) == 0) {
                memcpy(sender_pub, &envelope[try_off + 12], 32);
                // 47f: Cache this discovered key too
                memcpy(reply_queue_e2e_peer_public, sender_pub, 32);
                reply_queue_e2e_peer_valid = true;
                ESP_LOGW(TAG, "47f: Updated reply_queue_e2e_peer_public from fallback SPKI search!");
                *offset = try_off + 44;
                return true;
            }
        }
    }

    *offset = off;
    return false;
}

/**
 * Try multiple E2E decrypt methods against the ciphertext.
 * Methods: decrypt_client_msg, crypto_box, crypto_secretbox, simplex_secretbox
 *
 * @return 0 on success (e2e_plain filled), non-zero on failure
 */
static int try_e2e_decrypt(const uint8_t *envelope, size_t envelope_len,
                            int nonce_offset, int cipher_offset,
                            const uint8_t *sender_pub,
                            uint8_t *e2e_plain, size_t cipher_len)
{
    const uint8_t *cm_nonce = &envelope[nonce_offset];
    const uint8_t *ciphertext = &envelope[cipher_offset];

    // DH secret for methods 2-3
    uint8_t dh_secret[32];
    if (crypto_scalarmult(dh_secret, our_queue.e2e_private, sender_pub) != 0) {
        ESP_LOGE(TAG, "DH computation failed");
        return -1;
    }

    int ret = -1;

    // Method 0: decrypt_client_msg (Contact Queue style)
    ESP_LOGD(TAG, "Trying decrypt_client_msg...");
    {
        int block_len = envelope_len - nonce_offset;
        uint8_t *m0 = malloc(block_len);
        if (m0) {
            int dec = decrypt_client_msg(&envelope[nonce_offset], block_len,
                                          sender_pub, our_queue.e2e_private, m0);
            if (dec > 0) {
                memcpy(e2e_plain, m0, dec < (int)cipher_len ? dec : (int)cipher_len);
                ret = 0;
            }
            free(m0);
        }
    }

    // Method 1: crypto_box_open_easy
    if (ret != 0) {
        ESP_LOGD(TAG, "Trying crypto_box_open_easy...");
        ret = crypto_box_open_easy(e2e_plain, ciphertext, cipher_len,
                                    cm_nonce, sender_pub, our_queue.e2e_private);
    }

    // Method 2: crypto_secretbox_open_easy with DH secret
    if (ret != 0) {
        ESP_LOGD(TAG, "Trying crypto_secretbox_open_easy...");
        ret = crypto_secretbox_open_easy(e2e_plain, ciphertext, cipher_len,
                                          cm_nonce, dh_secret);
    }

    // Method 3: Custom simplex_secretbox
    if (ret != 0) {
        ESP_LOGD(TAG, "Trying simplex_secretbox_open_debug...");
        ret = simplex_secretbox_open_debug(e2e_plain, ciphertext, cipher_len,
                                            cm_nonce, dh_secret, "REPLY_E2E");
    }

    sodium_memzero(dh_secret, 32);
    return ret;
}

int smp_e2e_decrypt_reply_message(
    const uint8_t *encrypted, int encrypted_len,
    const uint8_t *msg_id, int msg_id_len,
    uint8_t **out_plain, size_t *out_plain_len)
{
    *out_plain = NULL;
    *out_plain_len = 0;

    // === Layer 1: Server-level decrypt ===
    uint8_t server_nonce[24];
    memset(server_nonce, 0, 24);
    memcpy(server_nonce, msg_id, msg_id_len > 24 ? 24 : msg_id_len);

    uint8_t *server_plain = malloc(encrypted_len);
    if (!server_plain) return -1;

    if (crypto_box_open_easy_afternm(server_plain, encrypted, encrypted_len,
                                      server_nonce, our_queue.shared_secret) != 0) {
        ESP_LOGE(TAG, "Server-level decrypt FAILED");
        free(server_plain);
        return -2;
    }
    int plain_len = encrypted_len - crypto_box_MACBYTES;
    ESP_LOGI(TAG, "Server decrypt OK (%d bytes)", plain_len);

    // 48a: Server decrypt diagnostic (DEBUG level)
    ESP_LOGD(TAG, "47e: enc_len=%d, msg_id_len=%d, plain_len=%d", encrypted_len, msg_id_len, plain_len);
    ESP_LOGD(TAG, "47e: shared_secret: %02x%02x%02x%02x %02x%02x%02x%02x",
             our_queue.shared_secret[0], our_queue.shared_secret[1],
             our_queue.shared_secret[2], our_queue.shared_secret[3],
             our_queue.shared_secret[4], our_queue.shared_secret[5],
             our_queue.shared_secret[6], our_queue.shared_secret[7]);

    // === Length prefix handling ===
    uint16_t raw_len_prefix = (server_plain[0] << 8) | server_plain[1];
    int prefix_len = 0;
    if (plain_len > 2 && (server_plain[0] != 0x00 || server_plain[1] != 0x00)) {
        prefix_len = 2;
    }

    const uint8_t *envelope = server_plain + prefix_len;
    size_t envelope_len = raw_len_prefix;

    // 48a: Envelope diagnostic (DEBUG level)
    ESP_LOGD(TAG, "47e: len_prefix=%u, prefix_len=%d, envelope_len=%zu", raw_len_prefix, prefix_len, envelope_len);
    ESP_LOGD(TAG, "47e: envelope first 32 bytes:");
    {
        char hex[128] = {0}; int hx = 0;
        for (int i = 0; i < 32 && i < (int)envelope_len; i++)
            hx += sprintf(&hex[hx], "%02x ", envelope[i]);
        ESP_LOGD(TAG, "  %s", hex);
    }

    // === Layer 2: E2E envelope parse + decrypt ===
    int offset = 12;
    uint8_t sender_pub[32];

    if (!extract_sender_key(envelope, envelope_len, sender_pub, &offset)) {
        ESP_LOGE(TAG, "No sender key found");
        free(server_plain);
        return -3;
    }

    // Auftrag 50b: Persist E2E key after extraction so it survives reboot
    if (reply_queue_e2e_peer_valid) {
        queue_save_credentials();
    }

    // 48a: Compact E2E key diagnostic
    ESP_LOGD(TAG, "E2E keys: sender=%02x%02x%02x%02x, our=%02x%02x%02x%02x",
             sender_pub[0], sender_pub[1], sender_pub[2], sender_pub[3],
             our_queue.e2e_public[0], our_queue.e2e_public[1],
             our_queue.e2e_public[2], our_queue.e2e_public[3]);

    // Check bounds for nonce + MAC
    if (envelope_len < (size_t)offset + 24 + 16) {
        ESP_LOGE(TAG, "Message too short for E2E decrypt");
        free(server_plain);
        return -4;
    }

    ESP_LOGD(TAG, "cmNonce at offset %d, cipher at %d, cipher_len=%zu",
             offset, offset + 24, envelope_len - offset - 24);

    int nonce_offset = offset;
    int cipher_offset = offset + 24;
    size_t cipher_len = envelope_len - cipher_offset;

    uint8_t *e2e_plain = malloc(cipher_len);
    if (!e2e_plain) {
        free(server_plain);
        return -1;
    }

    int ret = try_e2e_decrypt(envelope, envelope_len, nonce_offset, cipher_offset,
                               sender_pub, e2e_plain, cipher_len);

    if (ret != 0) {
        ESP_LOGE(TAG, "E2E decrypt FAILED (all methods)");
        free(e2e_plain);
        free(server_plain);
        return -5;
    }

    size_t result_len = cipher_len - 16;  // Remove Poly1305 MAC
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      +----------------------------------------------+");
    ESP_LOGI(TAG, "      |  🎉 E2E LAYER 2 DECRYPT SUCCESS!            |");
    ESP_LOGI(TAG, "      +----------------------------------------------+");
    ESP_LOGI(TAG, "      Decrypted %zu bytes! (nonce_offset=%d)", result_len, nonce_offset);

    *out_plain = e2e_plain;
    *out_plain_len = result_len;

    free(server_plain);
    return 0;
}
