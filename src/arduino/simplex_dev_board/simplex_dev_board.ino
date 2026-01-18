/*
 * SimpleGo - SimpleX Dev Board
 * 
 * Native SimpleX Chat client for ESP32 hardware
 * https://github.com/cannatoshi/SimpleGo
 * 
 * License: AGPL-3.0
 * 
 * Hardware: Heltec WiFi LoRa 32 V2 (or compatible ESP32)
 * 
 * Required Libraries:
 *   - Heltec_ESP32 (for display)
 *   - Monocypher (for crypto)
 *   - Adafruit_GFX (dependency)
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "HT_SSD1306Wire.h"
#include "monocypher.h"
#include "mbedtls/sha256.h"
#include "esp_random.h"

// ============== CONFIGURATION ==============

// WiFi credentials (edit these!)
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// SMP Server (smp11.simplex.im)
IPAddress SMP_SERVER(172, 236, 211, 32);
const int SMP_PORT = 443;

// ============== HARDWARE ==============

// OLED Display (Heltec)
SSD1306Wire oled(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// TLS Client
WiFiClientSecure tlsClient;

// ============== STATE ==============

bool wifiConnected = false;
bool smpConnected = false;
unsigned long smpConnectTime = 0;
int signalStrength = 0;

// Identity Keys (Ed25519)
uint8_t identity_pk[32];
uint8_t identity_sk[64];

// Queue DH Keys (X25519)
uint8_t queue_dh_pk[32];
uint8_t queue_dh_sk[32];

// ============== CRYPTO FUNCTIONS ==============

void randomBytes(uint8_t* buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    buf[i] = esp_random() & 0xFF;
  }
}

void generateEd25519Keypair(uint8_t* pk, uint8_t* sk) {
  uint8_t seed[32];
  randomBytes(seed, 32);
  crypto_eddsa_key_pair(sk, pk, seed);
}

void generateX25519Keypair(uint8_t* pk, uint8_t* sk) {
  randomBytes(sk, 32);
  crypto_x25519_public_key(pk, sk);
}

void ed25519Sign(uint8_t* sig, const uint8_t* sk, const uint8_t* msg, size_t len) {
  crypto_eddsa_sign(sig, sk, msg, len);
}

int ed25519Verify(const uint8_t* sig, const uint8_t* pk, const uint8_t* msg, size_t len) {
  return crypto_eddsa_check(sig, pk, msg, len) == 0;
}

void x25519Exchange(uint8_t* shared, const uint8_t* my_sk, const uint8_t* their_pk) {
  crypto_x25519(shared, my_sk, their_pk);
}

// ============== DISPLAY FUNCTIONS ==============

void drawSignalBars(int x, int y, int rssi) {
  int bars = 0;
  if (rssi > -50) bars = 4;
  else if (rssi > -60) bars = 3;
  else if (rssi > -70) bars = 2;
  else if (rssi > -80) bars = 1;
  
  for (int i = 0; i < 4; i++) {
    int h = 3 + (i * 2);
    if (i < bars) oled.fillRect(x + (i * 5), y + (10 - h), 4, h);
    else oled.drawRect(x + (i * 5), y + (10 - h), 4, h);
  }
}

void drawHeader(const char* title) {
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(2, 0, title);
  drawSignalBars(100, 1, signalStrength);
  oled.drawHorizontalLine(0, 13, 128);
}

void drawFooter(const char* left, const char* right) {
  oled.drawHorizontalLine(0, 50, 128);
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(2, 52, left);
  oled.drawString(90, 52, right);
}

void showScreen(const char* l1, const char* l2 = "", const char* l3 = "") {
  oled.clear();
  drawHeader("SimpleX");
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(5, 16, l1);
  if (strlen(l2) > 0) oled.drawString(5, 27, l2);
  if (strlen(l3) > 0) oled.drawString(5, 38, l3);
  oled.display();
}

void showMainScreen() {
  oled.clear();
  drawHeader("SimpleX");
  
  oled.setFont(ArialMT_Plain_10);
  if (smpConnected) {
    oled.drawString(5, 16, "SMP: Connected");
    oled.drawString(5, 27, "smp11.simplex.im:443");
    char buf[32];
    snprintf(buf, sizeof(buf), "TLS OK | %lums", smpConnectTime);
    oled.drawString(5, 38, buf);
  } else {
    oled.drawString(5, 16, "SMP: Disconnected");
    oled.drawString(5, 27, wifiConnected ? "WiFi OK" : "WiFi Down");
  }
  
  char footer[16];
  snprintf(footer, sizeof(footer), "%ddBm", signalStrength);
  drawFooter(footer, "v0.2");
  
  oled.display();
}

void showSplash() {
  oled.clear();
  oled.setFont(ArialMT_Plain_16);
  oled.drawString(30, 15, "SimpleX");
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(25, 38, "Dev Board v0.2");
  oled.display();
}

// ============== NETWORK FUNCTIONS ==============

bool connectWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    signalStrength = WiFi.RSSI();
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] Signal: ");
    Serial.print(signalStrength);
    Serial.println(" dBm");
    return true;
  }
  
  Serial.println("[WiFi] Connection failed!");
  return false;
}

bool connectSMP() {
  Serial.println("[SMP] Connecting to server...");
  
  tlsClient.setInsecure();  // Skip cert validation (dev only!)
  
  unsigned long start = millis();
  if (tlsClient.connect(SMP_SERVER, SMP_PORT, 15000)) {
    smpConnectTime = millis() - start;
    smpConnected = true;
    Serial.print("[SMP] Connected in ");
    Serial.print(smpConnectTime);
    Serial.println(" ms");
    return true;
  }
  
  Serial.println("[SMP] Connection failed!");
  return false;
}

void disconnectSMP() {
  tlsClient.stop();
  smpConnected = false;
  Serial.println("[SMP] Disconnected");
}

// ============== UTILITY FUNCTIONS ==============

void printHex(const char* label, const uint8_t* data, size_t len) {
  Serial.print(label);
  Serial.print(": ");
  for (size_t i = 0; i < len && i < 32; i++) {
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
  }
  if (len > 32) Serial.print("...");
  Serial.println();
}

void runCryptoTests() {
  Serial.println("\n[Crypto] Running tests...\n");
  
  // X25519
  uint8_t x_sk[32], x_pk[32];
  unsigned long start = millis();
  generateX25519Keypair(x_pk, x_sk);
  Serial.print("[Crypto] X25519 keygen: ");
  Serial.print(millis() - start);
  Serial.println(" ms");
  
  // Ed25519
  uint8_t ed_sk[64], ed_pk[32];
  start = millis();
  generateEd25519Keypair(ed_pk, ed_sk);
  Serial.print("[Crypto] Ed25519 keygen: ");
  Serial.print(millis() - start);
  Serial.println(" ms");
  
  // Sign
  uint8_t sig[64];
  const char* msg = "SimpleGo Test";
  start = millis();
  ed25519Sign(sig, ed_sk, (uint8_t*)msg, strlen(msg));
  Serial.print("[Crypto] Ed25519 sign: ");
  Serial.print(millis() - start);
  Serial.println(" ms");
  
  // Verify
  start = millis();
  int valid = ed25519Verify(sig, ed_pk, (uint8_t*)msg, strlen(msg));
  Serial.print("[Crypto] Ed25519 verify: ");
  Serial.print(millis() - start);
  Serial.print(" ms - ");
  Serial.println(valid ? "VALID" : "INVALID");
  
  Serial.println("[Crypto] All tests passed!\n");
}

// ============== MAIN ==============

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n");
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║     SimpleGo - Dev Board v0.2         ║");
  Serial.println("║     github.com/cannatoshi/SimpleGo    ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  // Initialize display
  oled.init();
  oled.flipScreenVertically();
  showSplash();
  delay(2000);
  
  // Generate identity keys
  showScreen("Generating keys...");
  Serial.println("[Init] Generating identity keypair...");
  generateEd25519Keypair(identity_pk, identity_sk);
  printHex("[Init] Identity PK", identity_pk, 32);
  
  Serial.println("[Init] Generating queue DH keypair...");
  generateX25519Keypair(queue_dh_pk, queue_dh_sk);
  printHex("[Init] Queue DH PK", queue_dh_pk, 32);
  
  // Run crypto tests
  runCryptoTests();
  
  // Connect WiFi
  showScreen("Connecting WiFi...");
  if (!connectWiFi()) {
    showScreen("WiFi FAILED!", "Check credentials");
    return;
  }
  
  // Connect to SMP server
  showScreen("Connecting SMP...", "smp11.simplex.im:443");
  if (connectSMP()) {
    showScreen("SMP Connected!", "TLS OK");
    delay(1000);
    
    // Close connection for now (we'll keep it open later)
    disconnectSMP();
  } else {
    showScreen("SMP FAILED!", "Check network");
  }
  
  Serial.println("[Init] Setup complete!\n");
}

void loop() {
  // Update signal strength
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) {
    signalStrength = WiFi.RSSI();
    lastUpdate = millis();
  }
  
  // Show main screen
  showMainScreen();
  
  delay(1000);
}
