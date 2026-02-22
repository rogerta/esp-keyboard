# ESP32-C3 and ESP32-C6 Bluetooth Low Energy Reference

Both the ESP32-C3 and ESP32-C6 are RISC-V based SoCs with BLE-only radios. Neither supports Classic Bluetooth (BR/EDR). This document covers their BLE capabilities, differences from the original ESP32, and differences from each other.

---

## Architecture Overview

### Original ESP32 Comparison

The original ESP32 (Xtensa LX6, dual-core) supports both Classic Bluetooth and BLE. The C3 and C6 drop Classic BT entirely and use RISC-V cores:

| Property | ESP32 | ESP32-C3 | ESP32-C6 |
|---|---|---|---|
| Core | Xtensa LX6 (dual) | RISC-V RV32IMC (single) | RISC-V RV32IMAC (single) |
| Classic BT | Yes (BR/EDR) | No | No |
| BLE | Yes (BLE 4.2) | Yes (BLE 5.0 controller, 5.4 certified) | Yes (BLE 5.0 controller, 5.3 certified) |
| 802.15.4 (Thread/Zigbee) | No | No | Yes |
| WiFi | 802.11 b/g/n | 802.11 b/g/n | 802.11 b/g/n/ax (Wi-Fi 6) |

The Bluetooth controller layer on both C3 and C6 covers PHY, Baseband, Link Controller, Link Manager, Device Manager, and HCI. The host stack runs above HCI and is selectable between Bluedroid and NimBLE.

---

## BLE Specification Versions

### ESP32-C3

- Controller supports Bluetooth 5.0 (LE)
- Certified for Bluetooth LE **5.4** (meaning the implementation has passed SIG qualification at that level)
- All BLE 5.0 PHY and advertising extension features are fully supported in the controller

### ESP32-C6

- Controller supports Bluetooth 5.0 (LE)
- Certified for Bluetooth LE **5.3**
- Supports more BLE 5.3 features in the controller than the C3 (see feature table below)

The certification level is not strictly the same as controller capability. Both chips' controllers implement features beyond their certified version, and implementation status is tracked separately per feature.

---

## Host Stack Support

Both chips support two host stacks selectable at build time via menuconfig.

### ESP-Bluedroid

- Derived from the Android Bluetooth stack
- Implements BTU (Bluetooth Upper) and BTC (Bluetooth Controller) layers
- Supports: GATT, ATT, SMP, GAP, and higher-level profiles (BLE Mesh, BluFi)
- Higher flash and heap footprint than NimBLE
- On C3 and C6, BLE-only; no Classic BT profiles are compiled in

### ESP-NimBLE

- Based on Apache Mynewt NimBLE
- BLE-only stack
- Smaller code size and lower heap usage than Bluedroid
- Preferred for memory-constrained applications
- Some features (Enhanced ATT, LE GATT Security Levels) are available in NimBLE before Bluedroid

Both stacks are available on both the C3 and C6. There is no chip-level restriction that removes one stack option.

### Memory Footprint

Exact figures are not published in the ESP-IDF documentation pages. The documented guidance is:

- NimBLE requires less heap and less flash than Bluedroid
- For applications that only need BLE, NimBLE is the lower-resource option
- A new transport layer between NimBLE host and the ESP controller manages a pool of transport buffers and handles formatting between host and controller

For specific RAM/ROM numbers, consult the `examples/bluetooth/nimble` and `examples/bluetooth/bluedroid` build outputs with `idf.py size` on the target chip.

---

## BLE Feature Support: Detailed Comparison

The tables below use the following status values from the ESP-IDF documentation:

- **Supported** - completed development and internal testing
- **Experimental** - developed but still undergoing internal testing
- **Developing (date)** - in active development, target completion date given
- **Unsupported** - not supported on this chip series
- **NA** - not applicable (host-only or controller-only feature, not relevant to this component)

### BLE 4.2 Features

| Feature | C3 Controller | C3 Bluedroid | C3 NimBLE | C6 Controller | C6 Bluedroid | C6 NimBLE |
|---|---|---|---|---|---|---|
| LE Data Packet Length Extension | Supported | Supported | Supported | Supported | Supported | Supported |
| LE Secure Connections | Supported | Supported | Supported | Supported | Supported | Supported |
| Link Layer Privacy | Supported | Supported | Supported | Supported | Supported | Supported |
| Link Layer Extended Filter Policies | Supported | Supported | Supported | Supported | Supported | Supported |

