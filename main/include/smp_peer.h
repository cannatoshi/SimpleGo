/**
 * SimpleGo - smp_peer.h
 * Peer server connection for AgentConfirmation
 */

#ifndef SMP_PEER_H
#define SMP_PEER_H

#include <stdbool.h>
#include <stdint.h>
#include "smp_types.h"

// Connect to peer's SMP server
bool peer_connect(const char *host, int port);

// Disconnect from peer
void peer_disconnect(void);

// Send AgentConfirmation to peer
bool send_agent_confirmation(contact_t *contact);

// Auftrag 44a: Send chat message to peer
bool peer_send_chat_message(contact_t *contact, const char *message);

// Auftrag 49b: Send delivery receipt to peer (enables ✓✓)
bool peer_send_receipt(contact_t *contact, uint64_t peer_snd_msg_id, const uint8_t *msg_hash);

#endif // SMP_PEER_H