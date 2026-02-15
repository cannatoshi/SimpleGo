/**
 * SimpleGo - smp_storage.c
 * Persistent Storage Module Implementation
 * v0.1.17-alpha — Phase 3: NVS Writer Proxy
 *
 * Two-Phase Init Architecture:
 *   Phase 1: smp_storage_init()    → NVS only (before display, no SPI)
 *   Phase 2: smp_storage_init_sd() → SD card on existing SPI bus (after display)
 *
 * NVS Writer Proxy:
 *   Flash writes crash when called from tasks with PSRAM stacks (ESP32
 *   disables cache during flash operations, making PSRAM inaccessible).
 *   Solution: A small proxy task with internal RAM stack handles all
 *   NVS writes. Callers block on a semaphore until write completes.
 *
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_storage.h"
#include "smp_ratchet.h"

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static const char *TAG = "SMP_STOR";

// ============== T-Deck SD Card Configuration ==============
#define SD_PIN_CS       GPIO_NUM_39
#define SD_SPI_HOST     SPI2_HOST

// ============== Internal State ==============

static struct {
    nvs_handle_t nvs_handle;
    bool nvs_ready;
    bool sd_mounted;
    sdmmc_card_t *sd_card;
} storage = {0};

// ============== NVS Writer Proxy ==============

#define NVS_WRITER_STACK    3072    /* Internal RAM — small, just NVS calls */
#define NVS_WRITER_PRIO     8       /* Higher than App Task to unblock quickly */
#define NVS_WRITER_QUEUE    4       /* Max queued write requests */

typedef enum {
    NVS_OP_SAVE,        /* nvs_set_blob + commit */
    NVS_OP_SAVE_SYNC,   /* nvs_set_blob + commit + verify */
    NVS_OP_DELETE,       /* nvs_erase_key + commit */
} nvs_op_type_t;

typedef struct {
    nvs_op_type_t op;
    const char *key;
    const void *data;       /* For SAVE/SAVE_SYNC */
    size_t len;             /* For SAVE/SAVE_SYNC */
    esp_err_t *result;      /* Caller's result pointer */
    SemaphoreHandle_t done; /* Signal completion */
} nvs_write_req_t;

static TaskHandle_t nvs_writer_handle = NULL;
static QueueHandle_t nvs_write_queue = NULL;
static bool nvs_writer_running = false;

/**
 * NVS Writer Task — runs on internal RAM stack.
 * Processes write requests from any task (including PSRAM-stack tasks).
 */
