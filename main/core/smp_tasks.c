/**
 * @file smp_tasks.c
 * @brief FreeRTOS task manager for SimpleGo — Phase 3
 *
 * Network Task (Core 0): TLS read loop + send outgoing ACKs
 * App Task (Core 1): Frame processing, decrypt pipeline, keyboard
 * UI Task (Core 1): Stub (Phase 4)
 *
 * Memory strategy: All tasks use PSRAM stacks (plenty of room).
 * NVS/flash writes are proxied through a small internal-RAM task
 * in smp_storage.c (flash ops disable cache → PSRAM inaccessible).
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#include "smp_tasks.h"
#include "smp_events.h"
#include "smp_frame_pool.h"
#include "smp_msg_router.h"
#include "smp_network.h"
#include "smp_contacts.h"
#include "smp_queue.h"
#include "smp_peer.h"
#include "smp_handshake.h"
#include "smp_storage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"

#include "ui_connect.h"

#include <string.h>
#include <unistd.h>

static const char *TAG = "smp_tasks";

/* ========================================================================
 * Task Configuration
 * ======================================================================== */

#define NETWORK_TASK_STACK  16384
#define NETWORK_TASK_PRIO   7
#define NETWORK_TASK_CORE   0

#define APP_TASK_STACK      32768
#define APP_TASK_PRIO       6
#define APP_TASK_CORE       1

#define UI_TASK_STACK       8192
#define UI_TASK_PRIO        5
#define UI_TASK_CORE        1

/* Ring buffer sizes */
#define NET_TO_APP_RB_SIZE  8192
#define APP_TO_NET_RB_SIZE  4096

/* Queue depths */
#define APP_TO_UI_DEPTH     10
#define UI_TO_APP_DEPTH     10

/* Network read timeout (ms) — short so we can check outgoing queue */
#define NET_READ_TIMEOUT_MS 200

/* Heap monitor interval */
#define HEAP_MONITOR_INTERVAL_MS 30000

/* SMP block size (must fit largest SMP frame + 2 byte header) */
#ifndef SMP_BLOCK_SIZE
#define SMP_BLOCK_SIZE 18000
#endif

/* ========================================================================
 * Exported Handles & Shared State
 * ======================================================================== */

TaskHandle_t network_task_handle = NULL;
TaskHandle_t app_task_handle     = NULL;
TaskHandle_t ui_task_handle      = NULL;

RingbufHandle_t net_to_app_rb = NULL;
RingbufHandle_t app_to_net_rb = NULL;

QueueHandle_t app_to_ui_queue = NULL;
QueueHandle_t ui_to_app_queue = NULL;

QueueHandle_t kbd_msg_queue = NULL;  /* Created in main.c, consumed by App Task */

uint8_t net_session_id[32] = {0};
volatile bool need_resubscribe = false;

/* ========================================================================
 * TLS Context (owned by Network Task, initialized by smp_net_init)
 * ======================================================================== */

static mbedtls_ssl_context   net_ssl;
static mbedtls_ssl_config    net_conf;
static mbedtls_entropy_context net_entropy;
static mbedtls_ctr_drbg_context net_ctr_drbg;
static int                   net_sock = -1;
static uint8_t              *net_block = NULL;  /* SMP_BLOCK_SIZE work buffer */
static bool                  net_initialized = false;

/* ========================================================================
 * smp_net_init() — Connection Setup (runs in main thread)
 * ======================================================================== */

