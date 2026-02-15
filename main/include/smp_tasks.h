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

// Task stack sizes
#define NETWORK_TASK_STACK  (12 * 1024)
#define APP_TASK_STACK      (16 * 1024)
#define UI_TASK_STACK       (8 * 1024)

// Task priorities (higher = more important)
#define NETWORK_TASK_PRIO   7
#define APP_TASK_PRIO       6
#define UI_TASK_PRIO        5

// Ring buffer sizes (transport event structs, not frames)
#define NET_TO_APP_BUF_SIZE 2048
#define APP_TO_NET_BUF_SIZE 1024

// Task handles (extern for status queries)
extern TaskHandle_t network_task_handle;
extern TaskHandle_t app_task_handle;
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
 * @return 0 on success, -1 on error
 */
int smp_tasks_start(mbedtls_ssl_context *ssl_context);

/**
 * Graceful shutdown of all tasks.
 */
void smp_tasks_stop(void);

#endif // SMP_TASKS_H
