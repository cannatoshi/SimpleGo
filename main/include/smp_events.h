/**
 * SimpleGo - smp_events.h
 * Event types for inter-task communication
 */
#ifndef SMP_EVENTS_H
#define SMP_EVENTS_H

#include <stdint.h>
#include <stddef.h>

// Event types for task communication
typedef enum {
    SMP_EVENT_FRAME_RECEIVED,   // Network → App: raw frame from server
    SMP_EVENT_FRAME_SEND,       // App → Network: frame to send to server
    SMP_EVENT_MSG_DECODED,      // App → UI: decrypted message ready
    SMP_EVENT_ACK_READY,        // App → Network: ACK/receipt to send
    SMP_EVENT_RECONNECT,        // Any → Network: trigger reconnection
    SMP_EVENT_SHUTDOWN          // Any → All: graceful shutdown
} smp_event_type_t;

// Frame source identifier
typedef enum {
    SMP_SOURCE_NETWORK,
    SMP_SOURCE_APP,
    SMP_SOURCE_UI
} smp_source_t;

// Frame container (4KB data buffer)
typedef struct {
    uint8_t data[4096];
    size_t len;
    smp_source_t source;
} smp_frame_t;

// Event container for inter-task queues
typedef struct {
    smp_event_type_t event_type;
    void *payload;
    size_t payload_len;
    uint32_t timestamp;
} smp_event_t;

#endif // SMP_EVENTS_H
