/**
 * SimpleGo - smp_tasks.h
 * FreeRTOS task management for multi-task architecture
 */
#ifndef SMP_TASKS_H
#define SMP_TASKS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "mbedtls/ssl.h"

// Task stack sizes (all allocated in PSRAM)
#define NETWORK_TASK_STACK  (12 * 1024)
// App logic runs on main task (64KB Internal SRAM stack, needed for NVS writes)
#define UI_TASK_STACK       (8 * 1024)

// Task priorities (higher = more important)
#define NETWORK_TASK_PRIO   7
// App logic: main task priority (not configurable here)
#define UI_TASK_PRIO        5

// Ring buffer sizes
// Phase 3: net->app carries full SMP blocks (SMP_BLOCK_SIZE = 16384)
// NOSPLIT ring buffers have significant internal overhead (headers, alignment, free-block)
// 2x block size gives room for at least 1 frame plus management overhead
#define NET_TO_APP_BUF_SIZE (2 * 16384 + 4096)
#define APP_TO_NET_BUF_SIZE 1024

// Task handles (extern for status queries)
extern TaskHandle_t network_task_handle;
extern TaskHandle_t ui_task_handle;

// Ring buffer handles
extern RingbufHandle_t net_to_app_buf;
extern RingbufHandle_t app_to_net_buf;

/**
 * Initialize task infrastructure (queues, ring buffers, frame pool).
 * Call BEFORE smp_tasks_start().
 *
 * @return 0 on success, -1 on error
 */
int smp_tasks_init(void);

/**
 * Start all tasks after successful smp_connect().
 * Network task on Core 0, App + UI on Core 1.
 *
 * @param ssl_context  Active SSL context from smp_connect()
 * @param session_id   32-byte TLS session ID for ACK signing
 * @return 0 on success, -1 on error
 */
int smp_tasks_start(mbedtls_ssl_context *ssl_context, const uint8_t *session_id);

/**
 * Graceful shutdown of all tasks.
 */
void smp_tasks_stop(void);

/**
 * Run app logic (ring buffer read, parse, decrypt, ACK).
 * MUST be called from main task (needs Internal SRAM stack for NVS writes).
 * This function blocks (infinite loop) until connection error.
 */
void smp_app_run(void);

#endif // SMP_TASKS_H
