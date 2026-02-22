# ESP32 BLE Security: SMP and BluFi

Sources:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/smp.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/blufi.html

---

## Table of Contents

1. [Security Manager Protocol (SMP)](#1-security-manager-protocol-smp)
   - 1.1 [Core Concepts](#11-core-concepts)
   - 1.2 [Pairing Methods](#12-pairing-methods)
   - 1.3 [Legacy Pairing vs LE Secure Connections](#13-legacy-pairing-vs-le-secure-connections)
   - 1.4 [Encryption Flows](#14-encryption-flows)
   - 1.5 [Key Types and Distribution](#15-key-types-and-distribution)
   - 1.6 [Bonding and Key Storage](#16-bonding-and-key-storage)
   - 1.7 [Security Modes and Levels](#17-security-modes-and-levels)
   - 1.8 [SMP API and Configuration](#18-smp-api-and-configuration)
2. [BluFi Protocol](#2-blufi-protocol)
   - 2.1 [What BluFi Is](#21-what-blufi-is)
   - 2.2 [Architecture and GATT Service Structure](#22-architecture-and-gatt-service-structure)
   - 2.3 [Frame Format](#23-frame-format)
   - 2.4 [Frame Control Field](#24-frame-control-field)
   - 2.5 [Frame Types and Subtypes](#25-frame-types-and-subtypes)
   - 2.6 [Security Mode Configuration Frame](#26-security-mode-configuration-frame)
   - 2.7 [ACK Frame](#27-ack-frame)
   - 2.8 [BluFi Security Implementation](#28-blufi-security-implementation)
   - 2.9 [Provisioning Flow](#29-provisioning-flow)
   - 2.10 [BluFi API](#210-blufi-api)
3. [Appendix: Status and Recommendations](#3-appendix-status-and-recommendations)

---

## 1. Security Manager Protocol (SMP)

The Security Manager Protocol operates within Bluetooth LE's GAP module. It is responsible for:

- Generating encryption keys and identity keys
- Defining and executing pairing protocols
- Enabling secure communication between devices

SMP requires an established data link layer (LL) connection to operate. It calls encryption APIs in the BLE GAP layer, registers callbacks, and surfaces encryption status through event return values.

### 1.1 Core Concepts

**Pairing**
The process by which two devices establish a connection at a defined security level. Pairing negotiates the method, exchanges or generates keys, and optionally bonds the devices.

**Bonding**
Bonding extends pairing by persistently storing the exchanged security keys (LTK, CSRK, IRK) so they can be reused on future connections without repeating the full pairing procedure. Bonding is optional — pairing can occur without it. Bonding must be initiated by the **master (central) device** after a connection has been established.

**Authentication**
Verification of a device's identity and confirmation of security attributes. During pairing, a Short-Term Key (STK) is generated to authenticate the session. Devices that possess input/output capabilities or Out-of-Band (OOB) channels can achieve Man-in-the-Middle (MITM) protection. The "Just Works" method does not provide MITM protection.

**Authorization**
Granting permission to perform specific operations at the application layer, distinct from the lower-level authentication. This is an application-layer decision even after secure pairing completes.

### 1.2 Pairing Methods

The pairing method used is determined during the "Phase 1: Pairing Feature Exchange" step based on the IO capabilities declared by both devices and whether OOB data is available.

#### Just Works

- Numerically derived passkey is not shown to the user; both sides accept automatically.
- Provides **no MITM protection**.
- Used when at least one device has no display and no keyboard (e.g., headless sensors).
- Suitable only for environments where eavesdropping and active attacks are not a concern.

#### Passkey Entry

- One device displays a 6-digit passkey; the other device's user enters it.
- Provides **MITM protection** against passive eavesdroppers.
- The passkey serves as input to the STK (Legacy) or to the commitment scheme (Secure Connections).
- Variants:
  - Initiator displays, responder enters.
  - Responder displays, initiator enters.
  - Both enter (keyboard-only on both sides, both use the same passkey).

#### Numeric Comparison (LE Secure Connections only)

- Both devices display a 6-digit number; the user confirms they match on both.
- Provides **MITM protection**.
- Not available in Legacy Pairing.

#### Out-of-Band (OOB)

- Cryptographic data is exchanged via a channel other than BLE (e.g., NFC, QR code, a pre-shared backend secret).
- Provides **MITM protection** if the OOB channel itself is secure.
- If OOB data is available on both sides, OOB takes priority over IO capability negotiation.

### 1.3 Legacy Pairing vs LE Secure Connections

#### Legacy Pairing

- Defined in Bluetooth 4.0/4.1.
- Uses the **TK (Temporary Key)** → **STK (Short-Term Key)** → **LTK (Long-Term Key)** derivation chain.
- The STK is derived from TK (which is 0 for Just Works, or the 6-digit PIN for Passkey Entry) and random nonces exchanged between devices.
- The STK is used to encrypt the connection during Phase 2; the LTK is then distributed over this encrypted link in Phase 3.
- **Vulnerability:** The STK derivation is susceptible to brute-force if the TK is short (e.g., Just Works TK = 0, Passkey TK = 000000–999999). A passive eavesdropper who records the pairing exchange can offline-attack the STK and decrypt subsequent traffic.
- Keys generated: STK (ephemeral), LTK + EDIV + RAND (long-term), optionally IRK, CSRK.

#### LE Secure Connections (LESC)

- Introduced in Bluetooth 4.2.
- Uses **Elliptic Curve Diffie-Hellman (ECDH)** on the P-256 curve for key agreement.
- Both devices generate an ECDH key pair (public + private). Public keys are exchanged.
- The **DHKey** (shared ECDH secret) is computed independently on both sides.
- The **LTK** is derived directly from the DHKey (using a CMAC-based key derivation function), never transmitted over the air.
- Because the LTK is derived from ECDH and never sent, passive eavesdropping of the pairing exchange does not compromise the LTK.
- Numeric Comparison association model is only available with LESC.
- Keys generated: LTK (derived), optionally IRK, CSRK. No EDIV/RAND needed (LTK is the same on both sides).

| Property | Legacy Pairing | LE Secure Connections |
|---|---|---|
| Key agreement | TK + random nonces | ECDH P-256 |
| LTK transmitted? | Yes (encrypted by STK) | No (derived locally) |
| Brute-force risk | High (short TK) | Negligible (ECDH) |
| Numeric Comparison | No | Yes |
| Min Bluetooth version | 4.0 | 4.2 |
| Forward secrecy | No | No (LTK reused across sessions) |

### 1.4 Encryption Flows

#### Initial Encryption (No Prior Bond)

This covers the case where two devices pair for the first time without an existing bond.

```
Initiator                                Responder
   |                                         |
   |--- Pairing Request (IO caps, OOB, ...) ->|
   |<-- Pairing Response (IO caps, OOB,...) --|
   |                                         |
   |     [Phase 2: Key Generation]            |
   |  Legacy: exchange TK, nonces -> STK     |
   |  LESC:   exchange ECDH pubkeys -> LTK   |
   |                                         |
   |--- LL_START_ENC_REQ (EDIV, RAND, STK) ->|
   |<-- LL_START_ENC_RSP -------------------|
   |                                         |
   |  [Connection is now encrypted with STK  |
   |   (Legacy) or LTK (LESC)]               |
   |                                         |
   |  [Phase 3 (if bonding): key dist.]      |
   |<-- LTK, IRK, CSRK (if Legacy) ---------|
   |--- LTK, IRK, CSRK (if Legacy) -------->|
```

#### Reconnection Encryption (Bonded Devices)

When devices have previously bonded and stored a LTK:

```
Initiator (Central)                      Responder (Peripheral)
   |                                         |
   |  [Connection established]               |
   |                                         |
   |--- LL_START_ENC_REQ (EDIV, RAND) ------>|
   |   (Legacy: EDIV+RAND identify the LTK)  |
   |   (LESC:  no EDIV/RAND needed)          |
   |                                         |
   |  Responder looks up LTK in bond DB      |
   |<-- LL_START_ENC_RSP --------------------|
   |                                         |
   |  [Connection encrypted with stored LTK] |
```

#### Just Works Pairing Flow (Legacy)

```
Initiator                                Responder
   |--- Pairing Request --------------------->|
   |<-- Pairing Response --------------------|
   |                                         |
   |  TK = 0 (on both sides, not exchanged)  |
   |                                         |
   |--- Mconfirm (Confirm value) ------------>|
   |<-- Sconfirm (Confirm value) ------------|
   |--- Mrand (Random value) ---------------->|
   |<-- Srand (Random value) ---------------- |
   |                                         |
   |  Both compute STK = s1(TK, Srand, Mrand)|
   |--- LL_START_ENC_REQ (STK) -------------->|
   |<-- LL_START_ENC_RSP --------------------|
   |  [Encrypted with STK]                   |
```

#### Passkey Entry Pairing Flow (Legacy)

```
Initiator                                Responder
   |--- Pairing Request --------------------->|
   |<-- Pairing Response --------------------|
   |                                         |
   |  TK = 6-digit passkey (user input)      |
   |  (entered on one or both sides)         |
   |                                         |
   |--- Mconfirm --------------------------->|
   |<-- Sconfirm ----------------------------|
   |--- Mrand ------------------------------>|
   |<-- Srand -------------------------------|
   |                                         |
   |  Both compute STK = s1(TK, Srand, Mrand)|
   |--- LL_START_ENC_REQ (STK) -------------->|
   |<-- LL_START_ENC_RSP --------------------|
   |  [Encrypted with STK]                   |
```

### 1.5 Key Types and Distribution

#### Short-Term Key (STK)

- Generated during Legacy Pairing Phase 2.
- Used to encrypt the BLE connection during the current pairing session only.
- Never stored; discarded after Phase 3 key distribution is complete.
- Derived from: `s1(TK, Srand, Mrand)` using AES-128.

#### Long-Term Key (LTK)

- The primary key for re-encrypting a bonded connection.
- In Legacy Pairing: generated by the responder (slave), distributed to the initiator over the STK-encrypted link. Identified by EDIV + RAND values.
- In LE Secure Connections: derived on both sides from the ECDH DHKey; never transmitted.
- Size: 128 bits.
- Algorithm: AES-128 CCM used for BLE link encryption.

#### EDIV and RAND (Legacy only)

- EDIV: 16-bit Encrypted Diversifier.
- RAND: 64-bit random value.
- Together they identify which LTK to look up in the bond database during reconnection.

#### Identity Resolving Key (IRK)

- Used to generate and resolve Resolvable Private Addresses (RPAs).
- Allows a bonded device to recognize a peer even when the peer is using a rotating private Bluetooth address.
- Size: 128 bits.
- Distributed during Phase 3 if the device uses RPAs.

#### Connection Signature Resolving Key (CSRK)

- Used to sign data on unencrypted ATT connections (data signing, not encryption).
- Allows the receiver to verify the authenticity of signed ATT writes without requiring link-layer encryption.
- Size: 128 bits.
- Distributed during Phase 3 if data signing is required.

#### Key Distribution Summary

Phase 3 (Key Distribution) occurs over the encrypted STK/LTK link after Phase 2. Each side independently indicates in the Pairing Request/Response which keys it will send and which it wants to receive.

| Key | Who distributes | Direction | Purpose |
|---|---|---|---|
| LTK + EDIV + RAND | Responder (Legacy) | Responder → Initiator | Re-encryption |
| LTK | Derived locally (LESC) | Not transmitted | Re-encryption |
| IRK | Either side | Both directions | Address resolution |
| CSRK | Either side | Both directions | Data signing |
| BD_ADDR / Identity | With IRK | Both directions | Pairing with RPA |

### 1.6 Bonding and Key Storage

After key distribution, both devices store the received keys in a non-volatile **bond database** (NVS in ESP-IDF). The bond database entry for each peer typically contains:

- Peer Bluetooth address and address type (or IRK for address resolution)
- LTK (and EDIV + RAND for Legacy)
- IRK (if distributed)
- CSRK (if distributed)
- Security level achieved during pairing

On reconnection, the central uses the peer's address (or resolves its RPA via the stored IRK) to look up the bond entry and initiates encryption using the stored LTK.

**Initiating bond from master (GAP API call):**

```c
// After connection, master calls:
esp_ble_set_encryption(remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
// or
esp_ble_set_encryption(remote_bda, ESP_BLE_SEC_ENCRYPT_NO_MITM);
```

**Registering security callbacks:**

```c
esp_ble_gap_register_callback(gap_event_handler);
```

Relevant GAP events for SMP:

| Event | Description |
|---|---|
| `ESP_GAP_BLE_AUTH_CMPL_EVT` | Authentication complete; contains status and auth level |
| `ESP_GAP_BLE_PASSKEY_NOTIF_EVT` | Local passkey to display to user |
| `ESP_GAP_BLE_PASSKEY_REQ_EVT` | Request user to enter passkey |
| `ESP_GAP_BLE_NC_REQ_EVT` | Numeric comparison confirmation request |
| `ESP_GAP_BLE_OOB_REQ_EVT` | OOB data requested |
| `ESP_GAP_BLE_SEC_REQ_EVT` | Security request from peer |
| `ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT` | Bond removal complete |
| `ESP_GAP_BLE_GET_BOND_DEV_COMPLETE_EVT` | Bond device list retrieved |

**Setting security parameters:**

```c
esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
// Flags: ESP_LE_AUTH_BOND, ESP_LE_AUTH_REQ_MITM,
//        ESP_LE_AUTH_REQ_SC_ONLY, ESP_LE_AUTH_REQ_SC_MITM_BOND, etc.

esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE; // or IO_CAP_IN, IO_CAP_OUT, IO_CAP_KBDISP, etc.
uint8_t key_size = 16; // 7–16 bytes
uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
uint8_t rsp_key  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,   &auth_req, sizeof(auth_req));
esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,        &iocap,    sizeof(iocap));
esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,      &key_size, sizeof(key_size));
esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,      &init_key, sizeof(init_key));
esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,       &rsp_key,  sizeof(rsp_key));
```

**Responding to passkey events:**

```c
// Display passkey:
case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
    ESP_LOGI(TAG, "Passkey: %06d", param->ble_security.key_notif.passkey);
    break;

// Enter passkey:
case ESP_GAP_BLE_PASSKEY_REQ_EVT:
    esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 123456);
    break;

// Numeric comparison:
case ESP_GAP_BLE_NC_REQ_EVT:
    // Show param->ble_security.key_notif.passkey to user, get confirmation
    esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
    break;
```

### 1.7 Security Modes and Levels

Bluetooth LE defines two security modes, each with multiple levels. The mode/level required for a given GATT characteristic is declared in its ATT permissions.

#### Security Mode 1 (Encryption-based)

| Level | Description | MITM | Encryption |
|---|---|---|---|
| Level 1 | No security (open) | No | No |
| Level 2 | Unauthenticated pairing with encryption | No | Yes |
| Level 3 | Authenticated pairing with encryption | Yes | Yes |
| Level 4 | Authenticated LE Secure Connections pairing with 128-bit key | Yes | Yes (LESC) |

- **Level 1**: No requirements. Any device can read/write.
- **Level 2**: Just Works pairing is sufficient. Link is encrypted but not authenticated.
- **Level 3**: Passkey Entry, OOB, or Numeric Comparison (LESC) required. Provides MITM protection.
- **Level 4**: Requires LESC with Numeric Comparison or OOB. Enforces 128-bit LTK. Strongest standard mode.

#### Security Mode 2 (Signing-based, unencrypted)

| Level | Description | MITM | Encryption |
|---|---|---|---|
| Level 1 | Unauthenticated data signing | No | No |
| Level 2 | Authenticated data signing | Yes | No |

Mode 2 uses CSRK signing for ATT attribute writes without link encryption. Less common in practice.

### 1.8 SMP API and Configuration

**Key `esp_ble_gap_set_security_param` parameters:**

| Parameter ID | Type | Description |
|---|---|---|
| `ESP_BLE_SM_AUTHEN_REQ_MODE` | `esp_ble_auth_req_t` | Authentication requirements (bond, MITM, SC) |
| `ESP_BLE_SM_IOCAP_MODE` | `esp_ble_io_cap_t` | IO capabilities of local device |
| `ESP_BLE_SM_MAX_KEY_SIZE` | `uint8_t` | Maximum LTK size (7–16 bytes; use 16) |
| `ESP_BLE_SM_MIN_KEY_SIZE` | `uint8_t` | Minimum acceptable key size |
| `ESP_BLE_SM_SET_INIT_KEY` | `uint8_t` (bitmask) | Keys initiator distributes |
| `ESP_BLE_SM_SET_RSP_KEY` | `uint8_t` (bitmask) | Keys responder distributes |
| `ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH` | `uint8_t` | Reject peers below specified auth level |
| `ESP_BLE_SM_OOB_SUPPORT` | `uint8_t` | Enable OOB pairing |

**IO Capability values (`esp_ble_io_cap_t`):**

| Value | Constant | Input | Output | Typical device |
|---|---|---|---|---|
| 0 | `ESP_IO_CAP_OUT` | No | Yes (display) | Display-only |
| 1 | `ESP_IO_CAP_IO` | Yes (keyboard) | Yes (display) | Keyboard + display |
| 2 | `ESP_IO_CAP_IN` | Yes (keyboard) | No | Keyboard-only |
| 3 | `ESP_IO_CAP_NONE` | No | No | Headless device |
| 4 | `ESP_IO_CAP_KBDISP` | Yes (yes/no) | Yes (display) | Numeric comparison capable |

**Auth request flags:**

| Constant | Description |
|---|---|
| `ESP_LE_AUTH_NO_BOND` | No bonding |
| `ESP_LE_AUTH_BOND` | Bonding requested |
| `ESP_LE_AUTH_REQ_MITM` | MITM protection required |
| `ESP_LE_AUTH_REQ_SC_ONLY` | LE Secure Connections only (no legacy fallback) |
| `ESP_LE_AUTH_REQ_SC_BOND` | SC + Bonding |
| `ESP_LE_AUTH_REQ_SC_MITM` | SC + MITM |
| `ESP_LE_AUTH_REQ_SC_MITM_BOND` | SC + MITM + Bonding (most secure) |

**Key mask bits:**

| Bit constant | Key |
|---|---|
| `ESP_BLE_ENC_KEY_MASK` | LTK (encryption key) |
| `ESP_BLE_ID_KEY_MASK` | IRK + BD_ADDR (identity key) |
| `ESP_BLE_CSR_KEY_MASK` | CSRK (signing key) |
| `ESP_BLE_LINK_KEY_MASK` | BR/EDR link key (for dual-mode) |

---

## 2. BluFi Protocol

### 2.1 What BluFi Is

BluFi is a Wi-Fi network configuration (provisioning) protocol for ESP32 that operates over a Bluetooth Low Energy channel. Its purpose is to securely transmit Wi-Fi credentials (SSID, password, operating mode) from a mobile application to an ESP32 device that has not yet been connected to a Wi-Fi network.

BluFi handles:
- **Fragmentation** of data that exceeds BLE MTU limits
- **Data encryption** (default: 128-bit AES, negotiated via DH key exchange)
- **Checksum verification** (default: CRC16)
- Wi-Fi configuration for Station (STA) mode, SoftAP mode, or combined STA+SoftAP mode

> **Maintenance notice**: BluFi is in maintenance mode; no new features are planned. Espressif recommends the `network_provisioning` component for new projects.

### 2.2 Architecture and GATT Service Structure

BluFi implements a GATT server on the ESP32. The mobile app acts as a GATT client.

```
Mobile App (GATT Client)
        |
        | BLE (GATT over ATT)
        |
ESP32 (GATT Server)
  └── BluFi Service  UUID: 0xFFFF
        ├── Characteristic 0xFF01  [WRITE, WRITE_NO_RSP]
        │     Mobile app → ESP32
        │     Carries: control frames, data frames from phone
        │
        └── Characteristic 0xFF02  [READ, NOTIFY]
              ESP32 → Mobile app
              Carries: status responses, error reports, Wi-Fi scan results
```

**Service UUID:** `0xFFFF` (16-bit)
**Write Characteristic UUID:** `0xFF01` — writable by the mobile app (phone-to-device channel)
**Notify Characteristic UUID:** `0xFF02` — readable and notifiable (device-to-phone channel)

The mobile app must subscribe to notifications on `0xFF02` to receive asynchronous responses from the ESP32.

### 2.3 Frame Format

Every BluFi frame (both directions) uses the following structure:

```
+--------+---------------+----------------+-------------+------------------+-----------+
|  Type  | Frame Control | Sequence Number | Data Length |      Data        | CheckSum  |
| 1 byte |    1 byte     |    1 byte       |   1 byte    |  variable length | 2 bytes   |
|        |               |                 |             |                  | (MSB last)|
+--------+---------------+----------------+-------------+------------------+-----------+
```

**Type (1 byte):** Encodes both the frame type category (bits [1:0]) and the subtype (bits [7:2]).

**Frame Control (1 byte):** Bitfield of flags controlling encryption, checksum, direction, ACK, and fragmentation.

**Sequence Number (1 byte):** Monotonically increasing counter. Starts at 0, increments by 1 for every frame sent regardless of frame type or direction. Resets to 0 on each new GATT connection. Used to detect replay attacks and out-of-order frames.

**Data Length (1 byte):** Length of the Data field in bytes.

**Data (variable):** The payload. Content depends on frame type/subtype. May be encrypted.

**CheckSum (2 bytes, MSB first):** CRC16 computed over: `Sequence Number || Data Length || Data (plaintext)`. Present only when the checksum flag is set in Frame Control.

#### Fragmented Frame Data Layout

When the Frame Control fragment bit is set, the Data field begins with a 2-byte **Total Content Length** field (big-endian) indicating the total unfragmented payload size, followed by the fragment's portion of the payload:

```
+----------------------+------------------------+
| Total Content Length | Fragment Payload Chunk |
|       2 bytes        |       variable         |
+----------------------+------------------------+
```

### 2.4 Frame Control Field

```
Bit 7  Bit 6  Bit 5  Bit 4  Bit 3  Bit 2  Bit 1  Bit 0
  0      0      0      0      |      |      |      |
                            [ACK] [Frag] [Dir]  [Enc] [Chk]
```

| Bit | Mask | Name | Meaning when set |
|---|---|---|---|
| 0 | `0x01` | Encrypted | Data field is encrypted |
| 1 | `0x02` | Checksum | CheckSum field is appended |
| 2 | `0x04` | Direction | 1 = device→phone, 0 = phone→device |
| 3 | `0x08` | ACK Required | Sender expects an ACK frame in response |
| 4 | `0x10` | Fragment | Frame is a fragment; Total Content Length present in Data |

### 2.5 Frame Types and Subtypes

The **Type** byte encodes:
- **Bits [1:0]**: Frame category (0 = Control, 1 = Data)
- **Bits [7:2]**: Subtype (6-bit value, giving up to 64 subtypes per category)

#### Control Frames (Type bits [1:0] = 0b00)

Control frames are never encrypted. They may optionally carry a checksum.

| Subtype | Value | Name | Description |
|---|---|---|---|
| 0 | `0x0` | ACK | Acknowledge receipt of a frame. Data = 2-byte acked sequence number. |
| 1 | `0x1` | Set Security Mode | Configure encryption/checksum for subsequent data frames. |
| 2 | `0x2` | Set Wi-Fi Op Mode | Set NULL / STA / SoftAP / STA+SoftAP operation mode. |
| 3 | `0x3` | Connect to AP | Trigger the ESP32 to connect to the configured AP. |
| 4 | `0x4` | Disconnect from AP | Disconnect from the current AP. |
| 5 | `0x5` | Query Wi-Fi Status | Request the device to report its current Wi-Fi state. |
| 6 | `0x6` | Disconnect STA from SoftAP | Remove a connected station (Data = MAC address of STA). |
| 7 | `0x7` | Query Version | Request firmware/BluFi version. |
| 8 | `0x8` | Disconnect GATT Link | Instruct ESP32 to terminate the BLE connection. |
| 9 | `0x9` | Scan Wi-Fi List | Instruct ESP32 to scan for available APs. |

#### Data Frames (Type bits [1:0] = 0b01)

Data frames may be encrypted and/or checksummed per the negotiated security mode.

| Subtype | Value | Name | Description |
|---|---|---|---|
| 0 | `0x0` | Negotiation Data | Key negotiation payload; passed to application negotiation callback. |
| 1 | `0x1` | Station BSSID | BSSID of target AP (used for hidden SSID networks). |
| 2 | `0x2` | Station SSID | SSID of the target AP for Station mode. |
| 3 | `0x3` | Station Password | Password/PSK for the target AP. |
| 4 | `0x4` | SoftAP SSID | SSID for the ESP32's own SoftAP. |
| 5 | `0x5` | SoftAP Password | Password for the ESP32's SoftAP. |
| 6 | `0x6` | SoftAP Max Connections | Max connected stations (1–4). |
| 7 | `0x7` | SoftAP Auth Mode | `OPEN` / `WEP` / `WPA_PSK` / `WPA2_PSK` / `WPA_WPA2_PSK`. |
| 8 | `0x8` | SoftAP Channel | Channel number (1–14). |
| 9 | `0x9` | Username | Username for enterprise Wi-Fi (EAP). |
| 10 | `0xA` | CA Certificate | CA certificate for enterprise Wi-Fi. |
| 11 | `0xB` | Client Certificate | Client certificate for enterprise Wi-Fi. |
| 12 | `0xC` | Server Certificate | Server certificate for enterprise Wi-Fi. |
| 13 | `0xD` | Client Private Key | Client private key for enterprise Wi-Fi. |
| 14 | `0xE` | Server Private Key | Server private key for enterprise Wi-Fi. |
| 15 | `0xF` | Wi-Fi Connection State | Status report (opmode, STA state, SoftAP state). |
| 16 | `0x10` | Version | Firmware version as major.minor bytes. |
| 17 | `0x11` | Wi-Fi AP List | Scanned network list: per-entry format is length (1B) + RSSI (1B) + SSID. |
| 18 | `0x12` | Error Report | Error code indicating failure type (see error codes below). |
| 19 | `0x13` | Custom Data | Application-specific payload passed to app callback. |
| 20 | `0x14` | Max Reconnect Count | Maximum number of reconnect attempts. |
| 21 | `0x15` | Connection End Reason | Reason code for disconnect. |
| 22 | `0x16` | RSSI at Disconnect | RSSI value (-128 if unavailable). |

**Error Report codes (subtype 0x12 data field):**

| Code | Error |
|---|---|
| 0 | Sequence number error |
| 1 | Checksum error |
| 2 | Decrypt error |
| 3 | Encrypt error |
| 4 | Init error |
| 5 | DH (key negotiation) error |
| 6 | Data format error |

### 2.6 Security Mode Configuration Frame

The Set Security Mode control frame (subtype `0x1`) carries a 1-byte data payload:

```
Bits [7:4]: Security mode for Control frames
Bits [3:0]: Security mode for Data frames
```

Each nibble encodes:

| Nibble value | Checksum | Encryption |
|---|---|---|
| `0b0000` (0) | No | No |
| `0b0001` (1) | Yes | No |
| `0b0010` (2) | No | Yes |
| `0b0011` (3) | Yes | Yes |

Example: `0x31` means Control frames use checksum+encryption (3), Data frames use checksum only (1).

Note: Control frames are **never encrypted** even if the control nibble requests encryption; the spec reserves encryption for data frames only. The checksum for control frames is still valid.

### 2.7 ACK Frame

The ACK control frame acknowledges receipt of a frame identified by its sequence number.

```
+------+---------------+---------+--------+---------------------------+-----------+
| Type | Frame Control | Seq Num | Len=2  |  Acked Sequence Number    | CheckSum  |
| ACK  |               |         |        |  2 bytes (big-endian)     |           |
+------+---------------+---------+--------+---------------------------+-----------+
```

The Acked Sequence Number in the Data field is the sequence number of the frame being acknowledged.

### 2.8 BluFi Security Implementation

#### Key Negotiation

BluFi's default security flow uses **Diffie-Hellman (DH)** key exchange to establish a shared symmetric key without transmitting the key over BLE. Alternative algorithms (RSA, ECC) can be used by providing custom security callbacks.

Flow:
1. Mobile app generates DH parameters and its own DH public key.
2. Mobile sends DH public key to ESP32 via Data frame subtype `0x0` (Negotiation Data).
3. ESP32's application callback (`esp_blufi_negotiate_data_handler_t`) processes the negotiation data, generates its own DH key pair, computes the shared secret, and replies with its public key.
4. Mobile computes the same shared secret from ESP32's public key.
5. Both sides now have an identical shared secret, used as the AES key.

The **IV8** value (the low 8 bits of the frame sequence number) is provided to the encrypt/decrypt functions as an initialization vector seed, enabling different IV per frame.

#### Encryption

- Default algorithm: **AES-128** (128-bit key, symmetric block cipher).
- Encryption applies to the **Data field only** of data frames (when enabled).
- The encrypted data must be the **same length** as the plaintext (no padding expansion exposed externally — the implementation must handle this, e.g., by using AES in CTR or similar stream-compatible mode).
- Custom encrypt/decrypt functions can be registered.

#### Checksum

- Default algorithm: **CRC16**.
- Computed over: `Sequence Number (1B) || Data Length (1B) || Plaintext Data`.
- Appended as 2 bytes (MSB first) at the end of the frame.
- The receiver recomputes the CRC and compares before processing.

#### Replay Protection

- The monotonically incrementing **Sequence Number** provides replay detection.
- Sequence numbers reset on each new GATT connection.

#### Custom Security Callbacks

Registered via `esp_blufi_register_callbacks()` as part of the `esp_blufi_callbacks_t` structure:

```c
typedef struct {
    esp_blufi_event_cb_t         event_cb;
    esp_blufi_negotiate_data_handler_t  negotiate_data_handler;
    esp_blufi_encrypt_func_t     encrypt_func;
    esp_blufi_decrypt_func_t     decrypt_func;
    esp_blufi_checksum_func_t    checksum_func;
} esp_blufi_callbacks_t;
```

Callback signatures:

```c
// Called when Negotiation Data frame received from phone
void negotiate_data_handler(uint8_t *data, int len,
                             uint8_t **output_data, int *output_len,
                             bool *need_free);

// Encrypt data in-place (data length must not change)
// iv8: low byte of frame sequence number
int encrypt_func(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

// Decrypt data in-place (data length must not change)
int decrypt_func(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

// Compute checksum over buf[0..len-1]
uint16_t checksum_func(uint8_t iv8, uint8_t *buf, int len);
```

### 2.9 Provisioning Flow

#### Full Station Mode Provisioning Flow

```
Mobile App                                          ESP32
    |                                                  |
    |  [BLE scan — discover ESP32 advertising BluFi]   |
    |                                                  |
    |--- BLE Connect --------------------------------->|
    |<-- Connection established ----------------------|
    |                                                  |
    |  [Subscribe to 0xFF02 notifications]             |
    |                                                  |
    |=== Phase 1: Key Negotiation =================== |
    |                                                  |
    |--- Write 0xFF01: Data frame (Negotiation Data) ->|
    |   Subtype 0x0, contains DH public key           |
    |                                                  |
    |   ESP32 negotiate_data_handler called            |
    |   ESP32 computes shared secret                   |
    |                                                  |
    |<-- Notify 0xFF02: Data frame (Negotiation Data)--|
    |   Contains ESP32's DH public key                |
    |                                                  |
    |   Mobile computes shared secret                  |
    |   Both sides now have AES key                    |
    |                                                  |
    |=== Phase 2: Set Security Mode ================= |
    |                                                  |
    |--- Write 0xFF01: Control frame (Set Sec Mode) -->|
    |   Subtype 0x1, data = 0x03 (enc+crc for data)  |
    |                                                  |
    |=== Phase 3: Send Wi-Fi Configuration ========== |
    |                                                  |
    |--- Write 0xFF01: Data frame (Set Op Mode) ------>|
    |   Subtype 0x2 (control), mode = STA            |
    |                                                  |
    |--- Write 0xFF01: Data frame (Station SSID) ----->|
    |   Subtype 0x2, encrypted, contains SSID        |
    |                                                  |
    |--- Write 0xFF01: Data frame (Station Password) ->|
    |   Subtype 0x3, encrypted, contains password    |
    |                                                  |
    |--- Write 0xFF01: Control frame (Connect AP) ---->|
    |   Subtype 0x3                                   |
    |                                                  |
    |=== Phase 4: Wait for Connection Result ======== |
    |                                                  |
    |   ESP32 attempts Wi-Fi connection               |
    |                                                  |
    |<-- Notify 0xFF02: Data frame (Wi-Fi State) ------|
    |   Subtype 0xF, opmode + STA connected state    |
    |                                                  |
    |=== Phase 5: Disconnect BLE (optional) ========= |
    |                                                  |
    |--- Write 0xFF01: Control frame (Disconnect) ---->|
    |   Subtype 0x8                                   |
    |                                                  |
    |<-- BLE disconnected -----------------------------|
```

#### SoftAP Mode Provisioning Flow

Similar to STA flow, but uses Data subtypes `0x4` (SoftAP SSID), `0x5` (SoftAP Password), `0x6` (max connections), `0x7` (auth mode), `0x8` (channel) instead of STA subtypes. Op mode set to SoftAP (`0x2`) or STA+SoftAP (`0x3`).

#### Wi-Fi Scan Flow

```
Mobile App                                          ESP32
    |                                                  |
    |--- Write 0xFF01: Control frame (Scan) ---------->|
    |   Subtype 0x9                                   |
    |                                                  |
    |   ESP32 performs Wi-Fi scan                     |
    |                                                  |
    |<-- Notify 0xFF02: Data frame (Wi-Fi AP List) ----|
    |   Subtype 0x11                                  |
    |   Format per AP: [len:1][rssi:1][ssid:len-1]   |
    |   Multiple APs concatenated                     |
```

#### Wi-Fi Connection State Report (Subtype 0xF) Data Format

```
+----------+---------------+------------------+
| Op Mode  |  STA State    |  SoftAP State    |
|  1 byte  |   1 byte      |    1 byte        |
+----------+---------------+------------------+
```

- **Op Mode**: 0=NULL, 1=STA, 2=SoftAP, 3=STA+SoftAP
- **STA State**: 0=idle, 1=connecting, 2=connected, 3=got IP, 4=connection failed
- **SoftAP State**: 0=idle, 1=started, 2=stopped

### 2.10 BluFi API

**Initialization:**

```c
#include "esp_blufi_api.h"
#include "esp_blufi.h"

// 1. Initialize Bluetooth controller and host (NimBLE or Bluedroid)
esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
esp_bt_controller_init(&bt_cfg);
esp_bt_controller_enable(ESP_BT_MODE_BLE);
esp_bluedroid_init();
esp_bluedroid_enable();

// 2. Register GAP and GATT callbacks (for BLE stack)
esp_ble_gap_register_callback(gap_event_handler);
esp_ble_gatts_register_callback(gatts_event_handler);

// 3. Register BluFi callbacks
esp_blufi_callbacks_t blufi_cbs = {
    .event_cb               = blufi_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func           = blufi_aes_encrypt,
    .decrypt_func           = blufi_aes_decrypt,
    .checksum_func          = blufi_crc_checksum,
};
esp_blufi_register_callbacks(&blufi_cbs);

// 4. Initialize BluFi profile
esp_blufi_profile_init();
```

**Sending data to the mobile app:**

```c
// Report Wi-Fi connection state
esp_blufi_extra_info_t info = {
    .sta_bssid_set = true,
    .sta_ssid = (uint8_t *)ssid,
    .sta_ssid_len = strlen(ssid),
};
esp_blufi_send_wifi_conn_report(wifi_mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_conn_num, &info);

// Send custom data to phone
esp_blufi_send_custom_data(data, data_len);

// Send error to phone
esp_blufi_send_error_info(ESP_BLUFI_SEQUENCE_ERROR);
```

**Key BluFi events in the event callback:**

| Event | Trigger |
|---|---|
| `ESP_BLUFI_EVENT_INIT_FINISH` | BluFi profile initialized |
| `ESP_BLUFI_EVENT_DEINIT_FINISH` | BluFi profile deinitialized |
| `ESP_BLUFI_EVENT_BLE_CONNECT` | Mobile app connected via BLE |
| `ESP_BLUFI_EVENT_BLE_DISCONNECT` | Mobile app disconnected |
| `ESP_BLUFI_EVENT_SET_WIFI_OPMODE` | Op mode frame received |
| `ESP_BLUFI_EVENT_RECV_STA_SSID` | Station SSID received |
| `ESP_BLUFI_EVENT_RECV_STA_PASSWD` | Station password received |
| `ESP_BLUFI_EVENT_RECV_SOFTAP_SSID` | SoftAP SSID received |
| `ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD` | SoftAP password received |
| `ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM` | SoftAP max connections received |
| `ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE` | SoftAP auth mode received |
| `ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL` | SoftAP channel received |
| `ESP_BLUFI_EVENT_GET_WIFI_STATUS` | Query Wi-Fi status received |
| `ESP_BLUFI_EVENT_CONN_TO_AP` | Connect-to-AP command received |
| `ESP_BLUFI_EVENT_DISCONN_FROM_AP` | Disconnect-from-AP command received |
| `ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE` | Disconnect GATT command received |
| `ESP_BLUFI_EVENT_RECV_USERNAME` | Enterprise username received |
| `ESP_BLUFI_EVENT_RECV_CA_CERT` | CA cert received |
| `ESP_BLUFI_EVENT_RECV_CLIENT_CERT` | Client cert received |
| `ESP_BLUFI_EVENT_RECV_SERVER_CERT` | Server cert received |
| `ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY` | Client private key received |
| `ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY` | Server private key received |
| `ESP_BLUFI_EVENT_RECV_CUSTOM_DATA` | Custom data frame received |
| `ESP_BLUFI_EVENT_REPORT_ERROR` | Error condition to report |

**menuconfig (Kconfig) options:**

```
Component config → Bluetooth → Bluedroid Options → BluFi
  CONFIG_BT_BLUFI_ENABLE=y

Component config → Bluetooth → Controller Options
  CONFIG_BT_CTRL_MODE_EFF=1  (BLE only)
```

---

## 3. Appendix: Status and Recommendations

### BluFi

BluFi is **in maintenance mode**. Espressif does not plan new features. For new designs, use the `network_provisioning` component (IDF component registry), which is actively maintained and provides a more modern, feature-complete provisioning solution with support for BLE and SoftAP transport simultaneously.

### SMP Security Recommendations

- Always use **LE Secure Connections** (`ESP_LE_AUTH_REQ_SC_MITM_BOND`) for new designs.
- Set `ESP_BLE_SM_MAX_KEY_SIZE` to 16 (128-bit keys).
- Use Passkey Entry or Numeric Comparison; avoid Just Works for any sensitive application.
- Distribute IRK if the device uses Resolvable Private Addresses (recommended to prevent address tracking).
- Require Security Mode 1 Level 3 or Level 4 on GATT characteristics that expose sensitive data.
- Store bond data in encrypted NVS partitions when the application handles sensitive credentials.

### Key References

- Bluetooth Core Specification 5.x, Vol. 3, Part H — Security Manager Specification
- Bluetooth Core Specification 5.x, Vol. 6, Part B — Link Layer Specification
- ESP-IDF API Reference: `esp_gap_ble_api.h`, `esp_blufi_api.h`
- ESP-IDF example: `examples/bluetooth/bluedroid/ble/blufi/`
- ESP-IDF example: `examples/bluetooth/bluedroid/ble/gatt_security_server/`