static void nvs_writer_task(void *pvParameters)
{
    ESP_LOGI(TAG, "NVS Writer started (Core %d, internal RAM)", xPortGetCoreID());
    nvs_write_req_t req;

    while (1) {
        if (xQueueReceive(nvs_write_queue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        esp_err_t ret = ESP_FAIL;

        switch (req.op) {
        case NVS_OP_SAVE: {
            ret = nvs_set_blob(storage.nvs_handle, req.key, req.data, req.len);
            if (ret == ESP_OK) {
                ret = nvs_commit(storage.nvs_handle);
            }
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "NVS write '%s' failed: %s", req.key, esp_err_to_name(ret));
            } else {
                ESP_LOGD(TAG, "NVS save: '%s' (%zu bytes)", req.key, req.len);
            }
            break;
        }

        case NVS_OP_SAVE_SYNC: {
            int64_t t_start = esp_timer_get_time();

            ret = nvs_set_blob(storage.nvs_handle, req.key, req.data, req.len);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "SYNC write '%s' failed: %s", req.key, esp_err_to_name(ret));
                break;
            }

            ret = nvs_commit(storage.nvs_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "SYNC commit '%s' failed: %s", req.key, esp_err_to_name(ret));
                break;
            }

            /* Verify read-back */
            uint8_t *verify_buf = malloc(req.len);
            if (!verify_buf) {
                ESP_LOGE(TAG, "SYNC verify malloc failed (%zu bytes)", req.len);
                ret = ESP_ERR_NO_MEM;
                break;
            }

            size_t verify_len = req.len;
            ret = nvs_get_blob(storage.nvs_handle, req.key, verify_buf, &verify_len);
            if (ret != ESP_OK || verify_len != req.len ||
                memcmp(req.data, verify_buf, req.len) != 0) {
                ESP_LOGE(TAG, "SYNC verify FAILED for '%s'! Data corruption!", req.key);
                free(verify_buf);
                ret = ESP_ERR_INVALID_RESPONSE;
                break;
            }
            free(verify_buf);

            int64_t elapsed_us = esp_timer_get_time() - t_start;
            ESP_LOGI(TAG, "SYNC save: '%s' (%zu bytes) verified in %lld us",
                     req.key, req.len, elapsed_us);
            break;
        }

        case NVS_OP_DELETE: {
            ret = nvs_erase_key(storage.nvs_handle, req.key);
            if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGE(TAG, "nvs_erase_key('%s') failed: %s", req.key, esp_err_to_name(ret));
            } else {
                nvs_commit(storage.nvs_handle);
                ESP_LOGD(TAG, "NVS delete: '%s' %s", req.key,
                         ret == ESP_ERR_NVS_NOT_FOUND ? "(not found)" : "OK");
            }
            break;
        }
        }

        /* Signal result and completion */
        if (req.result) {
            *req.result = ret;
        }
        if (req.done) {
            xSemaphoreGive(req.done);
        }
    }
}

/**
 * Submit an NVS write request and wait for completion.
 */
