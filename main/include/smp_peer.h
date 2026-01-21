/**
 * SimpleGo - smp_peer.h
 * Peer server connection for AgentConfirmation
 */

#ifndef SMP_PEER_H
#define SMP_PEER_H

#include <stdbool.h>
#include "smp_types.h"

// Connect to peer's SMP server
bool peer_connect(const char *host, int port);

// Disconnect from peer
void peer_disconnect(void);

// Send AgentConfirmation to peer
bool send_agent_confirmation(contact_t *contact);

#endif // SMP_PEER_H
