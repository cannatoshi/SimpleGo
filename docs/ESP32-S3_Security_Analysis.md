# ESP32-S3 Security Features Analysis for SimpleGo

**ESP-IDF Version:** 5.5.2
**Target:** ESP32-S3 (LilyGo T-Deck, 16 MB Flash, 8 MB PSRAM)
**Reference:** https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/security/

---

## Inhaltsverzeichnis

1. [Ist-Zustand](#1-ist-zustand)
2. [Flash Encryption](#2-flash-encryption)
3. [NVS Encryption](#3-nvs-encryption)
4. [Secure Boot v2](#4-secure-boot-v2)
5. [Empfohlene Aktivierungsreihenfolge](#5-empfohlene-aktivierungsreihenfolge)
6. [Konkrete Änderungen für SimpleGo](#6-konkrete-änderungen-für-simplego)

---

## 1. Ist-Zustand

### Aktuelle partitions.csv

```
Name,      Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x20000  (128 KB)
phy_init,  data, phy,     0x29000,  0x1000   (4 KB)
factory,   app,  factory, 0x30000,  0x1D0000 (1856 KB)
```

### Sicherheitsrelevante Daten im NVS (Namespace: "simplego")

| Key | Inhalt | Größe |
|-----|--------|-------|
| `rat_XX` | Double Ratchet State (Root Key, Chain Keys, X448 DH Keypair, Header Keys) | ~536 Bytes |
| `queue_our` | Queue Credentials (Ed25519 Private Key, X25519 Private Key, Shared Secret) | ~344 Bytes |
| `queue_e2e` | E2E Peer Public Key + Valid Flag | 33 Bytes |
| `contacts` | Contact Database (Ed25519/X25519 Key Material pro Kontakt) | ~3000-4000 Bytes |

**Problem:** Alle kryptographischen Schlüssel liegen **unverschlüsselt** im Flash. Ein Angreifer mit physischem Zugang kann den Flash-Chip auslesen und alle Private Keys extrahieren.

### Aktuelle sdkconfig-Lücken

- Kein `CONFIG_SECURE_FLASH_ENC_ENABLED`
- Kein `CONFIG_SECURE_BOOT`
- Kein `CONFIG_NVS_ENCRYPTION`
- `CONFIG_PARTITION_TABLE_SINGLE_APP=y` (keine OTA-Partition)

---

## 2. Flash Encryption

### 2.1 Was es tut

Flash Encryption verschlüsselt den gesamten Flash-Inhalt mit XTS-AES. Der Schlüssel wird in eFuses gespeichert und ist per Hardware geschützt — Software kann ihn nicht auslesen. Jeder Lesezugriff wird transparent durch die Hardware entschlüsselt.

### 2.2 Benötigte sdkconfig-Einstellungen

```ini
# === Flash Encryption aktivieren ===
CONFIG_SECURE_FLASH_ENC_ENABLED=y

# Development Mode (zum Testen) oder Release Mode (Produktion)
# Optionen: "Development Mode" oder "Release Mode"
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y
# CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y  # Für Produktion

# XTS-AES Key Size (256-bit empfohlen, braucht 1 eFuse-Block)
CONFIG_SECURE_FLASH_ENCRYPTION_KEYSIZE_AES_256=y

# UART Download Mode einschränken
CONFIG_SECURE_UART_ROM_DL_MODE_PERMANENT_DISABLED=y  # Release
# CONFIG_SECURE_UART_ROM_DL_MODE_ENABLED=y           # Development

# Partition Table auf custom setzen (nötig für nvs_keys Partition)
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

### 2.3 Development Mode vs. Release Mode

| Eigenschaft | Development Mode | Release Mode |
|-------------|-----------------|--------------|
| **Plaintext reflashen** | Ja, Bootloader verschlüsselt automatisch | Nein, nur OTA oder pre-encrypted |
| **UART Download** | Aktiv | Dauerhaft deaktiviert |
| **`SPI_BOOT_CRYPT_CNT`** | Nicht write-protected, kann getoggelt werden | Write-protected (irreversibel) |
| **Reflash-Limit** | Begrenzt durch eFuse-Bits (3 Bit = max. 3× umschalten) | Kein Reflash möglich |
| **Empfehlung** | Entwicklung & Debugging | Produktion & Endgeräte |

**Kritischer Unterschied:** Im Development Mode kann `SPI_BOOT_CRYPT_CNT` zwischen Werten getoggelt werden. Da ESP32-S3 nur 3 Bits hat, kann man Verschlüsselung maximal **3 Mal** aktivieren/deaktivieren, bevor die eFuse-Bits aufgebraucht sind.

### 2.4 Welche Partitionen werden verschlüsselt?

| Partition | Automatisch verschlüsselt? | Hinweis |
|-----------|-----------------------------|---------|
| Bootloader (2nd stage) | **Ja** | Immer |
| Partition Table | **Ja** | Immer |
| Alle `app`-Partitionen | **Ja** | `factory`, `ota_0`, `ota_1` |
| `otadata` | **Ja** | Immer |
| `nvs_keys` | **Ja** | Wenn `encrypted` Flag gesetzt |
| **`nvs`** | **Nein** | NVS-Library ist nicht kompatibel mit Flash Encryption! |
| `phy_init` | **Nein** (optional) | Kann mit `encrypted` Flag markiert werden |

**WICHTIG:** Die NVS-Partition wird von Flash Encryption **NICHT** verschlüsselt! NVS verwendet ein eigenes Speicherformat, das inkompatibel mit der transparenten Flash-Verschlüsselung ist. Für NVS-Daten braucht man **NVS Encryption** (siehe Abschnitt 3).

### 2.5 Brick-Risiken

| Risiko | Ursache | Vermeidung |
|--------|---------|------------|
| **Bootloop "flash read err"** | Plaintext-Bootloader auf verschlüsselten Flash geflasht | Im Dev-Mode: `idf.py encrypted-flash`. Release: Nur OTA |
| **Stromausfall beim Erst-Boot** | Verschlüsselungsprozess unterbrochen | Stabile Stromversorgung beim allerersten Boot sicherstellen |
| **eFuse-Bits aufgebraucht** | Development Mode: zu oft zwischen encrypted/unencrypted gewechselt | Max 3× umschalten. Bei Tests: QEMU oder separate Dev-Boards |
| **Key verloren** | eFuse mit Key ist read-protected, kein Backup möglich | Keys werden in Hardware generiert. Kein Backup möglich — by design |
| **Release Mode irreversibel** | `DIS_DOWNLOAD_MANUAL_ENCRYPT` + `SPI_BOOT_CRYPT_CNT` write-protected | Gründlich testen bevor Release Mode aktiviert wird |

**Worst Case:** Ein Gerät im Release Mode mit korruptem Bootloader ist **permanent gebrickt**. Es gibt keinen Recovery-Mechanismus, weil UART-Download deaktiviert und der Encryption Key in Hardware locked ist.

### 2.6 Flash-Befehle im Development Mode

```bash
# Erstes Flashen (Plaintext, wird beim ersten Boot verschlüsselt)
idf.py flash monitor

# Nachfolgendes Flashen (muss encrypted sein!)
idf.py encrypted-flash monitor

# Einzelne Partition flashen
idf.py encrypted-app-flash monitor
```

---

## 3. NVS Encryption

### 3.1 Warum separat nötig?

Flash Encryption verschlüsselt die NVS-Partition **nicht**, weil das NVS-Speicherformat (32-Byte Einträge mit eigenem Wear-Leveling) inkompatibel mit der sektorbasierten XTS-AES Verschlüsselung ist. NVS Encryption ist eine eigene Schicht, die XTS-AES auf Entry-Ebene anwendet.

### 3.2 Zwei Varianten

#### Variante A: Flash-Encryption-basiert (empfohlen wenn Flash Encryption aktiv)

- Braucht eine zusätzliche `nvs_keys`-Partition (min. 4 KB, mit `encrypted` Flag)
- Keys werden beim ersten Boot automatisch generiert und in der `nvs_keys`-Partition gespeichert
- Die `nvs_keys`-Partition selbst wird durch Flash Encryption geschützt
- `nvs_flash_init()` erledigt die Key-Generierung automatisch

#### Variante B: HMAC-basiert (ohne Flash Encryption möglich)

- Benutzt die HMAC-Peripherie des ESP32-S3
- Key wird aus einem eFuse-Block abgeleitet — **kein Key im Flash gespeichert**
- Braucht **keine** extra `nvs_keys`-Partition
- Besser gegen physische Angriffe, da Key nie im Flash liegt

**Empfehlung für SimpleGo:** Variante A, da wir Flash Encryption ohnehin brauchen. Falls Flash Encryption nicht gewünscht: Variante B als Standalone-Lösung.

### 3.3 sdkconfig-Einstellungen

```ini
# NVS Encryption aktivieren
CONFIG_NVS_ENCRYPTION=y

# Variante A (Flash Encryption-basiert):
CONFIG_NVS_SEC_KEY_PROTECTION_SCHEME=NVS_SEC_SCHEME_FLASH_ENC
# → Braucht nvs_keys Partition in partitions.csv

# Variante B (HMAC-basiert):
# CONFIG_NVS_SEC_KEY_PROTECTION_SCHEME=NVS_SEC_SCHEME_HMAC
# CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID=0  # eFuse Block 0-5
```

### 3.4 Partition Table Änderung (Variante A)

Neue `nvs_keys`-Partition nötig:

```
# Name,     Type, SubType,  Offset,   Size,    Flags
nvs,        data, nvs,      0x9000,   0x20000,
nvs_keys,   data, nvs_keys, 0x29000,  0x1000,  encrypted
phy_init,   data, phy,      0x2a000,  0x1000,
factory,    app,  factory,  0x30000,  0x1D0000,
```

**Hinweis:** Die `nvs_keys`-Partition braucht nur 4 KB (0x1000). Sie speichert: 32 Byte XTS Encryption Key + 32 Byte XTS Tweak Key + 4 Byte CRC32.

### 3.5 Storage Overhead Analyse

#### NVS internes Format

- Jeder Entry ist **32 Bytes** fix
- Eine Page hat **126 Entries** (+ 32 Byte Header + 32 Byte Bitmap)
- Page-Größe: 4096 Bytes
- 128 KB Partition = **32 Pages** = **4032 Entries** theoretisch

#### Blob-Overhead

Blobs verbrauchen:
- 2 Overhead-Entries (Blob-Index + Namespace)
- 1 Entry pro 32 Bytes Daten

**Beispiel: Ratchet State (536 Bytes)**
- Overhead: 2 Entries = 64 Bytes
- Daten: ⌈536/32⌉ = 17 Entries = 544 Bytes
- **Gesamt: 19 Entries = 608 Bytes** (vs. 536 Bytes Nutzdaten → **13% Overhead**)

#### Deine ~520 Bytes pro Nachricht

Für einen typischen Blob von ~520 Bytes:

| Komponente | Entries | Bytes |
|------------|---------|-------|
| Blob-Index (Overhead) | 2 | 64 |
| Daten: ⌈520/32⌉ = 17 | 17 | 544 |
| **Gesamt** | **19** | **608** |

**Overhead: ~17%** (608 Bytes statt 520 Bytes)

#### NVS Encryption zusätzlicher Overhead

NVS Encryption fügt **keinen zusätzlichen Storage-Overhead** hinzu. Die XTS-AES Verschlüsselung arbeitet in-place auf den 32-Byte Entries. Die einzigen "Kosten" sind:
- 4 KB für die `nvs_keys`-Partition
- Geringfügig höhere Schreib-/Leselatenz durch Ver-/Entschlüsselung

#### Kapazität mit 128 KB

| Szenario | Entries verfügbar | Ratchet States (à 19 Entries) |
|----------|-------------------|-------------------------------|
| Ohne Encryption | ~4032 | ~212 |
| Mit NVS Encryption | ~4032 (gleich) | ~212 |

Für SimpleGo bei aktuellem Nutzungsmuster (1 Ratchet + 1 Queue + 1 Contacts DB ≈ 60 Entries):
**Auslastung < 2% — 128 KB ist mehr als ausreichend.**

### 3.6 Code-Änderungen

**Keine!** NVS Encryption ist transparent. Die bestehenden `nvs_set_blob()` / `nvs_get_blob()` Aufrufe in `smp_storage.c` funktionieren unverändert. Die Verschlüsselung wird in `nvs_flash_init()` beim Boot automatisch aktiviert.

Einzige mögliche Änderung — für Variante A muss sichergestellt werden, dass `nvs_flash_init()` korrekt aufgerufen wird (ist es bereits in `main.c:641`).

---

## 4. Secure Boot v2

### 4.1 RSA-3072 vs. ECDSA

**ESP32-S3 unterstützt ausschließlich RSA-3072 (RSA-PSS mit SHA-256).** ECDSA wird für Secure Boot v2 auf dem ESP32-S3 **nicht** angeboten. (ECDSA-basierter Secure Boot ist nur auf neueren Chips wie ESP32-C3/C6/H2 verfügbar.)

| Eigenschaft | RSA-3072 (ESP32-S3) |
|-------------|---------------------|
| Signatur-Schema | RSA-PSS mit SHA-256, 32-Byte Salt |
| Key-Größe | 3072 Bit (384 Bytes) |
| Signatur-Block | 4 KB (angehängt an Image) |
| Max. Keys | 3 (für Key-Rotation) |
| Hardware-Verifikation | Ja, ROM-Code prüft Bootloader |

### 4.2 sdkconfig-Einstellungen

```ini
# Secure Boot v2 aktivieren
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y

# Signing Key Pfad (RSA-3072 PEM)
CONFIG_SECURE_BOOT_SIGNING_KEY="keys/secure_boot_signing_key.pem"

# Optional: Automatisch beim Build signieren
CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y

# Key-Revocation Policy
# CONFIG_SECURE_BOOT_ALLOW_UNUSED_DIGEST_SLOTS=y    # Unused Slots nicht sperren
# CONFIG_SECURE_BOOT_ENABLE_AGGRESSIVE_KEY_REVOKE=y  # Aggressive Revocation (Vorsicht!)
```

### 4.3 Key-Generierung

```bash
# Signing Key generieren (einmalig, sicher aufbewahren!)
idf.py secure-generate-signing-key secure_boot_signing_key.pem

# Oder mit OpenSSL
openssl genrsa -out secure_boot_signing_key.pem 3072
```

**KRITISCH:** Den Signing Key verlieren = keine Updates mehr möglich. Den Key kompromittieren = Angreifer kann beliebige Firmware signieren. Empfehlung: HSM oder verschlüsselter Offline-Speicher.

### 4.4 OTA-Updates signieren

```bash
# Image manuell signieren
idf.py secure-sign-data \
    --keyfile secure_boot_signing_key.pem \
    --output signed_firmware.bin \
    build/simplex_client.bin

# Zweite Signatur anhängen (Multi-Key)
idf.py secure-sign-data \
    --keyfile second_key.pem \
    --output signed_firmware.bin \
    --append_signatures \
    signed_firmware.bin
```

Für CI/CD-Pipelines: `espsecure.py` unterstützt HSM-Integration für Remote-Signing.

Bei OTA: Das signierte Image wird über `esp_ota_ops` geflasht. Der Bootloader verifiziert die Signatur beim nächsten Boot automatisch. Ungültige Signaturen → Boot wird verweigert.

### 4.5 Secure Boot NACH Flash Encryption aktivieren?

**Ja, aber nur in dieser Reihenfolge: Flash Encryption ZUERST, dann Secure Boot.**

Die Reihenfolge ist technisch erzwungen:

1. **Flash Encryption zuerst:** Der FE-Key wird in einem eFuse-Block gespeichert und **read-protected** (niemand, auch nicht Software, kann ihn lesen). Dafür wird das `RD_DIS`-eFuse beschrieben.

2. **Secure Boot danach:** Der SB-Key-Digest wird in einem anderen eFuse-Block gespeichert und muss **lesbar bleiben** (ROM-Code muss ihn lesen können). Beim Aktivieren wird `RD_DIS` **write-protected**, damit niemand nachträglich weitere Keys read-protecten kann.

**Umgekehrte Reihenfolge ist nicht möglich:** Wenn Secure Boot zuerst aktiviert wird, ist `RD_DIS` bereits write-protected. Dann kann der Flash Encryption Key nicht mehr read-protected werden — er wäre per Software auslesbar, was die Verschlüsselung nutzlos macht.

### 4.6 eFuse-Verbrauch (Gesamtübersicht)

| Feature | eFuse-Blöcke | Zweck |
|---------|-------------|-------|
| Flash Encryption | 1× BLOCK_KEY (256-bit) oder 2× (512-bit) | XTS-AES Key |
| Secure Boot | 1-3× BLOCK_KEY | SHA-256 Digest des Public Key |
| NVS HMAC (falls Variante B) | 1× BLOCK_KEY | HMAC-Schlüssel |

ESP32-S3 hat **6 Key-Blöcke** (BLOCK_KEY0 bis BLOCK_KEY5). Mit Flash Encryption (1 Block) + Secure Boot (1 Block) + optional HMAC NVS (1 Block) sind 2-3 von 6 belegt.

---

## 5. Empfohlene Aktivierungsreihenfolge

```
Phase 1: Development Mode (Testen & Validieren)
─────────────────────────────────────────────────
  1. partitions.csv anpassen (nvs_keys + OTA Partitionen)
  2. sdkconfig: Flash Encryption Development Mode aktivieren
  3. sdkconfig: NVS Encryption aktivieren (Flash-Enc-basiert)
  4. Testen: Flash, Boot, NVS Read/Write, OTA
  5. sdkconfig: Secure Boot v2 aktivieren (mit Test-Signing-Key)
  6. Testen: Signierte Builds, OTA mit signierten Images

Phase 2: Release Mode (Produktionsgeräte)
─────────────────────────────────────────────────
  1. Produktions-Signing-Key generieren (sicher verwahren!)
  2. sdkconfig: Flash Encryption Release Mode
  3. sdkconfig: Secure Boot mit Produktions-Key
  4. Ersten Boot auf Zielgerät durchführen (stabile Stromversorgung!)
  5. OTA-Workflow validieren (einziger Update-Weg im Release Mode)
```

**Warnung:** Jedes Gerät im Release Mode ist **individuell verschlüsselt** (eigener FE-Key in eFuse). Es gibt kein "Master-Key" um mehrere Geräte zu entschlüsseln. OTA ist der einzige Update-Mechanismus.

---

## 6. Konkrete Änderungen für SimpleGo

### 6.1 Neue partitions.csv (mit OTA-Support)

```
# SimpleGo Partition Table — Security Enabled
# Name,      Type, SubType,  Offset,    Size,      Flags
nvs,         data, nvs,      0x9000,    0x20000,
nvs_keys,    data, nvs_keys, 0x29000,   0x1000,    encrypted
otadata,     data, ota,      0x2a000,   0x2000,
phy_init,    data, phy,      0x2c000,   0x1000,
ota_0,       app,  ota_0,    0x30000,   0x1D0000,
ota_1,       app,  ota_1,    0x200000,  0x1D0000,
```

**Hinweis:** OTA braucht zwei App-Partitionen. Bei 16 MB Flash ist das kein Problem. Die `factory`-Partition wird durch `ota_0` / `ota_1` ersetzt.

### 6.2 sdkconfig.defaults Ergänzungen (Development)

```ini
# === SECURITY: Flash Encryption ===
CONFIG_SECURE_FLASH_ENC_ENABLED=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y
CONFIG_SECURE_FLASH_ENCRYPTION_KEYSIZE_AES_256=y

# === SECURITY: NVS Encryption ===
CONFIG_NVS_ENCRYPTION=y
CONFIG_NVS_SEC_KEY_PROTECTION_SCHEME=NVS_SEC_SCHEME_FLASH_ENC

# === SECURITY: Secure Boot v2 ===
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y
CONFIG_SECURE_BOOT_SIGNING_KEY="keys/secure_boot_signing_key.pem"

# === Partition Table ===
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

### 6.3 Checkliste vor Aktivierung

- [ ] OTA-Funktionalität implementieren (aktuell nur `factory` Partition)
- [ ] Signing Key generieren und sicher verwahren
- [ ] `nvs_keys`-Partition in `partitions.csv` einfügen
- [ ] Testen auf separatem Dev-Board (nicht auf dem einzigen T-Deck!)
- [ ] NVS-Migration testen (bestehende Daten gehen beim ersten verschlüsselten Boot verloren)
- [ ] `smp_storage_self_test()` nach Encryption-Aktivierung validieren
- [ ] SD-Karten-Daten sind NICHT durch Flash Encryption geschützt (separates Thema)