esp_err_t smp_net_init(const char *host, int port, bool restored)
{
    int ret;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  SimpleGo v0.1.17-alpha Connection!    |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "");

    /* Allocate work buffer */
    net_block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!net_block) {
        ESP_LOGE(TAG, "Failed to allocate block buffer!");
        return ESP_ERR_NO_MEM;
    }

    /* Init mbedTLS */
    mbedtls_ssl_init(&net_ssl);
    mbedtls_ssl_config_init(&net_conf);
    mbedtls_entropy_init(&net_entropy);
    mbedtls_ctr_drbg_init(&net_ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&net_ctr_drbg, mbedtls_entropy_func, &net_entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "ctr_drbg seed failed: -0x%04X", -ret);
        goto fail;
    }

    /* ========== Step 1: TCP + TLS ========== */
    ESP_LOGI(TAG, "[1/5] Connecting to %s:%d...", host, port);
    net_sock = smp_tcp_connect(host, port);
    if (net_sock < 0) {
        ESP_LOGE(TAG, "TCP connect failed!");
        goto fail;
    }

    ret = mbedtls_ssl_config_defaults(&net_conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) goto fail;

    mbedtls_ssl_conf_min_tls_version(&net_conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&net_conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&net_conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&net_conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&net_conf, mbedtls_ctr_drbg_random, &net_ctr_drbg);

    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&net_conf, alpn_list);

    ret = mbedtls_ssl_setup(&net_ssl, &net_conf);
    if (ret != 0) goto fail;

    mbedtls_ssl_set_hostname(&net_ssl, host);
    mbedtls_ssl_set_bio(&net_ssl, &net_sock, my_send_cb, my_recv_cb, NULL);

    while ((ret = mbedtls_ssl_handshake(&net_ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "      TLS failed: -0x%04X", -ret);
            goto fail;
        }
    }
    ESP_LOGI(TAG, "      TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&net_ssl));

    /* ========== Step 2: ServerHello ========== */
    ESP_LOGI(TAG, "[2/5] Waiting for ServerHello...");
    int content_len = smp_read_block(&net_ssl, net_block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "      No ServerHello");
        goto fail;
    }

    uint8_t *hello = net_block + 2;
    uint16_t minVer = (hello[0] << 8) | hello[1];
    uint16_t maxVer = (hello[2] << 8) | hello[3];
    uint8_t sessIdLen = hello[4];

    if (sessIdLen != 32) {
        ESP_LOGE(TAG, "      Unexpected sessionId length: %d", sessIdLen);
        goto fail;
    }
    memcpy(net_session_id, &hello[5], 32);

    ESP_LOGI(TAG, "      Versions: %d-%d", minVer, maxVer);
    ESP_LOGI(TAG, "      SessionId: %02x%02x%02x%02x...",
             net_session_id[0], net_session_id[1],
             net_session_id[2], net_session_id[3]);

    /* ========== Step 3: ClientHello ========== */
    ESP_LOGI(TAG, "[3/5] Sending ClientHello...");

    uint8_t ca_hash[32];
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);

    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
    }

    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;
    client_hello[pos++] = 32;
    memcpy(&client_hello[pos], ca_hash, 32);
    pos += 32;

    ret = smp_write_handshake_block(&net_ssl, net_block, client_hello, pos);
    if (ret != 0) goto fail;
    ESP_LOGI(TAG, "      ClientHello sent!");

    /* ========== Step 4: Load or Create Contacts ========== */
    ESP_LOGI(TAG, "[4/5] Loading contacts...");

    if (!restored) {
        ESP_LOGW(TAG, "      Clearing old contacts for fresh test...");
        clear_all_contacts();
    } else {
        ESP_LOGI(TAG, "      Session restored — keeping persisted contacts");
    }

    load_contacts_from_nvs();

    if (contacts_db.num_contacts == 0) {
        if (restored) {
            ESP_LOGE(TAG, "      ⚠️ Session restored but no contacts found! Fresh start...");
            restored = false;
        }
        ESP_LOGI(TAG, "      No contacts found - creating 'ESP32'...");
        int idx = add_contact(&net_ssl, net_block, net_session_id, "ESP32");
        if (idx < 0) {
            ESP_LOGE(TAG, "      Failed to create contact!");
            goto fail;
        }
    } else {
        ESP_LOGI(TAG, "      %d contact(s) loaded from NVS", contacts_db.num_contacts);
    }

    list_contacts();

    /* ========== Step 5: Subscribe All Contacts ========== */
    ESP_LOGI(TAG, "[5/5] Subscribing to queues...");
    subscribe_all_contacts(&net_ssl, net_block, net_session_id);

    print_invitation_links(ca_hash, host, port);

    /* Send invite link to UI */
    {
        static char invite_link[1500];
        if (get_invite_link(ca_hash, host, port, invite_link, sizeof(invite_link))) {
            ui_connect_set_invite_link(invite_link);
        }
    }

    ESP_LOGI(TAG, "+--------------------------------------+");
    ESP_LOGI(TAG, "|   Connection ready — starting tasks  |");
    ESP_LOGI(TAG, "+--------------------------------------+");
    ESP_LOGI(TAG, "");

    net_initialized = true;
    return ESP_OK;

