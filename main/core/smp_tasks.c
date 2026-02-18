/**
 * SimpleGo - smp_tasks.c
 * FreeRTOS task management for multi-task architecture
 *
 * All heavy allocations use PSRAM (SPIRAM) to preserve internal
 * SRAM for TLS/WiFi operations (~40KB needed).
 */

#include "smp_tasks.h"
#include "smp_frame_pool.h"
#include "smp_events.h"
#include "smp_network.h"   // Phase 3: smp_read_block()
#include "smp_types.h"     // Phase 3: SMP_BLOCK_SIZE
#include "smp_contacts.h"  // Phase 3 T2: find_contact_by_recipient_id, contacts_db
#include "smp_queue.h"     // Phase 3 T2: our_queue; T4e: queue_reconnect/subscribe/send_key/read_raw/send_ack
#include "smp_e2e.h"       // Phase 3 T3: smp_e2e_decrypt_reply_message()
#include "smp_agent.h"     // Phase 3 T3: smp_agent_process_message()
#include "smp_crypto.h"    // Phase 3 T3: decrypt_smp_message()
#include "smp_parser.h"    // Phase 3 T3: parse_agent_message()
#include "sodium.h"        // Phase 3 T3: crypto_box_MACBYTES
#include "smp_ack.h"       // Phase 3 T4c: smp_send_ack()
#include "smp_peer.h"      // Phase 3 T4e: peer_send_chat_message()
#include <string.h>
#include <sys/socket.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

// peer_send_hello not in any header (declared extern in main.c too)
extern bool peer_send_hello(contact_t *contact);

static const char *TAG = "SMP_TASKS";
static const char *TAG_APP = "SMP_APP";  // Phase 3 T2: App Task logging

// Task handles
TaskHandle_t network_task_handle = NULL;
TaskHandle_t ui_task_handle = NULL;

// Ring buffer handles
RingbufHandle_t net_to_app_buf = NULL;
RingbufHandle_t app_to_net_buf = NULL;

// Stored SSL context (set by smp_tasks_start, used by network task later)
static mbedtls_ssl_context *s_ssl = NULL;
static int s_sock_fd = -1;  // T6-Fix: Socket FD for timeout control
static uint8_t s_session_id[32];  // Phase 3 T4b: session ID for ACK signing

// Phase 3 T3: Post-confirmation state (set by Reply Queue decrypt, read by 42d handler later)
static uint8_t s_peer_sender_auth_key[44];
static bool s_has_peer_sender_auth = false;