All BLE 4.2 features are fully supported on both chips across all stack options.

### BLE 5.0 Features

| Feature | C3 Controller | C3 Bluedroid | C3 NimBLE | C6 Controller | C6 Bluedroid | C6 NimBLE |
|---|---|---|---|---|---|---|
| 2 Msym/s PHY for LE (2M PHY) | Supported | Supported | Supported | Supported | Supported | Supported |
| LE Long Range (Coded PHY S=2/S=8) | Supported | Supported | Supported | Supported | Supported | Supported |
| High Duty Cycle Non-Connectable Advertising | Supported | Supported | Supported | Supported | Supported | Supported |
| LE Advertising Extensions | Supported | Supported | Supported | Supported | Supported | Supported |
| LE Channel Selection Algorithm #2 | Supported | Supported | Supported | Supported | Supported | Supported |

All BLE 5.0 features are fully supported on both chips. This means extended advertising (up to 1650 bytes payload), 2M PHY (2x throughput vs 1M), and Coded PHY for long range (up to 4x range vs 1M at S=8) all work on both C3 and C6.

### BLE 5.1 Features

| Feature | C3 Controller | C3 Bluedroid | C3 NimBLE | C6 Controller | C6 Bluedroid | C6 NimBLE |
|---|---|---|---|---|---|---|
| Angle of Arrival/Departure (AoA/AoD, CTE) | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported |
| GATT Caching | NA | Experimental | Experimental | NA | Experimental | Experimental |
| Randomized Advertising Channel Indexing | Developing (2026/03) | NA | NA | Developing (2026/03) | NA | NA |
| Periodic Advertising Sync Transfer (PAST) | Unsupported | Unsupported | Unsupported | Supported | Supported | Supported |

**Key C6 advantage:** Periodic Advertising Sync Transfer (PAST) is supported on C6 but unsupported on C3. This allows an already-synced device to transfer the sync information to another device without that device having to scan for the periodic advertiser itself.

Neither chip supports AoA/AoD (Constant Tone Extension for direction finding).

### BLE 5.2 Features

| Feature | C3 Controller | C3 Bluedroid | C3 NimBLE | C6 Controller | C6 Bluedroid | C6 NimBLE |
|---|---|---|---|---|---|---|
| LE Isochronous Channels (BIS/CIS) | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported |
| Enhanced Attribute Protocol (EATT) | NA | Unsupported | Experimental | NA | Unsupported | Experimental |
| LE Power Control | Experimental | Experimental | Experimental | Experimental | Experimental | Experimental |

LE Isochronous Channels (used for LE Audio / BLE broadcast audio) are not supported on either chip. EATT is available as experimental in NimBLE only on both chips. LE Power Control is experimental on both.

### BLE 5.3 Features

| Feature | C3 Controller | C3 Bluedroid | C3 NimBLE | C6 Controller | C6 Bluedroid | C6 NimBLE |
|---|---|---|---|---|---|---|
| AdvDataInfo in Periodic Advertising | Unsupported | Unsupported | Unsupported | Supported | Supported | Supported |
| LE Enhanced Connection Update | Experimental | Experimental | Experimental | Experimental | Experimental | Experimental |
| LE Channel Classification | Unsupported | Unsupported | Unsupported | Experimental | Experimental | Experimental |

**C6 advantage:** AdvDataInfo in Periodic Advertising and LE Channel Classification are supported (or experimental) on C6 but unsupported on C3. These align with the C6 being certified at BLE 5.3.

### BLE 5.4 Features

| Feature | C3 Controller | C3 Bluedroid | C3 NimBLE | C6 Controller | C6 Bluedroid | C6 NimBLE |
|---|---|---|---|---|---|---|
| Advertising Coding Selection | Experimental | Experimental | Experimental | Experimental | Experimental | Experimental |
| Encrypted Advertising Data | NA | Developing (2025/12) | Experimental | NA | Developing (2025/12) | Experimental |
| LE GATT Security Levels Characteristic | NA | Unsupported | Experimental | NA | Unsupported | Experimental |
| Periodic Advertising with Responses (PAwR) | Unsupported | Unsupported | Unsupported | Experimental | Experimental | Experimental |

