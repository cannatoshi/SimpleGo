/**
 * @file smp_tasks.h
 * @brief FreeRTOS task manager for SimpleGo
 *
 * Phase 3: Network Task owns TLS, App Task processes frames.
 * Inter-task communication via ring buffers (pointer refs)
 * and FreeRTOS queues.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#ifndef SMP_TASKS_H
#define SMP_TASKS_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Frame Reference (passed through ring buffer — pointer + length)
 * ======================================================================== */

/**
 * @brief Reference to a network frame (passed via net_to_app ring buffer)
 *
 * Network Task allocates data via malloc, sends this struct through
 * ring buffer. App Task processes the frame and frees data.
 */
typedef struct {
    uint8_t *data;      /**< Heap-allocated frame data (block+2 content) */
    int content_len;    /**< Content length */
} frame_ref_t;

/* ========================================================================
 * Shared State
 * ======================================================================== */

/** SMP session ID (32 bytes) — set by smp_net_init(), read by App Task */
extern uint8_t net_session_id[32];

/** Flag: App Task requests Network Task to re-subscribe all contacts */
extern volatile bool need_resubscribe;

/* ========================================================================
 * Task Handles
 * ======================================================================== */

extern TaskHandle_t network_task_handle;
extern TaskHandle_t app_task_handle;
extern TaskHandle_t ui_task_handle;

/* Ring buffers (Network <-> App) */
extern RingbufHandle_t net_to_app_rb;
extern RingbufHandle_t app_to_net_rb;

/* Queues (App <-> UI) */
extern QueueHandle_t app_to_ui_queue;
extern QueueHandle_t ui_to_app_queue;

/* Keyboard message queue (created in main.c, consumed by App Task) */
extern QueueHandle_t kbd_msg_queue;

/* ========================================================================
 * Public API
 * ======================================================================== */

/**
 * @brief Initialize queues and ring buffers
 * Must be called early (before tasks start).
 */
esp_err_t smp_tasks_init(void);

/**
 * @brief Initialize network connection (TLS + SMP handshake)
 *
 * Runs synchronously in the calling thread. Sets up TLS,
 * performs SMP handshake, loads contacts, subscribes to queues.
 * After this returns, the Network Task can enter its receive loop.
 *
 * @param host SMP server hostname
 * @param port SMP server port
 * @param restored true if session was restored from NVS
 * @return ESP_OK on success
 */
esp_err_t smp_net_init(const char *host, int port, bool restored);

/**
 * @brief Start all FreeRTOS tasks
 *
 * Creates Network Task (Core 0), App Task (Core 1),
 * and UI Task (Core 1). Call after smp_net_init().
 */
esp_err_t smp_tasks_start(void);

#ifdef __cplusplus
}
#endif

#endif /* SMP_TASKS_H */
