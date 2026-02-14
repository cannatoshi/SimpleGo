/**
 * @file smp_frame_pool.h
 * @brief Static memory pool for SMP frame buffers
 *
 * Provides a fixed pool of 8 frame buffers (4096 bytes each)
 * to prevent heap fragmentation during SMP frame processing.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#ifndef SMP_FRAME_POOL_H
#define SMP_FRAME_POOL_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_POOL_COUNT    8
#define FRAME_POOL_BUF_SIZE 4096

/**
 * @brief Initialize the frame pool
 *
 * Creates the internal free-list queue and populates it
 * with pointers to static frame buffers. Must be called
 * once at startup before any alloc/free calls.
 */
void frame_pool_init(void);

/**
 * @brief Allocate a frame buffer from the pool
 *
 * @param timeout Maximum time to wait if no buffer available
 * @return Pointer to 4096-byte buffer, or NULL on timeout
 */
uint8_t *frame_pool_alloc(TickType_t timeout);

/**
 * @brief Return a frame buffer to the pool
 *
 * Validates that the pointer belongs to the pool before returning it.
 *
 * @param buf Pointer previously obtained from frame_pool_alloc()
 */
void frame_pool_free(uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* SMP_FRAME_POOL_H */