fail:
    if (net_block) { free(net_block); net_block = NULL; }
    mbedtls_ssl_free(&net_ssl);
    mbedtls_ssl_config_free(&net_conf);
    mbedtls_ctr_drbg_free(&net_ctr_drbg);
    mbedtls_entropy_free(&net_entropy);
    if (net_sock >= 0) { close(net_sock); net_sock = -1; }
    return ESP_FAIL;
}

/* ========================================================================
 * Network Task — TLS Read Loop + Send Outgoing
 * ======================================================================== */

static void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network Task started on Core %d", xPortGetCoreID());

    if (!net_initialized) {
        ESP_LOGE(TAG, "Network not initialized! Task exiting.");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        /* --- 1. Check for re-subscribe request from App Task --- */
        if (need_resubscribe) {
            ESP_LOGW(TAG, "Re-subscribing to all contacts...");
            subscribe_all_contacts(&net_ssl, net_block, net_session_id);
            need_resubscribe = false;
            ESP_LOGW(TAG, "Re-subscribe done!");
        }

        /* --- 2. Check outgoing ring buffer (ACKs from App Task) --- */
        size_t send_size = 0;
        void *send_item = xRingbufferReceive(app_to_net_rb, &send_size, 0);
        if (send_item != NULL) {
            ESP_LOGD(TAG, "Sending outgoing frame (%zu bytes)", send_size);
            smp_write_command_block(&net_ssl, net_block, (uint8_t *)send_item, send_size);
            vRingbufferReturnItem(app_to_net_rb, send_item);
        }

        /* --- 3. Read incoming frame (short timeout) --- */
        int content_len = smp_read_block(&net_ssl, net_block, NET_READ_TIMEOUT_MS);

        if (content_len == -2) {
            /* Timeout — no data, loop back to check outgoing + read again */
            continue;
        }

        if (content_len < 0) {
            /* Connection error or closed */
            ESP_LOGE(TAG, "Connection lost! (ret=%d)", content_len);
            /* TODO: Reconnect logic */
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        /* --- 4. Valid frame: copy to heap and send ref to App Task --- */
        int frame_size = content_len + 2;  /* Include 2-byte header */
        uint8_t *frame_copy = (uint8_t *)malloc(frame_size);
        if (!frame_copy) {
            ESP_LOGE(TAG, "Failed to allocate frame copy (%d bytes)!", frame_size);
            continue;
        }
        memcpy(frame_copy, net_block, frame_size);

        frame_ref_t ref = {
            .data = frame_copy,
            .content_len = content_len
        };

        if (xRingbufferSend(net_to_app_rb, &ref, sizeof(ref),
                            pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Ring buffer full! Dropping frame.");
            free(frame_copy);
        } else {
            ESP_LOGD(TAG, "Frame sent to App Task (%d bytes)", content_len);
        }
    }
}

/* ========================================================================
 * App Task — Frame Processing + Keyboard + Heap Monitor
 * ======================================================================== */

static void app_task(void *pvParameters)
{
    ESP_LOGI(TAG, "App Task started on Core %d", xPortGetCoreID());

    msg_router_init();

    TickType_t last_heap_log = xTaskGetTickCount();

    while (1) {
        bool had_event = false;

        /* --- 1. Receive frame from Network Task --- */
        size_t item_size = 0;
        void *item = xRingbufferReceive(net_to_app_rb, &item_size, pdMS_TO_TICKS(100));
        if (item != NULL && item_size == sizeof(frame_ref_t)) {
            frame_ref_t ref;
            memcpy(&ref, item, sizeof(frame_ref_t));
            vRingbufferReturnItem(net_to_app_rb, item);

            /* Process frame through decrypt pipeline */
            msg_router_process_frame(ref.data, ref.content_len);

            /* Free the heap-allocated frame copy */
            free(ref.data);
            had_event = true;
        } else if (item != NULL) {
            /* Wrong size — shouldn't happen */
            ESP_LOGE(TAG, "Unexpected ring buffer item size: %zu", item_size);
            vRingbufferReturnItem(net_to_app_rb, item);
        }

        /* --- 2. Check keyboard queue --- */
        char kbd_msg[256];
        while (kbd_msg_queue && xQueueReceive(kbd_msg_queue, kbd_msg, 0) == pdTRUE) {
            ESP_LOGI(TAG, "⌨️ Sending: \"%s\"", kbd_msg);
            contact_t *msg_contact = &contacts_db.contacts[0];
            if (peer_send_chat_message(msg_contact, kbd_msg)) {
                ESP_LOGI(TAG, "   ✅ Keyboard message sent!");
            } else {
                ESP_LOGE(TAG, "   ❌ Keyboard message send failed!");
            }
            had_event = true;
        }

        /* --- 3. Check UI events --- */
        app_event_t ui_event;
        if (xQueueReceive(ui_to_app_queue, &ui_event, 0) == pdTRUE) {
            ESP_LOGI(TAG, "UI event received (type: %d)", ui_event.type);
            /* TODO Phase 4: Handle UI events */
            had_event = true;
        }

        /* --- 4. Heap monitor (every 30s) --- */
        if ((xTaskGetTickCount() - last_heap_log) >= pdMS_TO_TICKS(HEAP_MONITOR_INTERVAL_MS)) {
            ESP_LOGI(TAG, "Heap: free=%lu, min=%lu, largest=%lu",
                     (unsigned long)esp_get_free_heap_size(),
                     (unsigned long)esp_get_minimum_free_heap_size(),
                     (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            last_heap_log = xTaskGetTickCount();
        }

        /* --- 5. Yield if no events --- */
        if (!had_event) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* ========================================================================
 * UI Task — Stub (Phase 4)
 * ======================================================================== */

static void ui_task(void *pvParameters)
{
    ESP_LOGI(TAG, "UI Task started on Core %d", xPortGetCoreID());
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ========================================================================
 * Public API
 * ======================================================================== */

esp_err_t smp_tasks_init(void)
{
    /* Create ring buffers for Network <-> App */
    net_to_app_rb = xRingbufferCreate(NET_TO_APP_RB_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (net_to_app_rb == NULL) {
        ESP_LOGE(TAG, "Failed to create net_to_app ring buffer");
        return ESP_FAIL;
    }

    app_to_net_rb = xRingbufferCreate(APP_TO_NET_RB_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (app_to_net_rb == NULL) {
        ESP_LOGE(TAG, "Failed to create app_to_net ring buffer");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Ring Buffers initialized (net->app: %d, app->net: %d)",
             NET_TO_APP_RB_SIZE, APP_TO_NET_RB_SIZE);

    /* Create queues for App <-> UI */
    app_to_ui_queue = xQueueCreate(APP_TO_UI_DEPTH, sizeof(ui_event_t));
    if (app_to_ui_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create app_to_ui queue");
        return ESP_FAIL;
    }

    ui_to_app_queue = xQueueCreate(UI_TO_APP_DEPTH, sizeof(app_event_t));
    if (ui_to_app_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create ui_to_app queue");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Queues initialized (app->ui: %d, ui->app: %d)",
             APP_TO_UI_DEPTH, UI_TO_APP_DEPTH);

    return ESP_OK;
}

esp_err_t smp_tasks_start(void)
{
    BaseType_t ret;

    ESP_LOGW(TAG, "Heap before tasks: free=%lu, largest=%lu",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    /* Start NVS Writer proxy BEFORE creating PSRAM tasks */
    if (smp_storage_start_writer() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start NVS Writer proxy!");
        return ESP_FAIL;
    }

    /* All tasks use PSRAM stacks — NVS writes go through proxy */

    ret = xTaskCreatePinnedToCoreWithCaps(network_task, "network_task",
        NETWORK_TASK_STACK, NULL, NETWORK_TASK_PRIO,
        &network_task_handle, NETWORK_TASK_CORE, MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Network Task");
        return ESP_FAIL;
    }

    ret = xTaskCreatePinnedToCoreWithCaps(app_task, "app_task",
        APP_TASK_STACK, NULL, APP_TASK_PRIO,
        &app_task_handle, APP_TASK_CORE, MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create App Task");
        return ESP_FAIL;
    }

    ret = xTaskCreatePinnedToCoreWithCaps(ui_task, "ui_task",
        UI_TASK_STACK, NULL, UI_TASK_PRIO,
        &ui_task_handle, UI_TASK_CORE, MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI Task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "All tasks started successfully");
    return ESP_OK;
}
