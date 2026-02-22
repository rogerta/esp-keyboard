# ESP32 NimBLE Stack and Bluetooth Controller/HCI Reference

Sources:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/nimble/index.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/controller_vhci.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/bt_common.html

---

## Table of Contents

1. [NimBLE Stack Overview](#1-nimble-stack-overview)
2. [NimBLE vs Bluedroid](#2-nimble-vs-bluedroid)
3. [NimBLE Architecture](#3-nimble-architecture)
4. [NimBLE API Structure](#4-nimble-api-structure)
5. [NimBLE HCI Transport Layer](#5-nimble-hci-transport-layer)
6. [NimBLE Initialization Sequence](#6-nimble-initialization-sequence)
7. [Controller and VHCI](#7-controller-and-vhci)
8. [esp_bt_controller_config_t](#8-esp_bt_controller_config_t)
9. [Controller Lifecycle Functions](#9-controller-lifecycle-functions)
10. [VHCI Interface](#10-vhci-interface)
11. [HCI Packet Format and Transport](#11-hci-packet-format-and-transport)
12. [Memory Management](#12-memory-management)
13. [TX Power Configuration](#13-tx-power-configuration)
14. [Sleep and Low-Power Management](#14-sleep-and-low-power-management)
15. [Bluetooth Common (bt_common)](#15-bluetooth-common-bt_common)
16. [Bluetooth Defines (esp_bt_defs.h)](#16-bluetooth-defines-esp_bt_defsh)
17. [Bluedroid Stack Lifecycle (esp_bt_main.h)](#17-bluedroid-stack-lifecycle-esp_bt_mainh)
18. [Bluetooth Device APIs (esp_bt_device.h)](#18-bluetooth-device-apis-esp_bt_deviceh)
19. [WiFi and Bluetooth Coexistence](#19-wifi-and-bluetooth-coexistence)
20. [Controller Status Enumeration](#20-controller-status-enumeration)

---

## 1. NimBLE Stack Overview

NimBLE is a highly configurable, open-source Bluetooth Low Energy (BLE) stack originating from the Apache MyNewt project. Espressif has ported NimBLE specifically for ESP32 and FreeRTOS integration. It is fully BLE-only (no Classic Bluetooth support) and is Bluetooth SIG qualifiable.

NimBLE provides both host and controller functionalities, but in ESP-IDF usage it is employed as the **host** stack sitting on top of the ESP32 Bluetooth controller, communicating with the controller via the VHCI interface.

Key characteristics:
- Apache MyNewt project, Apache 2.0 licensed
- BLE-only (no BR/EDR Classic Bluetooth)
- Supports Bluetooth 5.0 features: LE Advertising Extensions, 2 Msym/s PHY, Coded PHY (long range), LE Privacy 1.2, LE Secure Connections (FIPS algorithms), Data Length Extension
- Highly configurable at compile time
- Designed for minimal footprint embedded systems

---

## 2. NimBLE vs Bluedroid

### When to Choose NimBLE

Use NimBLE when:
- You need BLE only (no Classic Bluetooth / BR/EDR)
- RAM is constrained — NimBLE is significantly lighter than Bluedroid
- You want a cleaner, more maintainable open-source codebase
- You need Bluetooth 5.0 BLE features
- You want a highly configurable stack (compile-time feature flags)

Use Bluedroid when:
- You need Classic Bluetooth (A2DP, HFP, SPP, AVRC, etc.)
- You need simultaneous BLE and Classic Bluetooth (dual mode)
- You are using existing Bluedroid-based code

### Memory Savings

NimBLE uses substantially less RAM than Bluedroid. The exact savings depend on features enabled, but NimBLE is designed for constrained devices. Releasing unused controller memory via `esp_bt_controller_mem_release()` (approximately 70 KB freed) applies to both stacks. With NimBLE, the host stack itself also consumes less heap and BSS than Bluedroid.

### Feature Comparison

| Feature | NimBLE | Bluedroid |
|---|---|---|
| BLE (GAP, GATT, SM) | Yes | Yes |
| Classic Bluetooth (BR/EDR) | No | Yes |
| Dual mode | No | Yes |
| BLE 5.0 features | Yes | Partial |
| Open source | Yes (Apache 2.0) | Partial |
| RAM footprint | Lower | Higher |
| Configurability | High (compile flags) | Lower |

### Selecting the Stack in menuconfig

In `idf.py menuconfig`, navigate to `Component config -> Bluetooth` and select:
- `Bluedroid` for Classic BT + BLE
- `NimBLE` for BLE-only with lower footprint

---

## 3. NimBLE Architecture

### Layer Structure

```
+----------------------------+
|  Application               |
+----------------------------+
|  GAP  |  GATT  |  SM      |  (NimBLE Host)
+----------------------------+
|  L2CAP                     |
+----------------------------+
|  HCI (Host side)           |
+----------------------------+
|  ESP NimBLE HCI Transport  |  (VHCI bridge layer)
+----------------------------+
|  ESP32 BT Controller       |  (firmware, runs on BT stack)
+----------------------------+
|  PHY (2.4 GHz radio)       |
+----------------------------+
```

### Controller Layer (Link Layer)

The ESP32 BT controller manages the Link Layer with five operational states:
- Standby
- Advertising
- Scanning
- Initiating
- Connection

The physical layer uses adaptive frequency-hopping GFSK radio across 40 RF channels (channels 0–39).

### Host Layer Components

- **L2CAP** — Logical Link Control and Adaptation Protocol; multiplexes higher-layer protocols
- **SM (Security Manager)** — SMP-based pairing, bonding, encryption key distribution
- **ATT** — Attribute Protocol; client/server attribute access over L2CAP fixed channel
- **GATT** — Generic Attribute Profile; builds service/characteristic model on top of ATT
- **GAP** — Generic Access Profile; advertising, scanning, connection, device discovery
- **HCI** — Host Controller Interface; standard interface between host and controller

### Threading Model

NimBLE is flexible in how it schedules execution:

- Default: `nimble_port_freertos_init()` spawns an independent FreeRTOS task for the NimBLE host
- Alternative: applications can run the NimBLE host within their own task
- BLE Mesh: uses an additional advertising thread to feed advertisement events

### Custom Transport Layer

ESP32 NimBLE does **not** use NimBLE's standard RAM transport. Instead, Espressif implements a specialized transport layer because:

> "The buffering schemes used by NimBLE host is incompatible with that used by ESP controller."

This transport layer:
- Maintains its own buffer pools
- Formats HCI packet exchanges to match the VHCI interface requirements
- Registers host callbacks with the controller via `esp_vhci_host_register_callback()`
- Is initialized by `esp_nimble_hci_init()`

---

## 4. NimBLE API Structure

### GAP (Generic Access Profile)

GAP handles:
- Advertising (connectable, non-connectable, directed, extended)
- Scanning (passive, active)
- Connection initiation and management
- Device discovery
- Security operations (initiation side)

GAP API is accessed through the NimBLE `host/ble_gap.h` header. Key operations include setting advertising parameters and data, starting/stopping advertising, scanning, connecting to peers, and handling connection events via callbacks.

### GATT (Generic Attribute Profile)

GATT provides the service/characteristic data model used by all BLE profiles:

- **GATT Server**: Exposes services and characteristics to remote GATT clients (peripheral role). Applications define a service table with descriptors and register it.
- **GATT Client**: Discovers and accesses services on a remote GATT server (central role). Performs service discovery, reads, writes, and subscribes to notifications/indications.

GATT API is accessed through `host/ble_gatt.h`.

### Security Manager (SM)

The Security Manager implements the SMP protocol for:
- Pairing (Legacy Pairing and LE Secure Connections)
- Bonding (storing long-term keys in NVS)
- Encryption key distribution: LTK, IRK, CSRK
- FIPS-compliant algorithms for LE Secure Connections

Relevant NimBLE Bluetooth 5.0 security features:
- LE Secure Connections (LESC) with ECDH-based key agreement
- LE Privacy 1.2 with Resolvable Private Addresses (RPA)

### L2CAP

L2CAP supports:
- Fixed channel for ATT (channel 4)
- Fixed channel for SM (channel 6)
- Connection-oriented channels (CoC) for custom protocols

### HCI

The HCI layer in NimBLE handles serialization of commands, events, and ACL data packets between the NimBLE host and the transport to the controller.

---

## 5. NimBLE HCI Transport Layer

Header: `components/bt/host/nimble/esp-hci/include/esp_nimble_hci.h`

```c
#include "nimble/transport.h"
```

### HCI H4 Packet Type Macros

These define the UART H4 packet type indicator byte that prefixes every HCI packet:

```c
#define BLE_HCI_UART_H4_NONE  0x00  // No packet / invalid
#define BLE_HCI_UART_H4_CMD   0x01  // HCI Command packet (Host -> Controller)
#define BLE_HCI_UART_H4_ACL   0x02  // HCI ACL Data packet (bidirectional)
#define BLE_HCI_UART_H4_SCO   0x03  // HCI SCO Data packet (audio, Classic BT)
#define BLE_HCI_UART_H4_EVT   0x04  // HCI Event packet (Controller -> Host)
```

### Transport Initialization Functions

```c
/**
 * Initialize VHCI transport layer between NimBLE Host and ESP Bluetooth controller.
 * Initializes transport buffers and registers host callbacks with the controller.
 *
 * Must be called after esp_bt_controller_enable() and before nimble_port_init().
 *
 * Returns: ESP_OK on success, error code on failure.
 */
esp_err_t esp_nimble_hci_init(void);

/**
 * Deinitialize VHCI transport layer between NimBLE Host and ESP Bluetooth controller.
 * Must be called after NimBLE host deinitialization (nimble_port_deinit()).
 *
 * Returns: ESP_OK on success, error code on failure.
 */
esp_err_t esp_nimble_hci_deinit(void);
```

---

## 6. NimBLE Initialization Sequence

The correct order for bringing up NimBLE:

```c
// 1. Initialize NVS (required for BT bonding keys and controller calibration data)
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);

// 2. Release memory for unused BT mode if BLE-only
// (Classic BT memory can be released when using NimBLE BLE-only)
ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

// 3. Initialize and enable the ESP32 BT controller
esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

// 4. Initialize NimBLE port (host + VHCI transport)
ESP_ERROR_CHECK(esp_nimble_hci_init());
nimble_port_init();

// 5. Configure NimBLE host parameters and callbacks
// (set ble_hs_cfg fields: reset_cb, sync_cb, gatts_register_cb, store_config, etc.)
ble_hs_cfg.reset_cb = my_reset_cb;
ble_hs_cfg.sync_cb  = my_sync_cb;

// 6. Application-specific initialization (register GATT services, etc.)
my_gatt_svr_init();

// 7. Start NimBLE host task
nimble_port_freertos_init(my_host_task);
```

### Teardown Sequence

```c
// 1. Stop the NimBLE host task
nimble_port_freertos_deinit();

// 2. Deinitialize NimBLE port
nimble_port_deinit();

// 3. Deinitialize VHCI transport
ESP_ERROR_CHECK(esp_nimble_hci_deinit());

// 4. Disable and deinitialize controller
ESP_ERROR_CHECK(esp_bt_controller_disable());
ESP_ERROR_CHECK(esp_bt_controller_deinit());
```

---

## 7. Controller and VHCI

The ESP32 Bluetooth controller is a firmware component that runs independently on the ESP32 and implements the Link Layer and portions of the HCI. The host (NimBLE or Bluedroid) communicates with it through the **VHCI (Virtual Host Controller Interface)**.

VHCI is an in-memory, callback-based transport that replaces the physical UART HCI transport used in split host/controller designs. Because both the controller firmware and the host stack run on the same chip, VHCI provides zero-copy or low-copy packet exchange through shared memory regions, with callbacks for flow control.

---

## 8. esp_bt_controller_config_t

This structure configures the BT controller. Always initialize it from the default macro and then override specific fields:

```c
esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
```

### Field Reference

| Field | Type | Description |
|---|---|---|
| `controller_task_stack_size` | `uint16_t` | Stack size in bytes for the controller task |
| `controller_task_prio` | `uint8_t` | FreeRTOS priority of the controller task |
| `hci_uart_no` | `uint8_t` | UART interface number for HCI (1 or 2, when using UART transport) |
| `hci_uart_baudrate` | `uint32_t` | HCI UART baudrate; range 115200–921600 bps |
| `mode` | `uint8_t` | Controller mode: 1=BLE, 2=Classic BT (BR/EDR), 3=Dual (BTDM) |
| `ble_max_conn` | `uint8_t` | Maximum simultaneous BLE connections; range 1–9, default 3 |
| `bt_max_acl_conn` | `uint8_t` | Maximum BR/EDR ACL connections; range 1–7, default 2 |
| `bt_max_sync_conn` | `uint8_t` | Maximum BR/EDR synchronous (SCO/eSCO) connections; range 0–3 |
| `scan_duplicate_mode` | `uint8_t` | Scan duplicate filtering mode: 0=normal, 1=BLE Mesh |
| `scan_duplicate_type` | `uint8_t` | Type of duplicate filtering applied |
| `normal_adv_size` | `uint16_t` | Scan duplicate list size for normal advertising; range 10–1000 |
| `mesh_adv_size` | `uint16_t` | Scan duplicate list size for Mesh advertising; range 10–1000 |
| `ble_scan_backoff` | `bool` | Enable BLE scan backoff to reduce power during scanning |
| `ble_sca` | `uint32_t` | BLE sleep clock accuracy (SCA) in PPM |
| `ble_ping_en` | `bool` | Enable BLE authenticated payload timeout (ping) |
| `ble_chan_assess_mode` | `uint8_t` | BLE channel assessment mode |
| `ble_adv_dup_filt_max` | `uint16_t` | Maximum entries in advertising duplicate filter |
| `enc_key_size_min` | `uint8_t` | Minimum encryption key size; range 7–16 bytes, default 7 |
| `pcm_role` | `uint8_t` | PCM audio role (master or slave) for SCO |
| `pcm_polar` | `uint8_t` | PCM polarity configuration |
| `pcm_fsync_shape` | `uint8_t` | PCM frame sync shape |
| `sco_data_path` | `uint8_t` | SCO audio data path: 0=HCI, 1=PCM |
| `ble_aa_check` | `bool` | Enable Access Address validation |

### Mode Values (esp_bt_mode_t)

```c
typedef enum {
    ESP_BT_MODE_IDLE        = 0x00,  // Controller disabled
    ESP_BT_MODE_BLE         = 0x01,  // BLE only
    ESP_BT_MODE_CLASSIC_BT  = 0x02,  // Classic Bluetooth (BR/EDR) only
    ESP_BT_MODE_BTDM        = 0x03,  // Dual mode: BLE + Classic BT
} esp_bt_mode_t;
```

---

## 9. Controller Lifecycle Functions

All functions return `esp_err_t`. The controller must follow the state machine:
`IDLE -> INITED -> ENABLED -> INITED -> IDLE`

```c
/**
 * Initialize the Bluetooth Controller. Allocates tasks and resources.
 * Must be called exactly once before any other BT operation.
 * cfg: pointer to BT controller configuration structure.
 */
esp_err_t esp_bt_controller_init(esp_bt_controller_config_t *cfg);

/**
 * Enable the Bluetooth Controller in the specified mode.
 * Controller must be in INITED state.
 * mode: one of ESP_BT_MODE_BLE, ESP_BT_MODE_CLASSIC_BT, ESP_BT_MODE_BTDM.
 */
esp_err_t esp_bt_controller_enable(esp_bt_mode_t mode);

/**
 * Disable the Bluetooth Controller. Transitions ENABLED -> INITED.
 */
esp_err_t esp_bt_controller_disable(void);

/**
 * De-initialize the Bluetooth Controller. Frees resources and deletes tasks.
 * Controller must be in INITED (disabled) state.
 */
esp_err_t esp_bt_controller_deinit(void);

/**
 * Get current controller status.
 * Returns one of: IDLE, INITED, ENABLED.
 */
esp_bt_controller_status_t esp_bt_controller_get_status(void);
```

---

## 10. VHCI Interface

The VHCI provides in-process HCI packet exchange between the host stack and the controller firmware.

### Callback Structure

```c
typedef struct esp_vhci_host_callback {
    /**
     * Called by the controller when it becomes ready to receive HCI data from the host.
     * The host should call esp_vhci_host_send_packet() in response.
     */
    void (*notify_host_send_available)(void);

    /**
     * Called by the controller when it has HCI data to deliver to the host.
     * data: pointer to HCI packet bytes (including H4 type byte)
     * len:  total length of the packet
     * Returns: 0 on success
     */
    int (*notify_host_recv)(uint8_t *data, uint16_t len);
} esp_vhci_host_callback_t;
```

### VHCI Functions

```c
/**
 * Check whether the controller is ready to receive an HCI packet from the host.
 * Returns: true if send is possible, false if controller is not ready.
 *
 * Must be called before esp_vhci_host_send_packet() to avoid dropping packets.
 */
bool esp_vhci_host_check_send_available(void);

/**
 * Send an HCI packet from the host to the controller.
 * data: pointer to HCI packet bytes (must include H4 type indicator byte as first byte)
 * len:  total length including the type byte
 *
 * Only call after esp_vhci_host_check_send_available() returns true.
 */
void esp_vhci_host_send_packet(uint8_t *data, uint16_t len);

/**
 * Register VHCI host callbacks with the controller.
 * Must be called before enabling the controller.
 * callback: pointer to filled esp_vhci_host_callback_t structure.
 * Returns: ESP_OK on success.
 */
esp_err_t esp_vhci_host_register_callback(const esp_vhci_host_callback_t *callback);
```

### VHCI Usage Pattern

```c
static esp_vhci_host_callback_t vhci_host_cb = {
    .notify_host_send_available = host_send_available_cb,
    .notify_host_recv           = host_recv_cb,
};

// Register before enabling controller
esp_vhci_host_register_callback(&vhci_host_cb);

// To send a command packet from host to controller:
if (esp_vhci_host_check_send_available()) {
    esp_vhci_host_send_packet(hci_cmd_buf, hci_cmd_len);
}

// Receiving: controller calls notify_host_recv() with event/ACL data
static int host_recv_cb(uint8_t *data, uint16_t len) {
    // data[0] is H4 packet type (BLE_HCI_UART_H4_EVT or BLE_HCI_UART_H4_ACL)
    // data[1..] is the actual HCI packet
    process_hci_packet(data, len);
    return 0;
}
```

---

## 11. HCI Packet Format and Transport

### H4 UART Framing

All HCI packets exchanged via VHCI use the H4 framing convention with a one-byte packet type indicator prepended:

```
+--------+---------------------------+
| Type   | HCI Packet Payload        |
| 1 byte | variable length           |
+--------+---------------------------+
```

Packet type values:

| Constant | Value | Direction | Description |
|---|---|---|---|
| `BLE_HCI_UART_H4_NONE` | `0x00` | — | Invalid / no packet |
| `BLE_HCI_UART_H4_CMD`  | `0x01` | Host -> Controller | HCI Command packet |
| `BLE_HCI_UART_H4_ACL`  | `0x02` | Bidirectional | HCI ACL Data packet |
| `BLE_HCI_UART_H4_SCO`  | `0x03` | Bidirectional | HCI SCO Data (audio) |
| `BLE_HCI_UART_H4_EVT`  | `0x04` | Controller -> Host | HCI Event packet |

### HCI Command Packet Structure (after type byte)

```
+----------+----------+-------------------+
| OpCode   | ParamLen | Parameters        |
| 2 bytes  | 1 byte   | 0-255 bytes       |
+----------+----------+-------------------+
```

- OpCode: 10-bit OGF (Opcode Group Field) + 6-bit OCF (Opcode Command Field)

### HCI Event Packet Structure (after type byte)

```
+----------+-------------------+-------------------+
| EventCode| ParamLen          | Parameters        |
| 1 byte   | 1 byte            | 0-255 bytes       |
+----------+-------------------+-------------------+
```

### HCI ACL Data Packet Structure (after type byte)

```
+--------------------+----------+------------------+
| Handle + Flags     | DataLen  | Data             |
| 2 bytes            | 2 bytes  | 0-DataLen bytes  |
+--------------------+----------+------------------+
```

### Shared Memory / Buffer Transport

In the VHCI model, the controller and host share memory on the same die. The NimBLE HCI transport layer (`esp_nimble_hci_init`) sets up buffer pools compatible with the controller's buffering scheme. Packets are exchanged by pointer/reference through these pools rather than by copy when possible, reducing latency and CPU overhead compared to a physical UART.

The VHCI flow control model:
1. Controller signals readiness via `notify_host_send_available()` callback
2. Host checks `esp_vhci_host_check_send_available()` before sending
3. Controller delivers received HCI data to host via `notify_host_recv()` callback

---

## 12. Memory Management

### Releasing Controller Memory

When using only BLE (with NimBLE or Bluedroid BLE-only), approximately 70 KB of controller memory used by Classic BT can be released back to the heap:

```c
// Release Classic BT controller memory (BLE-only scenario)
ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

// OR release BLE controller memory (Classic BT only scenario)
ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
```

```c
/**
 * Release BSS, data, and other sections of the controller to heap.
 * Total freed size is approximately 70 KB for the unused mode.
 *
 * Constraints:
 * - Controller must be in IDLE state (not initialized)
 * - Cannot be reversed — once released, that memory cannot be reclaimed for BT
 * - Must be called before esp_bt_controller_init()
 */
esp_err_t esp_bt_controller_mem_release(esp_bt_mode_t mode);
```

### Releasing Both Controller and Host Memory

```c
/**
 * Release both controller and host (Bluedroid) stack memory.
 * More aggressive than esp_bt_controller_mem_release().
 * Same constraints apply: only works in IDLE state, irreversible.
 */
esp_err_t esp_bt_mem_release(esp_bt_mode_t mode);
```

### Scan Duplicate Filter Management

```c
/**
 * Flush the BLE scan duplicate filter list manually.
 * Useful when the list fills up and new devices stop appearing in scan results.
 */
esp_err_t esp_ble_scan_duplicate_list_flush(void);
```

The duplicate filter list size is configured via `normal_adv_size` and `mesh_adv_size` in `esp_bt_controller_config_t` (range: 10–1000 entries each).

---

## 13. TX Power Configuration

### BLE TX Power

```c
/**
 * Power type selects which BLE operation the power level applies to.
 */
typedef enum {
    ESP_BLE_PWR_TYPE_CONN_HDL0  = 0,   // Connection handle 0
    ESP_BLE_PWR_TYPE_CONN_HDL1  = 1,   // Connection handle 1
    ESP_BLE_PWR_TYPE_CONN_HDL2  = 2,   // Connection handle 2
    ESP_BLE_PWR_TYPE_CONN_HDL3  = 3,   // Connection handle 3
    ESP_BLE_PWR_TYPE_CONN_HDL4  = 4,   // Connection handle 4
    ESP_BLE_PWR_TYPE_CONN_HDL5  = 5,   // Connection handle 5
    ESP_BLE_PWR_TYPE_CONN_HDL6  = 6,   // Connection handle 6
    ESP_BLE_PWR_TYPE_CONN_HDL7  = 7,   // Connection handle 7
    ESP_BLE_PWR_TYPE_CONN_HDL8  = 8,   // Connection handle 8
    ESP_BLE_PWR_TYPE_ADV        = 9,   // Advertising
    ESP_BLE_PWR_TYPE_SCAN       = 10,  // Scanning
    ESP_BLE_PWR_TYPE_DEFAULT    = 11,  // Default (applies to unspecified types)
    ESP_BLE_PWR_TYPE_NUM        = 12,
} esp_ble_power_type_t;

/**
 * Power levels in dBm.
 */
typedef enum {
    ESP_PWR_LVL_N12 = 0,  // -12 dBm
    ESP_PWR_LVL_N9  = 1,  //  -9 dBm
    ESP_PWR_LVL_N6  = 2,  //  -6 dBm
    ESP_PWR_LVL_N3  = 3,  //  -3 dBm
    ESP_PWR_LVL_N0  = 4,  //   0 dBm  (alias: ESP_PWR_LVL_P0 for compatibility)
    ESP_PWR_LVL_P3  = 5,  //  +3 dBm
    ESP_PWR_LVL_P6  = 6,  //  +6 dBm
    ESP_PWR_LVL_P9  = 7,  //  +9 dBm
} esp_power_level_t;

/**
 * Set TX power for a specific BLE operation type.
 */
esp_err_t esp_ble_tx_power_set(esp_ble_power_type_t power_type,
                               esp_power_level_t power_level);

/**
 * Get current TX power for a specific BLE operation type.
 * Returns: esp_power_level_t value.
 */
esp_power_level_t esp_ble_tx_power_get(esp_ble_power_type_t power_type);
```

### BR/EDR (Classic BT) TX Power

```c
/**
 * Set TX power range for BR/EDR (Classic Bluetooth).
 * min_power_level: minimum allowed power level
 * max_power_level: maximum allowed power level
 */
esp_err_t esp_bredr_tx_power_set(esp_power_level_t min_power_level,
                                 esp_power_level_t max_power_level);

/**
 * Get BR/EDR TX power range.
 * min_power_level: output pointer for minimum
 * max_power_level: output pointer for maximum
 */
esp_err_t esp_bredr_tx_power_get(esp_power_level_t *min_power_level,
                                 esp_power_level_t *max_power_level);
```

### SCO Data Path

```c
typedef enum {
    ESP_SCO_DATA_PATH_HCI = 0,  // SCO audio data transported via HCI
    ESP_SCO_DATA_PATH_PCM = 1,  // SCO audio data transported via PCM interface
} esp_sco_data_path_t;

/**
 * Configure SCO (synchronous connection oriented) data path for Classic BT audio.
 */
esp_err_t esp_bredr_sco_datapath_set(esp_sco_data_path_t data_path);
```

---

## 14. Sleep and Low-Power Management

### Sleep Modes

```c
/**
 * Enable Bluetooth modem sleep. The controller periodically wakes to handle BT events.
 * Reduces power consumption during idle periods.
 * Controller must be enabled before calling this.
 */
esp_err_t esp_bt_sleep_enable(void);

/**
 * Disable Bluetooth modem sleep. Controller stays active continuously.
 */
esp_err_t esp_bt_sleep_disable(void);
```

### Low-Power Clock Source

```c
typedef enum {
    ESP_BT_SLEEP_CLOCK_NONE         = 0,  // No sleep clock (sleep disabled)
    ESP_BT_SLEEP_CLOCK_MAIN_XTAL    = 1,  // Use main crystal oscillator
    ESP_BT_SLEEP_CLOCK_EXT_32K_XTAL = 2,  // Use external 32 kHz crystal
} esp_bt_sleep_clock_t;

/**
 * Get the current low-power clock source used by the BT controller.
 */
esp_bt_sleep_clock_t esp_bt_get_lpclk_src(void);

/**
 * Set the low-power clock source for the BT controller sleep mode.
 * Must be called before esp_bt_controller_enable().
 */
esp_err_t esp_bt_set_lpclk_src(esp_bt_sleep_clock_t clk_src);
```

---

## 15. Bluetooth Common (bt_common)

The `bt_common` component provides shared definitions and lifecycle APIs used by both NimBLE and Bluedroid. It is organized into three sub-modules.

### Sub-modules

| Sub-module | Header | Purpose |
|---|---|---|
| Bluetooth Define | `esp_bt_defs.h` | Shared types, enumerations, macros for BT and BLE |
| Bluetooth Main | `esp_bt_main.h` | Bluedroid stack init/enable/disable lifecycle |
| Bluetooth Device | `esp_bt_device.h` | Device address, name, visibility, coexistence |

---

## 16. Bluetooth Defines (esp_bt_defs.h)

```c
#include "esp_bt_defs.h"
```

### Fundamental Types

```c
// Bluetooth device address: 6 bytes, MSB first
#define ESP_BD_ADDR_LEN   6
typedef uint8_t esp_bd_addr_t[ESP_BD_ADDR_LEN];

// Octet arrays
typedef uint8_t esp_bt_octet16_t[16];
typedef uint8_t esp_bt_octet8_t[8];

// Link key (classic BT pairing)
typedef uint8_t esp_link_key[16];  // 16 octets

// PHY preference mask
typedef uint8_t esp_ble_phy_mask_t;

// Encryption key mask
typedef uint8_t esp_ble_key_mask_t;
```

### UUID Type

```c
#define ESP_UUID_LEN_16   2
#define ESP_UUID_LEN_32   4
#define ESP_UUID_LEN_128  16

typedef struct {
    uint16_t len;       // UUID length: ESP_UUID_LEN_16, _32, or _128
    union {
        uint16_t uuid16;              // 16-bit UUID
        uint32_t uuid32;              // 32-bit UUID
        uint8_t  uuid128[ESP_UUID_LEN_128]; // 128-bit UUID
    } uuid;
} esp_bt_uuid_t;
```

### BLE Connection Parameters

```c
typedef struct {
    uint16_t scan_interval;       // Initial scan interval (0.625 ms units, range 0x0004-0xFFFF)
    uint16_t scan_window;         // Initial scan window  (0.625 ms units, range 0x0004-0xFFFF)
    uint16_t interval_min;        // Min connection interval (1.25 ms units, range 0x0006-0x0C80)
    uint16_t interval_max;        // Max connection interval (1.25 ms units, range 0x0006-0x0C80)
    uint16_t latency;             // Peripheral latency in connection events (range 0x0000-0x01F3)
    uint16_t supervision_timeout; // Link supervision timeout (10 ms units, range 0x000A-0x0C80)
    uint16_t min_ce_len;          // Min connection event length (0.625 ms units)
    uint16_t max_ce_len;          // Max connection event length (0.625 ms units)
} esp_ble_conn_params_t;
```

### Status Enumeration

```c
typedef enum {
    ESP_BT_STATUS_SUCCESS            = 0,
    ESP_BT_STATUS_FAIL               = 1,
    ESP_BT_STATUS_NOT_READY          = 2,
    ESP_BT_STATUS_NOMEM              = 3,
    ESP_BT_STATUS_BUSY               = 4,
    ESP_BT_STATUS_DONE               = 5,
    ESP_BT_STATUS_UNSUPPORTED        = 6,
    ESP_BT_STATUS_PARM_INVALID       = 7,
    ESP_BT_STATUS_UNHANDLED          = 8,
    ESP_BT_STATUS_AUTH_FAILURE       = 9,
    ESP_BT_STATUS_RMT_DEV_DOWN       = 10,
    ESP_BT_STATUS_AUTH_REJECTED      = 11,
    ESP_BT_STATUS_INVALID_STATIC_RAND_ADDR = 12,
    ESP_BT_STATUS_PENDING            = 13,
    ESP_BT_STATUS_UNACCEPTED         = 14,
    ESP_BT_STATUS_INTERVAL_TOO_SHORT = 15,
    ESP_BT_STATUS_TIMEOUT            = 16,
    // ... additional HCI-mapped error codes
} esp_bt_status_t;
```

### Device Type

```c
typedef enum {
    ESP_BT_DEVICE_TYPE_BREDR = 0x01,  // Classic Bluetooth (BR/EDR)
    ESP_BT_DEVICE_TYPE_BLE   = 0x02,  // Bluetooth Low Energy
    ESP_BT_DEVICE_TYPE_DUMO  = 0x03,  // Dual-mode (BR/EDR + BLE)
} esp_bt_dev_type_t;
```

### BLE Address Types

```c
typedef enum {
    BLE_ADDR_TYPE_PUBLIC      = 0x00,  // IEEE public address
    BLE_ADDR_TYPE_RANDOM      = 0x01,  // Random static or private address
    BLE_ADDR_TYPE_RPA_PUBLIC  = 0x02,  // Resolvable Private Address, public identity
    BLE_ADDR_TYPE_RPA_RANDOM  = 0x03,  // Resolvable Private Address, random identity
} esp_ble_addr_type_t;

typedef enum {
    BLE_WL_ADDR_TYPE_PUBLIC = 0x00,  // White list: public address
    BLE_WL_ADDR_TYPE_RANDOM = 0x01,  // White list: random address
} esp_ble_wl_addr_type_t;
```

### PHY and Key Masks

```c
// PHY preference masks for BLE 5.0
#define ESP_BLE_PHY_1M_PREF_MASK     (1 << 0)  // Prefer 1 Msym/s PHY
#define ESP_BLE_PHY_2M_PREF_MASK     (1 << 1)  // Prefer 2 Msym/s PHY
#define ESP_BLE_PHY_CODED_PREF_MASK  (1 << 2)  // Prefer Coded PHY (long range)

// Encryption key distribution flags
#define ESP_BLE_ENC_KEY_MASK   (1 << 0)  // Distribute LTK (encryption key)
#define ESP_BLE_ID_KEY_MASK    (1 << 1)  // Distribute IRK (identity key)
#define ESP_BLE_CSR_KEY_MASK   (1 << 2)  // Distribute CSRK (signing key)
#define ESP_BLE_LINK_KEY_MASK  (1 << 3)  // Distribute Link Key (Classic BT)
```

### Connection Interval Bounds

```c
#define ESP_BLE_CONN_INT_MIN  0x0006   // 7.5 ms minimum connection interval
#define ESP_BLE_CONN_INT_MAX  0x0C80   // 4000 ms maximum connection interval
```

---

## 17. Bluedroid Stack Lifecycle (esp_bt_main.h)

```c
#include "esp_bt_main.h"
```

This API manages the Bluedroid host stack (used when Bluedroid is selected instead of NimBLE).

### Status Enumeration

```c
typedef enum {
    ESP_BLUEDROID_STATUS_UNINITIALIZED = 0,  // Stack not initialized
    ESP_BLUEDROID_STATUS_INITIALIZED   = 1,  // Initialized, not enabled
    ESP_BLUEDROID_STATUS_ENABLED       = 2,  // Fully operational
    ESP_BLUEDROID_STATUS_DISABLING     = 3,  // Shutdown in progress
} esp_bluedroid_status_t;
```

### Configuration Structure

```c
typedef struct {
    bool ssp_en;  // Enable Secure Simple Pairing (true) or use legacy pairing (false)
                  // Applies to Classic Bluetooth only
    bool sc_en;   // Enable Secure Connections host support for Classic Bluetooth
} esp_bluedroid_config_t;

// Default configuration macro
#define BT_BLUEDROID_INIT_CONFIG_DEFAULT() { \
    .ssp_en = true, \
    .sc_en  = true, \
}
```

### Lifecycle Functions

```c
/**
 * Initialize the Bluedroid stack. Allocates resources.
 * Must be called after esp_bt_controller_enable().
 * Must precede all Bluetooth operations.
 */
esp_err_t esp_bluedroid_init(void);

/**
 * Initialize Bluedroid with explicit configuration.
 * Allows customizing SSP and Secure Connections behavior.
 */
esp_err_t esp_bluedroid_init_with_cfg(esp_bluedroid_config_t *cfg);

/**
 * Enable the Bluedroid stack. Transitions INITIALIZED -> ENABLED.
 * Requires esp_bluedroid_init() to have completed successfully.
 */
esp_err_t esp_bluedroid_enable(void);

/**
 * Disable the Bluedroid stack. All active connections must be closed first.
 * Transitions ENABLED -> INITIALIZED.
 */
esp_err_t esp_bluedroid_disable(void);

/**
 * De-initialize Bluedroid. Frees all resources.
 * Must be called after esp_bluedroid_disable().
 */
esp_err_t esp_bluedroid_deinit(void);

/**
 * Query current Bluedroid stack state.
 */
esp_bluedroid_status_t esp_bluedroid_get_status(void);
```

### Bluedroid Initialization Sequence

```c
// Controller must be initialized and enabled first (see section 9)
ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM)); // or BLE / CLASSIC_BT

// Then initialize Bluedroid host
ESP_ERROR_CHECK(esp_bluedroid_init());
ESP_ERROR_CHECK(esp_bluedroid_enable());

// Now register GAP/GATT callbacks and start application
```

### Bluedroid Teardown Sequence

```c
ESP_ERROR_CHECK(esp_bluedroid_disable());
ESP_ERROR_CHECK(esp_bluedroid_deinit());
ESP_ERROR_CHECK(esp_bt_controller_disable());
ESP_ERROR_CHECK(esp_bt_controller_deinit());
```

---

## 18. Bluetooth Device APIs (esp_bt_device.h)

```c
#include "esp_bt_device.h"
```

Requires `esp_bluedroid_enable()` to have completed before use.

### Device Address

```c
/**
 * Get the 6-byte Bluetooth device address.
 * Returns pointer to a static 6-byte array (ESP_BD_ADDR_LEN bytes).
 */
const uint8_t *esp_bt_dev_get_address(void);
```

### Callback Registration

```c
typedef void (*esp_bt_dev_cb_t)(esp_bt_dev_cb_event_t event,
                                esp_bt_dev_cb_param_t *param);

/**
 * Register a callback function to receive device-level events.
 * Must be called after esp_bluedroid_enable().
 */
esp_err_t esp_bt_dev_register_callback(esp_bt_dev_cb_t callback);
```

### Callback Events

```c
typedef enum {
    ESP_BT_DEV_NAME_RES_EVT = 0,  // Device name query result
    ESP_BT_DEV_EVT_MAX,
} esp_bt_dev_cb_event_t;
```

### Callback Parameter Union

```c
typedef union {
    struct {
        esp_bt_status_t status;  // Status of the name resolution operation
        char *name;              // Pointer to device name string
    } name_res_param;            // For ESP_BT_DEV_NAME_RES_EVT
} esp_bt_dev_cb_param_t;
```

### Coexistence Configuration

```c
typedef enum {
    ESP_BT_DEV_COEX_TYPE_BLE = 0,  // BLE coexistence type
    ESP_BT_DEV_COEX_TYPE_BT  = 1,  // Classic BT coexistence type
} esp_bt_dev_coex_type_t;

typedef enum {
    ESP_BT_DEV_COEX_OP_CLEAR = 0,  // Clear coexistence status bit
    ESP_BT_DEV_COEX_OP_SET   = 1,  // Set coexistence status bit
} esp_bt_dev_coex_op_t;

/**
 * Configure Bluetooth device coexistence status.
 * type:   which BT type (BLE or Classic)
 * op:     SET or CLEAR the status bit
 * status: status value to set or clear
 */
esp_err_t esp_bt_dev_coex_status_config(esp_bt_dev_coex_type_t type,
                                        esp_bt_dev_coex_op_t op,
                                        uint8_t status);
```

### NVS Bond Key Path Management

```c
/**
 * Get the NVS namespace path where Bluetooth bond keys are stored.
 * file_path: output buffer to receive the path string.
 */
esp_err_t esp_bt_config_file_path_get(char *file_path);

/**
 * Update the NVS namespace path for Bluetooth bond key storage.
 * Must be called before esp_bluedroid_init_with_cfg().
 * file_path: null-terminated path string for the NVS namespace.
 */
esp_err_t esp_bt_config_file_path_update(const char *file_path);
```

---

## 19. WiFi and Bluetooth Coexistence

The ESP32 has a single 2.4 GHz RF module shared among WiFi, Classic BT (BR/EDR), and BLE. Coexistence is achieved through software-controlled time-division multiplexing with priority-based arbitration.

### Supported Coexistence Scenarios

| WiFi Mode | BT Mode | Stability |
|---|---|---|
| STA | BLE (advertising/scanning/connected) | Stable |
| SoftAP | BLE (advertising/scanning) | Stable |
| SoftAP | BLE (connected) | Unstable |
| STA | Classic BT (inquiry/page/connected) | Stable |
| STA + SoftAP | Classic BT | Stable (STA mode only) |
| ESP-NOW TX/RX | BT/BLE | Stable in STA mode |

### Coexistence Mechanism

The coexistence module uses a **priority-based arbitration** model:

- WiFi, BT, and BLE each request RF access from a central coexistence arbitration module
- The coexistence period is divided into time slices: WiFi slice, BT slice, BLE slice
- Priorities are **dynamic**: certain BLE advertising events receive elevated priority and can preempt their allocated time slice

Time allocation adjusts based on WiFi state:
- **WiFi IDLE**: Bluetooth controls coexistence timing
- **WiFi CONNECTED**: ~50/50 split between WiFi and BLE (example with 100 ms period)
- **WiFi SCAN/CONNECTING**: Extended WiFi slice with reduced BT/BLE allocation

### Required Configuration

```
# Enable software coexistence arbitration
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y

# Pin BT controller and host to same CPU (do not split across cores)
CONFIG_BTDM_CTRL_PINNED_TO_CORE_CHOICE=0   # Core 0 for BT controller
CONFIG_BT_BLUEDROID_PINNED_TO_CORE_CHOICE=0 # Core 0 for BT host

# Pin WiFi protocol stack to the OTHER CPU
CONFIG_ESP_WIFI_TASK_CORE_ID=1

# Optional: Enable for continuous BLE scanning when coexisting with WiFi
CONFIG_BTDM_CTRL_FULL_SCAN_SUPPORTED=y
```

### BLE MESH Coexistence API

Applications running BLE Mesh must explicitly report their current state to the coexistence module:

```c
// Available BLE Mesh coexistence states
ESP_COEX_BLE_ST_MESH_CONFIG   // Provisioning phase
ESP_COEX_BLE_ST_MESH_TRAFFIC  // Active data transmission
ESP_COEX_BLE_ST_MESH_STANDBY  // Idle

// Usage pattern: always clear previous state before setting new state
esp_coex_status_bit_clear(ESP_COEX_TYPE_BLE, ESP_COEX_BLE_ST_MESH_TRAFFIC);
esp_coex_status_bit_set(ESP_COEX_TYPE_BLE, ESP_COEX_BLE_ST_MESH_CONFIG);
```

### Performance Considerations

- **BLE SCAN interruption**: WiFi activity can interrupt BLE scanning. Enable `CONFIG_BTDM_CTRL_FULL_SCAN_SUPPORTED` for automatic recovery.
- **WiFi connectionless power-save**: Non-default Window/Interval values can cause extra WiFi priority requests outside the WiFi time slice, degrading Bluetooth throughput. Use defaults unless thoroughly tested.
- **Memory**: Both WiFi and BT stacks have tunable buffer parameters. Reducing buffer sizes can reduce RAM pressure in coexistence scenarios.
- **CPU pinning**: Always pin BT controller and BT host to the same core, and WiFi to the other. Splitting BT controller and host across cores degrades coexistence performance.

---

## 20. Controller Status Enumeration

```c
typedef enum {
    ESP_BT_CONTROLLER_STATUS_IDLE    = 0,  // Not initialized
    ESP_BT_CONTROLLER_STATUS_INITED  = 1,  // Initialized, not enabled
    ESP_BT_CONTROLLER_STATUS_ENABLED = 2,  // Initialized and enabled (active)
    ESP_BT_CONTROLLER_STATUS_NUM     = 3,
} esp_bt_controller_status_t;
```

The controller state machine transitions:

```
[Power on / reset]
        |
        v
     IDLE (0)
        |  esp_bt_controller_init()
        v
    INITED (1)
        |  esp_bt_controller_enable()
        v
   ENABLED (2)
        |  esp_bt_controller_disable()
        v
    INITED (1)
        |  esp_bt_controller_deinit()
        v
     IDLE (0)
```

Memory release via `esp_bt_controller_mem_release()` is only valid in the **IDLE** state and is irreversible.

---

## Appendix: Include Headers Summary

| Component | Header | Purpose |
|---|---|---|
| NimBLE HCI transport | `esp_nimble_hci.h` | NimBLE VHCI init/deinit, H4 macros |
| BT Controller | `esp_bt.h` | Controller init/enable/disable, VHCI, power, sleep |
| BT Common defines | `esp_bt_defs.h` | Shared types, UUIDs, addresses, status codes |
| Bluedroid main | `esp_bt_main.h` | Bluedroid stack lifecycle |
| BT Device | `esp_bt_device.h` | Device address, name, coexistence |
| NimBLE port | `nimble/nimble_port.h` | `nimble_port_init`, `nimble_port_deinit` |
| NimBLE FreeRTOS | `nimble/nimble_port_freertos.h` | `nimble_port_freertos_init/deinit` |
| NimBLE transport | `nimble/transport.h` | Transport layer internals |
