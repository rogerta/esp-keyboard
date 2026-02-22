# ESP32 Bluetooth Low Energy (BLE) Overview

Source documentation:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/overview.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/ble-feature-support-status.html

---

## Introduction

ESP32 supports Dual-Mode Bluetooth 4.2 and is certified for Dual-Mode Bluetooth 4.2 and Bluetooth LE 5.0. The ESP-IDF BLE stack is organized into a layered architecture: a hardware controller at the bottom, one of two host stack implementations in the middle, and profiles and applications at the top.

---

## Stack Architecture

The BLE stack is divided into three tiers:

### 1. ESP Bluetooth Controller (Bottom Layer)

Manages hardware-level operations. Contains the following modules:

- PHY
- Baseband
- Link Controller
- Link Manager
- Device Manager
- HCI (Host Controller Interface)

This layer handles the radio interface and link-level operations and is exposed to higher layers through APIs and libraries.

### 2. Host Stacks (Middle Layer)

Two host stack implementations are available. They are mutually exclusive at build time.

#### ESP-Bluedroid

- Based on Android's Bluedroid stack, modified for ESP-IDF.
- Composed of two sublayers:
  - **BTU (Bluetooth Upper Layer)**: processes lower-layer Bluetooth protocols including L2CAP, GATT/ATT, SMP, GAP, and other profiles.
  - **BTC (Bluetooth Transport Controller)**: interfaces between BTU and the application layer.
- Supports both **Classic Bluetooth** and **Bluetooth LE**.
- Higher heap and flash footprint than NimBLE.

#### ESP-NimBLE

- Based on Apache Mynewt's NimBLE open-source stack.
- **Bluetooth LE only** — does not support Classic Bluetooth.
- Requires less heap and flash size than ESP-Bluedroid.
- Suitable for resource-constrained applications.

### 3. Profiles and Applications (Top Layer)

#### ESP-BLE-MESH

- Based on Zephyr's Bluetooth Mesh implementation.
- Supports provisioning, Proxy, Relay, Low Power, and Friend node features.

#### BluFi

- Provides a secure protocol to pass Wi-Fi configuration and credentials to the ESP32 over Bluetooth.
- Not available on ESP32-C2 and ESP32-H2.

#### Custom Profiles and Applications

User-developed applications and GATT profiles built on top of the host stack APIs.

---

## Chip Support Matrix

| Chip     | Controller | ESP-Bluedroid | ESP-NimBLE | ESP-BLE-MESH | BluFi |
|----------|-----------|--------------|-----------|-------------|-------|
| ESP32    | Y         | Y            | Y         | Y           | Y     |
| ESP32-S3 | Y         | Y            | Y         | Y           | Y     |
| ESP32-C2 | Y         | Y            | Y         | -           | Y     |
| ESP32-C3 | Y         | Y            | Y         | Y           | Y     |
| ESP32-C5 | Y         | Y            | Y         | Y           | Y     |
| ESP32-C6 | Y         | Y            | Y         | Y           | Y     |
| ESP32-C61| Y         | Y            | Y         | Y           | Y     |
| ESP32-H2 | Y         | Y            | Y         | Y           | -     |

Y = supported, - = not available

---

## Protocol Layer Support

### GAP (Generic Access Profile)

Handled in the host layer (BTU in Bluedroid, NimBLE core). GAP manages device discovery, connection establishment, and link security procedures including:

- Advertising (connectable, non-connectable, scannable)
- Scanning
- Connection initiation and parameter negotiation
- Bonding, pairing, and privacy (Link Layer Privacy is fully supported as of BLE 4.2)

### L2CAP (Logical Link Control and Adaptation Protocol)

Processed in the BTU sublayer of ESP-Bluedroid and in the NimBLE core. Provides:

- Protocol multiplexing
- Segmentation and reassembly
- Credit-based flow control (LE CoC — LE Credit-Based Flow Control channels)

### GATT/ATT (Generic Attribute Profile / Attribute Protocol)

Processed in the BTU sublayer. Provides the client/server model for data exchange:

