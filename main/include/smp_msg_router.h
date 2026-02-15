/**
 * @file smp_msg_router.h
 * @brief Central message router for the App Task
 *
 * Phase 3: Added msg_router_process_frame() for processing
 * individual frames received from the Network Task.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#ifndef SMP_MSG_ROUTER_H
#define SMP_MSG_ROUTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the message router
 */
void msg_router_init(void);

/**
 * @brief Run the message router event loop (Phase 4)
 */
void msg_router_run(void);

/**
 * @brief Process a single SMP frame from the Network Task
 *
 * Handles transport parsing, contact/queue identification,
 * decrypt pipeline, and ACK building.
 *
 * @param frame_data  Heap-allocated frame (includes 2-byte header)
 * @param content_len Content length (data starts at frame_data+2)
 */
void msg_router_process_frame(uint8_t *frame_data, int content_len);

#ifdef __cplusplus
}
#endif

#endif /* SMP_MSG_ROUTER_H */
