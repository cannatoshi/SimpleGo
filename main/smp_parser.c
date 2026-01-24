/**
 * SimpleGo - smp_parser.c
 * Message parsing for Agent Protocol
 * v0.1.15-alpha - Full URI parsing with e2e= parameter and X448 keys
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

// ============== Helper: Find simplex:/invitation in decrypted data ==============

static char* extract_full_invitation_uri(const uint8_t *data, int data_len, int *out_len) {
    for (int i = 0; i < data_len - 20; i++) {
        if (data[i] == 's' && data[i+1] == 'i' && data[i+2] == 'm' && 
            data[i+3] == 'p' && data[i+4] == 'l' && data[i+5] == 'e' &&
            data[i+6] == 'x' && data[i+7] == ':') {
            
            int start = i;
            int end = i;
            
            while (end < data_len && data[end] >= 32 && data[end] < 127) {
                end++;
            }
            
            int uri_len = end - start;
            if (uri_len > 50 && uri_len < 15000) {
                char *uri = malloc(uri_len + 1);
                if (uri) {
                    memcpy(uri, &data[start], uri_len);
                    uri[uri_len] = 0;
                    *out_len = uri_len;
                    return uri;
                }
            }
        }
    }
    return NULL;
}

// ============== Helper: Parse e2e parameter ==============

static void parse_e2e_params(const char *uri) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      🔐 PARSING E2E RATCHET PARAMETERS...");
    
    // Find e2e version (e2e=v=2-3 or after decode)
    const char *e2e_v = strstr(uri, "e2e=v=");
    if (!e2e_v) {
        e2e_v = strstr(uri, "&e2e=v");
    }
    
    if (e2e_v) {
        const char *v_start = strstr(e2e_v, "v=");
        if (v_start) {
            v_start += 2;
            char version[20] = {0};
            int vi = 0;
            while (*v_start && *v_start != '&' && *v_start != ' ' && vi < 19) {
                version[vi++] = *v_start++;
            }
            ESP_LOGI(TAG, "      E2E Version: %s", version);
        }
    }
    
    // Find x3dh= directly in the (decoded) URI
    const char *x3dh = strstr(uri, "x3dh=");
    if (!x3dh) {
        ESP_LOGW(TAG, "      ⚠️ No x3dh= parameter found!");
        return;
    }
    
    x3dh += 5;  // skip "x3dh="
    
    ESP_LOGI(TAG, "      🔑 X3DH Keys found!");
    
    // Find comma separator between two keys
    const char *comma = strchr(x3dh, ',');
    if (!comma) {
        ESP_LOGW(TAG, "      ⚠️ No comma found in x3dh - expected two keys!");
        return;
    }
    
    // First key (before comma)
    int key1_len = comma - x3dh;
    char key1_b64[150] = {0};
    if (key1_len > 0 && key1_len < (int)sizeof(key1_b64)) {
        memcpy(key1_b64, x3dh, key1_len);
        ESP_LOGI(TAG, "      Key1 B64 (%d chars): %.60s...", key1_len, key1_b64);
        
        // Clean up the base64 string - remove any trailing whitespace
        int clean_len = strlen(key1_b64);
        while (clean_len > 0 && (key1_b64[clean_len-1] == ' ' || key1_b64[clean_len-1] == '\n')) {
            key1_b64[--clean_len] = '\0';
        }
        
        uint8_t key1_spki[100];
        int key1_decoded = base64url_decode(key1_b64, key1_spki, sizeof(key1_spki));
        ESP_LOGI(TAG, "      Key1 decode result: %d bytes", key1_decoded);
        if (key1_decoded > 0) {
            ESP_LOGI(TAG, "      Key1 decoded: %d bytes", key1_decoded);
            
            printf("         Key1 SPKI header: ");
            for (int i = 0; i < 12 && i < key1_decoded; i++) {
                printf("%02x ", key1_spki[i]);
            }
            printf("\n");
            
            // Verify X448 OID (1.3.101.111 = 2b 65 6f)
            if (key1_decoded >= 68 && 
                key1_spki[0] == 0x30 && key1_spki[2] == 0x30 &&
                key1_spki[4] == 0x06 && key1_spki[5] == 0x03 &&
                key1_spki[6] == 0x2b && key1_spki[7] == 0x65 && key1_spki[8] == 0x6f) {
                
                ESP_LOGI(TAG, "      ✅ Key1 is X448 (OID 1.3.101.111)!");
                memcpy(pending_peer.e2e_key1, key1_spki + 12, 56);
                pending_peer.has_e2e = 1;
                
                printf("         Key1 raw (first 16): ");
                for (int i = 0; i < 16; i++) {
                    printf("%02x ", pending_peer.e2e_key1[i]);
                }
                printf("...\n");
            } else {
                ESP_LOGW(TAG, "      ⚠️ Key1 format unexpected (not X448 SPKI?)");
            }
        } else {
            ESP_LOGW(TAG, "      ❌ Key1 decode FAILED! Check base64url_decode");
            // Show first and last chars of the base64 for debugging
            ESP_LOGW(TAG, "         First 20 chars: %.20s", key1_b64);
            ESP_LOGW(TAG, "         Last 10 chars: %s", key1_b64 + (key1_len > 10 ? key1_len - 10 : 0));
        }
    }
    
    // Second key (after comma, until & or end)
    const char *key2_start = comma + 1;
    const char *key2_end = key2_start;
    while (*key2_end && *key2_end != '&' && *key2_end != ' ') key2_end++;
    
    int key2_len = key2_end - key2_start;
    char key2_b64[150] = {0};
    if (key2_len > 0 && key2_len < (int)sizeof(key2_b64)) {
        memcpy(key2_b64, key2_start, key2_len);
        ESP_LOGI(TAG, "      Key2 B64 (%d chars): %.60s...", key2_len, key2_b64);
        
        // Clean up
        int clean_len = strlen(key2_b64);
        while (clean_len > 0 && (key2_b64[clean_len-1] == ' ' || key2_b64[clean_len-1] == '\n')) {
            key2_b64[--clean_len] = '\0';
        }
        
        uint8_t key2_spki[100];
        int key2_decoded = base64url_decode(key2_b64, key2_spki, sizeof(key2_spki));
        ESP_LOGI(TAG, "      Key2 decode result: %d bytes", key2_decoded);
        if (key2_decoded > 0) {
            ESP_LOGI(TAG, "      Key2 decoded: %d bytes", key2_decoded);
            
            printf("         Key2 SPKI header: ");
            for (int i = 0; i < 12 && i < key2_decoded; i++) {
                printf("%02x ", key2_spki[i]);
            }
            printf("\n");
            
            // Verify X448 OID
            if (key2_decoded >= 68 && 
                key2_spki[0] == 0x30 && key2_spki[2] == 0x30 &&
                key2_spki[4] == 0x06 && key2_spki[5] == 0x03 &&
                key2_spki[6] == 0x2b && key2_spki[7] == 0x65 && key2_spki[8] == 0x6f) {
                
                ESP_LOGI(TAG, "      ✅ Key2 is X448 (OID 1.3.101.111)!");
                memcpy(pending_peer.e2e_key2, key2_spki + 12, 56);
                
                printf("         Key2 raw (first 16): ");
                for (int i = 0; i < 16; i++) {
                    printf("%02x ", pending_peer.e2e_key2[i]);
                }
                printf("...\n");
            } else {
                ESP_LOGW(TAG, "      ⚠️ Key2 format unexpected");
            }
        } else {
            ESP_LOGW(TAG, "      ❌ Key2 decode FAILED!");
        }
    }
    
    // Check for kem_key (PQ encryption)
    if (strstr(uri, "kem_key=")) {
        ESP_LOGI(TAG, "      🔒 KEM key found (Post-Quantum encryption!)");
    }
    
    if (pending_peer.has_e2e) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      ✅ E2E KEYS EXTRACTED SUCCESSFULLY!");
        ESP_LOGI(TAG, "      ⚠️  NOTE: X448 crypto not yet implemented!");
        ESP_LOGI(TAG, "      ⚠️  Need wolfSSL for X448 DH operations.");
    }
}

// ============== Agent Message Parsing ==============

void parse_agent_message(contact_t *contact, const uint8_t *plain, int plain_len) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   🔬 AGENT MESSAGE ANALYSIS:");
    
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
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      📊 Raw message structure (first 64 bytes):");
    printf("         ");
    for (int i = 0; i < 64 && i < content_len; i++) {
        printf("%02x ", content[i]);
        if ((i + 1) % 16 == 0) printf("\n         ");
    }
    printf("\n");
    
    // Scan for X25519 SPKI header
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
    
    int after_key_offset = sender_key_offset + 44;
    int after_key_len = content_len - after_key_offset;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      📦 Data after SPKI key (%d bytes):", after_key_len);
    if (after_key_len > 0) {
        dump_hex("After key", &content[after_key_offset], after_key_len, 64);
    }
    
    // Try DH decryption
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
                        
                        int uri_len = 0;
                        char *full_uri = extract_full_invitation_uri(decrypted, dec_len, &uri_len);
                        
                        if (full_uri) {
                            ESP_LOGI(TAG, "");
                            ESP_LOGI(TAG, "      📋 FULL INVITATION URI (%d chars):", uri_len);
                            
                            for (int pass = 0; pass < 4; pass++) {
                                if (!strchr(full_uri, '%')) break;
                                url_decode_inplace(full_uri);
                            }
                            
                            int show_len = strlen(full_uri);
                            for (int i = 0; i < show_len; i += 120) {
                                ESP_LOGI(TAG, "         %.120s", full_uri + i);
                            }
                            
                            // Parse SMP server info
                            char *smp_start = strstr(full_uri, "smp://");
                            if (smp_start) {
                                char *at = strchr(smp_start, '@');
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
                                        
                                        ESP_LOGI(TAG, "");
                                        ESP_LOGI(TAG, "      🖥️  Peer Server: %s:%d", 
                                                 pending_peer.host, pending_peer.port);
                                    }
                                    
                                    char *qend = hash ? hash : (slash + strlen(slash));
                                    char *q_mark = strchr(slash, '?');
                                    if (q_mark && (!qend || q_mark < qend)) qend = q_mark;
                                    
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
                                
                                // Find dh= parameter (SMP level)
                                char *dh = strstr(smp_start, "dh=");
                                if (dh) {
                                    dh += 3;
                                    char *dh_end = dh;
                                    while (*dh_end && *dh_end != '&' && *dh_end != ' ' && *dh_end != '#') dh_end++;
                                    int dh_len = dh_end - dh;
                                    
                                    if (dh_len > 40 && dh_len < 120) {
                                        char dh_b64[100] = {0};
                                        memcpy(dh_b64, dh, dh_len);
                                        
                                        uint8_t spki[48];
                                        int spki_len = base64url_decode(dh_b64, spki, 48);
                                        
                                        if (spki_len >= 44) {
                                            memcpy(pending_peer.dh_public, spki + 12, 32);
                                            pending_peer.has_dh = 1;
                                            ESP_LOGI(TAG, "      🔑 SMP DH Key: %02x%02x%02x%02x... ✅",
                                                     pending_peer.dh_public[0], pending_peer.dh_public[1],
                                                     pending_peer.dh_public[2], pending_peer.dh_public[3]);
                                        }
                                    }
                                }
                            }
                            
                            // Parse E2E parameters (the critical part!)
                            parse_e2e_params(full_uri);
                            
                            pending_peer.valid = (strlen(pending_peer.host) > 0 && 
                                                  pending_peer.queue_id_len > 0);
                            
                            if (pending_peer.valid && pending_peer.has_dh) {
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      ╔══════════════════════════════════════╗");
                                if (pending_peer.has_e2e) {
                                    ESP_LOGI(TAG, "      ║  🎯 READY! (with E2E Ratchet keys)  ║");
                                } else {
                                    ESP_LOGI(TAG, "      ║  ⚠️  READY! (NO E2E keys found!)    ║");
                                }
                                ESP_LOGI(TAG, "      ╚══════════════════════════════════════╝");
                                
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      🚀 Auto-connecting to peer...");
                                
                                if (peer_connect(pending_peer.host, pending_peer.port)) {
                                    send_agent_confirmation(contact);
                                    peer_disconnect();
                                }
                            }
                            
                            free(full_uri);
                        } else {
                            ESP_LOGW(TAG, "      ⚠️ Could not extract invitation URI!");
                        }
                    }
                }
                
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "      📝 Decrypted text (first 500 chars):");
                printf("         \"");
                for (int i = 0; i < dec_len && i < 500; i++) {
                    char c = decrypted[i];
                    if (c >= 32 && c < 127) printf("%c", c);
                    else if (c == '\n') printf("\\n");
                    else printf(".");
                }
                printf("\"\n");
            }
            free(decrypted);
        }
    }
    
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