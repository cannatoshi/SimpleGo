/**
 * @file smp_msg_router.c
 * @brief Central message router for the App Task — Phase 3
 *
 * Contains the frame processing logic previously in main.c's
 * blocking receive loop. Handles transport parsing, decrypt
 * pipeline dispatch, and ACK building.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#include "smp_msg_router.h"
#include "smp_tasks.h"
#include "smp_events.h"
#include "smp_frame_pool.h"
#include "smp_network.h"
#include "smp_contacts.h"
#include "smp_queue.h"
#include "smp_peer.h"
#include "smp_ack.h"
#include "smp_e2e.h"
#include "smp_agent.h"
#include "smp_handshake.h"
#include "smp_parser.h"
#include "smp_crypto.h"
#include "simplex_crypto.h"
#include "smp_types.h"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"

#include "sodium.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "smp_msg_router";

/* Post-Confirmation state (42d flow) */
static uint8_t peer_sender_auth_key[44];
static bool has_peer_sender_auth = false;

extern bool peer_send_hello(contact_t *contact);

/* ========================================================================
 * Helper: Send ACK via Ring Buffer to Network Task
 * ======================================================================== */

static bool send_ack_via_ringbuf(const uint8_t *recipient_id, int recipient_id_len,
                                  const uint8_t *msg_id, int msg_id_len,
                                  const uint8_t *rcv_auth_secret)
{
    uint8_t ack_buf[192];
    int ack_len = smp_build_ack(ack_buf, sizeof(ack_buf),
                                 net_session_id,
                                 recipient_id, recipient_id_len,
                                 msg_id, msg_id_len,
                                 rcv_auth_secret);
    if (ack_len < 0) {
        ESP_LOGE(TAG, "Failed to build ACK");
        return false;
    }

    if (xRingbufferSend(app_to_net_rb, ack_buf, ack_len,
                        pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send ACK to ring buffer!");
        return false;
    }

    ESP_LOGD(TAG, "ACK queued (%d bytes)", ack_len);
    return true;
}

/* ========================================================================
 * msg_router_init / msg_router_run
 * ======================================================================== */

void msg_router_init(void)
{
    has_peer_sender_auth = false;
    ESP_LOGI(TAG, "Message Router initialized");
}

void msg_router_run(void)
{
    /* Not used in Phase 3 — app_task calls msg_router_process_frame directly */
    ESP_LOGI(TAG, "Message Router event loop (unused in Phase 3)");
}

/* ========================================================================
 * msg_router_process_frame() — Main frame processing
 *
 * Migrated from main.c lines 301-607.
 * frame_data points to heap copy including 2-byte header.
 * ======================================================================== */

void msg_router_process_frame(uint8_t *frame_data, int content_len)
{
    if (!frame_data || content_len <= 0) return;

    uint8_t *resp = frame_data + 2;  /* Skip 2-byte length header */

    /* Debug: log first 64 bytes */
    ESP_LOGW(TAG, "Frame received, content_len=%d", content_len);
    if (content_len > 0) {
        ESP_LOGW(TAG, "First 64 bytes:");
        for (int i = 0; i < 64 && i < content_len + 2; i += 16) {
            char hex[64] = {0}; int hx = 0;
            for (int j = 0; j < 16 && (i+j) < content_len + 2; j++)
                hx += sprintf(&hex[hx], "%02x ", frame_data[i+j]);
            ESP_LOGW(TAG, "  +%04d: %s", i, hex);
        }
    }

    /* ===== Parse transport format ===== */
    int p = 0;
    uint8_t tx_count = resp[p];
    ESP_LOGD(TAG, "   txCount: %d", tx_count);
    p++;
    p += 2;  /* skip 2 bytes */

    int authLen = resp[p++]; p += authLen;
    int sessLen = resp[p++]; p += sessLen;
    int corrLen = resp[p++]; p += corrLen;

    int entLen = resp[p++];
    uint8_t entity_id[24];
    if (entLen > 24) entLen = 24;
    memcpy(entity_id, &resp[p], entLen);
    p += entLen;

    /* ===== Find contact / check reply queue ===== */
    int contact_idx = find_contact_by_recipient_id(entity_id, entLen);
    contact_t *contact = (contact_idx >= 0) ? &contacts_db.contacts[contact_idx] : NULL;

    bool is_reply_queue = (our_queue.rcv_id_len > 0 &&
                           entLen == our_queue.rcv_id_len &&
                           memcmp(entity_id, our_queue.rcv_id, entLen) == 0);
    if (is_reply_queue) {
        ESP_LOGI(TAG, "   Message on REPLY QUEUE from peer!");
    }

    /* Debug: command identification */
    ESP_LOGW(TAG, "entity=%02x%02x%02x%02x, contact=%s, reply_q=%d, cmd=%c%c%c",
             entity_id[0], entity_id[1], entity_id[2], entity_id[3],
             contact ? contact->name : "NULL", is_reply_queue,
             (p < content_len) ? resp[p] : '?',
             (p+1 < content_len) ? resp[p+1] : '?',
             (p+2 < content_len) ? resp[p+2] : '?');

    /* ===== Handle OK ===== */
    if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
        ESP_LOGI(TAG, "   OK");
        return;
    }

    /* ===== Handle END ===== */
    if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'N' && resp[p+2] == 'D') {
        if (contact) {
            ESP_LOGI(TAG, "   END [%s] - No more messages", contact->name);
        } else {
            ESP_LOGI(TAG, "   END - No more messages");
        }
        return;
    }

    /* ===== Handle ERR ===== */
    if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
        ESP_LOGE(TAG, "   ERR: %.*s",
                 (content_len - p > 20) ? 20 : content_len - p, &resp[p]);
        return;
    }

    /* ===== Handle MSG ===== */
    if (p + 3 < content_len && resp[p] == 'M' && resp[p+1] == 'S' && resp[p+2] == 'G' && resp[p+3] == ' ') {
        p += 4;

        uint8_t msgIdLen = resp[p++];
        uint8_t msg_id[24];
        memset(msg_id, 0, 24);
        if (msgIdLen > 24) msgIdLen = 24;
        memcpy(msg_id, &resp[p], msgIdLen);
        p += msgIdLen;

        int enc_len = content_len - p;

        if (contact) {
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "+----------------------------------------------------------+");
            ESP_LOGI(TAG, "|   MESSAGE RECEIVED for [%s]!", contact->name);
            ESP_LOGI(TAG, "+----------------------------------------------------------+");
        } else if (is_reply_queue) {
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "+----------------------------------------------------------+");
            ESP_LOGI(TAG, "|   MESSAGE on REPLY QUEUE!                                |");
            ESP_LOGI(TAG, "+----------------------------------------------------------+");
        } else {
            ESP_LOGI(TAG, "   MESSAGE (unknown contact)!");
        }
        ESP_LOGD(TAG, "   MsgId: %02x%02x%02x%02x...", msg_id[0], msg_id[1], msg_id[2], msg_id[3]);
        ESP_LOGD(TAG, "   Encrypted: %d bytes", enc_len);

        /* ==============================================================
         * REPLY QUEUE: E2E Decrypt + Agent Process
         * ============================================================== */
        if (is_reply_queue && our_queue.valid && enc_len > crypto_box_MACBYTES) {
            ESP_LOGI(TAG, "   Decrypting REPLY QUEUE message...");

            uint8_t *e2e_plain = NULL;
            size_t e2e_plain_len = 0;

            int e2e_ret = smp_e2e_decrypt_reply_message(
                &resp[p], enc_len, msg_id, msgIdLen,
                &e2e_plain, &e2e_plain_len);

            if (e2e_ret == 0 && e2e_plain) {
                smp_agent_process_message(e2e_plain, e2e_plain_len,
                       &contacts_db.contacts[0],
                       peer_sender_auth_key, &has_peer_sender_auth);
                free(e2e_plain);
            }

            /* === Post-Confirmation: KEY + HELLO + Read Reply (42d) === */
            if (has_peer_sender_auth) {
                ESP_LOGI(TAG, "   Reconnecting to Reply Queue for KEY...");

                if (!queue_reconnect()) {
                    ESP_LOGE(TAG, "   Reconnect failed!");
                    goto skip_42d;
                }
                if (!queue_subscribe()) {
                    ESP_LOGE(TAG, "   SUB failed!");
                    goto skip_42d;
                }
                if (!queue_send_key(peer_sender_auth_key, 44)) {
                    ESP_LOGE(TAG, "   KEY failed!");
                    goto skip_42d;
                }
                ESP_LOGI(TAG, "   KEY accepted!");

                /* Send HELLO on Contact Queue Q_A */
                ESP_LOGI(TAG, "   Sending HELLO on Contact Queue Q_A...");
                {
                    contact_t *hello_contact = &contacts_db.contacts[0];
                    if (peer_send_hello(hello_contact)) {
                        ESP_LOGI(TAG, "   HELLO sent!");
                    } else {
                        ESP_LOGE(TAG, "   HELLO send failed!");
                    }
                }

                /* Read + Decrypt Reply Queue response */
                ESP_LOGI(TAG, "   Reading Reply Queue message...");
                {
                    uint8_t *rq_block = heap_caps_malloc(18000, MALLOC_CAP_8BIT);
                    if (!rq_block) goto skip_42d;

                    for (int rq_try = 0; rq_try < 3; rq_try++) {
                        int rq_len = queue_read_raw(rq_block, 18000, 15000);
                        if (rq_len < 0) break;

                        uint8_t *rq_resp = rq_block + 2;

                        /* Parse SMP transport */
                        int rp = 0;
                        rp++;  /* txCount */
                        rp += 2;
                        int rq_authLen = rq_resp[rp++]; rp += rq_authLen;
                        int rq_sessLen = rq_resp[rp++]; rp += rq_sessLen;
                        int rq_corrLen = rq_resp[rp++]; rp += rq_corrLen;
                        int rq_entLen  = rq_resp[rp++]; rp += rq_entLen;

                        /* Skip OK / END */
                        if (rp + 1 < rq_len && rq_resp[rp] == 'O' && rq_resp[rp+1] == 'K') continue;
                        if (rp + 2 < rq_len && rq_resp[rp] == 'E' && rq_resp[rp+1] == 'N' && rq_resp[rp+2] == 'D') continue;
                        if (!(rp + 3 < rq_len && rq_resp[rp] == 'M' && rq_resp[rp+1] == 'S' && rq_resp[rp+2] == 'G')) continue;

                        /* MSG on Reply Queue */
                        rp += 4;
                        uint8_t rq_msgIdLen = rq_resp[rp++];
                        uint8_t rq_msg_id[24] = {0};
                        if (rq_msgIdLen > 24) rq_msgIdLen = 24;
                        memcpy(rq_msg_id, &rq_resp[rp], rq_msgIdLen);
                        rp += rq_msgIdLen;

                        int rq_enc_len = rq_len - rp;
                        ESP_LOGI(TAG, "   Reply Queue MSG received! (%d bytes)", rq_enc_len);

                        uint8_t *rq_plain = NULL;
                        size_t rq_plain_len = 0;

                        if (smp_e2e_decrypt_reply_message(&rq_resp[rp], rq_enc_len,
                                rq_msg_id, rq_msgIdLen, &rq_plain, &rq_plain_len) == 0 && rq_plain) {
                            uint8_t dummy_key[44];
                            bool dummy_auth = false;
                            smp_agent_process_message(rq_plain, rq_plain_len,
                                                        &contacts_db.contacts[0],
                                                        dummy_key, &dummy_auth);
                            free(rq_plain);
                        }

                        queue_send_ack(rq_msg_id, rq_msgIdLen);
                        break;
                    }
                    free(rq_block);
                }

                /* Send first chat message */
                ESP_LOGI(TAG, "   Sending first chat message in 3 seconds...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                {
                    contact_t *msg_contact = &contacts_db.contacts[0];
                    if (peer_send_chat_message(msg_contact, "Hello from ESP32!")) {
                        ESP_LOGI(TAG, "   ✅ Chat message sent!");
                    } else {
                        ESP_LOGE(TAG, "   ❌ Chat message send failed!");
                    }
                }

                ESP_LOGI(TAG, "   Returning to message processing...");
                has_peer_sender_auth = false;

                skip_42d: ;
                /* Request Network Task to re-subscribe (was: subscribe_all_contacts(&ssl...)) */
                need_resubscribe = true;
                ESP_LOGW(TAG, "Requested re-subscribe after 42d handshake");
            }

            /* ACK Reply Queue message via ring buffer */
            send_ack_via_ringbuf(our_queue.rcv_id, our_queue.rcv_id_len,
                                  msg_id, msgIdLen,
                                  our_queue.rcv_auth_private);
        }

        /* ==============================================================
         * CONTACT QUEUE: SMP Decrypt + Agent Parse
         * ============================================================== */
        if (contact && contact->have_srv_dh && enc_len > crypto_box_MACBYTES) {
            ESP_LOGW(TAG, "Contact Queue decrypt attempt! contact=%s, enc_len=%d",
                     contact->name, enc_len);
            uint8_t *plain = malloc(enc_len);
            if (plain) {
                int plain_len = 0;
                if (decrypt_smp_message(contact, &resp[p], enc_len, msg_id, msgIdLen,
                                         plain, &plain_len)) {
                    ESP_LOGI(TAG, "   SMP-Level Decryption OK! (%d bytes)", plain_len);

                    /* Extract e2ePubKey (for reference only, NOT cached for reply queue) */
                    if (contact && plain_len > 60) {
                        const uint8_t x25519_spki[] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03,
                                                       0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00};

                        if (memcmp(&plain[14], x25519_spki, 12) == 0) {
                            ESP_LOGI(TAG, "   Contact Queue E2E key at offset 14 (NOT caching)");
                            ESP_LOGI(TAG, "        Key: %02x%02x%02x%02x...",
                                     plain[26], plain[27], plain[28], plain[29]);
                        } else {
                            for (int i = 0; i < 100 && i < plain_len - 44; i++) {
                                if (memcmp(&plain[i], x25519_spki, 12) == 0) {
                                    ESP_LOGI(TAG, "   Contact Queue E2E key at offset %d (NOT caching)", i);
                                    break;
                                }
                            }
                        }
                    }

                    parse_agent_message(contact, plain, plain_len);

                    /* ACK via ring buffer */
                    send_ack_via_ringbuf(contact->recipient_id, contact->recipient_id_len,
                                          msg_id, msgIdLen,
                                          contact->rcv_auth_secret);
                } else {
                    ESP_LOGE(TAG, "   Decryption failed!");
                }
                free(plain);
            }
        } else if (!is_reply_queue) {
            ESP_LOGW(TAG, "      Cannot decrypt - no contact keys");
        }
        ESP_LOGI(TAG, "");
        return;
    }

    /* ===== Unknown command ===== */
    ESP_LOGW(TAG, "   Unknown: %c%c%c",
             (p < content_len) ? resp[p] : '?',
             (p+1 < content_len) ? resp[p+1] : '?',
             (p+2 < content_len) ? resp[p+2] : '?');
}
