/**
 * SimpleGo - Crypto Test Sketch
 * Tests all cryptographic primitives on ESP32
 * SPDX-License-Identifier: AGPL-3.0
 */

#include <Arduino.h>
#include <heltec.h>
#include "../../crypto/crypto.h"

void printHex(const char* label, const uint8_t* data, size_t len) {
    Serial.print(label);
    Serial.print(": ");
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
    }
    Serial.println();
}

void showStatus(const char* test, bool passed) {
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "SimpleGo Crypto Test");
    Heltec.display->drawString(0, 16, test);
    Heltec.display->drawString(0, 32, passed ? "PASSED" : "FAILED");
    Heltec.display->display();
    
    Serial.print("[TEST] ");
    Serial.print(test);
    Serial.println(passed ? " - PASSED" : " - FAILED");
}

bool test_x448() {
    Serial.println("\n========== X448 TEST ==========");
    
    uint8_t alice_pk[X448_KEY_SIZE], alice_sk[X448_KEY_SIZE];
    uint8_t bob_pk[X448_KEY_SIZE], bob_sk[X448_KEY_SIZE];
    uint8_t alice_shared[X448_SHARED_SIZE], bob_shared[X448_SHARED_SIZE];
    
    unsigned long start = millis();
    if (x448_keypair(alice_pk, alice_sk) != CRYPTO_OK) {
        Serial.println("[X448] Alice keygen FAILED");
        return false;
    }
    Serial.print("[X448] Alice keygen: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    start = millis();
    if (x448_keypair(bob_pk, bob_sk) != CRYPTO_OK) {
        Serial.println("[X448] Bob keygen FAILED");
        return false;
    }
    Serial.print("[X448] Bob keygen: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    start = millis();
    if (x448_shared_secret(alice_shared, alice_sk, bob_pk) != CRYPTO_OK) {
        Serial.println("[X448] Alice DH FAILED");
        return false;
    }
    Serial.print("[X448] Alice DH: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    start = millis();
    if (x448_shared_secret(bob_shared, bob_sk, alice_pk) != CRYPTO_OK) {
        Serial.println("[X448] Bob DH FAILED");
        return false;
    }
    Serial.print("[X448] Bob DH: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    bool match = (memcmp(alice_shared, bob_shared, X448_SHARED_SIZE) == 0);
    printHex("[X448] Shared", alice_shared, 32);
    
    crypto_wipe(alice_sk, X448_KEY_SIZE);
    crypto_wipe(bob_sk, X448_KEY_SIZE);
    
    return match;
}

bool test_ed25519() {
    Serial.println("\n========== ED25519 TEST ==========");
    
    uint8_t pk[ED25519_PK_SIZE], sk[ED25519_SK_SIZE];
    uint8_t sig[ED25519_SIG_SIZE];
    const char* msg = "SimpleGo: Freedom in your pocket!";
    
    unsigned long start = millis();
    if (ed25519_keypair(pk, sk) != CRYPTO_OK) {
        Serial.println("[ED25519] Keygen FAILED");
        return false;
    }
    Serial.print("[ED25519] Keygen: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    start = millis();
    if (ed25519_sign(sig, sk, (uint8_t*)msg, strlen(msg)) != CRYPTO_OK) {
        Serial.println("[ED25519] Sign FAILED");
        return false;
    }
    Serial.print("[ED25519] Sign: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    start = millis();
    int valid = ed25519_verify(sig, pk, (uint8_t*)msg, strlen(msg));
    Serial.print("[ED25519] Verify: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    sig[0] ^= 0x01;
    int tampered = ed25519_verify(sig, pk, (uint8_t*)msg, strlen(msg));
    
    crypto_wipe(sk, ED25519_SK_SIZE);
    
    return (valid == CRYPTO_OK) && (tampered != CRYPTO_OK);
}

bool test_chacha20poly1305() {
    Serial.println("\n========== CHACHA20-POLY1305 TEST ==========");
    
    uint8_t key[CHACHA20_KEY_SIZE];
    uint8_t nonce[CHACHA20_NONCE_SIZE];
    uint8_t tag[POLY1305_TAG_SIZE];
    
    const char* plaintext = "Top secret SimpleGo message!";
    size_t pt_len = strlen(plaintext);
    const char* aad = "SimpleGo v0.1";
    
    uint8_t ciphertext[64];
    uint8_t decrypted[64];
    
    crypto_random(key, CHACHA20_KEY_SIZE);
    crypto_random(nonce, CHACHA20_NONCE_SIZE);
    
    unsigned long start = millis();
    if (chacha20poly1305_encrypt(ciphertext, tag, key, nonce,
                                  (uint8_t*)plaintext, pt_len,
                                  (uint8_t*)aad, strlen(aad)) != CRYPTO_OK) {
        Serial.println("[AEAD] Encrypt FAILED");
        return false;
    }
    Serial.print("[AEAD] Encrypt: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    start = millis();
    if (chacha20poly1305_decrypt(decrypted, key, nonce,
                                  ciphertext, pt_len, tag,
                                  (uint8_t*)aad, strlen(aad)) != CRYPTO_OK) {
        Serial.println("[AEAD] Decrypt FAILED");
        return false;
    }
    Serial.print("[AEAD] Decrypt: ");
    Serial.print(millis() - start);
    Serial.println(" ms");
    
    bool match = (memcmp(plaintext, decrypted, pt_len) == 0);
    
    ciphertext[0] ^= 0x01;
    int tampered = chacha20poly1305_decrypt(decrypted, key, nonce,
                                             ciphertext, pt_len, tag,
                                             (uint8_t*)aad, strlen(aad));
    
    crypto_wipe(key, CHACHA20_KEY_SIZE);
    
    return match && (tampered != CRYPTO_OK);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Heltec.begin(true, false, true);
    Heltec.display->clear();
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(0, 0, "SimpleGo Crypto Test");
    Heltec.display->display();
    
    Serial.println("\n");
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.println("║     SimpleGo - Crypto Test Suite      ║");
    Serial.println("╚═══════════════════════════════════════╝\n");
    
    if (crypto_init() != CRYPTO_OK) {
        showStatus("INIT", false);
        return;
    }
    showStatus("INIT", true);
    delay(1000);
    
    bool x448_ok = test_x448();
    showStatus("X448", x448_ok);
    delay(2000);
    
    bool ed25519_ok = test_ed25519();
    showStatus("ED25519", ed25519_ok);
    delay(2000);
    
    bool aead_ok = test_chacha20poly1305();
    showStatus("AEAD", aead_ok);
    delay(2000);
    
    Serial.println("\n========== SUMMARY ==========");
    Serial.printf("[RESULT] X448: %s\n", x448_ok ? "PASS" : "FAIL");
    Serial.printf("[RESULT] Ed25519: %s\n", ed25519_ok ? "PASS" : "FAIL");
    Serial.printf("[RESULT] AEAD: %s\n", aead_ok ? "PASS" : "FAIL");
    
    bool all_passed = x448_ok && ed25519_ok && aead_ok;
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, all_passed ? "ALL TESTS PASSED!" : "SOME TESTS FAILED");
    Heltec.display->display();
    
    crypto_cleanup();
}

void loop() {
    delay(10000);
}