static esp_err_t nvs_proxy_submit(nvs_op_type_t op, const char *key,
                                   const void *data, size_t len)
{
    if (!nvs_writer_running) {
        /* Fallback: writer not started yet (early init), do directly */
        switch (op) {
        case NVS_OP_SAVE: {
            esp_err_t r = nvs_set_blob(storage.nvs_handle, key, data, len);
            if (r == ESP_OK) r = nvs_commit(storage.nvs_handle);
            return r;
        }
        case NVS_OP_DELETE: {
            esp_err_t r = nvs_erase_key(storage.nvs_handle, key);
            if (r == ESP_OK || r == ESP_ERR_NVS_NOT_FOUND) nvs_commit(storage.nvs_handle);
            return r;
        }
        case NVS_OP_SAVE_SYNC: {
            /* Direct sync write (for early init / self-test) */
            int64_t t_start = esp_timer_get_time();
            esp_err_t r = nvs_set_blob(storage.nvs_handle, key, data, len);
            if (r != ESP_OK) return r;
            r = nvs_commit(storage.nvs_handle);
            if (r != ESP_OK) return r;
            uint8_t *vb = malloc(len);
            if (!vb) return ESP_ERR_NO_MEM;
            size_t vl = len;
            r = nvs_get_blob(storage.nvs_handle, key, vb, &vl);
            if (r != ESP_OK || vl != len || memcmp(data, vb, len) != 0) {
                free(vb);
                return ESP_ERR_INVALID_RESPONSE;
            }
            free(vb);
            int64_t elapsed_us = esp_timer_get_time() - t_start;
            ESP_LOGI(TAG, "SYNC save: '%s' (%zu bytes) verified in %lld us", key, len, elapsed_us);
            return ESP_OK;
        }
        }
        return ESP_FAIL;
    }

    /* Create one-shot semaphore for sync */
    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (!done) {
        ESP_LOGE(TAG, "Failed to create completion semaphore");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = ESP_FAIL;

    nvs_write_req_t req = {
        .op = op,
        .key = key,
        .data = data,
        .len = len,
        .result = &result,
        .done = done,
    };

    if (xQueueSend(nvs_write_queue, &req, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "NVS write queue full! Dropping '%s'", key);
        vSemaphoreDelete(done);
        return ESP_ERR_TIMEOUT;
    }

    /* Wait for writer task to complete the operation */
    if (xSemaphoreTake(done, pdMS_TO_TICKS(10000)) != pdTRUE) {
        ESP_LOGE(TAG, "NVS write timeout for '%s'!", key);
        vSemaphoreDelete(done);
        return ESP_ERR_TIMEOUT;
    }

    vSemaphoreDelete(done);
    return result;
}

/**
 * Start the NVS Writer proxy task.
 * Call after smp_storage_init() but before tasks that need NVS writes.
 */
esp_err_t smp_storage_start_writer(void)
{
    if (nvs_writer_running) return ESP_OK;

    nvs_write_queue = xQueueCreate(NVS_WRITER_QUEUE, sizeof(nvs_write_req_t));
    if (!nvs_write_queue) {
        ESP_LOGE(TAG, "Failed to create NVS write queue");
        return ESP_FAIL;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(
        nvs_writer_task, "nvs_writer",
        NVS_WRITER_STACK, NULL, NVS_WRITER_PRIO,
        &nvs_writer_handle, 0  /* Core 0 — same as Network, away from App */
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create NVS Writer task");
        return ESP_FAIL;
    }

    nvs_writer_running = true;
    ESP_LOGI(TAG, "NVS Writer proxy started (stack=%d, internal RAM)", NVS_WRITER_STACK);
    return ESP_OK;
}

// ============== Helper: Create directory recursively ==============

static esp_err_t mkdir_p(const char *path) {
    char tmp[128];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    int ret = mkdir(tmp, 0755);
    if (ret != 0 && errno != EEXIST) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ============== Phase 1: NVS Init (call BEFORE display) ==============

esp_err_t smp_storage_init(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SimpleGo Storage Init (Phase 1: NVS) ===");

    ret = nvs_open(SMP_STORAGE_NVS_NAMESPACE, NVS_READWRITE, &storage.nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return ret;
    }
    storage.nvs_ready = true;
    ESP_LOGI(TAG, "NVS namespace '%s' opened — ready for Write-Before-Send", SMP_STORAGE_NVS_NAMESPACE);
    ESP_LOGI(TAG, "");

    return ESP_OK;
}

// ============== Phase 2: SD Card Init (call AFTER display) ==============

esp_err_t smp_storage_init_sd(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SimpleGo Storage Init (Phase 2: SD Card) ===");

    if (storage.sd_mounted) {
        ESP_LOGW(TAG, "SD already mounted");
        return ESP_OK;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = SD_SPI_HOST;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(
        SMP_STORAGE_SD_MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &storage.sd_card
    );

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "  (This is OK — message history will use NVS fallback)");
        storage.sd_mounted = false;
    } else {
        storage.sd_mounted = true;
        ESP_LOGI(TAG, "SD card mounted at %s", SMP_STORAGE_SD_MOUNT_POINT);
        sdmmc_card_print_info(stdout, storage.sd_card);
        mkdir_p(SMP_STORAGE_SD_MSG_DIR);
        ESP_LOGI(TAG, "SD directories created");
    }

    ESP_LOGI(TAG, "Storage complete: NVS=%s, SD=%s",
             storage.nvs_ready ? "OK" : "FAIL",
             storage.sd_mounted ? "OK" : "N/A");
    ESP_LOGI(TAG, "");

    return ESP_OK;
}

void smp_storage_deinit(void) {
    if (storage.nvs_ready) {
        nvs_close(storage.nvs_handle);
        storage.nvs_ready = false;
        ESP_LOGI(TAG, "NVS closed");
    }

    if (storage.sd_mounted) {
        esp_vfs_fat_sdcard_unmount(SMP_STORAGE_SD_MOUNT_POINT, storage.sd_card);
        storage.sd_mounted = false;
        ESP_LOGI(TAG, "SD card unmounted");
    }
}

bool smp_storage_sd_available(void) {
    return storage.sd_mounted;
}

// ============== NVS Backend (routed through proxy) ==============

esp_err_t smp_storage_save_blob(const char *key, const void *data, size_t len) {
    if (!storage.nvs_ready) {
        ESP_LOGE(TAG, "NVS not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!key || !data || len == 0) return ESP_ERR_INVALID_ARG;
    if (len > SMP_STORAGE_MAX_BLOB_SIZE) {
        ESP_LOGE(TAG, "Blob too large: %zu > %d", len, SMP_STORAGE_MAX_BLOB_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    return nvs_proxy_submit(NVS_OP_SAVE, key, data, len);
}

esp_err_t smp_storage_load_blob(const char *key, void *buf, size_t buf_len, size_t *out_len) {
    if (!storage.nvs_ready) {
        ESP_LOGE(TAG, "NVS not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!key || !buf || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Reads are safe from any task — flash reads don't disable cache */
    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(storage.nvs_handle, key, NULL, &required_size);
    if (ret != ESP_OK) {
        return ret;
    }

    if (required_size > buf_len) {
        ESP_LOGE(TAG, "Buffer too small for '%s': need %zu, have %zu", key, required_size, buf_len);
        return ESP_ERR_INVALID_SIZE;
    }

    ret = nvs_get_blob(storage.nvs_handle, key, buf, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob('%s') failed: %s", key, esp_err_to_name(ret));
        return ret;
    }

    if (out_len) {
        *out_len = required_size;
    }

    ESP_LOGD(TAG, "NVS load: '%s' (%zu bytes)", key, required_size);
    return ESP_OK;
}

esp_err_t smp_storage_delete(const char *key) {
    if (!storage.nvs_ready) return ESP_ERR_INVALID_STATE;
    if (!key) return ESP_ERR_INVALID_ARG;

    return nvs_proxy_submit(NVS_OP_DELETE, key, NULL, 0);
}

bool smp_storage_exists(const char *key) {
    if (!storage.nvs_ready || !key) return false;

    /* Reads are safe from any task */
    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(storage.nvs_handle, key, NULL, &required_size);
    return (ret == ESP_OK && required_size > 0);
}

// ============== Write-Before-Send (Evgeny's Pattern) ==============

esp_err_t smp_storage_save_blob_sync(const char *key, const void *data, size_t len) {
    if (!storage.nvs_ready) return ESP_ERR_INVALID_STATE;
    if (!key || !data || len == 0) return ESP_ERR_INVALID_ARG;
    if (len > SMP_STORAGE_MAX_BLOB_SIZE) return ESP_ERR_INVALID_SIZE;

    return nvs_proxy_submit(NVS_OP_SAVE_SYNC, key, data, len);
}

// ============== SD Card Backend ==============

esp_err_t smp_storage_sd_write(const char *path, const void *data, size_t len) {
    if (!storage.sd_mounted) {
        ESP_LOGW(TAG, "SD write skipped — no SD card");
        return ESP_ERR_NOT_FOUND;
    }
    if (!path || !data || len == 0) return ESP_ERR_INVALID_ARG;

    char dir[128];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_p(dir);
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "SD fopen failed: %s (errno=%d)", path, errno);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    if (written != len) {
        ESP_LOGE(TAG, "SD write incomplete: %zu/%zu bytes for %s", written, len, path);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "SD write: %s (%zu bytes)", path, len);
    return ESP_OK;
}

esp_err_t smp_storage_sd_read(const char *path, void *buf, size_t buf_len, size_t *out_len) {
    if (!storage.sd_mounted) return ESP_ERR_NOT_FOUND;
    if (!path || !buf || buf_len == 0) return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t bytes_read = fread(buf, 1, buf_len, f);
    fclose(f);

    if (out_len) {
        *out_len = bytes_read;
    }

    ESP_LOGD(TAG, "SD read: %s (%zu bytes)", path, bytes_read);
    return ESP_OK;
}

esp_err_t smp_storage_sd_delete(const char *path) {
    if (!storage.sd_mounted) return ESP_ERR_NOT_FOUND;
    if (!path) return ESP_ERR_INVALID_ARG;

    if (remove(path) != 0) {
        if (errno == ENOENT) return ESP_ERR_NOT_FOUND;
        ESP_LOGE(TAG, "SD delete failed: %s (errno=%d)", path, errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "SD delete: %s", path);
    return ESP_OK;
}

bool smp_storage_sd_file_exists(const char *path) {
    if (!storage.sd_mounted || !path) return false;

    struct stat st;
    return (stat(path, &st) == 0);
}

// ============== Diagnostics ==============

void smp_storage_print_info(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "|     SimpleGo Storage Diagnostics         |");
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "");

    ESP_LOGI(TAG, "--- NVS ---");
    ESP_LOGI(TAG, "  Namespace: '%s'", SMP_STORAGE_NVS_NAMESPACE);
    ESP_LOGI(TAG, "  Ready: %s", storage.nvs_ready ? "YES" : "NO");

    if (storage.nvs_ready) {
        nvs_stats_t nvs_stats;
        if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
            ESP_LOGI(TAG, "  Entries: used=%zu, free=%zu, total=%zu, ns_count=%zu",
                     nvs_stats.used_entries, nvs_stats.free_entries,
                     nvs_stats.total_entries, nvs_stats.namespace_count);
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- SD Card ---");
    ESP_LOGI(TAG, "  Mounted: %s", storage.sd_mounted ? "YES" : "NO");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Struct Sizes ---");
    ESP_LOGI(TAG, "  ratchet_state_t:   %4zu bytes", sizeof(ratchet_state_t));
    ESP_LOGI(TAG, "  Skipped Key Entry: %4zu bytes (est: 32+4+32 = 68)",
             (size_t)68);

    size_t ratchet_size = sizeof(ratchet_state_t);
    size_t queue_cred_est = 300;
    size_t per_contact = ratchet_size + queue_cred_est + (50 * 68);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Capacity Estimates ---");
    ESP_LOGI(TAG, "  Per contact: ~%zu bytes", per_contact);
    ESP_LOGI(TAG, "    Ratchet State: %zu", ratchet_size);
    ESP_LOGI(TAG, "    Queue Creds:   ~%zu (est)", queue_cred_est);
    ESP_LOGI(TAG, "    Skipped Keys:  ~%zu (50 avg x 68B)", (size_t)(50 * 68));
    ESP_LOGI(TAG, "  128KB NVS fits:  ~%zu contacts", (128 * 1024) / per_contact);
    ESP_LOGI(TAG, "");
}

// ============== Self-Test ==============

esp_err_t smp_storage_self_test(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "|     SimpleGo Storage Self-Test           |");
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "");

    esp_err_t result = ESP_OK;
    const size_t TEST_SIZE = 256;
    uint8_t test_data[256];
    uint8_t read_buf[256];
    size_t read_len = 0;

    esp_fill_random(test_data, TEST_SIZE);

    // ==========================================
    // Test A: NVS Basic Roundtrip
    // ==========================================
    ESP_LOGI(TAG, "--- Test A: NVS Basic Roundtrip ---");
    {
        const char *test_key = "test_rt";

        esp_err_t ret = smp_storage_save_blob(test_key, test_data, TEST_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: save_blob returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_b;
        }

        if (!smp_storage_exists(test_key)) {
            ESP_LOGE(TAG, "  FAIL: exists() returned false after save");
            result = ESP_FAIL;
            goto test_b;
        }

        memset(read_buf, 0, TEST_SIZE);
        ret = smp_storage_load_blob(test_key, read_buf, TEST_SIZE, &read_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: load_blob returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_b;
        }

        if (read_len != TEST_SIZE || memcmp(test_data, read_buf, TEST_SIZE) != 0) {
            ESP_LOGE(TAG, "  FAIL: data mismatch! wrote %zu, read %zu", TEST_SIZE, read_len);
            result = ESP_FAIL;
            goto test_b;
        }

        ret = smp_storage_delete(test_key);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: delete returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_b;
        }

        if (smp_storage_exists(test_key)) {
            ESP_LOGE(TAG, "  FAIL: exists() returned true after delete");
            result = ESP_FAIL;
            goto test_b;
        }

        ESP_LOGI(TAG, "  PASSED: NVS roundtrip OK (%zu bytes)", TEST_SIZE);
    }

test_b:
    // ==========================================
    // Test B: NVS Write-Before-Send + Timing
    // ==========================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Test B: Write-Before-Send (Sync) + Timing ---");
    {
        const char *test_key = "test_wbs";

        int64_t t_start = esp_timer_get_time();

        esp_err_t ret = smp_storage_save_blob_sync(test_key, test_data, TEST_SIZE);

        int64_t t_end = esp_timer_get_time();
        int64_t elapsed = t_end - t_start;

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: save_blob_sync returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_c;
        }

        memset(read_buf, 0, TEST_SIZE);
        ret = smp_storage_load_blob(test_key, read_buf, TEST_SIZE, &read_len);
        if (ret != ESP_OK || read_len != TEST_SIZE || memcmp(test_data, read_buf, TEST_SIZE) != 0) {
            ESP_LOGE(TAG, "  FAIL: immediate read-back mismatch");
            result = ESP_FAIL;
            goto test_c;
        }

        smp_storage_delete(test_key);

        ESP_LOGI(TAG, "  PASSED: Sync write+verify in %lld us (%.1f ms)",
                 elapsed, elapsed / 1000.0);

        if (elapsed < 5000) {
            ESP_LOGI(TAG, "     Excellent: <5ms");
        } else if (elapsed < 20000) {
            ESP_LOGI(TAG, "     Good: <20ms — acceptable for Write-Before-Send");
        } else {
            ESP_LOGW(TAG, "     Slow: %lldms — may impact real-time feel", elapsed / 1000);
        }
    }

test_c:
    // ==========================================
    // Test C: SD Card (if available)
    // ==========================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Test C: SD Card Roundtrip ---");
    {
        if (!storage.sd_mounted) {
            ESP_LOGW(TAG, "  SKIPPED: SD card not available");
            goto test_done;
        }

        const char *test_path = SMP_STORAGE_SD_MSG_DIR "/test_file.bin";

        esp_err_t ret = smp_storage_sd_write(test_path, test_data, TEST_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: sd_write returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_done;
        }

        if (!smp_storage_sd_file_exists(test_path)) {
            ESP_LOGE(TAG, "  FAIL: sd_file_exists returned false after write");
            result = ESP_FAIL;
            goto test_done;
        }

        memset(read_buf, 0, TEST_SIZE);
        ret = smp_storage_sd_read(test_path, read_buf, TEST_SIZE, &read_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: sd_read returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_done;
        }

        if (read_len != TEST_SIZE || memcmp(test_data, read_buf, TEST_SIZE) != 0) {
            ESP_LOGE(TAG, "  FAIL: SD data mismatch! wrote %zu, read %zu", TEST_SIZE, read_len);
            result = ESP_FAIL;
            goto test_done;
        }

        ret = smp_storage_sd_delete(test_path);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: sd_delete returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_done;
        }

        if (smp_storage_sd_file_exists(test_path)) {
            ESP_LOGE(TAG, "  FAIL: sd_file_exists returned true after delete");
            result = ESP_FAIL;
            goto test_done;
        }

        ESP_LOGI(TAG, "  PASSED: SD roundtrip OK (%zu bytes)", TEST_SIZE);
    }

test_done:
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+==========================================+");
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "|  ALL STORAGE TESTS PASSED                |");
    } else {
        ESP_LOGE(TAG, "|  SOME TESTS FAILED                       |");
    }
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "");

    return result;
}