// Helper: log both internal and PSRAM heap
static void log_heap(const char *label)
{
    ESP_LOGI(TAG, "  [%s] Internal: %lu, PSRAM: %lu",
             label,
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// --- Task functions ---
// Network task: Phase 3 SSL read loop
// App/UI tasks: empty loops (Phase 2, will be filled in later phases)

static void network_task(void *arg)
{
    ESP_LOGI(TAG, "Network task running on core %d", xPortGetCoreID());
    log_heap("net_task");

    // Phase 3 T1: Allocate block buffer in PSRAM (preserve internal SRAM for TLS/WiFi)
    uint8_t *block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
    if (!block) {
        ESP_LOGE(TAG, "Network task: Failed to allocate read buffer in PSRAM!");
        vTaskDelete(NULL);
        return;
    }
    log_heap("net_read_buf");

    ESP_LOGI(TAG, "Network task: SSL read loop starting...");

    // T6-Fix: Reduce socket timeout for responsive read loop
    {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(s_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ESP_LOGI(TAG, "Network task: socket timeout set to 1s");
    }

    while (1) {
        // T6-Debug: Heartbeat counter
        static int loop_count = 0;
        loop_count++;
        if (loop_count % 30 == 0) {
            ESP_LOGI(TAG, "NET: heartbeat #%d", loop_count / 30);
        }

        // === 1. SSL READ (T1, timeout reduced from 5000 to 1000 for T4c) ===
        int content_len = smp_read_block(s_ssl, block, 1000);

        if (content_len > 0) {
            ESP_LOGI(TAG, "Network task: Frame received, content_len=%d, writing to ring buffer",
                     content_len);

            BaseType_t sent = xRingbufferSend(net_to_app_buf, block,
                                               content_len + 2,
                                               pdMS_TO_TICKS(1000));
            if (sent != pdTRUE) {
                ESP_LOGW(TAG, "Network task: Ring buffer full, frame dropped!");
            }
        } else if (content_len == -2) {
            // Timeout — log every 30th
            if (loop_count % 30 == 0) {
                ESP_LOGD(TAG, "NET: timeout (loop %d)", loop_count);
            }
        } else {
            ESP_LOGE(TAG, "Network task: SSL read error %d", content_len);
            break;
        }

        // === 2. COMMAND CHANNEL: Process App Task commands (T4c) ===
        size_t cmd_size = 0;
        void *cmd_item = xRingbufferReceive(app_to_net_buf, &cmd_size, 0);  // Non-blocking!

        while (cmd_item) {
            if (cmd_size == sizeof(net_cmd_t)) {
                net_cmd_t *cmd = (net_cmd_t *)cmd_item;

                switch (cmd->cmd) {
                    case NET_CMD_SEND_ACK:
                        ESP_LOGI(TAG, "NET: Executing ACK command");
                        smp_send_ack(s_ssl, block, s_session_id,
                                     cmd->recipient_id, cmd->recipient_id_len,
                                     cmd->msg_id, cmd->msg_id_len,
                                     cmd->rcv_auth_secret);
                        break;

                    case NET_CMD_SUBSCRIBE_ALL:
                        ESP_LOGI(TAG, "NET: Executing SUBSCRIBE_ALL command");
                        subscribe_all_contacts(s_ssl, block, s_session_id);
                        break;
                }
            } else {
                ESP_LOGW(TAG, "NET: Invalid command size %d (expected %d)",
                         (int)cmd_size, (int)sizeof(net_cmd_t));
            }
            vRingbufferReturnItem(app_to_net_buf, cmd_item);

            // Check for more commands
            cmd_item = xRingbufferReceive(app_to_net_buf, &cmd_size, 0);
        }
    }

    ESP_LOGW(TAG, "Network task: SSL read loop ended, cleaning up");
    heap_caps_free(block);
    vTaskDelete(NULL);
}

// === Phase 3 T4d: App Task command helpers ===

// Helper: Send ACK via Ring Buffer to Network Task
static void app_send_ack(const uint8_t *recipient_id, int recipient_id_len,
                         const uint8_t *msg_id, int msg_id_len,
                         const uint8_t *rcv_auth_secret)
{
    net_cmd_t cmd = {0};
    cmd.cmd = NET_CMD_SEND_ACK;
    memcpy(cmd.recipient_id, recipient_id, recipient_id_len);
    cmd.recipient_id_len = recipient_id_len;
    memcpy(cmd.msg_id, msg_id, msg_id_len);
    cmd.msg_id_len = msg_id_len;
    memcpy(cmd.rcv_auth_secret, rcv_auth_secret, 64);

    if (xRingbufferSend(app_to_net_buf, &cmd, sizeof(cmd), pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG_APP, "APP: Failed to send ACK command to Network Task!");
    } else {
        ESP_LOGI(TAG_APP, "APP: ACK command queued for Network Task");
    }
}

// Helper: Request re-subscribe via Ring Buffer
static void app_request_subscribe_all(void)
{
    net_cmd_t cmd = {0};
    cmd.cmd = NET_CMD_SUBSCRIBE_ALL;

    if (xRingbufferSend(app_to_net_buf, &cmd, sizeof(cmd), pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG_APP, "APP: Failed to send SUBSCRIBE command!");
    } else {
        ESP_LOGI(TAG_APP, "APP: SUBSCRIBE_ALL command queued");
    }
}

// Phase 3 T4: App logic runs on Main Task (64KB Internal SRAM stack)
// Required because NVS writes crash with PSRAM stack (SPI Flash disables cache)
void smp_app_run(QueueHandle_t kbd_queue)
{
    ESP_LOGI(TAG_APP, "App logic running on main task, core %d", xPortGetCoreID());
    log_heap("app_run");

    // Phase 3 T2: Allocate local parse buffer in PSRAM (once, not per iteration)
    uint8_t *local_block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE + 2, MALLOC_CAP_SPIRAM);
    if (!local_block) {
        ESP_LOGE(TAG_APP, "Failed to allocate parse buffer in PSRAM!");
        return;
    }
    log_heap("app_parse_buf");

    ESP_LOGI(TAG_APP, "App logic: parse loop starting...");

    // T6-Fix2: Re-subscribe after task handover to ensure server delivers on this connection
    ESP_LOGI(TAG_APP, "APP: Sending initial re-subscribe...");
    app_request_subscribe_all();
    // T6-Fix5: Send wildcard ACK to clear any stuck delivery state
    // Per SMP spec: empty msgId resets delivered = Nothing on server
    if (contacts_db.num_contacts > 0) {
        contact_t *c = &contacts_db.contacts[0];
        ESP_LOGI(TAG_APP, "APP: Sending wildcard ACK for [%s] to clear delivery state", c->name);
        uint8_t empty_msg_id[1] = {0};
        app_send_ack(c->recipient_id, c->recipient_id_len,
                     empty_msg_id, 0,
                     c->rcv_auth_secret);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));  // Give Network Task time to execute
    ESP_LOGI(TAG_APP, "APP: Initial re-subscribe sent, entering main loop");

    while (1) {
        // T5: Keyboard send (non-blocking poll)
        {
            char kbd_msg[256];
            while (kbd_queue && xQueueReceive(kbd_queue, kbd_msg, 0) == pdTRUE) {
                ESP_LOGI(TAG_APP, "⌨️ Sending: \"%s\"", kbd_msg);
                contact_t *msg_contact = &contacts_db.contacts[0];
                if (peer_send_chat_message(msg_contact, kbd_msg)) {
                    ESP_LOGI(TAG_APP, "   ✅ Keyboard message sent!");
                } else {
                    ESP_LOGE(TAG_APP, "   ❌ Keyboard message send failed!");
                }
            }
        }

        // 1. Read frame from ring buffer (blocking with timeout)
        size_t item_size = 0;
        void *item = xRingbufferReceive(net_to_app_buf, &item_size, pdMS_TO_TICKS(1000));

        if (!item) continue;  // Timeout, keep waiting

        // 2. Copy to local buffer and return ring buffer item immediately
        if (item_size > SMP_BLOCK_SIZE + 2) item_size = SMP_BLOCK_SIZE + 2;
        memcpy(local_block, item, item_size);
        vRingbufferReturnItem(net_to_app_buf, item);

        // 3. Transport-Parsing (ported from main.c Z. 353-391)
        int content_len = (int)item_size - 2;
        if (content_len < 4) {
            ESP_LOGW(TAG_APP, "Frame too short: %d bytes", content_len);
            continue;
        }
        uint8_t *resp = local_block + 2;

        int p = 0;

        // txCount + 2 skip bytes
        if (p + 3 > content_len) { ESP_LOGW(TAG_APP, "Frame truncated at txCount"); continue; }
        uint8_t tx_count = resp[p]; p++;
        (void)tx_count;  // will be used in later phases
        p += 2;  // skip 2 bytes

        // authLen
        if (p >= content_len) { ESP_LOGW(TAG_APP, "Frame truncated at authLen"); continue; }
        int authLen = resp[p++];
        if (p + authLen > content_len) { ESP_LOGW(TAG_APP, "Frame truncated in auth"); continue; }
        p += authLen;

        // v7: no sessLen in response

        // corrLen
        if (p >= content_len) { ESP_LOGW(TAG_APP, "Frame truncated at corrLen"); continue; }
        int corrLen = resp[p++];
        if (p + corrLen > content_len) { ESP_LOGW(TAG_APP, "Frame truncated in corr"); continue; }
        p += corrLen;

        // entLen + entity_id
        if (p >= content_len) { ESP_LOGW(TAG_APP, "Frame truncated at entLen"); continue; }
        int entLen = resp[p++];
        if (p + entLen > content_len) { ESP_LOGW(TAG_APP, "Frame truncated in entity"); continue; }
        uint8_t entity_id[24];
        if (entLen > 24) entLen = 24;
        memcpy(entity_id, &resp[p], entLen);
        p += entLen;

        // Contact-Lookup
        int contact_idx = find_contact_by_recipient_id(entity_id, entLen);
        contact_t *contact = (contact_idx >= 0) ? &contacts_db.contacts[contact_idx] : NULL;

        // Reply Queue Check
        bool is_reply_queue = (our_queue.rcv_id_len > 0 &&
                               entLen == our_queue.rcv_id_len &&
                               memcmp(entity_id, our_queue.rcv_id, entLen) == 0);
        if (is_reply_queue) {
            ESP_LOGI(TAG_APP, "Message on REPLY QUEUE from peer!");
        }

        // 4. Command-Dispatch: ONLY LOG, no processing yet
        if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
            ESP_LOGI(TAG_APP, "APP: OK [%s]",
                     contact ? contact->name : (is_reply_queue ? "reply_q" : "unknown"));
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'N' && resp[p+2] == 'D') {
            ESP_LOGI(TAG_APP, "APP: END [%s]",
                     contact ? contact->name : (is_reply_queue ? "reply_q" : "unknown"));
        }
        else if (p + 3 < content_len && resp[p] == 'M' && resp[p+1] == 'S' && resp[p+2] == 'G' && resp[p+3] == ' ') {
            p += 4;

            // MsgId extrahieren
            if (p >= content_len) { ESP_LOGW(TAG_APP, "Frame truncated at msgIdLen"); continue; }
            uint8_t msgIdLen = resp[p++];
            uint8_t msg_id[24];
            memset(msg_id, 0, 24);
            if (msgIdLen > 24) msgIdLen = 24;
            if (p + msgIdLen > content_len) { ESP_LOGW(TAG_APP, "Frame truncated in msgId"); continue; }
            memcpy(msg_id, &resp[p], msgIdLen);
            p += msgIdLen;

            int enc_len = content_len - p;

            ESP_LOGW(TAG_APP, "APP: MSG for [%s] reply_q=%d, enc_len=%d",
                     contact ? contact->name : "unknown", is_reply_queue, enc_len);

            // ==============================================================
            // REPLY QUEUE: E2E Decrypt + Agent Process + 42d
            // ==============================================================
            if (is_reply_queue && our_queue.valid && enc_len > crypto_box_MACBYTES) {
                ESP_LOGI(TAG_APP, "   Decrypting REPLY QUEUE message...");

                uint8_t *e2e_plain = NULL;
                size_t e2e_plain_len = 0;

                int e2e_ret = smp_e2e_decrypt_reply_message(
                    &resp[p], enc_len, msg_id, msgIdLen,
                    &e2e_plain, &e2e_plain_len);

                if (e2e_ret == 0 && e2e_plain) {
                    smp_agent_process_message(e2e_plain, e2e_plain_len,
                           &contacts_db.contacts[0],
                           s_peer_sender_auth_key, &s_has_peer_sender_auth);
                    free(e2e_plain);
                } else {
                    ESP_LOGE(TAG_APP, "   Reply Queue decrypt failed! ret=%d", e2e_ret);
                }

                log_heap("after_rq_decrypt");

                // === T4e: 42d Post-Confirmation Block ===
                if (s_has_peer_sender_auth) {
                    ESP_LOGI(TAG_APP, "APP: 42d — Starting post-confirmation handshake");

                    // 1. Reconnect Reply Queue (eigene SSL-Verbindung, nicht Haupt-SSL)
                    if (!queue_reconnect()) {
                        ESP_LOGE(TAG_APP, "APP: 42d — queue_reconnect failed!");
                        goto skip_42d_app;
                    }
                    if (!queue_subscribe()) {
                        ESP_LOGE(TAG_APP, "APP: 42d — queue_subscribe failed!");
                        goto skip_42d_app;
                    }
                    if (!queue_send_key(s_peer_sender_auth_key, 44)) {
                        ESP_LOGE(TAG_APP, "APP: 42d — queue_send_key failed!");
                        goto skip_42d_app;
                    }
                    ESP_LOGI(TAG_APP, "APP: 42d — KEY accepted!");

                    // 2. HELLO auf Contact Queue (eigene Peer-Verbindung)
                    ESP_LOGI(TAG_APP, "APP: 42d — Sending HELLO on Contact Queue Q_A...");
                    {
                        contact_t *hello_contact = &contacts_db.contacts[0];
                        if (peer_send_hello(hello_contact)) {
                            ESP_LOGI(TAG_APP, "APP: 42d — HELLO sent!");
                        } else {
                            ESP_LOGE(TAG_APP, "APP: 42d — HELLO failed!");
                        }
                    }

                    // 3. Read + Decrypt Reply Queue response (eigene Verbindung)
                    ESP_LOGI(TAG_APP, "APP: 42d — Reading Reply Queue message...");
                    {
                        uint8_t *rq_block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
                        if (!rq_block) goto skip_42d_app;

                        for (int rq_try = 0; rq_try < 3; rq_try++) {
                            int rq_len = queue_read_raw(rq_block, SMP_BLOCK_SIZE, 15000);
                            if (rq_len < 0) break;

                            uint8_t *rq_resp = rq_block + 2;
                            int rp = 0;
                            rp++;  // txCount
                            rp += 2;
                            int rq_authLen = rq_resp[rp++]; rp += rq_authLen;
                            // v7: no sessLen in response
                            int rq_corrLen = rq_resp[rp++]; rp += rq_corrLen;
                            int rq_entLen  = rq_resp[rp++]; rp += rq_entLen;

                            if (rp + 1 < rq_len && rq_resp[rp] == 'O' && rq_resp[rp+1] == 'K') continue;
                            if (rp + 2 < rq_len && rq_resp[rp] == 'E' && rq_resp[rp+1] == 'N' && rq_resp[rp+2] == 'D') continue;
                            if (!(rp + 3 < rq_len && rq_resp[rp] == 'M' && rq_resp[rp+1] == 'S' && rq_resp[rp+2] == 'G')) continue;

                            rp += 4;
                            uint8_t rq_msgIdLen = rq_resp[rp++];
                            uint8_t rq_msg_id[24] = {0};
                            if (rq_msgIdLen > 24) rq_msgIdLen = 24;
                            memcpy(rq_msg_id, &rq_resp[rp], rq_msgIdLen);
                            rp += rq_msgIdLen;

                            int rq_enc_len = rq_len - rp;
                            ESP_LOGI(TAG_APP, "APP: 42d — Reply Queue MSG (%d bytes)", rq_enc_len);

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
                        heap_caps_free(rq_block);
                    }

                    // 4. First chat message (eigene Peer-Verbindung)
                    ESP_LOGI(TAG_APP, "APP: 42d — Sending first chat message in 3 seconds...");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    {
                        contact_t *msg_contact = &contacts_db.contacts[0];
                        if (peer_send_chat_message(msg_contact, "Hello from ESP32!")) {
                            ESP_LOGI(TAG_APP, "APP: 42d — Chat message sent!");
                        } else {
                            ESP_LOGE(TAG_APP, "APP: 42d — Chat message failed!");
                        }
                    }

                    s_has_peer_sender_auth = false;  // Don't re-trigger

                    skip_42d_app: ;

                    log_heap("after_42d");
                }

                // T4d: ACK Reply Queue on main connection via Network Task
                app_send_ack(our_queue.rcv_id, our_queue.rcv_id_len,
                             msg_id, msgIdLen, our_queue.rcv_auth_private);

                // T4d: Re-subscribe after handshake
                app_request_subscribe_all();

                ESP_LOGI(TAG_APP, "APP: Reply Queue processing complete.");
            }

            // ==============================================================
            // CONTACT QUEUE: SMP Decrypt + Agent Parse + ACK
            // ==============================================================
            if (contact && contact->have_srv_dh && enc_len > crypto_box_MACBYTES) {
                ESP_LOGW(TAG_APP, "APP: Contact Queue decrypt for [%s], enc_len=%d",
                         contact->name, enc_len);

                uint8_t *plain = heap_caps_malloc(enc_len, MALLOC_CAP_SPIRAM);
                if (plain) {
                    int plain_len = 0;
                    if (decrypt_smp_message(contact, &resp[p], enc_len,
                                            msg_id, msgIdLen, plain, &plain_len)) {
                        ESP_LOGI(TAG_APP, "   SMP Decrypt OK! (%d bytes)", plain_len);
                        parse_agent_message(contact, plain, plain_len);
                    } else {
                        ESP_LOGE(TAG_APP, "   Decrypt FAILED!");
                    }
                    heap_caps_free(plain);
                } else {
                    ESP_LOGE(TAG_APP, "   Failed to allocate decrypt buffer!");
                }

                log_heap("after_cq_decrypt");

                // T4d: ACK Contact Queue on main connection via Network Task
                app_send_ack(contact->recipient_id, contact->recipient_id_len,
                             msg_id, msgIdLen, contact->rcv_auth_secret);
            } else if (!is_reply_queue) {
                ESP_LOGW(TAG_APP, "   Cannot decrypt - contact=%s, have_srv_dh=%d, enc_len=%d",
                         contact ? contact->name : "NULL",
                         contact ? contact->have_srv_dh : -1,
                         enc_len);
            }
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
            ESP_LOGE(TAG_APP, "APP: ERR [%s]",
                     contact ? contact->name : "unknown");
        }
        else {
            ESP_LOGW(TAG_APP, "APP: Unknown cmd=%c%c%c",
                     (p < content_len) ? resp[p] : '?',
                     (p+1 < content_len) ? resp[p+1] : '?',
                     (p+2 < content_len) ? resp[p+2] : '?');
        }
    }

    ESP_LOGW(TAG_APP, "App logic: parse loop ended, cleaning up");
    heap_caps_free(local_block);
}

static void ui_task(void *arg)
{
    ESP_LOGI(TAG, "UI task running on core %d", xPortGetCoreID());
    log_heap("ui_task");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- Public API ---

int smp_tasks_init(void)
{
    ESP_LOGI(TAG, "Initializing task infrastructure...");
    log_heap("before_init");

    // Initialize frame pool (4 x 4KB = 16KB in PSRAM)
    smp_frame_pool_init();
    ESP_LOGI(TAG, "  Frame pool: %d frames available (PSRAM)",
             smp_frame_pool_available());
    log_heap("after_pool");

    // Create ring buffers in PSRAM (NOSPLIT = items not split across wrap)
    net_to_app_buf = xRingbufferCreateWithCaps(NET_TO_APP_BUF_SIZE,
                                                RINGBUF_TYPE_NOSPLIT,
                                                MALLOC_CAP_SPIRAM);
    if (!net_to_app_buf) {
        ESP_LOGE(TAG, "Failed to create net_to_app ring buffer");
        return -1;
    }

    app_to_net_buf = xRingbufferCreateWithCaps(APP_TO_NET_BUF_SIZE,
                                                RINGBUF_TYPE_NOSPLIT,
                                                MALLOC_CAP_SPIRAM);
    if (!app_to_net_buf) {
        ESP_LOGE(TAG, "Failed to create app_to_net ring buffer");
        vRingbufferDeleteWithCaps(net_to_app_buf);
        net_to_app_buf = NULL;
        return -1;
    }

    ESP_LOGI(TAG, "  Ring buffers (PSRAM): net->app %dB, app->net %dB",
             NET_TO_APP_BUF_SIZE, APP_TO_NET_BUF_SIZE);
    log_heap("after_init");

    return 0;
}

int smp_tasks_start(mbedtls_ssl_context *ssl_context, const uint8_t *session_id, int sock_fd)
{
    if (!ssl_context || !session_id) {
        ESP_LOGE(TAG, "SSL context or session_id is NULL");
        return -1;
    }

    s_ssl = ssl_context;
    memcpy(s_session_id, session_id, 32);
    s_sock_fd = sock_fd;  // T6-Fix: Store for network task timeout

    ESP_LOGI(TAG, "Starting tasks (all in PSRAM)...");
    log_heap("before_tasks");

    // Network task on Core 0 (stack in PSRAM)
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(
        network_task, "net_task",
        NETWORK_TASK_STACK, NULL,
        NETWORK_TASK_PRIO, &network_task_handle, 0,
        MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network task");
        return -1;
    }

    // App logic: NOT a separate task anymore!
    // Runs on main task (64KB Internal SRAM stack) because NVS writes
    // crash with PSRAM stack (SPI Flash disables cache = PSRAM inaccessible)
    // Call smp_app_run() from main.c after smp_tasks_start()

    // UI task on Core 1 (stack in PSRAM)
    ret = xTaskCreatePinnedToCoreWithCaps(
        ui_task, "ui_task",
        UI_TASK_STACK, NULL,
        UI_TASK_PRIO, &ui_task_handle, 1,
        MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI task");
        smp_tasks_stop();
        return -1;
    }

    ESP_LOGI(TAG, "Tasks started (net+ui PSRAM, app on main task)");
    log_heap("after_tasks");

    return 0;
}

void smp_tasks_stop(void)
{
    ESP_LOGW(TAG, "Stopping tasks...");

    if (network_task_handle) {
        vTaskDeleteWithCaps(network_task_handle);
        network_task_handle = NULL;
    }
    if (ui_task_handle) {
        vTaskDeleteWithCaps(ui_task_handle);
        ui_task_handle = NULL;
    }

    if (net_to_app_buf) {
        vRingbufferDeleteWithCaps(net_to_app_buf);
        net_to_app_buf = NULL;
    }
    if (app_to_net_buf) {
        vRingbufferDeleteWithCaps(app_to_net_buf);
        app_to_net_buf = NULL;
    }

    s_ssl = NULL;

    ESP_LOGW(TAG, "All tasks stopped");
}