- GATT Server: exposes services and characteristics
- GATT Client: discovers and accesses remote services
- ATT is the underlying protocol carrying GATT PDUs
- **GATT Caching** (BLE 5.1): Experimental status in both Bluedroid and NimBLE
- **Enhanced Attribute Protocol / EATT** (BLE 5.2): Experimental in NimBLE only; unsupported in Bluedroid

### SMP (Security Manager Protocol)

Handled in the host layer alongside GAP. Manages:

- Pairing procedures (LE Legacy Pairing, LE Secure Connections — BLE 4.2 Secure Connections fully supported)
- Key distribution and storage
- Encryption setup

---

## Advertising Modes

The following advertising modes are relevant to ESP32 BLE:

| Mode | Description | Support Status |
|------|-------------|---------------|
| Connectable undirected advertising | Standard discoverable/connectable mode | Supported (legacy) |
| Connectable directed advertising (high duty cycle) | Fast connection to a known peer | Supported (legacy) |
| Non-connectable undirected advertising | Beacon/broadcast only | Supported (legacy) |
| Scannable undirected advertising | Allows scan responses | Supported (legacy) |
| High Duty Cycle Non-Connectable Advertising (BLE 5.0 extended) | Extended advertising type | **Unsupported** on ESP32 |
| LE Advertising Extensions (BLE 5.0) | Extended advertising PDUs with longer payload | **Unsupported** on ESP32 |
| Periodic Advertising with Responses (BLE 5.4) | Bidirectional periodic advertising | **Unsupported** on ESP32 |

Note: All BLE 5.0 and later extended advertising features are unsupported on the ESP32 chip series.

---

## Connection Modes

- Standard LE connection establishment via GAP is supported.
- **LE Channel Selection Algorithm #2** (BLE 5.0): Unsupported.
- **LE Enhanced Connection Update / Connection Subrating** (BLE 5.3): Unsupported.
- **LE Isochronous Channels** (BIS/CIS, BLE 5.2): Unsupported — no support for audio/isochronous data streams.

---

## Qualification

ESP32 is certified for:

- **Dual-Mode Bluetooth 4.2**
- **Bluetooth LE 5.0**

The BLE feature support information provided by Espressif is not a binding commitment and is subject to change without notice. Customers should consult Espressif support teams for current verification of specific feature availability.

---

## BLE Feature Support Status

The table below shows the complete support status of BLE major features on the ESP32 chip series across three implementation layers.

### Status Definitions

| Status | Meaning |
|--------|---------|
| Supported | Feature has completed development and internal testing. |
| Experimental | Feature has been developed and is currently undergoing internal testing. May have issues. |
| Developing (YYYY/MM) | Feature is actively being developed; expected to be supported by end of the indicated month. |
| Unsupported | Feature is not supported on this chip series. |
| NA | Not applicable — either a host-only feature (controller column shows NA) or a controller-only feature (host columns show NA). |

### Full Feature Support Table

