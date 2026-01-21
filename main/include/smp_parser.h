/**
 * SimpleGo - smp_parser.h
 * Message parsing for Agent Protocol
 */

#ifndef SMP_PARSER_H
#define SMP_PARSER_H

#include <stdbool.h>
#include "smp_types.h"

// Parse agent message (after SMP decryption)
void parse_agent_message(contact_t *contact, const uint8_t *plain, int plain_len);

// Message type constants
#define AGENT_MSG_CONFIRMATION  'C'
#define AGENT_MSG_INVITATION    'I'
#define AGENT_MSG_ENVELOPE      'M'
#define AGENT_MSG_RATCHET_KEY   'R'

#endif // SMP_PARSER_H
