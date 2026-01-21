/**
 * SimpleGo - smp_parser.c
 * Message parsing for Agent Protocol
 */

#include "smp_parser.h"
#include "smp_types.h"
#include "smp_utils.h"
#include "smp_crypto.h"
#include "smp_peer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "SMP_PARS";

// ============== Agent Message Parsing ==============

void parse_agent_message(contact_t *contact, const uint8_t *plain, int plain_len) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   🔬 AGENT MESSAGE ANALYSIS:");
    
    // Step 1: Check for length prefix
    const uint8_t *content = plain;
    int content_len = plain_len;
    
    if (plain_len >= 2) {
        uint16_t prefix_len = (plain[0] << 8) | plain[1];
        if (prefix_len > 0 && prefix_len < plain_len - 2 && prefix_len < 16100) {
            ESP_LOGI(TAG, "      📏 Length prefix: %d bytes (total: %d)", prefix_len, plain_len);
            content = plain + 2;
            content_len = prefix_len;
        }
    }
    
    // Show raw structure
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      📊 Raw message structure (first 64 bytes):");
    printf("         ");
    for (int i = 0; i < 64 && i < content_len; i++) {
        printf("%02x ", content[i]);
        if ((i + 1) % 16 == 0) printf("\n         ");
    }
    printf("\n");
    
    // Step 2: Scan for X25519 SPKI header
    int sender_key_offset = -1;
    uint8_t sender_key_raw[32];
    
    for (int i = 0; i < content_len - 44; i++) {
        if (content[i] == 0x30 && content[i+1] == 0x2a && content[i+2] == 0x30 &&
            content[i+3] == 0x05 && content[i+4] == 0x06 && content[i+5] == 0x03 &&
            content[i+6] == 0x2b && content[i+7] == 0x65 && content[i+8] == 0x6e &&
            content[i+9] == 0x03 && content[i+10] == 0x21 && content[i+11] == 0x00) {
            
            sender_key_offset = i;
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "      🔑 Found X25519 SPKI at offset %d!", i);
            dump_hex("SPKI key", &content[i], 44, 44);
            
            memcpy(sender_key_raw, &content[i + 12], 32);
            dump_hex("Raw key", sender_key_raw, 32, 32);
            break;
        }
    }
    
    if (sender_key_offset < 0) {
        ESP_LOGW(TAG, "      ⚠️ No X25519 SPKI found in message!");
        return;
    }
    
    // Step 3: Analyze data after SPKI key
    int after_key_offset = sender_key_offset + 44;
    int after_key_len = content_len - after_key_offset;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      📦 Data after SPKI key (%d bytes):", after_key_len);
    if (after_key_len > 0) {
        dump_hex("After key", &content[after_key_offset], after_key_len, 64);
    }
    
    // Step 4: Try DH decryption
    if (after_key_len > 40) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      🔐 Attempting DH decryption on post-key data...");
        
        uint8_t *decrypted = malloc(after_key_len);
        if (decrypted) {
            int dec_len = decrypt_client_msg(&content[after_key_offset], after_key_len,
                                             sender_key_raw,
                                             contact->rcv_dh_secret,
                                             decrypted);
            
            if (dec_len > 0) {
                ESP_LOGI(TAG, "      ✅ DH Decryption SUCCESS! (%d bytes)", dec_len);
                dump_hex("Decrypted", decrypted, dec_len, 100);
                
                if (dec_len >= 6) {
                    // Find '_' delimiter
                    int toff = -1;
                    for (int i = 0; i < 10 && i < dec_len - 3; i++) {
                        if (decrypted[i] == '_') { toff = i; break; }
                    }
                    if (toff < 0) toff = 2;
                    
                    uint16_t ver = (decrypted[toff + 1] << 8) | decrypted[toff + 2];
                    char type = decrypted[toff + 3];
                    
                    ESP_LOGI(TAG, "");
                    ESP_LOGI(TAG, "      📦 Agent Version: %d", ver);
                    ESP_LOGI(TAG, "      📬 Message Type: '%c' (0x%02x)", 
                             (type >= 32 && type < 127) ? type : '?', type);
                    
                    if (type == 'C') {
                        ESP_LOGI(TAG, "      🎉 CONFIRMATION received!");
                    }
                    else if (type == 'I') {
                        ESP_LOGI(TAG, "      🎉 INVITATION received!");
                        
                        // Parse invitation URI
                        for (int j = toff + 4; j < dec_len - 10; j++) {
                            if ((decrypted[j] == 's' && decrypted[j+1] == 'm' && 
                                 decrypted[j+2] == 'p' && (decrypted[j+3] == '=' || decrypted[j+3] == '%'))) {
                                
                                int uri_start = j + 4;
                                if (decrypted[j+3] == '%') uri_start = j;
                                
                                int uri_end = uri_start;
                                while (uri_end < dec_len && decrypted[uri_end] != '&' && 
                                       decrypted[uri_end] > 32 && decrypted[uri_end] < 127 &&
                                       uri_end - uri_start < 600) uri_end++;
                                
                                int uri_len = uri_end - uri_start;
                                if (uri_len > 50 && uri_len < 600) {
                                    char *uri = malloc(uri_len + 1);
                                    if (uri) {
                                        memcpy(uri, &decrypted[uri_start], uri_len);
                                        uri[uri_len] = 0;
                                        
                                        // Decode multiple times
                                        for (int pass = 0; pass < 3; pass++) {
                                            if (!strchr(uri, '%')) break;
                                            url_decode_inplace(uri);
                                        }
                                        
                                        ESP_LOGI(TAG, "      📋 Peer SMP URI: %.80s...", uri);
                                        
                                        // Parse smp://keyHash@host:port/queueId
                                        char *at = strchr(uri, '@');
                                        char *slash = at ? strchr(at, '/') : NULL;
                                        char *hash = slash ? strchr(slash, '#') : NULL;
                                        
                                        if (at && slash) {
                                            int hostlen = slash - at - 1;
                                            if (hostlen > 0 && hostlen < 63) {
                                                memcpy(pending_peer.host, at + 1, hostlen);
                                                pending_peer.host[hostlen] = 0;
                                                
                                                char *colon = strchr(pending_peer.host, ':');
                                                if (colon) {
                                                    pending_peer.port = atoi(colon + 1);
                                                    *colon = 0;
                                                } else {
                                                    pending_peer.port = 5223;
                                                }
                                                
                                                ESP_LOGI(TAG, "      🖥️  Peer Server: %s:%d", 
                                                         pending_peer.host, pending_peer.port);
                                            }
                                            
                                            // Extract queue ID
                                            char *qend = hash ? hash : (slash + strlen(slash));
                                            int qlen = qend - slash - 1;
                                            if (qlen > 0 && qlen < 48) {
                                                char qid_b64[48] = {0};
                                                memcpy(qid_b64, slash + 1, qlen);
                                                pending_peer.queue_id_len = base64url_decode(
                                                    qid_b64, pending_peer.queue_id, 32);
                                                ESP_LOGI(TAG, "      📮 Queue ID: %s (%d bytes)", 
                                                         qid_b64, pending_peer.queue_id_len);
                                            }
                                        }
                                        
                                        // Find dh= parameter
                                        char *dh = strstr(uri, "dh=");
                                        if (dh) {
                                            ESP_LOGI(TAG, "      🔍 Found dh= in URI at offset %d", (int)(dh - uri));
                                            dh += 3;
                                            char *dh_end = dh;
                                            while (*dh_end && *dh_end != '&' && *dh_end != ' ') dh_end++;
                                            int dh_len = dh_end - dh;
                                            ESP_LOGI(TAG, "      🔍 DH value length: %d", dh_len);
                                            
                                            if (dh_len > 40 && dh_len < 120) {
                                                char dh_b64[100] = {0};
                                                memcpy(dh_b64, dh, dh_len);
                                                
                                                url_decode_inplace(dh_b64);
                                                url_decode_inplace(dh_b64);
                                                
                                                // Clean Base64 for decoding
                                                char dh_clean[100];
                                                strncpy(dh_clean, dh_b64, sizeof(dh_clean) - 1);
                                                dh_clean[sizeof(dh_clean) - 1] = 0;
                                                
                                                int clean_len = strlen(dh_clean);
                                                while (clean_len > 0 && dh_clean[clean_len - 1] == '=') {
                                                    dh_clean[--clean_len] = '\0';
                                                }
                                                for (int x = 0; x < clean_len; x++) {
                                                    if (dh_clean[x] == '+') dh_clean[x] = '-';
                                                    if (dh_clean[x] == '/') dh_clean[x] = '_';
                                                }
                                                
                                                ESP_LOGI(TAG, "      🔍 DH clean (%d chars): %s", clean_len, dh_clean);
                                                
                                                uint8_t spki[48];
                                                int spki_len = base64url_decode(dh_clean, spki, 48);
                                                
                                                if (spki_len >= 44) {
                                                    memcpy(pending_peer.dh_public, spki + 12, 32);
                                                    pending_peer.has_dh = 1;
                                                    ESP_LOGI(TAG, "      🔑 DH Key: %02x%02x%02x%02x... ✅",
                                                             pending_peer.dh_public[0], pending_peer.dh_public[1],
                                                             pending_peer.dh_public[2], pending_peer.dh_public[3]);
                                                }
                                            }
                                        }
                                        
                                        pending_peer.valid = (strlen(pending_peer.host) > 0 && 
                                                              pending_peer.queue_id_len > 0);
                                        
                                        if (pending_peer.valid && pending_peer.has_dh) {
                                            ESP_LOGI(TAG, "");
                                            ESP_LOGI(TAG, "      ╔══════════════════════════════════════╗");
                                            ESP_LOGI(TAG, "      ║  🎯 READY TO SEND CONFIRMATION!     ║");
                                            ESP_LOGI(TAG, "      ╚══════════════════════════════════════╝");
                                            
                                            // AUTO-CONNECT
                                            ESP_LOGI(TAG, "");
                                            ESP_LOGI(TAG, "      🚀 Auto-connecting to peer...");
                                            
                                            if (peer_connect(pending_peer.host, pending_peer.port)) {
                                                send_agent_confirmation(contact);
                                                peer_disconnect();
                                            }
                                        }
                                        
                                        free(uri);
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                
                // Show as text
                printf("         Text: \"");
                for (int i = 0; i < dec_len && i < 150; i++) {
                    char c = decrypted[i];
                    if (c >= 32 && c < 127) printf("%c", c);
                }
                printf("\"\n");
            }
            free(decrypted);
        }
    }
    
    // Data before SPKI key
    if (sender_key_offset > 0) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      📋 Data BEFORE SPKI key (%d bytes):", sender_key_offset);
        dump_hex("Before key", content, sender_key_offset, 32);
        
        printf("         Text: \"");
        for (int i = 0; i < sender_key_offset; i++) {
            char c = content[i];
            if (c >= 32 && c < 127) printf("%c", c);
            else printf(".");
        }
        printf("\"\n");
        
        for (int i = 0; i < sender_key_offset - 1; i++) {
            if (content[i] >= '1' && content[i] <= '9' && content[i+1] == ',') {
                ESP_LOGI(TAG, "      📌 Version '%c' found at offset %d", content[i], i);
            }
        }
    }
    
    ESP_LOGI(TAG, "");
}