| Core Spec | Major Feature | ESP Controller | ESP-Bluedroid Host | ESP-NimBLE Host |
|-----------|--------------|---------------|-------------------|----------------|
| 4.2 | LE Data Packet Length Extension | Supported | Supported | Supported |
| 4.2 | LE Secure Connections | Supported | Supported | Supported |
| 4.2 | Link Layer Privacy | Supported | Supported | Supported |
| 4.2 | Link Layer Extended Filter Policies | Supported | Supported | Supported |
| 5.0 | 2 Msym/s PHY for LE | Unsupported | Unsupported | Unsupported |
| 5.0 | LE Long Range (Coded PHY S=2/S=8) | Unsupported | Unsupported | Unsupported |
| 5.0 | High Duty Cycle Non-Connectable Advertising | Unsupported | Unsupported | Unsupported |
| 5.0 | LE Advertising Extensions | Unsupported | Unsupported | Unsupported |
| 5.0 | LE Channel Selection Algorithm #2 | Unsupported | Unsupported | Unsupported |
| 5.1 | Angle of Arrival (AoA) / Angle of Departure (AoD) | Unsupported | Unsupported | Unsupported |
| 5.1 | GATT Caching | NA (host-only) | Experimental | Experimental |
| 5.1 | Randomized Advertising Channel Indexing | Developing (2026/03) | NA (controller-only) | NA (controller-only) |
| 5.1 | Periodic Advertising Sync Transfer | Unsupported | Unsupported | Unsupported |
| 5.2 | LE Isochronous Channels (BIS/CIS) | Unsupported | Unsupported | Unsupported |
| 5.2 | Enhanced Attribute Protocol (EATT) | NA (host-only) | Unsupported | Experimental |
| 5.2 | LE Power Control | Unsupported | Unsupported | Unsupported |
| 5.3 | AdvDataInfo in Periodic Advertising | Unsupported | Unsupported | Unsupported |
| 5.3 | LE Enhanced Connection Update (Connection Subrating) | Unsupported | Unsupported | Unsupported |
| 5.3 | LE Channel Classification | Unsupported | Unsupported | Unsupported |
| 5.4 | Advertising Coding Selection | Unsupported | Unsupported | Unsupported |
| 5.4 | Encrypted Advertising Data | NA (host-only) | Developing (2025/12) | Experimental |
| 5.4 | LE GATT Security Levels Characteristic | NA (host-only) | Unsupported | Experimental |
| 5.4 | Periodic Advertising with Responses | Unsupported | Unsupported | Unsupported |
| 6.0 | Channel Sounding | Unsupported | Unsupported | Unsupported |
| 6.0 | LL Extended Feature Set | Unsupported | Unsupported | Unsupported |
| 6.0 | Decision-Based Advertising Filtering | Unsupported | Unsupported | Unsupported |
| 6.0 | Enhancements for ISOAL | Unsupported | Unsupported | Unsupported |
| 6.0 | Monitoring Advertisers | Unsupported | Unsupported | Unsupported |
| 6.0 | Frame Space Update | Unsupported | Unsupported | Unsupported |

### Summary by Specification Version

| Core Spec | Total Features | Fully Supported | Experimental | Developing | Unsupported |
|-----------|---------------|-----------------|-------------|-----------|-------------|
| 4.2 | 4 | 4 | 0 | 0 | 0 |
| 5.0 | 5 | 0 | 0 | 0 | 5 |
| 5.1 | 4 | 0 | 2 | 1 | 1 |
| 5.2 | 3 | 0 | 1 | 0 | 2 |
| 5.3 | 3 | 0 | 0 | 0 | 3 |
| 5.4 | 4 | 0 | 2 | 1 | 1 |
| 6.0 | 6 | 0 | 0 | 0 | 6 |

---

## Key Observations

1. **Full BLE 4.2 support**: All four BLE 4.2 core features (Data Packet Length Extension, Secure Connections, Link Layer Privacy, Extended Filter Policies) are fully supported across the controller and both host stacks.

2. **No BLE 5.0 support on ESP32**: All BLE 5.0 features are unsupported on the ESP32 chip series. This includes the 2 Msym/s PHY, Coded PHY (Long Range), and all extended advertising features.

3. **ESP-NimBLE has broader experimental feature coverage**: NimBLE has experimental support for GATT Caching (5.1), EATT (5.2), Encrypted Advertising Data (5.4), and LE GATT Security Levels Characteristic (5.4), whereas Bluedroid has none of these in experimental state except GATT Caching.

4. **No isochronous channel support**: LE Isochronous Channels (BIS/CIS), required for Bluetooth LE Audio, are unsupported. Channel Sounding (6.0) is also unsupported.

5. **In-progress controller work**: Randomized Advertising Channel Indexing (5.1) was targeted for completion by 2026/03. Encrypted Advertising Data (5.4) was targeted for the Bluedroid host by 2025/12.

6. **BLE 6.0 features are all unsupported** on the ESP32 chip series.

---

## Disclaimer

The feature support status information provided in this document reflects the upstream Espressif documentation as of the fetch date. This information is not a binding commitment. Feature support status is subject to change without notice. Consult Espressif support teams for current verification before relying on specific feature availability in production designs.
