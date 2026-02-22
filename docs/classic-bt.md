# ESP32 Classic Bluetooth: Comprehensive Technical Reference

Source URLs:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/classic-bt/overview.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/classic-bt/profiles-protocols.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/classic_bt.html

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Controller Layer](#controller-layer)
3. [Host Stack: ESP-Bluedroid](#host-stack-esp-bluedroid)
4. [Protocol Stack Layers](#protocol-stack-layers)
5. [Supported Profiles](#supported-profiles)
6. [API Reference: Initialization](#api-reference-initialization)
7. [API Reference: GAP](#api-reference-gap)
8. [API Reference: A2DP](#api-reference-a2dp)
9. [API Reference: AVRCP](#api-reference-avrcp)
10. [API Reference: HFP](#api-reference-hfp)
11. [API Reference: SPP](#api-reference-spp)
12. [API Reference: HID Device](#api-reference-hid-device)
13. [Classic BT vs BLE](#classic-bt-vs-ble)
14. [Use Cases](#use-cases)
15. [Memory Management](#memory-management)
16. [Build System Integration](#build-system-integration)

---

## Architecture Overview

The ESP32 supports Dual-Mode Bluetooth 4.2, enabling simultaneous Classic Bluetooth (BR/EDR) and Bluetooth Low Energy (BLE) operation. The stack is organized into three layers:

```
+-------------------------------------------------------------+
|                        APPLICATION                          |
+-------------------------------------------------------------+
|                     PROFILES (BTC layer)                    |
|   GAP | A2DP | AVRCP | HFP | SPP | HID                     |
+-------------------------------------------------------------+
|                  HOST: ESP-Bluedroid (BTU layer)            |
|   L2CAP | SDP | RFCOMM | AVDTP | AVCTP                     |
+-------------------------------------------------------------+
|               ESP BLUETOOTH CONTROLLER (HCI)                |
|   PHY | Baseband | Link Controller | Link Manager |          |
|   Device Manager | HCI                                      |
+-------------------------------------------------------------+
```

**Dual-Mode operation modes (esp_bt_mode_t):**

| Mode | Value | Description |
|------|-------|-------------|
| `ESP_BT_MODE_IDLE` | 0x00 | Controller disabled |
| `ESP_BT_MODE_BLE` | 0x01 | BLE only |
| `ESP_BT_MODE_CLASSIC_BT` | 0x02 | Classic BR/EDR only |
| `ESP_BT_MODE_BTDM` | 0x03 | Both Classic and BLE |

---

## Controller Layer

The controller is a closed-form binary library that manages all hardware interactions. It contains:

- **PHY**: Physical radio layer, 2.4 GHz ISM band
- **Baseband**: Handles packet formatting, timing, and frequency hopping
- **Link Controller**: State machine for Bluetooth link states
- **Link Manager (LMP)**: Negotiates link parameters with remote devices
- **Device Manager**: Manages local device state and connections
- **HCI**: Host Controller Interface, the boundary between controller and host

The controller is configured via `esp_bt_controller_config_t` and managed through the controller API.

### Controller Configuration Structure

```c
typedef struct {
    uint16_t controller_task_stack_size;  // Stack size for controller task
    uint8_t  controller_task_prio;        // Task priority
    uint8_t  hci_uart_no;                 // UART number for HCI (if used)
    uint32_t hci_uart_baudrate;           // UART baud rate
    uint8_t  scan_duplicate_mode;         // Duplicate scan filter mode
    uint8_t  scan_duplicate_type;         // Duplicate scan filter type
    uint16_t normal_adv_size;             // Normal advertise cache size
    uint16_t mesh_adv_size;               // Mesh advertise cache size
    uint16_t send_adv_reserved_size;      // Reserved advertising buffer
    uint32_t  controller_debug_flag;      // Controller debug flags
    uint8_t  mode;                        // esp_bt_mode_t
    uint8_t  ble_max_conn;               // BLE max connections (1-9)
    uint8_t  bt_max_acl_conn;            // Classic max ACL connections (1-7)
    uint8_t  bt_sco_datapath;            // SCO data path selection
    bool     auto_latency;               // BLE auto latency for lower power
    bool     bt_legacy_auth_vs_evt;      // Legacy auth vendor event
    uint8_t  bt_max_sync_conn;           // Max BR/EDR sync connections
    uint8_t  ble_sca;                    // BLE sleep clock accuracy
    uint8_t  pcm_role;                   // PCM role (master/slave)
    uint8_t  pcm_polar;                  // PCM polarity
    bool     hli;                        // High level interrupt
    uint16_t ble_scan_backoff_upperlimitmax; // BLE scan backoff limit
    uint32_t magic;                      // Magic number for validation
} esp_bt_controller_config_t;
```

Use the macro `BT_CONTROLLER_INIT_CONFIG_DEFAULT()` to get default values.

### Controller API Functions

```c
// Initialize the controller; must be called first
esp_err_t esp_bt_controller_init(esp_bt_controller_config_t *cfg);

// De-initialize and free resources
esp_err_t esp_bt_controller_deinit(void);

// Enable the controller in a specific mode
esp_err_t esp_bt_controller_enable(esp_bt_mode_t mode);

// Disable the controller
esp_err_t esp_bt_controller_disable(void);

// Get current controller status
esp_bt_controller_status_t esp_bt_controller_get_status(void);

// Release memory for unused Bluetooth modes
esp_err_t esp_bt_controller_mem_release(esp_bt_mode_t mode);

// Release both controller and host stack memory
esp_err_t esp_bt_mem_release(esp_bt_mode_t mode);
```

**Controller status enum:**
- `ESP_BT_CONTROLLER_STATUS_IDLE` - Not initialized
- `ESP_BT_CONTROLLER_STATUS_INITED` - Initialized, not enabled
- `ESP_BT_CONTROLLER_STATUS_ENABLED` - Active

### Transmission Power

```c
// Classic BT (BR/EDR) TX power
esp_err_t esp_bredr_tx_power_set(esp_power_level_t min_power_level,
                                  esp_power_level_t max_power_level);
esp_err_t esp_bredr_tx_power_get(esp_power_level_t *min_power_level,
                                  esp_power_level_t *max_power_level);

// BLE TX power
esp_err_t esp_ble_tx_power_set(esp_ble_power_type_t power_type,
                                esp_power_level_t power_level);
esp_err_t esp_ble_tx_power_get(esp_ble_power_type_t power_type);
```

**Power levels (esp_power_level_t):**

| Enum | dBm |
|------|-----|
| `ESP_PWR_LVL_N12` | -12 |
| `ESP_PWR_LVL_N9`  | -9  |
| `ESP_PWR_LVL_N6`  | -6  |
| `ESP_PWR_LVL_N3`  | -3  |
| `ESP_PWR_LVL_N0`  |  0  |
| `ESP_PWR_LVL_P3`  | +3  |
| `ESP_PWR_LVL_P6`  | +6  |
| `ESP_PWR_LVL_P9`  | +9  |

### Modem Sleep

```c
esp_err_t esp_bt_sleep_enable(void);   // Enable modem sleep (lower power)
esp_err_t esp_bt_sleep_disable(void);  // Disable modem sleep
```

### VHCI (Virtual HCI)

Used when implementing a custom host or raw HCI access:

```c
// Check if controller can accept an HCI packet
bool esp_vhci_host_check_send_available(void);

// Send an HCI packet to the controller
void esp_vhci_host_send_packet(uint8_t *data, uint16_t len);

// Register callbacks for receiving HCI packets from controller
esp_err_t esp_vhci_host_register_callback(const esp_vhci_host_callback_t *callback);

// Callback structure
typedef struct {
    void (*notify_host_send_available)(void);           // Controller ready to receive
    int  (*notify_host_recv)(uint8_t *data, uint16_t len); // Incoming HCI packet
} esp_vhci_host_callback_t;
```

---

## Host Stack: ESP-Bluedroid

ESP-Bluedroid is a modified version of the Android Bluetooth stack (Bluedroid). It runs on top of the controller and implements all Bluetooth protocols and profiles. It is the only supported host for Classic Bluetooth on ESP32.

**Internal structure:**

- **BTU (Bluetooth Upper Layer)**: Processes Bluetooth protocols (L2CAP, SDP, RFCOMM, AVDTP, AVCTP) and profiles. Internal interfaces are prefixed with `bta_`.
- **BTC (Bluetooth Transport Controller)**: Application-facing adapter layer. Exposes APIs prefixed with `esp_`.

Developers must only use `esp_`-prefixed APIs. The `bta_` layer is internal and not part of the public API.

### Bluedroid Initialization API

```c
// Initialize Bluedroid with default config
esp_err_t esp_bluedroid_init(void);

// Initialize Bluedroid with custom config
esp_err_t esp_bluedroid_init_with_cfg(esp_bluedroid_config_t *cfg);

// Enable the Bluedroid stack (call after init)
esp_err_t esp_bluedroid_enable(void);

// Disable the stack (call before deinit)
esp_err_t esp_bluedroid_disable(void);

// De-initialize and free all resources
esp_err_t esp_bluedroid_deinit(void);

// Query stack status
esp_bluedroid_status_t esp_bluedroid_get_status(void);
```

**Bluedroid configuration:**

```c
typedef struct {
    bool ssp_en;  // true = Secure Simple Pairing; false = legacy PIN pairing
    bool sc_en;   // true = Secure Connections (AES-CCM) host support
} esp_bluedroid_config_t;
```

**Bluedroid status enum:**
- `ESP_BLUEDROID_STATUS_UNINITIALIZED`
- `ESP_BLUEDROID_STATUS_INITIALIZED`
- `ESP_BLUEDROID_STATUS_ENABLED`
- `ESP_BLUEDROID_STATUS_DISABLING`

### Standard Initialization Sequence

```c
esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
ESP_ERROR_CHECK(esp_bluedroid_init());
ESP_ERROR_CHECK(esp_bluedroid_enable());

// Register profile callbacks and initialize profiles after this point
```

### Device API

```c
#include "esp_bt_device.h"

// Register device-level callback
esp_err_t esp_bt_dev_register_callback(esp_bt_dev_cb_t callback);

// Get the 6-byte Bluetooth device address
const uint8_t *esp_bt_dev_get_address(void);

// Configure coexistence with Wi-Fi
esp_err_t esp_bt_dev_coex_status_config(esp_bt_dev_coex_type_t type,
                                         esp_bt_dev_coex_op_t op,
                                         uint8_t status);

// Get NVS path for bond key storage
esp_err_t esp_bt_config_file_path_get(char *file_path);

// Update NVS path for bond key storage (call before bluedroid_init)
esp_err_t esp_bt_config_file_path_update(const char *file_path);
```

---

## Protocol Stack Layers

Classic Bluetooth uses a layered protocol stack. Each layer builds on the one below it.

### L2CAP (Logical Link Control and Adaptation Protocol)

L2CAP is the foundational multiplexing layer above the Baseband/HCI. All higher-level protocols and profiles route their data through L2CAP channels.

**Responsibilities:**
- Protocol multiplexing via Protocol/Service Multiplexer (PSM) values
- Segmentation and reassembly of large packets
- Quality of Service (QoS) negotiation
- Flow control and error recovery

**L2CAP channel modes:**
- **Basic Mode**: Simple, no error recovery. Lowest overhead.
- **Flow Control Mode**: Adds sliding window and retransmission.
- **Retransmission Mode**: Reliable delivery via acknowledgement.
- **Enhanced Retransmission Mode (ERTM)**: Improved reliability with selective reject.
- **Streaming Mode**: Unidirectional, no acknowledgement (used for audio streams).

**PSM assignments (well-known):**

| PSM | Protocol |
|-----|----------|
| 0x0001 | SDP |
| 0x0003 | RFCOMM |
| 0x000F | BNEP |
| 0x0011 | HID Control |
| 0x0013 | HID Interrupt |
| 0x0017 | AVCTP |
| 0x0019 | AVDTP |
| 0x001B | AVCTP Browsing |

### SDP (Service Discovery Protocol)

SDP enables devices to discover services available on a remote device before connecting.

**How it works:**
1. Device connects an L2CAP channel to PSM 0x0001 on the remote.
2. SDP queries for service records matching a UUID or attribute.
3. Remote responds with service records containing connection parameters (RFCOMM channel number, PSM, codec capabilities, etc.).
4. Initiating device disconnects the SDP channel and connects to the discovered service.

**Key attributes in a service record:**
- Service Class ID List (UUIDs identifying the profile)
- Protocol Descriptor List (stack of protocols: L2CAP, RFCOMM/AVDTP/AVCTP, and their parameters)
- Profile Descriptor List (profile version)
- Service Name, Provider, Description (human-readable)
- Additional profile-specific attributes

### RFCOMM (Radio Frequency Communication)

RFCOMM emulates an RS-232 serial cable over L2CAP. It is the transport layer for SPP and HFP.

**Key characteristics:**
- Multiplexes up to 30 simultaneous virtual serial ports (channels, SCN 1-30) over a single L2CAP connection
- Provides flow control (credit-based)
- Preserves byte ordering
- Used by SPP for general serial data and by HFP for AT command exchange

### AVDTP (Audio/Video Distribution Transport Protocol)

AVDTP is the transport protocol used by A2DP to stream audio.

**Concepts:**
- **Stream End Point (SEP)**: A source or sink endpoint identified by a SEID (Stream End Point Identifier)
- **Signaling channel**: L2CAP channel for capability negotiation (PSM 0x0019)
- **Transport channel**: L2CAP channel for media data (streaming mode, no ack)
- **Procedures**: DISCOVER, GET_CAPABILITIES, SET_CONFIGURATION, OPEN, START, SUSPEND, ABORT, CLOSE, RECONFIGURE

### AVCTP (Audio/Video Control Transport Protocol)

AVCTP carries AVRCP command/response messages over L2CAP (PSM 0x0017). A separate browsing channel uses PSM 0x001B.

---

## Supported Profiles

ESP-Bluedroid supports the following Classic Bluetooth profiles:

| Profile | Full Name | Roles | Transport |
|---------|-----------|-------|-----------|
| GAP | Generic Access Profile | N/A | Direct HCI/L2CAP |
| A2DP | Advanced Audio Distribution Profile | Source, Sink | AVDTP over L2CAP |
| AVRCP | Audio/Video Remote Control Profile | Controller (CT), Target (TG) | AVCTP over L2CAP |
| HFP | Hands-Free Profile | Audio Gateway (AG), Hands-Free (HF) | RFCOMM over L2CAP |
| SPP | Serial Port Profile | Server, Client | RFCOMM over L2CAP |
| HID | Human Interface Device Profile | Device | L2CAP (PSM 0x0011, 0x0013) |

### Profile Relationships

```
Application
    |
    +---> GAP (device discovery, pairing, security)
    |
    +---> A2DP Source/Sink
    |         |
    |         +---> AVDTP (media transport)
    |                   |
    |                   +---> L2CAP (PSM 0x0019)
    |
    +---> AVRCP Controller/Target
    |         |
    |         +---> AVCTP (control transport)
    |                   |
    |                   +---> L2CAP (PSM 0x0017, 0x001B)
    |
    +---> HFP AG/HF
    |         |
    |         +---> RFCOMM (SCN from SDP)
    |                   |
    |                   +---> L2CAP (PSM 0x0003)
    |
    +---> SPP Server/Client
    |         |
    |         +---> RFCOMM (SCN from SDP or fixed)
    |                   |
    |                   +---> L2CAP (PSM 0x0003)
    |
    +---> HID Device
              |
              +---> L2CAP (PSM 0x0011 control, 0x0013 interrupt)
```

---

## API Reference: Initialization

### Required Headers

```c
#include "esp_bt.h"            // Controller and VHCI
#include "esp_bt_main.h"       // Bluedroid init/enable
#include "esp_bt_device.h"     // Device address, coexistence
#include "esp_gap_bt_api.h"    // GAP Classic BT
#include "esp_a2dp_api.h"      // A2DP
#include "esp_avrc_api.h"      // AVRCP
#include "esp_hf_client_api.h" // HFP Hands-Free client
#include "esp_hf_ag_api.h"     // HFP Audio Gateway
#include "esp_spp_api.h"       // SPP
#include "esp_hidd_api.h"      // HID Device
```

---

## API Reference: GAP

GAP (Generic Access Profile) governs device discovery, connection establishment, and security for Classic Bluetooth. All Classic BT connections start with GAP.

### Callback Registration

```c
esp_err_t esp_bt_gap_register_callback(esp_bt_gap_cb_t callback);

typedef void (*esp_bt_gap_cb_t)(esp_bt_gap_cb_event_t event,
                                esp_bt_gap_cb_param_t *param);
```

### GAP Callback Events (esp_bt_gap_cb_event_t)

| Event | Trigger |
|-------|---------|
| `ESP_BT_GAP_DISC_RES_EVT` | Device found during inquiry |
| `ESP_BT_GAP_DISC_STATE_CHANGED_EVT` | Discovery started or stopped |
| `ESP_BT_GAP_RMT_SRVCS_EVT` | Remote services returned |
| `ESP_BT_GAP_RMT_SRVC_REC_EVT` | Remote service record returned |
| `ESP_BT_GAP_AUTH_CMPL_EVT` | Authentication completed |
| `ESP_BT_GAP_PIN_REQ_EVT` | PIN code requested (legacy pairing) |
| `ESP_BT_GAP_CFM_REQ_EVT` | SSP numeric comparison confirmation needed |
| `ESP_BT_GAP_KEY_NOTIF_EVT` | SSP passkey to display |
| `ESP_BT_GAP_KEY_REQ_EVT` | SSP passkey input required |
| `ESP_BT_GAP_READ_RSSI_DELTA_EVT` | RSSI delta read result |
| `ESP_BT_GAP_CONFIG_EIR_DATA_EVT` | EIR data configuration done |
| `ESP_BT_GAP_SET_AFH_CHANNELS_EVT` | AFH channels set |
| `ESP_BT_GAP_READ_REMOTE_NAME_EVT` | Remote device name read |
| `ESP_BT_GAP_MODE_CHG_EVT` | Power mode changed |
| `ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT` | Bond device removed |
| `ESP_BT_GAP_QOS_CMPL_EVT` | QoS setup completed |
| `ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT` | ACL connection status |
| `ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT` | ACL disconnection status |
| `ESP_BT_GAP_SET_PAGE_TO_EVT` | Page timeout set |
| `ESP_BT_GAP_GET_PAGE_TO_EVT` | Page timeout read |
| `ESP_BT_GAP_ACL_PKT_TYPE_CHANGED_EVT` | ACL packet type changed |
| `ESP_BT_GAP_ENC_CHG_EVT` | Encryption state changed |
| `ESP_BT_GAP_SET_MIN_ENC_KEY_SIZE_EVT` | Min encryption key size set |
| `ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT` | Local device name read |
| `ESP_BT_GAP_PROF_STATE_EVT` | GAP profile state change |

### Device Discovery

```c
// Start inquiry (device discovery)
// mode: ESP_BT_INQ_MODE_GENERAL_INQUIRY or ESP_BT_INQ_MODE_LIMITED_INQUIRY
// inq_len: inquiry duration in units of 1.28s (range 1-48, i.e., 1.28s-61.44s)
// num_rsps: max devices to find (0 = unlimited)
esp_err_t esp_bt_gap_start_discovery(esp_bt_inq_mode_t mode,
                                      uint8_t inq_len,
                                      uint8_t num_rsps);

esp_err_t esp_bt_gap_cancel_discovery(void);
```

### Scan/Visibility Mode

```c
// c_mode: ESP_BT_NON_CONNECTABLE | ESP_BT_CONNECTABLE
// d_mode: ESP_BT_NON_DISCOVERABLE | ESP_BT_LIMITED_DISCOVERABLE | ESP_BT_GENERAL_DISCOVERABLE
esp_err_t esp_bt_gap_set_scan_mode(esp_bt_connection_mode_t c_mode,
                                    esp_bt_discovery_mode_t d_mode);
```

### Service Discovery

```c
// Retrieve all service UUIDs from remote device via SDP
esp_err_t esp_bt_gap_get_remote_services(esp_bd_addr_t remote_bda);

// Get full SDP service record for a specific UUID
esp_err_t esp_bt_gap_get_remote_service_record(esp_bd_addr_t remote_bda,
                                                esp_bt_uuid_t *uuid);
```

### Device Name and Class

```c
esp_err_t esp_bt_gap_set_device_name(const char *name);
esp_err_t esp_bt_gap_get_device_name(void);  // async, result in callback

// cod: Class of Device structure
// mode: ESP_BT_SET_COD_MAJOR_MINOR | ESP_BT_SET_COD_SERVICE_CLASS | etc.
esp_err_t esp_bt_gap_set_cod(esp_bt_cod_t cod, esp_bt_cod_mode_t mode);
esp_err_t esp_bt_gap_get_cod(esp_bt_cod_t *cod);
```

**Class of Device structure:**

```c
typedef struct {
    uint32_t reserved_2  : 2;   // Bits 1-0
    uint32_t minor       : 6;   // Bits 7-2
    uint32_t major       : 5;   // Bits 12-8
    uint32_t service     : 11;  // Bits 23-13
    uint32_t reserved_8  : 8;   // Bits 31-24
} esp_bt_cod_t;
```

### Security and Pairing

```c
// Set security parameters
// param_type: ESP_BT_SP_IOCAP_MODE | ESP_BT_SP_OOB_DATA | etc.
esp_err_t esp_bt_gap_set_security_param(esp_bt_sp_param_t param_type,
                                         void *value,
                                         uint8_t len);

// Set minimum encryption key size (7-16 bytes)
esp_err_t esp_bt_gap_set_min_enc_key_size(uint8_t key_size);

// Legacy pairing: set fixed PIN
// pin_type: ESP_BT_PIN_TYPE_VARIABLE | ESP_BT_PIN_TYPE_FIXED
esp_err_t esp_bt_gap_set_pin(esp_bt_pin_type_t pin_type,
                              uint8_t pin_code_len,
                              esp_bt_pin_code_t pin_code);

// Legacy pairing: respond to PIN request
esp_err_t esp_bt_gap_pin_reply(esp_bd_addr_t bd_addr,
                                bool accept,
                                uint8_t pin_code_len,
                                esp_bt_pin_code_t pin_code);

// SSP: respond to numeric comparison (Just Works / Numeric Comparison)
esp_err_t esp_bt_gap_ssp_confirm_reply(esp_bd_addr_t bd_addr, bool accept);

// SSP: enter passkey (Passkey Entry)
esp_err_t esp_bt_gap_ssp_passkey_reply(esp_bd_addr_t bd_addr,
                                        bool accept,
                                        uint32_t passkey);
```

### Bond Management

```c
esp_err_t esp_bt_gap_remove_bond_device(esp_bd_addr_t bd_addr);
int       esp_bt_gap_get_bond_device_num(void);
esp_err_t esp_bt_gap_get_bond_device_list(int *dev_num,
                                           esp_bd_addr_t *dev_list);
```

### Link and Channel Management

```c
// Read RSSI relative to last measurement
esp_err_t esp_bt_gap_read_rssi_delta(esp_bd_addr_t remote_addr);

// Read remote device name asynchronously
esp_err_t esp_bt_gap_read_remote_name(esp_bd_addr_t remote_bda);

// Set QoS poll interval (t_poll in slots of 625us)
esp_err_t esp_bt_gap_set_qos(esp_bd_addr_t remote_bda, uint32_t t_poll);

// Page timeout (in slots of 625us, default ~5s)
esp_err_t esp_bt_gap_set_page_timeout(uint16_t page_to);
esp_err_t esp_bt_gap_get_page_timeout(void);

// Set ACL packet types allowed on a link
esp_err_t esp_bt_gap_set_acl_pkt_types(esp_bd_addr_t remote_bda,
                                         esp_bt_acl_pkt_type_t pkt_types);

// Set AFH (Adaptive Frequency Hopping) channel map
esp_err_t esp_bt_gap_set_afh_channels(esp_bt_gap_afh_channels channels);
```

### Extended Inquiry Response (EIR)

EIR allows a device to include additional data in inquiry responses, eliminating the need for a separate name request round trip.

```c
// Configure and write EIR data
esp_err_t esp_bt_gap_config_eir_data(esp_bt_eir_data_t *eir_data);

// Parse EIR data from an inquiry result
// Returns pointer to the data of the requested type within the EIR buffer
uint8_t *esp_bt_gap_resolve_eir_data(uint8_t *eir,
                                      esp_bt_eir_type_t type,
                                      uint8_t *length);
```

```c
typedef struct {
    bool    fec_required;          // Require FEC encoding
    bool    include_txpower;       // Append TX power level
    bool    include_uuid;          // Append service UUIDs
    bool    include_name;          // Append device name
    uint8_t flag;                  // Discoverable/BR-EDR flags
    uint16_t manufacturer_len;     // Manufacturer specific data length
    uint8_t *p_manufacturer_data;  // Manufacturer specific data
    uint16_t url_len;              // URL length
    uint8_t *p_url;                // URL data
} esp_bt_eir_data_t;
```

### Device Property Types (in DISC_RES_EVT)

| Type | Description |
|------|-------------|
| `ESP_BT_GAP_DEV_PROP_BDNAME` | UTF-8 device name |
| `ESP_BT_GAP_DEV_PROP_COD` | Class of Device (uint32_t) |
| `ESP_BT_GAP_DEV_PROP_RSSI` | RSSI in dBm (int8_t) |
| `ESP_BT_GAP_DEV_PROP_EIR` | Raw EIR data (240 bytes max) |

---

## API Reference: A2DP

A2DP (Advanced Audio Distribution Profile) enables high-quality audio streaming. The ESP32 supports both Source (transmitter) and Sink (receiver) roles.

**Mandatory codec: SBC** (Sub-Band Coding). AAC and vendor codecs are optionally negotiated.

### Callback Registration

```c
esp_err_t esp_a2d_register_callback(esp_a2d_cb_t callback);

typedef void (*esp_a2d_cb_t)(esp_a2d_cb_event_t event,
                              esp_a2d_cb_param_t *param);
```

### Initialization

```c
// Sink role (speaker/headphone)
esp_err_t esp_a2d_sink_init(void);
esp_err_t esp_a2d_sink_deinit(void);

// Source role (phone/player)
esp_err_t esp_a2d_source_init(void);
esp_err_t esp_a2d_source_deinit(void);
```

### Connection Control

```c
// Sink: connect to a source device
esp_err_t esp_a2d_sink_connect(esp_bd_addr_t remote_bda);
esp_err_t esp_a2d_sink_disconnect(esp_bd_addr_t remote_bda);

// Source: connect to a sink device
esp_err_t esp_a2d_source_connect(esp_bd_addr_t remote_bda);
esp_err_t esp_a2d_source_disconnect(esp_bd_addr_t remote_bda);
```

### Audio Data Handling

```c
// Sink: register callback to receive decoded PCM audio
esp_err_t esp_a2d_sink_register_audio_data_callback(
    esp_a2d_sink_data_cb_t callback);

// Source: send encoded audio data to sink
// Audio must be SBC-encoded before sending
esp_err_t esp_a2d_source_audio_data_send(esp_a2d_audio_buff_t *audio_buf);

// Buffer allocation/deallocation
esp_a2d_audio_buff_t *esp_a2d_audio_buff_alloc(uint16_t size);
void esp_a2d_audio_buff_free(esp_a2d_audio_buff_t *audio_buf);
```

**Audio buffer structure:**

```c
typedef struct {
    uint16_t buff_size;    // Total allocated buffer size
    uint16_t number_frame; // Number of audio frames
    uint32_t timestamp;    // Timestamp (RTP)
    uint16_t data_len;     // Actual data length
    uint8_t  *data;        // Audio payload
} esp_a2d_audio_buff_t;
```

### Stream Endpoint Registration

```c
// Register a custom stream endpoint (SEP)
esp_err_t esp_a2d_sink_register_stream_endpoint(uint8_t seid,
                                                  esp_a2d_mcc_t *mcc);
esp_err_t esp_a2d_source_register_stream_endpoint(uint8_t seid,
                                                    esp_a2d_mcc_t *mcc);

// Set preferred codec configuration (source only)
esp_err_t esp_a2d_source_set_pref_mcc(esp_a2d_mcc_t *mcc);
```

### Media Control

```c
// Send media control commands
// ctrl: ESP_A2D_MEDIA_CTRL_NONE | ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY |
//       ESP_A2D_MEDIA_CTRL_START | ESP_A2D_MEDIA_CTRL_SUSPEND
esp_err_t esp_a2d_media_ctrl(esp_a2d_media_ctrl_t ctrl);
```

### Delay Reporting (Sink)

Allows the sink to report its audio rendering delay to the source for AV sync:

```c
esp_err_t esp_a2d_sink_set_delay_value(uint16_t delay_value); // in 1/10 ms
esp_err_t esp_a2d_sink_get_delay_value(void); // async
```

### Profile Status

```c
esp_err_t esp_a2d_get_profile_status(esp_a2d_profile_status_t *profile_status);

typedef struct {
    bool    a2d_snk_inited;  // Sink initialized
    bool    a2d_src_inited;  // Source initialized
    uint8_t conn_num;        // Number of active connections
} esp_a2d_profile_status_t;
```

### A2DP Callback Events

| Event | Description |
|-------|-------------|
| `ESP_A2D_CONNECTION_STATE_EVT` | Connection state changed |
| `ESP_A2D_AUDIO_STATE_EVT` | Audio stream started/suspended |
| `ESP_A2D_AUDIO_CFG_EVT` | Codec configuration negotiated (sink) |
| `ESP_A2D_MEDIA_CTRL_ACK_EVT` | Media control command acknowledged |
| `ESP_A2D_PROF_STATE_EVT` | Profile init/deinit complete |
| `ESP_A2D_SEP_REG_STATE_EVT` | Stream endpoint registration result |
| `ESP_A2D_SNK_PSC_CFG_EVT` | Sink codec capabilities reported |
| `ESP_A2D_SNK_SET_DELAY_VALUE_EVT` | Delay value set confirmation |
| `ESP_A2D_SNK_GET_DELAY_VALUE_EVT` | Delay value read result |
| `ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT` | Sink delay reported to source |
| `ESP_A2D_REPORT_SNK_CODEC_CAPS_EVT` | Sink codec capabilities |
| `ESP_A2D_SRC_SET_PREF_MCC_EVT` | Preferred codec set confirmation |

### Connection States

- `ESP_A2D_CONNECTION_STATE_DISCONNECTED`
- `ESP_A2D_CONNECTION_STATE_CONNECTING`
- `ESP_A2D_CONNECTION_STATE_CONNECTED`
- `ESP_A2D_CONNECTION_STATE_DISCONNECTING`

### Audio States

- `ESP_A2D_AUDIO_STATE_SUSPEND` - Stream paused
- `ESP_A2D_AUDIO_STATE_STARTED` - Stream active

### Codec Configuration (SBC)

```c
typedef struct {
    uint8_t ch_mode;       // Channel mode: MONO, DUAL, STEREO, JOINT_STEREO
    uint8_t samp_freq;     // Sample rate: 16K, 32K, 44.1K, 48K
    uint8_t alloc_mthd;    // Allocation: SNR or LOUDNESS
    uint8_t num_subbands;  // 4 or 8
    uint8_t block_len;     // 4, 8, 12, or 16
    uint8_t min_bitpool;   // Min bitpool value
    uint8_t max_bitpool;   // Max bitpool value
} esp_a2d_cie_sbc_t;

typedef struct {
    esp_a2d_mct_t type;    // ESP_A2D_MCT_SBC | ESP_A2D_MCT_M12 | ESP_A2D_MCT_M24 | etc.
    union {
        esp_a2d_cie_sbc_t sbc;
        // ... other codec types
    } cie;
} esp_a2d_mcc_t;
```

**Maximum stream endpoints:** `ESP_A2D_MAX_SEPS`

---

## API Reference: AVRCP

AVRCP (Audio/Video Remote Control Profile) provides remote control of media playback. The Controller (CT) role sends commands; the Target (TG) role responds. Both roles can coexist on the same device (e.g., a speaker that also controls volume).

### Controller (CT) Functions

```c
esp_err_t esp_avrc_ct_register_callback(esp_avrc_ct_cb_t callback);
esp_err_t esp_avrc_ct_init(void);
esp_err_t esp_avrc_ct_deinit(void);

// tl: transaction label (0-15, ESP_AVRC_TRANS_LABEL_MAX)

// Send playback control: PLAY, PAUSE, STOP, NEXT, PREV, etc.
esp_err_t esp_avrc_ct_send_passthrough_cmd(uint8_t tl,
                                            uint8_t key_code,
                                            uint8_t key_state);

// Request track metadata (title, artist, album, etc.)
esp_err_t esp_avrc_ct_send_metadata_cmd(uint8_t tl, uint8_t attr_mask);

// Set absolute volume on target (0-127)
esp_err_t esp_avrc_ct_send_set_absolute_volume_cmd(uint8_t tl, uint8_t volume);

// Query current playback status
esp_err_t esp_avrc_ct_send_get_play_status_cmd(uint8_t tl);

// Query which notifications the target supports
esp_err_t esp_avrc_ct_send_get_rn_capabilities_cmd(uint8_t tl);

// Register for a specific event notification
esp_err_t esp_avrc_ct_send_register_notification_cmd(uint8_t tl,
                                                      uint8_t event_id,
                                                      uint32_t event_parameter);

// Set player application setting (equalizer, repeat, shuffle, scan)
esp_err_t esp_avrc_ct_send_set_player_value_cmd(uint8_t tl,
                                                  uint8_t attr_id,
                                                  uint8_t value_id);

// Cover art (OBEX-based image download)
esp_err_t esp_avrc_ct_cover_art_connect(uint16_t mtu);
esp_err_t esp_avrc_ct_cover_art_disconnect(void);
esp_err_t esp_avrc_ct_cover_art_get_image_properties(uint8_t *image_handle);
esp_err_t esp_avrc_ct_cover_art_get_image(uint8_t *image_handle,
                                           uint8_t *image_descriptor,
                                           uint16_t image_descriptor_len);
esp_err_t esp_avrc_ct_cover_art_get_linked_thumbnail(uint8_t *image_handle);
```

### Target (TG) Functions

```c
esp_err_t esp_avrc_tg_register_callback(esp_avrc_tg_cb_t callback);
esp_err_t esp_avrc_tg_init(void);
esp_err_t esp_avrc_tg_deinit(void);

// Configure which passthrough commands this target supports
esp_err_t esp_avrc_tg_set_psth_cmd_filter(esp_avrc_psth_filter_t filter,
                                           const esp_avrc_psth_bit_mask_t *cmd_set);
esp_err_t esp_avrc_tg_get_psth_cmd_filter(esp_avrc_psth_filter_t filter,
                                           esp_avrc_psth_bit_mask_t *cmd_set);

// Bitmask helper for passthrough commands
bool esp_avrc_psth_bit_mask_operation(esp_avrc_bit_mask_op_t op,
                                       esp_avrc_psth_bit_mask_t *psth,
                                       esp_avrc_pt_cmd_t cmd);

// Configure which notification events this target supports
esp_err_t esp_avrc_tg_set_rn_evt_cap(const esp_avrc_rn_evt_cap_mask_t *evt_set);
esp_err_t esp_avrc_tg_get_rn_evt_cap(esp_avrc_rn_evt_cap_t cap,
                                      esp_avrc_rn_evt_cap_mask_t *evt_set);

// Bitmask helper for notification events
bool esp_avrc_rn_evt_bit_mask_operation(esp_avrc_bit_mask_op_t op,
                                         esp_avrc_rn_evt_cap_mask_t *events,
                                         esp_avrc_rn_event_ids_t event_id);

// Send notification response to controller
// rsp: ESP_AVRC_RN_RSP_INTERIM (first response) or ESP_AVRC_RN_RSP_CHANGED
esp_err_t esp_avrc_tg_send_rn_rsp(esp_avrc_rn_event_ids_t event_id,
                                   esp_avrc_rn_rsp_t rsp,
                                   esp_avrc_rn_param_t *param);
```

### Profile Status

```c
esp_err_t esp_avrc_get_profile_status(esp_avrc_profile_status_t *profile_status);

typedef struct {
    bool    avrc_ct_inited;  // Controller initialized
    bool    avrc_tg_inited;  // Target initialized
    uint8_t ca_conn_num;     // Active cover art connections
} esp_avrc_profile_status_t;
```

### AVRCP Passthrough Commands (esp_avrc_pt_cmd_t)

Key codes include: `ESP_AVRC_PT_CMD_PLAY`, `ESP_AVRC_PT_CMD_PAUSE`, `ESP_AVRC_PT_CMD_STOP`, `ESP_AVRC_PT_CMD_FORWARD`, `ESP_AVRC_PT_CMD_BACKWARD`, `ESP_AVRC_PT_CMD_REWIND`, `ESP_AVRC_PT_CMD_FAST_FORWARD`, `ESP_AVRC_PT_CMD_VOL_UP`, `ESP_AVRC_PT_CMD_VOL_DOWN`, `ESP_AVRC_PT_CMD_MUTE`, `ESP_AVRC_PT_CMD_POWER`, and 70+ more navigation/function keys.

Key states: `ESP_AVRC_PT_CMD_STATE_PRESSED`, `ESP_AVRC_PT_CMD_STATE_RELEASED`

### AVRCP Notification Events (esp_avrc_rn_event_ids_t)

| Event | Description |
|-------|-------------|
| `ESP_AVRC_RN_PLAY_STATUS_CHANGE` | Playback state changed |
| `ESP_AVRC_RN_TRACK_CHANGE` | Current track changed |
| `ESP_AVRC_RN_TRACK_REACHED_END` | Track ended |
| `ESP_AVRC_RN_TRACK_REACHED_START` | Track at beginning |
| `ESP_AVRC_RN_PLAY_POS_CHANGED` | Playback position changed |
| `ESP_AVRC_RN_BATTERY_STATUS_CHANGE` | Battery status changed |
| `ESP_AVRC_RN_SYSTEM_STATUS_CHANGE` | System status changed |
| `ESP_AVRC_RN_APP_SETTING_CHANGE` | Player settings changed |
| `ESP_AVRC_RN_NOW_PLAYING_CHANGE` | Now-playing list changed |
| `ESP_AVRC_RN_AVAILABLE_PLAYERS_CHANGE` | Available players changed |
| `ESP_AVRC_RN_ADDRESSED_PLAYER_CHANGE` | Addressed player changed |
| `ESP_AVRC_RN_UIDS_CHANGE` | UIDs changed |
| `ESP_AVRC_RN_VOLUME_CHANGE` | Absolute volume changed |

### Metadata Attributes (esp_avrc_md_attr_mask_t)

`ESP_AVRC_MD_ATTR_TITLE`, `ESP_AVRC_MD_ATTR_ARTIST`, `ESP_AVRC_MD_ATTR_ALBUM`, `ESP_AVRC_MD_ATTR_TRACK_NUM`, `ESP_AVRC_MD_ATTR_NUM_TRACKS`, `ESP_AVRC_MD_ATTR_GENRE`, `ESP_AVRC_MD_ATTR_PLAYING_TIME`, `ESP_AVRC_MD_ATTR_COVER_ART`

### Player Settings (esp_avrc_ps_attr_ids_t)

| Attribute | Values |
|-----------|--------|
| `ESP_AVRC_PS_EQUALIZER` | OFF, ON |
| `ESP_AVRC_PS_REPEAT_MODE` | OFF, SINGLE, GROUP |
| `ESP_AVRC_PS_SHUFFLE_MODE` | OFF, ALL, GROUP |
| `ESP_AVRC_PS_SCAN_MODE` | OFF, ALL, GROUP |

### Macros

- `ESP_AVRC_TRANS_LABEL_MAX` = 15 (max transaction label value)
- `ESP_AVRC_CA_IMAGE_HANDLE_LEN` = 7 (cover art image handle length in bytes)
- `ESP_AVRC_CA_MTU_MIN` / `ESP_AVRC_CA_MTU_MAX` (OBEX MTU bounds for cover art)

---

## API Reference: HFP

HFP (Hands-Free Profile) provides voice call functionality. ESP32 supports two roles:
- **Audio Gateway (AG)**: The phone side, exposes call control
- **Hands-Free (HF)**: The headset/car kit side, controls calls via AT commands

Audio uses SCO/eSCO links. Codecs: **CVSD** (narrowband, 8 kHz) and **mSBC** (wideband, 16 kHz).

### HFP Hands-Free Client (HF role)

```c
#include "esp_hf_client_api.h"

esp_err_t esp_hf_client_register_callback(esp_hf_client_cb_t callback);
esp_err_t esp_hf_client_init(void);
esp_err_t esp_hf_client_deinit(void);

// SLC (Service Level Connection) - RFCOMM + AT command channel
esp_err_t esp_hf_client_connect(esp_bd_addr_t remote_bda);
esp_err_t esp_hf_client_disconnect(esp_bd_addr_t remote_bda);

// Audio (SCO/eSCO) connection
esp_err_t esp_hf_client_connect_audio(esp_bd_addr_t remote_bda);
esp_err_t esp_hf_client_disconnect_audio(esp_bd_addr_t remote_bda);

// Call control
esp_err_t esp_hf_client_answer_call(void);
esp_err_t esp_hf_client_reject_call(void);
esp_err_t esp_hf_client_dial(const char *number);       // dial by number
esp_err_t esp_hf_client_dial_memory(int location);      // dial from memory

// Three-way calling (CHLD)
esp_err_t esp_hf_client_send_chld_cmd(esp_hf_chld_type_t chld, int idx);

// Response and Hold (BTRH)
esp_err_t esp_hf_client_send_btrh_cmd(esp_hf_btrh_cmd_t btrh);

// Voice recognition
esp_err_t esp_hf_client_start_voice_recognition(void);
esp_err_t esp_hf_client_stop_voice_recognition(void);

// DTMF tone generation
esp_err_t esp_hf_client_send_dtmf(char code);

// Noise reduction/echo cancellation disable request
esp_err_t esp_hf_client_send_nrec(void);

// Volume control (0-15)
// type: ESP_HF_VOLUME_CONTROL_TARGET_SPK | ESP_HF_VOLUME_CONTROL_TARGET_MIC
esp_err_t esp_hf_client_volume_update(esp_hf_volume_control_target_t type,
                                       int volume);

// Information queries
esp_err_t esp_hf_client_query_current_calls(void);
esp_err_t esp_hf_client_query_current_operator_name(void);
esp_err_t esp_hf_client_retrieve_subscriber_info(void);
esp_err_t esp_hf_client_request_last_voice_tag_number(void);

// Apple extensions (XAPL/IPHONEACCEV)
esp_err_t esp_hf_client_send_xapl(char *information, uint32_t features);
esp_err_t esp_hf_client_send_iphoneaccev(uint32_t bat_level, bool docked);

// PCM resampling (voice over HCI path)
void esp_hf_client_pcm_resample_init(uint32_t src_sps, uint32_t bits, uint32_t channels);
void esp_hf_client_pcm_resample_deinit(void);
int32_t esp_hf_client_pcm_resample(void *src, uint32_t in_bytes, void *dst);

// Audio data (Voice over HCI)
esp_err_t esp_hf_client_register_audio_data_callback(
    esp_hf_client_audio_data_cb_t callback);
esp_hf_audio_buff_t *esp_hf_client_audio_buff_alloc(uint16_t size);
void esp_hf_client_audio_buff_free(esp_hf_audio_buff_t *audio_buf);
esp_err_t esp_hf_client_audio_data_send(esp_hf_sync_conn_hdl_t sync_conn_hdl,
                                         esp_hf_audio_buff_t *audio_buf);

// Packet statistics
esp_err_t esp_hf_client_pkt_stat_nums_get(uint16_t sync_conn_handle);

// Profile status
esp_err_t esp_hf_client_get_profile_status(
    esp_hf_client_profile_status_t *profile_status);
```

**HF Client connection states:**
- `ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED`
- `ESP_HF_CLIENT_CONNECTION_STATE_CONNECTING`
- `ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED` (RFCOMM connected)
- `ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED` (AT negotiation complete)
- `ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTING`

**HF Client audio states:**
- `ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED`
- `ESP_HF_CLIENT_AUDIO_STATE_CONNECTING`
- `ESP_HF_CLIENT_AUDIO_STATE_CONNECTED` (CVSD, narrowband)
- `ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC` (mSBC, wideband)

**HF client peer feature macros:**
`ESP_HF_PEER_FEAT_3WAY`, `ESP_HF_PEER_FEAT_ECNR`, `ESP_HF_PEER_FEAT_VREC`, `ESP_HF_PEER_FEAT_REJECT`, `ESP_HF_PEER_FEAT_CODEC`, `ESP_HF_PEER_FEAT_HF_IND`, and more.

**Macros:** `ESP_BT_HF_CLIENT_NUMBER_LEN`, `ESP_BT_HF_CLIENT_OPERATOR_NAME_LEN`, `ESP_BT_HF_AT_SEND_XAPL_LEN`

### HFP Audio Gateway (AG role)

```c
#include "esp_hf_ag_api.h"

esp_err_t esp_hf_ag_register_callback(esp_hf_cb_t callback);
esp_err_t esp_hf_ag_init(void);
esp_err_t esp_hf_ag_deinit(void);

// Connection management
esp_err_t esp_hf_ag_slc_connect(esp_bd_addr_t remote_bda);
esp_err_t esp_hf_ag_slc_disconnect(esp_bd_addr_t remote_bda);
esp_err_t esp_hf_ag_audio_connect(esp_bd_addr_t remote_bda);
esp_err_t esp_hf_ag_audio_disconnect(esp_bd_addr_t remote_bda);

// Call lifecycle
esp_err_t esp_hf_ag_answer_call(esp_bd_addr_t remote_addr, ...);
esp_err_t esp_hf_ag_reject_call(esp_bd_addr_t remote_addr, ...);
esp_err_t esp_hf_ag_out_call(esp_bd_addr_t remote_addr, ...);
esp_err_t esp_hf_ag_end_call(esp_bd_addr_t remote_addr, ...);

// Status reporting to HF device
// ind_type: call status, signal strength, battery, roaming, etc.
esp_err_t esp_hf_ag_ciev_report(esp_bd_addr_t remote_addr,
                                  esp_hf_ciev_report_type_t ind_type,
                                  int value);
esp_err_t esp_hf_ag_cind_response(esp_bd_addr_t remote_addr, ...);

// AT response helpers
esp_err_t esp_hf_ag_cops_response(esp_bd_addr_t remote_addr, char *name);
esp_err_t esp_hf_ag_clcc_response(esp_bd_addr_t remote_addr, ...);
esp_err_t esp_hf_ag_cnum_response(esp_bd_addr_t remote_addr, ...);
esp_err_t esp_hf_ag_cmee_send(esp_bd_addr_t remote_bda, ...);
esp_err_t esp_hf_ag_unknown_at_send(esp_bd_addr_t remote_addr, char *unat);
esp_err_t esp_hf_ag_bsir(esp_bd_addr_t remote_addr,
                          esp_hf_in_band_ring_state_t state);

// Volume and voice recognition
esp_err_t esp_hf_ag_volume_control(esp_bd_addr_t remote_bda,
                                    esp_hf_volume_control_target_t type,
                                    int volume);
esp_err_t esp_hf_ag_vra_control(esp_bd_addr_t remote_bda,
                                  esp_hf_vr_state_t value);

// Audio data (Voice over HCI)
esp_err_t esp_hf_ag_register_audio_data_callback(
    esp_hf_ag_audio_data_cb_t callback);
esp_hf_audio_buff_t *esp_hf_ag_audio_buff_alloc(uint16_t size);
void esp_hf_ag_audio_buff_free(esp_hf_audio_buff_t *audio_buf);
esp_err_t esp_hf_ag_audio_data_send(esp_hf_sync_conn_hdl_t sync_conn_hdl,
                                      esp_hf_audio_buff_t *audio_buf);
esp_err_t esp_hf_ag_pkt_stat_nums_get(uint16_t sync_conn_handle);
esp_err_t esp_hf_ag_get_profile_status(esp_hf_profile_status_t *profile_status);
```

**HFP AG callback events:**

| Event | Description |
|-------|-------------|
| `ESP_HF_CONNECTION_STATE_EVT` | SLC state changed |
| `ESP_HF_AUDIO_STATE_EVT` | SCO audio link state |
| `ESP_HF_BVRA_RESPONSE_EVT` | Voice recognition toggle |
| `ESP_HF_VOLUME_CONTROL_EVT` | Volume adjustment |
| `ESP_HF_UNAT_RESPONSE_EVT` | Unknown AT command |
| `ESP_HF_DIAL_EVT` | HF device initiated a call |
| `ESP_HF_ATA_RESPONSE_EVT` | HF answered a call |
| `ESP_HF_CHUP_RESPONSE_EVT` | HF rejected/ended a call |
| `ESP_HF_CIND_RESPONSE_EVT` | HF queried device indicators |
| `ESP_HF_COPS_RESPONSE_EVT` | HF queried operator name |
| `ESP_HF_CLCC_RESPONSE_EVT` | HF queried current calls |
| `ESP_HF_CNUM_RESPONSE_EVT` | HF queried subscriber info |
| `ESP_HF_WBS_RESPONSE_EVT` | Codec negotiation (CVSD/mSBC) |
| `ESP_HF_BCS_RESPONSE_EVT` | Final codec confirmed |
| `ESP_HF_PKT_STAT_NUMS_GET_EVT` | Packet statistics |
| `ESP_HF_PROF_STATE_EVT` | Module init/deinit |

**HFP dial types (esp_hf_dial_type_t):**
- `ESP_HF_DIAL_NUM` - Dial by number string
- `ESP_HF_DIAL_VOIP` - VoIP call
- `ESP_HF_DIAL_MEM` - Dial from memory location

**CHLD call hold feature macros:** `ESP_HF_CHLD_FEAT_REL`, `ESP_HF_CHLD_FEAT_REL_ACC`, `ESP_HF_CHLD_FEAT_HOLD_ACC`, `ESP_HF_CHLD_FEAT_MERGE`, `ESP_HF_CHLD_FEAT_MERGE_DETACH`

---

## API Reference: SPP

SPP (Serial Port Profile) provides a virtual serial cable over Bluetooth using RFCOMM. It is the simplest profile for general-purpose bidirectional data transfer.

### Initialization

```c
#include "esp_spp_api.h"

esp_err_t esp_spp_register_callback(esp_spp_cb_t callback);

// Initialize with configuration
esp_err_t esp_spp_enhanced_init(const esp_spp_cfg_t *cfg);
esp_err_t esp_spp_deinit(void);
```

**Configuration structure:**

```c
typedef struct {
    esp_spp_mode_t mode;         // ESP_SPP_MODE_CB or ESP_SPP_MODE_VFS
    bool           enable_l2cap_ertm; // Use L2CAP Enhanced Retransmission Mode
    uint16_t       tx_buffer_size;    // TX buffer (ESP_SPP_MIN_TX_BUFFER_SIZE to
                                      //            ESP_SPP_MAX_TX_BUFFER_SIZE)
} esp_spp_cfg_t;

// Default config macro
BT_SPP_DEFAULT_CONFIG()
```

**SPP modes:**
- `ESP_SPP_MODE_CB`: Data delivered via callback (`ESP_SPP_DATA_IND_EVT`). Application calls `esp_spp_write()` to send.
- `ESP_SPP_MODE_VFS`: SPP channel exposed as a POSIX file descriptor. Read/write using standard `read()`/`write()` calls after calling `esp_spp_vfs_register()`.

### Server

```c
// Start an SPP server (listens for incoming connections)
// sec_mask: ESP_SPP_SEC_NONE | ESP_SPP_SEC_AUTHENTICATE | ESP_SPP_SEC_ENCRYPT | ...
// role: ESP_SPP_ROLE_SLAVE (server is always slave in Classic BT sense)
// local_scn: RFCOMM channel number (0 = auto-assign)
// name: service name registered in SDP
esp_err_t esp_spp_start_srv(esp_spp_sec_t sec_mask,
                             esp_spp_role_t role,
                             uint8_t local_scn,
                             const char *name);

// Start server with full config
esp_err_t esp_spp_start_srv_with_cfg(const esp_spp_start_srv_cfg_t *cfg);

// Stop all SPP servers
esp_err_t esp_spp_stop_srv(void);

// Stop server on specific channel
esp_err_t esp_spp_stop_srv_scn(uint8_t scn);
```

**Server config structure:**

```c
typedef struct {
    uint8_t       local_scn;          // RFCOMM channel (0 = auto)
    bool          create_spp_record;  // Register SDP service record
    esp_spp_sec_t sec_mask;           // Security requirements
    esp_spp_role_t role;
    const char    *name;              // Service name in SDP
} esp_spp_start_srv_cfg_t;
```

### Client

```c
// Discover SPP service on remote device (async, result in DISCOVERY_COMP_EVT)
esp_err_t esp_spp_start_discovery(esp_bd_addr_t bd_addr);

// Connect to remote SPP server
// remote_scn: channel number from discovery result
esp_err_t esp_spp_connect(esp_spp_sec_t sec_mask,
                           esp_spp_role_t role,
                           uint8_t remote_scn,
                           esp_bd_addr_t peer_bd_addr);

esp_err_t esp_spp_disconnect(uint32_t handle);
```

### Data Transfer

```c
// Send data (callback mode only)
esp_err_t esp_spp_write(uint32_t handle, int len, uint8_t *p_data);

// VFS mode: register/unregister file descriptor interface
esp_err_t esp_spp_vfs_register(void);
esp_err_t esp_spp_vfs_unregister(void);
```

### Profile Status

```c
esp_err_t esp_spp_get_profile_status(esp_spp_profile_status_t *profile_status);

typedef struct {
    bool    spp_inited;  // SPP initialized
    uint8_t conn_num;    // Number of active connections
} esp_spp_profile_status_t;
```

### SPP Callback Events

| Event | Description |
|-------|-------------|
| `ESP_SPP_INIT_EVT` | SPP initialized |
| `ESP_SPP_UNINIT_EVT` | SPP de-initialized |
| `ESP_SPP_DISCOVERY_COMP_EVT` | Service discovery complete (contains SCN list) |
| `ESP_SPP_OPEN_EVT` | Client connection opened |
| `ESP_SPP_CLOSE_EVT` | Connection closed |
| `ESP_SPP_START_EVT` | Server started |
| `ESP_SPP_CL_INIT_EVT` | Client initiated connection |
| `ESP_SPP_DATA_IND_EVT` | Data received (CB mode) |
| `ESP_SPP_CONG_EVT` | Congestion status changed |
| `ESP_SPP_WRITE_EVT` | Write complete |
| `ESP_SPP_SRV_OPEN_EVT` | Client connected to server |
| `ESP_SPP_SRV_STOP_EVT` | Server stopped |
| `ESP_SPP_VFS_REGISTER_EVT` | VFS registered |
| `ESP_SPP_VFS_UNREGISTER_EVT` | VFS unregistered |

### Security Flags

| Flag | Meaning |
|------|---------|
| `ESP_SPP_SEC_NONE` | No security |
| `ESP_SPP_SEC_AUTHORIZE` | Authorization required |
| `ESP_SPP_SEC_AUTHENTICATE` | Authentication required |
| `ESP_SPP_SEC_ENCRYPT` | Encryption required |
| `ESP_SPP_SEC_MODE4_LEVEL4` | Security Mode 4, Level 4 |
| `ESP_SPP_SEC_MITM` | Man-in-the-middle protection |
| `ESP_SPP_SEC_IN_16_DIGITS` | 16-digit PIN required |

### Macros

- `ESP_SPP_MAX_MTU` - Maximum RFCOMM MTU
- `ESP_SPP_MAX_SCN` - Maximum RFCOMM server channel number
- `ESP_SPP_MIN_TX_BUFFER_SIZE` / `ESP_SPP_MAX_TX_BUFFER_SIZE`

---

## API Reference: HID Device

The HID Device profile allows the ESP32 to emulate a keyboard, mouse, joystick, or other human interface device.

```c
#include "esp_hidd_api.h"

esp_err_t esp_bt_hid_device_register_callback(esp_hd_cb_t callback);
esp_err_t esp_bt_hid_device_init(void);
esp_err_t esp_bt_hid_device_deinit(void);

// Register HID application (sets SDP record, descriptors)
esp_err_t esp_bt_hid_device_register_app(esp_hidd_app_param_t *app_param,
                                          esp_hidd_qos_param_t *in_qos,
                                          esp_hidd_qos_param_t *out_qos);
esp_err_t esp_bt_hid_device_unregister_app(void);

// Connection management
esp_err_t esp_bt_hid_device_connect(esp_bd_addr_t bd_addr);
esp_err_t esp_bt_hid_device_disconnect(void);
esp_err_t esp_bt_hid_device_virtual_cable_unplug(void);

// Send a HID report
// type: ESP_HIDD_REPORT_TYPE_INPUT | ESP_HIDD_REPORT_TYPE_OUTPUT |
//       ESP_HIDD_REPORT_TYPE_FEATURE | ESP_HIDD_REPORT_TYPE_INTRDATA
esp_err_t esp_bt_hid_device_send_report(esp_hidd_report_type_t type,
                                         uint8_t id,
                                         uint16_t len,
                                         uint8_t *data);

// Send handshake error response
esp_err_t esp_bt_hid_device_report_error(esp_hidd_handshake_error_t error);

// Profile status
esp_err_t esp_bt_hid_device_get_profile_status(
    esp_hidd_profile_status_t *profile_status);
```

**Application parameters:**

```c
typedef struct {
    const char *name;          // Service name
    const char *description;   // Service description
    const char *provider;      // Provider name
    uint8_t    subclass;       // HID subclass (keyboard, mouse, etc.)
    uint8_t    *desc_list;     // HID report descriptor bytes
    int        desc_list_len;  // HID descriptor length
} esp_hidd_app_param_t;
```

**QoS parameters:**

```c
typedef struct {
    uint8_t  service_type;     // Best effort, guaranteed
    uint32_t token_rate;       // Bytes per second
    uint32_t token_bucket_size;
    uint32_t peak_bandwidth;   // Bytes per second
    uint32_t access_latency;   // Microseconds
    uint32_t delay_variation;  // Microseconds
} esp_hidd_qos_param_t;
```

**Profile status:**

```c
typedef struct {
    bool    hidd_inited;
    uint8_t conn_num;
    uint8_t plug_vc_dev_num;  // Virtually-cabled devices
    uint8_t reg_app_num;      // Registered applications
} esp_hidd_profile_status_t;
```

**HID callback events:** `INIT`, `DEINIT`, `REGISTER_APP`, `UNREGISTER_APP`, `OPEN`, `CLOSE`, `SEND_REPORT`, `REPORT_ERR`, `GET_REPORT`, `SET_REPORT`, `SET_PROTOCOL`, `INTR_DATA`, `VC_UNPLUG`, `API_ERR`

**Handshake errors:** `SUCCESS`, `NOT_READY`, `INVALID_REP_ID`, `UNSUPPORTED_REQ`, `INVALID_PARAM`, `UNKNOWN`, `FATAL`

**Protocol modes:** `REPORT_MODE`, `BOOT_MODE`, `UNSUPPORTED_MODE`

---

## Classic BT vs BLE

| Dimension | Classic BT (BR/EDR) | BLE |
|-----------|--------------------|----|
| **Standard** | Bluetooth 1.0+ (Basic Rate), 2.0+ (EDR) | Bluetooth 4.0+ |
| **Data rate** | 1 Mbps (BR), 2 or 3 Mbps (EDR) | 1 Mbps (BT 4.x), 2 Mbps (BT 5.x) |
| **Latency** | ~100 ms connection interval typical | < 10 ms possible |
| **Range** | ~10-100 m (same class) | ~10-100 m (longer with BT 5) |
| **Power** | Higher; continuous connection | Much lower; designed for duty cycling |
| **Audio** | Native SCO/eSCO for voice; A2DP for high-quality | No native audio in 4.x; LE Audio (BT 5.2+) added later |
| **Throughput** | Higher sustained throughput | Lower (optimized for short bursts) |
| **Connection setup** | Slower (~1-2 s inquiry + paging) | Faster (~100 ms advertising) |
| **Profiles** | A2DP, HFP, SPP, HID, AVRCP, etc. | GATT-based profiles (HRS, HID over GATT, etc.) |
| **Discovery** | Inquiry + SDP | Advertising + GATT service discovery |
| **Security** | PIN (legacy) or SSP (Secure Simple Pairing) | LE SMP (pairing with ECDH) |
| **ESP32 host** | ESP-Bluedroid only | ESP-Bluedroid or ESP-NimBLE |
| **Typical current** | 20-50 mA during streaming | 1-20 mA (application-dependent) |
| **Use for** | Audio streaming, voice calls, serial data, HID | Sensors, beacons, short bursts, low-power |

**Key technical differences:**
- Classic BT uses **frequency hopping spread spectrum** over 79 channels at 1 MHz spacing; BLE uses 40 channels (3 advertising, 37 data).
- Classic BT connections use **ACL** (Asynchronous Connection-Less) for data and **SCO/eSCO** (Synchronous Connection-Oriented) for audio; BLE has a single connection-oriented model.
- Classic BT **SDP** is a query/response discovery protocol; BLE uses **GATT** with a hierarchical attribute server model.
- Classic BT pairing can use Legacy PIN or SSP (Just Works, Numeric Comparison, Passkey Entry, OOB); BLE uses SMP with LE Legacy Pairing or LE Secure Connections (ECDH key exchange).

---

## Use Cases

### Choose Classic BT When

1. **High-quality audio streaming**: A2DP + AVRCP is the standard for wireless speakers, headphones, and car stereos. BLE LE Audio exists in BT 5.2+ but has much less device compatibility as of 2025.

2. **Voice calls / hands-free**: HFP is the universal standard for car kits and headsets. It supports call control, audio routing, and codec negotiation (CVSD/mSBC). There is no equivalent BLE profile with similar ecosystem coverage.

3. **Serial port replacement**: SPP is the easiest way to create a wireless serial link with classic-BT-capable hosts (PCs, phones with SPP support). Throughput is higher than BLE for continuous streams.

4. **HID devices (keyboard/mouse)**: Classic BT HID has better latency characteristics and broader host compatibility than BLE HID for desktop/laptop use cases, though BLE HID is better for battery-constrained devices.

5. **Legacy device compatibility**: Any device requiring compatibility with pre-BT 4.0 hardware must use Classic BT.

6. **Continuous high-throughput data**: Classic BT EDR (3 Mbps) provides higher sustained throughput than BLE for applications like data loggers uploading large buffers.

### Choose BLE When

1. **Battery-constrained sensors**: BLE's duty-cycling model (advertising, connection intervals, connection latency) allows coin-cell operation for months/years.

2. **Beacons and broadcasting**: BLE advertising allows broadcasting to multiple devices simultaneously without connection overhead.

3. **Mobile app integration**: GATT profiles are natively supported by iOS and Android BLE APIs without pairing in many cases.

4. **Low-latency sensor data**: Short, infrequent sensor readings (temperature, accelerometer) with minimal connection overhead.

5. **Mesh networking**: Bluetooth Mesh uses BLE as its foundation.

6. **Newer profiles**: Health profiles (HRS, CGM), proximity, device information, and similar GATT-based profiles.

### Dual-Mode (BTDM)

The ESP32 supports simultaneous Classic BT and BLE using `ESP_BT_MODE_BTDM`. This enables scenarios such as:
- A device that streams audio via A2DP while also advertising BLE sensor data
- A device that accepts SPP connections from a PC while also connecting to BLE peripherals

Memory usage is higher in BTDM mode. If only one mode is needed, release unused memory:

```c
// If only using Classic BT, free BLE memory
esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

// If only using BLE, free Classic BT memory
esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
```

---

## Memory Management

Classic BT and BLE controller code is loaded into IRAM and DRAM at startup. Releasing unused modes recovers significant heap.

```c
// Release memory for an unused mode BEFORE esp_bt_controller_init()
// or AFTER esp_bt_controller_deinit()
esp_err_t esp_bt_controller_mem_release(esp_bt_mode_t mode);

// Release both controller and host stack memory for a mode
esp_err_t esp_bt_mem_release(esp_bt_mode_t mode);
```

Typical memory usage:
- Classic BT controller: ~40-50 KB
- BLE controller: ~40-50 KB
- Bluedroid host stack: ~60-80 KB depending on profiles enabled

Profiles can also be selectively compiled via Kconfig (`CONFIG_BT_A2DP_ENABLE`, `CONFIG_BT_SPP_ENABLED`, `CONFIG_BT_HFP_CLIENT_ENABLE`, etc.).

---

## Build System Integration

### CMakeLists.txt

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash
)
```

Or with private dependency:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt nvs_flash
)
```

### menuconfig / sdkconfig Options

Key Kconfig options under `Component config -> Bluetooth`:

| Option | Description |
|--------|-------------|
| `CONFIG_BT_ENABLED` | Enable Bluetooth |
| `CONFIG_BT_CLASSIC_ENABLED` | Enable Classic Bluetooth |
| `CONFIG_BT_A2DP_ENABLE` | Enable A2DP profile |
| `CONFIG_BT_AVRC_ENABLE` | Enable AVRCP profile |
| `CONFIG_BT_AVRC_CT_ENABLE` | AVRCP Controller role |
| `CONFIG_BT_AVRC_TG_ENABLE` | AVRCP Target role |
| `CONFIG_BT_SPP_ENABLED` | Enable SPP profile |
| `CONFIG_BT_HFP_ENABLE` | Enable HFP profile |
| `CONFIG_BT_HFP_CLIENT_ENABLE` | HFP Hands-Free role |
| `CONFIG_BT_HFP_AG_ENABLE` | HFP Audio Gateway role |
| `CONFIG_BT_HFP_WBS_EXT` | HFP Wideband Speech (mSBC) |
| `CONFIG_BT_SSP_ENABLED` | Secure Simple Pairing |
| `CONFIG_BLUEDROID_ENABLED` | Use Bluedroid host stack |

### NVS Requirement

Bond keys and device names are stored in NVS (Non-Volatile Storage). NVS must be initialized before Bluedroid:

```c
#include "nvs_flash.h"

esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
    ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);
```

---

## Example: Minimal SPP Server

```c
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
    case ESP_SPP_INIT_EVT:
        esp_bt_gap_set_device_name("ESP32-SPP");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, "SPP_SERVER");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        // Client connected; param->srv_open.handle is the connection handle
        break;
    case ESP_SPP_DATA_IND_EVT:
        // Data received; param->data_ind.data, param->data_ind.len
        esp_spp_write(param->data_ind.handle,
                      param->data_ind.len,
                      param->data_ind.data);  // echo back
        break;
    case ESP_SPP_CLOSE_EVT:
        break;
    default:
        break;
    }
}

void app_main(void) {
    nvs_flash_init();

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&cfg);
    esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_spp_register_callback(spp_callback);
    esp_spp_cfg_t spp_cfg = BT_SPP_DEFAULT_CONFIG();
    esp_spp_enhanced_init(&spp_cfg);
}
```