**C6 advantage:** Periodic Advertising with Responses (PAwR) is experimental on C6 but unsupported on C3. PAwR is a key feature for LE Audio and large-scale synchronized device networks (e.g., Electronic Shelf Labels).

### BLE 6.0 Features

| Feature | C3 Controller | C3 Bluedroid | C3 NimBLE | C6 Controller | C6 Bluedroid | C6 NimBLE |
|---|---|---|---|---|---|---|
| Channel Sounding | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported |
| LL Extended Feature Set | Unsupported | Unsupported | Unsupported | Developing (2026/06) | Developing (2026/06) | Developing (2026/06) |
| Decision-Based Advertising Filtering | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported |
| Enhancements for ISOAL | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported |
| Monitoring Advertisers | Unsupported | Unsupported | Unsupported | Developing (2026/06) | Developing (2026/06) | Developing (2026/06) |
| Frame Space Update | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported | Unsupported |

**C6 advantage:** LL Extended Feature Set and Monitoring Advertisers are in development for C6 (target 2026/06); they are fully unsupported on C3.

---

## Summary: Key Feature Differences Between C3 and C6

| Feature | ESP32-C3 | ESP32-C6 |
|---|---|---|
| BLE certification level | 5.4 | 5.3 |
| 2M PHY | Supported | Supported |
| Coded PHY (Long Range) | Supported | Supported |
| Extended Advertising | Supported | Supported |
| Periodic Advertising | Supported | Supported |
| Periodic Advertising Sync Transfer (PAST) | Unsupported | Supported |
| AdvDataInfo in Periodic Advertising | Unsupported | Supported |
| LE Channel Classification | Unsupported | Experimental |
| Periodic Advertising with Responses (PAwR) | Unsupported | Experimental |
| LE Power Control | Experimental | Experimental |
| Enhanced ATT (EATT) | Experimental (NimBLE) | Experimental (NimBLE) |
| LE Isochronous Channels (LE Audio) | Unsupported | Unsupported |
| AoA/AoD (CTE, direction finding) | Unsupported | Unsupported |
| Channel Sounding (BLE 6.0) | Unsupported | Unsupported |
| LL Extended Feature Set (BLE 6.0) | Unsupported | Developing (2026/06) |
| 802.15.4 / Thread / Zigbee coexistence | No (no 802.15.4 radio) | Yes |
| Classic Bluetooth | No | No |

---

## Low Power BLE

Both chips support light-sleep while maintaining BLE connections. The clock source used during sleep determines accuracy and power consumption.

### Clock Sources for BLE Low Power Mode

Both C3 and C6 support three clock sources:

| Clock Source | Accuracy | Power | Notes |
|---|---|---|---|
| Main XTAL | Within 500 PPM | Highest | XTAL stays on during light-sleep; highest current draw |
| 32 kHz External Crystal | Within 500 PPM | Lowest | Best option; falls back to main XTAL if crystal not detected at init |
| 136 kHz RC Oscillator (internal) | Exceeds 500 PPM | Low | Cannot meet BLE spec accuracy; restricted use only |

BLE specification requires sleep clock accuracy within 500 PPM. The 136 kHz RC oscillator does not meet this and is therefore restricted.

### Restrictions of the 136 kHz RC Oscillator

- **C3 documentation:** "only suitable for scenarios with low clock accuracy requirements, such as legacy advertising or scanning"
- **C6 documentation:** "only supported for use with other ESP chips" as peers; connection to non-ESP devices and features like central role connections and periodic advertising with non-ESP devices are unreliable

### Practical Low Power Operation

To enter light-sleep during BLE operation:

- The device needs sufficient idle time between BLE events
- Excessive logging or continuous scanning prevents automatic light-sleep entry
- The active clock source is visible in the initialization log

### C3 vs C6 Low Power Differences

The documented low power mechanism is identical on both chips (same three clock sources, same 500 PPM requirement, same light-sleep behavior). No C6-specific low power BLE improvements are documented in the ESP-IDF low power BLE guide. The primary practical difference is that C6 must also coexist with the 802.15.4 radio if Thread or Zigbee is active, which consumes additional RF time and can affect BLE latency.

---

## ESP32-C6 Specific: 802.15.4 Coexistence with BLE

The ESP32-C6 has a single 2.4 GHz RF module shared among Wi-Fi, BLE, and IEEE 802.15.4 (Thread/Zigbee). This is not available on the C3, which has no 802.15.4 radio.

### Coexistence Mechanism

The chip uses time-division multiplexing (TDM) managed by a coexistence arbitration module. Each radio requests RF access based on priority; the arbitrator grants access to one at a time.

### BLE + Thread/Zigbee Coexistence Matrix

| BLE State | Thread/Zigbee State | Coexistence Result |
|---|---|---|
| Scanning | Scanning | Unsupported |
| Scanning | Other operations | Generally supported |
| Advertising | Thread/Zigbee operations | Largely supported (stable) |
| Connected | Scanning | Unstable (C1 - packet loss risk) |
| Connected | Other operations | Mostly supported |

**Key constraint:** Thread and Zigbee routers maintain unsynchronized links with neighbors requiring continuous signal reception. When BLE or Wi-Fi traffic increases, these nodes experience higher packet loss rates.

### Coexistence Priority by System State

| System State | RF Allocation Behavior |
|---|---|
| IDLE | BLE controls RF resources |
| Wi-Fi Connected | 50/50 time split (period > 100ms) |
| Wi-Fi Scanning | Extended Wi-Fi time slice |
| Wi-Fi Connecting | Extended Wi-Fi allocation |

BLE advertising events can receive elevated priority within their designated intervals. 802.15.4 priority hierarchy: time-critical transmissions and ACKs > normal receive.

### Enabling Coexistence

Coexistence must be enabled in menuconfig:

```
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y
```

Without this, the radios do not coordinate and concurrent operation of BLE and 802.15.4 is unreliable.

---

## Antenna and RF

Neither the C3 nor C6 ESP-IDF documentation pages provide explicit RF sensitivity or TX power specifications. Both chips operate on the 2.4 GHz ISM band. RF parameters (sensitivity, max TX power, antenna impedance) are covered in the respective hardware datasheets and technical reference manuals rather than the ESP-IDF software documentation.

Notable points from the architecture:

- The ESP Bluetooth Controller on both chips includes the PHY layer, meaning RF behavior is handled in the controller, not the host stack
- Both chips support the same PHY modes: 1M PHY, 2M PHY, Coded PHY S=2 (500 kbps), Coded PHY S=8 (125 kbps)
- The C6's shared RF module (Wi-Fi + BLE + 802.15.4) means the antenna must cover all three protocols; single-antenna designs are standard on ESP32-C6 modules

---

## Configuration Quick Reference

### Selecting the Host Stack (menuconfig)

```
Component config → Bluetooth → Bluetooth Host
  → NimBLE - BLE only (lower memory, recommended for BLE-only)
  → Bluedroid - Dual-mode (higher memory)
```

### BLE Low Power Clock Source (menuconfig)

```
Component config → Bluetooth → Controller Options → LPCLK Source
  → CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL   (default, higher power)
  → CONFIG_BT_CTRL_LPCLK_SEL_EXT_32K_XTAL (preferred for low power)
  → CONFIG_BT_CTRL_LPCLK_SEL_RTC_SLOW     (136 kHz RC, restricted use)
```

For C3, the 32 kHz option may also be set via `CONFIG_RTC_CLK_SRC_EXT_CRYS`.

---

## References

- ESP32-C6 BLE Feature Support Status: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/ble/ble-feature-support-status.html
- ESP32-C3 BLE Feature Support Status: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/ble/ble-feature-support-status.html
- ESP32-C6 Low Power BLE: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/low-power-mode/low-power-mode-ble.html
- ESP32-C3 Low Power BLE: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/low-power-mode/low-power-mode-ble.html
- ESP32-C6 Bluetooth API Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/bluetooth/index.html
- ESP32-C3 Bluetooth API Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/bluetooth/index.html
- ESP32-C6 Wireless Coexistence: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/coexist.html
- ESP32-C6 BLE Guide Index: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/ble/index.html
- ESP32-C3 BLE Guide Index: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/ble/index.html
