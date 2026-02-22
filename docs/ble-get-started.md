# ESP32 BLE Get-Started Guide

Comprehensive reference compiled from the ESP-IDF official documentation:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-introduction.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-device-discovery.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-connection.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-data-exchange.html

---

## Table of Contents

1. [BLE Introduction and Fundamentals](#1-ble-introduction-and-fundamentals)
2. [Device Discovery: Advertising and Scanning](#2-device-discovery-advertising-and-scanning)
3. [Connection Establishment](#3-connection-establishment)
4. [Data Exchange via GATT](#4-data-exchange-via-gatt)

---

## 1. BLE Introduction and Fundamentals

### 1.1 What is Bluetooth LE?

Bluetooth Low Energy (BLE) is a low-power Bluetooth protocol with a lower data transfer rate compared to Bluetooth Classic. It is designed for IoT applications such as smart switches, sensors, and wearables where battery life is a primary concern.

### 1.2 Layered Architecture

The Bluetooth LE protocol stack is divided into three software layers:

```
+---------------------+
|   Application Layer |  <-- BLE apps written here using Host Layer APIs
+---------------------+
|     Host Layer      |  <-- L2CAP, GATT/ATT, SMP, GAP
+---------------------+
|   Controller Layer  |  <-- Physical Layer (PHY) + Link Layer (LL)
+---------------------+
```

- **Application Layer**: Where BLE applications are developed using Host Layer APIs.
- **Host Layer**: Implements low-level Bluetooth protocols including L2CAP, GATT/ATT, SMP, and GAP. Communicates with the Controller via the Host Controller Interface (HCI).
- **Controller Layer**: Contains the Physical Layer and Link Layer; manages hardware interaction directly.

The Host and Controller layers can be physically separated (connected via SDIO, USB, or UART) or coexist on the same chip via the Virtual Host Controller Interface (VHCI), as is typical on ESP32.

### 1.3 GAP Layer (Generic Access Profile)

GAP defines device connection behaviors and roles.

#### States

| State | Description |
|---|---|
| Idle (Standby) | No active role assigned |
| Device Discovery | Device is advertising, scanning, or initiating |
| Connection | Device is acting as Peripheral or Central |

#### Roles

| Role | Description |
|---|---|
| Advertiser | Broadcasts presence via advertising packets |
| Scanner | Receives and processes advertising packets |
| Initiator | Sends Connection Requests to connectable advertisers |
| Peripheral | Former Advertiser once a connection is established |
| Central | Former Initiator (scanner) once a connection is established |

Key behaviors:
- Once connected, the Advertiser becomes the **Peripheral** and the Initiator becomes the **Central**.
- Devices can simultaneously hold multiple roles (**Multi-role Topology**).
- The **Filter Accept List** controls which devices are permitted to connect.

#### Network Topologies

| Topology | Description |
|---|---|
| Connected Topology | Each device plays a single role |
| Multi-role Topology | At least one device plays both Peripheral and Central roles |
| Broadcast Topology | Broadcasters send data; Observers receive only (connectionless) |

### 1.4 GATT/ATT Layer

#### ATT (Attribute Protocol)

ATT defines a server/client data architecture based on **Attributes**. Each attribute has four fields:

| Field | Description |
|---|---|
| Handle | Unsigned integer index in the attribute table |
| Type | Represented by a UUID (16-bit, 32-bit, or 128-bit) |
| Value | Actual data stored in the attribute |
| Permissions | Access controls (read, write, encrypt, authorize) |

Attributes are organized in **Attribute Tables** for structured management.

#### GATT (Generic Attribute Profile)

GATT builds on ATT with a hierarchical data model:

```
Profile
  └── Service
        └── Characteristic
              ├── Characteristic Declaration Attribute  (UUID 0x2803, read-only)
              ├── Characteristic Value Attribute        (user data)
              └── Characteristic Descriptor Attribute  (optional, e.g. CCCD 0x2902)
```

**Characteristic**: A composite structure containing:
- **Characteristic Declaration Attribute** - Contains properties and the characteristic's UUID; always read-only.
- **Characteristic Value Attribute** - The actual user data, identified by a type UUID.
- **Characteristic Descriptor Attributes** (optional) - Supplementary metadata.

**Client Characteristic Configuration Descriptor (CCCD, UUID 0x2902)**: A special descriptor that allows clients to enable or disable server-initiated Notifications and Indications via read-write access.

**Service**: Contains one or more related Characteristics. A Service Declaration Attribute (UUID 0x2800, read-only) holds the service type UUID. Services can reference other services via the Include mechanism.

**Profile**: A predefined set of services. A device implementing all services of a profile is said to comply with that profile.

#### GATT Roles

| Role | Description |
|---|---|
| GATT Server | Stores and manages characteristic data; responds to client requests |
| GATT Client | Accesses GATT Server characteristics; initiates reads, writes, and subscriptions |

### 1.5 Practical Example: NimBLE_GATT_Server

The NimBLE_GATT_Server example demonstrates:
- **Heart Rate Service** (UUID `0x180D`) with Heart Rate Measurement characteristic (UUID `0x2A37`)
- **Automation IO Service** (UUID `0x1815`) for LED control using a 128-bit vendor-specific UUID
- **CCCD** (UUID `0x2902`) managing notification/indication subscription state
- Read, write, and notification/indication operations exercised via a mobile app (nRF Connect for Mobile)

---

## 2. Device Discovery: Advertising and Scanning

### 2.1 Radio Frequency Band and Channels

BLE operates in the **2.4 GHz ISM band** (globally available, license-free), which is shared with WiFi and other protocols, making interference possible.

BLE divides the band into **40 RF channels**, each with a **2 MHz bandwidth**, ranging from 2402 MHz to 2480 MHz:

| Channel Type | Quantity | Channel Indices | Purpose |
|---|---|---|---|
| Advertising Channels | 3 | 37, 38, 39 | Sending advertising and scan response packets |
| Data Channels | 37 | 0 – 36 | Sending data channel packets during connections |

Advertisers transmit on all three advertising channels sequentially to complete one advertising cycle.

### 2.2 Advertising Interval

The advertising interval can range from **20 ms to 10.24 s**, with a step size of **0.625 ms**.

- Shorter intervals increase discoverability but consume more power.
- A random delay of **0–10 ms** is added between advertising events to reduce packet collisions between multiple advertisers operating simultaneously.

### 2.3 Advertising Packet Structure

Each physical advertising packet on the air has the following structure:

| # | Name | Size | Function |
|---|---|---|---|
| 1 | Preamble | 1 byte | Clock synchronization bit sequence |
| 2 | Access Address | 4 bytes | Marks this as an advertising packet |
| 3 | Protocol Data Unit (PDU) | 2–39 bytes | Actual data storage area |
| 4 | Cyclic Redundancy Check (CRC) | 3 bytes | Error detection |

#### PDU Structure

| # | Name | Size |
|---|---|---|
| 1 | Header | 2 bytes |
| 2 | Payload | 0–37 bytes |

#### PDU Header (16 bits total)

| # | Name | Bits | Notes |
|---|---|---|---|
| 1 | PDU Type | 4 | Determines advertising behavior (see table below) |
| 2 | Reserved for Future Use (RFU) | 1 | — |
| 3 | Channel Selection Bit (ChSel) | 1 | LE Channel Selection Algorithm #2 support |
| 4 | TX Address (TxAdd) | 1 | 0 = Public, 1 = Random address |
| 5 | RX Address (RxAdd) | 1 | 0 = Public, 1 = Random address |
| 6 | Payload Length | 8 | PDU payload size in bytes |

#### PDU Types

| Connectable | Scannable | Undirected | PDU Type | Purpose |
|---|---|---|---|---|
| Y | Y | Y | `ADV_IND` | Most common advertising type |
| Y | N | N | `ADV_DIRECT_IND` | Reconnecting with a previously bonded device |
| N | N | Y | `ADV_NONCONN_IND` | Beacon devices with no connection capability |
| N | Y | Y | `ADV_SCAN_IND` | Beacons with scan response for extra data |

#### PDU Payload

| # | Name | Size | Notes |
|---|---|---|---|
| 1 | Advertisement Address (AdvA) | 6 bytes | 48-bit Bluetooth device address |
| 2 | Advertisement Data (AdvData) | 0–31 bytes | One or more AD structures |

### 2.4 Advertisement Data (AD) Structure Format

Each AD structure inside AdvData follows this format:

| # | Name | Size | Notes |
|---|---|---|---|
| 1 | AD Length | 1 byte | Total length of Type + Data fields |
| 2 | AD Type | n bytes | Usually 1 byte; identifies the data type |
| 3 | AD Data | (AD Length - n) bytes | The actual data |

Common AD Types include: flags, complete/shortened device name, TX power level, appearance, LE role, device address, and URI.

Note: Addresses transmit in little-endian order on air packets — lower bytes appear first. For example, the appearance value `0x0200` (Generic Tag) transmits as `0x0002`.

### 2.5 Bluetooth Address Types

| Type | Description |
|---|---|
| Public Address | Globally unique; manufacturer-registered with IEEE (paid registration) |
| Random Static Address | Fixed or generated at startup; must not change during operation |
| Resolvable Random Private Address | Periodically changes using an Identity Resolving Key (IRK); trusted devices can resolve it |
| Non-resolvable Random Private Address | Completely random; purely prevents tracking; rarely used |

### 2.6 Extended Advertising (Bluetooth 5.0+)

Extended advertising overcomes the 31-byte payload limit by splitting packets across primary and secondary channels:

| Type | Channels Used | Max Payload Per Packet | Max Total Payload |
|---|---|---|---|
| Legacy Advertising | 37–39 | 31 bytes | 31 bytes |
| Extended: `ADV_EXT_IND` (primary) | 37–39 | 254 bytes | — |
| Extended: `AUX_ADV_IND` (secondary) | 0–36 | 254 bytes | 1650 bytes total |

Primary channel packets (`ADV_EXT_IND`) point to secondary channels for extended payload data (`AUX_ADV_IND`).

### 2.7 Scanning Concepts

#### Scan Parameters

| Parameter | Description |
|---|---|
| Scan Window | Duration of continuous packet reception on a single RF channel (e.g., 50 ms) |
| Scan Interval | Time between consecutive scan window starts; always >= Scan Window |

#### Scanning Modes

| Mode | Description |
|---|---|
| Passive Scanning | Scanner only listens to advertising packets; sends no requests |
| Active Scanning | Scanner sends a Scan Request to scannable advertisers; receives Scan Response with additional data |

#### Scan Request / Scan Response Flow

1. Active scanner receives an advertisement from a **scannable** advertiser.
2. Scanner transmits a **Scan Request** on the same advertising channel.
3. Advertiser briefly switches to RX mode after each advertisement to detect scan requests.
4. Upon receiving a Scan Request, the advertiser switches to TX mode and sends a **Scan Response** containing additional data (up to 31 bytes).

### 2.8 NimBLE Stack Implementation: NimBLE_Beacon Example

The NimBLE_Beacon example implements a non-connectable beacon that continuously broadcasts identifying information.

#### Initialization Sequence

1. Initialize NVS Flash (required for BLE stack configuration storage):

```c
void app_main(void) {
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }
}
```

2. Initialize the NimBLE host stack:

```c
void app_main(void) {
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ", ret);
        return;
    }
}
```

3. Initialize GAP service:

```c
void app_main(void) {
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }
}
```

4. Configure NimBLE host callbacks:

```c
static void nimble_host_config_init(void) {
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
}
```

5. Create FreeRTOS task running the NimBLE host:

```c
static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "nimble host task has been started!");
    nimble_port_run();
    vTaskDelete(NULL);
}

void app_main(void) {
    xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);
}
```

#### Advertising Initialization (on stack sync)

When the NimBLE stack synchronizes, `on_stack_sync` is called, which triggers `adv_init()`:

```c
static void on_stack_sync(void) {
    adv_init();
}
```

```c
void adv_init(void) {
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }
}
```

Steps:
1. `ble_hs_util_ensure_addr(0)` — Verify a Bluetooth address is available.
2. `ble_hs_id_infer_auto(0, &own_addr_type)` — Determine the optimal address type.
3. `ble_hs_id_copy_addr(own_addr_type, addr_val, NULL)` — Copy the device address.

#### Advertising Data Configuration

```c
static void start_advertising(void) {
    int rc = 0;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;

    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;

    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }
}
```

**Critical rule**: Each field requires both the data assignment AND the corresponding `_is_present` flag (or length field) to be set. If either is missing, the field is excluded from the packet silently.

#### Scan Response Data Configuration

```c
static void start_advertising(void) {
    struct ble_hs_adv_fields rsp_fields = {0};

    rsp_fields.device_addr = addr_val;
    rsp_fields.device_addr_type = own_addr_type;
    rsp_fields.device_addr_is_present = 1;

    rsp_fields.uri = esp_uri;
    rsp_fields.uri_len = sizeof(esp_uri);

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set scan response data, error code: %d", rc);
        return;
    }
}
```

#### Advertising Parameters and Start

```c
static void start_advertising(void) {
    struct ble_gap_adv_params adv_params = {0};

    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;  // Non-connectable beacon
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  // General discoverable

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                            NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}
```

`ble_gap_adv_params` key fields:
- `conn_mode`: `BLE_GAP_CONN_MODE_NON` (beacon), `BLE_GAP_CONN_MODE_UND` (connectable undirected)
- `disc_mode`: `BLE_GAP_DISC_MODE_GEN` (general discoverable)

`ble_gap_adv_start` signature:
```c
int ble_gap_adv_start(uint8_t own_addr_type,
                       const ble_addr_t *direct_addr,
                       int32_t duration_ms,
                       const struct ble_gap_adv_params *params,
                       ble_gap_event_fn *cb,
                       void *cb_arg);
```

#### Address Mode Variants

**Public Address Mode**: Simple continuous advertising with the IEEE-registered Bluetooth address. No special configuration required.

**Resolvable Random Private Address Mode**: The protocol stack auto-updates the address at 15-minute intervals (configurable). Set `own_addr_type` to `BLE_ADDR_TYPE_RPA_PUBLIC` or `BLE_ADDR_TYPE_RPA_RANDOM`. Advertising must not start until the `esp_ble_gap_config_local_privacy` event completes.

**Random Static Address Mode**: Requires manual address configuration via `esp_ble_gap_set_rand_addr` API before advertising with `BLE_ADDR_TYPE_RANDOM`.

### 2.9 Payload Size Constraints

| Packet | Maximum Size |
|---|---|
| Advertising packet payload | 31 bytes |
| Scan response payload | 31 bytes |
| Total across both | 62 bytes |

Exceeding these limits causes `ble_gap_adv_set_fields` or `ble_gap_adv_start` to return an error. The NimBLE_Beacon example uses 28 bytes in advertising data (5 AD structures) plus 17 bytes in scan response (2 AD structures).

---

## 3. Connection Establishment

### 3.1 Connection Initiation

When a scanner receives an advertising packet on an advertising channel and the advertiser is connectable (`ADV_IND` or `ADV_DIRECT_IND`), the scanner can transmit a **Connection Request** on the same advertising channel. After the connection request:

- The **Advertiser** becomes the **Peripheral**.
- The **Scanner/Initiator** becomes the **Central**.
- Bidirectional data exchange over data channels (0–36) begins.

The RX phase following each advertising transmission allows the advertiser to receive both scan requests and connection requests within the same time window.

### 3.2 Connection Parameters

#### Connection Interval and Connection Event

The **Connection Interval** determines how frequently the Central and Peripheral exchange data:

- Range: **7.5 ms to 4.0 s**
- Step size: **1.25 ms** (range: 6 steps to 3200 steps)

A **Connection Event** is one data exchange cycle: the Central transmits first, then the Peripheral responds — even empty packets are exchanged to maintain connectivity.

#### Supervision Timeout

Defines the maximum allowed time between two successful connection events. Exceeding this threshold causes the link to be considered lost and triggers disconnection. Critical for detecting unexpected power loss or out-of-range conditions.

#### Peripheral Latency

Specifies the maximum number of connection events the Peripheral device can skip when there is no data to send. This enables power reduction during idle periods without requiring adjustment of the connection interval. Example use case: a Bluetooth mouse can skip events when idle but still respond with low latency when the user moves it.

#### Maximum Transmission Unit (MTU)

| BLE Version | Max PDU Payload | Max ATT Data |
|---|---|---|
| Pre-4.2 | 27 bytes | 23 bytes |
| 4.2+ (with DLE) | 251 bytes | 247 bytes |

Default MTU is **23 bytes**, matching pre-4.2 specification limits. Data Length Extension (DLE) in BLE 4.2+ supports up to 251-byte PDU payloads, allowing larger MTU values (e.g., 140 bytes) within a single PDU.

### 3.3 Data Channel Packet Structure

```
+----------+------------------+------+
| Header   | Payload          | MIC  |
| 2 bytes  | 0-27 / 0-251 B   | 4 B* |
+----------+------------------+------+
           |                  |
           +--+---------------+
              |
     +--------+---------+------------------+
     | L2CAP Header     | ATT Header+Data  |
     | 4 bytes          | 0-23 / 0-247 B   |
     +------------------+------------------+
```

(*MIC = Message Integrity Check, optional, used with encryption)

### 3.4 NimBLE Stack Implementation: NimBLE_Connection Example

#### Advertising with Connection Mode

For a connectable device, the advertising parameters differ from the beacon:

```c
adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  // Undirected connectable
adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                        gap_event_handler, NULL);
```

The macro `BLE_GAP_ADV_ITVL_MS(500)` converts milliseconds to the 0.625 ms unit required by the API.

#### Scan Response with Advertising Interval Advertisement

```c
rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(500);
rsp_fields.adv_itvl_is_present = 1;
```

#### GAP Event Handler

The `gap_event_handler` callback is called by the NimBLE protocol stack whenever a GAP event is triggered. Three critical event types are handled:

**Connection Event** (`BLE_GAP_EVENT_CONNECT`):

```c
case BLE_GAP_EVENT_CONNECT:
    ESP_LOGI(TAG, "connection %s; status=%d",
             event->connect.status == 0 ? "established" : "failed",
             event->connect.status);

    if (event->connect.status == 0) {
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        print_conn_desc(&desc);
        led_on();

        struct ble_gap_upd_params params = {
            .itvl_min           = desc.conn_itvl,
            .itvl_max           = desc.conn_itvl,
            .latency            = 3,
            .supervision_timeout = desc.supervision_timeout,
        };
        rc = ble_gap_update_params(event->connect.conn_handle, &params);
    } else {
        start_advertising();
    }
    return rc;
```

On successful connection:
- `ble_gap_conn_find()` retrieves the connection descriptor containing handle, address types, and current parameter values.
- `ble_gap_update_params()` requests updated connection parameters (here, increasing peripheral latency to 3).
- The LED is turned on as a visual indicator.

On failed connection: advertising is restarted.

**Disconnect Event** (`BLE_GAP_EVENT_DISCONNECT`):

```c
case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "disconnected from peer; reason=%d",
             event->disconnect.reason);
    led_off();
    start_advertising();
    return rc;
```

Turns the LED off and restarts advertising.

**Connection Parameter Update Event** (`BLE_GAP_EVENT_CONN_UPDATE`):

```c
case BLE_GAP_EVENT_CONN_UPDATE:
    ESP_LOGI(TAG, "connection updated; status=%d",
             event->conn_update.status);

    rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
    print_conn_desc(&desc);
    return rc;
```

Logs the updated connection parameters by retrieving the connection descriptor.

### 3.5 Connection Parameter Log Example

Typical connection descriptor values seen in logs:

| Field | Initial Value | After Update |
|---|---|---|
| `conn_itvl` | 36 (45 ms) | 36 |
| `conn_latency` | 0 | 3 |
| `supervision_timeout` | 500 | 500 |
| `encrypted` | 0 | 0 |
| `authenticated` | 0 | 0 |
| `bonded` | 0 | 0 |

### 3.6 Connection Event Flow

```
Peripheral (Advertiser)          Central (Scanner/Initiator)
        |                                  |
        |--- ADV_IND (ch 37) ------------>|
        |--- ADV_IND (ch 38) ------------>|
        |--- ADV_IND (ch 39) ------------>|
        |<-- CONNECT_REQ (ch 39) ---------|
        |                                  |
   [Role: Peripheral]              [Role: Central]
        |                                  |
        |<== Data Channel (ch 0-36) ======>|
        |  (connection events at interval) |
        |                                  |
   [BLE_GAP_EVENT_CONNECT]         [connection established]
        |                                  |
        |<-- Param Update Request -------->|
   [BLE_GAP_EVENT_CONN_UPDATE]            |
        |                                  |
   [BLE_GAP_EVENT_DISCONNECT]     [disconnects or timeout]
        |                                  |
   [start_advertising()]                   |
```

---

## 4. Data Exchange via GATT

### 4.1 Attribute Structure

The minimum unit of BLE data exchange is an **Attribute**. Each attribute has four components:

| Component | Description |
|---|---|
| Handle | 16-bit unsigned integer; the index of the attribute in the server's attribute table |
| Type | UUID identifying the kind of data (16-bit SIG-defined or 128-bit manufacturer-defined) |
| Access Permission | Specifies read/write capability and encryption/authorization requirements |
| Value | Actual user data or metadata pointing to another attribute |

#### UUID Types

| Format | Size | Defined By |
|---|---|---|
| 16-bit UUID | 2 bytes | Bluetooth SIG (e.g., `0x180D` Heart Rate Service, `0x2A37` Heart Rate Measurement) |
| 128-bit UUID | 16 bytes | Manufacturers (vendor-specific services and characteristics) |

### 4.2 Characteristic Data Structure

A characteristic is composed of up to three attribute types:

| # | Attribute | UUID | Access | Content |
|---|---|---|---|---|
| 1 | Characteristic Declaration | `0x2803` | Read-only | Properties flags + characteristic UUID |
| 2 | Characteristic Value | (type-specific) | Per flags | Actual user data |
| 3 | Characteristic Descriptor | (type-specific) | Varies | Supplementary metadata |

**CCCD (Client Characteristic Configuration Descriptor, UUID `0x2902`)**: A read-write descriptor that enables the client to subscribe to server-initiated operations. Writing `0x0001` enables Notifications; writing `0x0002` enables Indications.

### 4.3 Service Structure

| # | Attribute | UUID | Access | Content |
|---|---|---|---|---|
| 1 | Service Declaration | `0x2800` | Read-only | Service type UUID |
| 2+ | Characteristic Definition(s) | (varies) | Per flags | One or more characteristics |

### 4.4 GATT Data Operations

#### Client-Initiated Operations

| Operation | Description |
|---|---|
| Read | Pulls the current value of a specific characteristic from the server |
| Write | Sends a new value to the server; server must acknowledge |
| Write Without Response | Sends a new value to the server; no acknowledgment required |

#### Server-Initiated Operations

| Operation | Description |
|---|---|
| Notify | Server actively pushes data to the client; no confirmation required from client |
| Indicate | Server pushes data to the client; client must send a confirmation; slower than Notify |

### 4.5 NimBLE Stack Implementation: NimBLE_GATT_Server Example

The NimBLE_GATT_Server example extends NimBLE_Connection with GATT services and characteristic access callbacks.

#### Program Entry and Heart Rate Task

```c
static void heart_rate_task(void *param) {
    ESP_LOGI(TAG, "heart rate task has been started!");
    while (1) {
        update_heart_rate();
        ESP_LOGI(TAG, "heart rate updated to %d", get_heart_rate());
        send_heart_rate_indication();
        vTaskDelay(HEART_RATE_TASK_PERIOD);  // 1000 ms (1 Hz)
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    // ... (NVS, NimBLE, GAP, GATT initialization) ...
    xTaskCreate(heart_rate_task, "Heart Rate", 4 * 1024, NULL, 5, NULL);
    return;
}
```

#### GATT Service Initialization

```c
int gatt_svc_init(void) {
    int rc;
    ble_svc_gatt_init();  // Initializes the GATT Service (UUID 0x1801)
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }
    return 0;
}
```

#### GATT Service Table Definition

```c
static const ble_uuid16_t heart_rate_svc_uuid = BLE_UUID16_INIT(0x180D);
static uint16_t heart_rate_chr_val_handle;
static const ble_uuid16_t heart_rate_chr_uuid = BLE_UUID16_INIT(0x2A37);
static uint16_t heart_rate_chr_conn_handle = 0;

static const ble_uuid16_t auto_io_svc_uuid = BLE_UUID16_INIT(0x1815);
static uint16_t led_chr_val_handle;
static const ble_uuid128_t led_chr_uuid =
    BLE_UUID128_INIT(0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15,
                     0xde, 0xef, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* Heart Rate Service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &heart_rate_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &heart_rate_chr_uuid.u,
                .access_cb  = heart_rate_chr_access,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                .val_handle = &heart_rate_chr_val_handle,
            },
            { 0 },  /* terminator */
        },
    },

    /* Automation IO Service (LED control) */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &auto_io_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &led_chr_uuid.u,
                .access_cb  = led_chr_access,
                .flags      = BLE_GATT_CHR_F_WRITE,
                .val_handle = &led_chr_val_handle,
            },
            { 0 },  /* terminator */
        },
    },

    { 0 },  /* service terminator */
};
```

Key `ble_gatt_svc_def` fields:
- `type`: `BLE_GATT_SVC_TYPE_PRIMARY` or `BLE_GATT_SVC_TYPE_SECONDARY`
- `uuid`: Pointer to service UUID
- `characteristics`: Null-terminated array of `ble_gatt_chr_def`

Key `ble_gatt_chr_def` fields:
- `uuid`: Pointer to characteristic UUID
- `access_cb`: Callback invoked on read/write access
- `flags`: Permission flags (`BLE_GATT_CHR_F_READ`, `BLE_GATT_CHR_F_WRITE`, `BLE_GATT_CHR_F_INDICATE`, `BLE_GATT_CHR_F_NOTIFY`, etc.)
- `val_handle`: Pointer to `uint16_t` that receives the assigned attribute handle

**Important**: When `BLE_GATT_CHR_F_INDICATE` (or `BLE_GATT_CHR_F_NOTIFY`) is set, the NimBLE stack automatically adds a CCCD descriptor for that characteristic.

#### LED Characteristic Access Callback (Write)

```c
static int led_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc;
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic write; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic write by nimble stack; attr_handle=%d",
                     attr_handle);
        }
        if (attr_handle == led_chr_val_handle) {
            if (ctxt->om->om_len == 1) {
                if (ctxt->om->om_data[0]) {
                    led_on();
                    ESP_LOGI(TAG, "led turned on!");
                } else {
                    led_off();
                    ESP_LOGI(TAG, "led turned off!");
                }
            } else {
                goto error;
            }
            return rc;
        }
        goto error;
    default:
        goto error;
    }
error:
    ESP_LOGE(TAG, "unexpected access operation to led characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}
```

This callback validates the attribute handle and data length (must be exactly 1 byte) before controlling the LED.

#### Heart Rate Characteristic Access Callback (Read)

```c
static int heart_rate_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc;
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }
        if (attr_handle == heart_rate_chr_val_handle) {
            heart_rate_chr_val[1] = get_heart_rate();
            rc = os_mbuf_append(ctxt->om, &heart_rate_chr_val,
                                sizeof(heart_rate_chr_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;
    default:
        goto error;
    }
error:
    ESP_LOGE(TAG,
             "unexpected access operation to heart rate characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}
```

Retrieves the latest heart rate value via `get_heart_rate()`, stores it in `heart_rate_chr_val[1]`, and appends it to the response buffer using `os_mbuf_append`.

#### Subscription Event Handling (CCCD)

In the GAP event handler, subscription events are forwarded to the GATT subscribe callback:

```c
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    // ...
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG,
                 "subscribe event; conn_handle=%d attr_handle=%d "
                 "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle,
                 event->subscribe.reason,
                 event->subscribe.prev_notify, event->subscribe.cur_notify,
                 event->subscribe.prev_indicate, event->subscribe.cur_indicate);
        gatt_svr_subscribe_cb(event);
        return rc;
}
```

The subscription callback updates state:

```c
void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }
    if (event->subscribe.attr_handle == heart_rate_chr_val_handle) {
        heart_rate_chr_conn_handle         = event->subscribe.conn_handle;
        heart_rate_chr_conn_handle_inited  = true;
        heart_rate_ind_status              = event->subscribe.cur_indicate;
    }
}
```

#### Sending Indications

```c
void send_heart_rate_indication(void) {
    if (heart_rate_ind_status && heart_rate_chr_conn_handle_inited) {
        ble_gatts_indicate(heart_rate_chr_conn_handle,
                           heart_rate_chr_val_handle);
        ESP_LOGI(TAG, "heart rate indication sent!");
    }
}
```

`ble_gatts_indicate(conn_handle, chr_val_handle)` sends the current characteristic value as an Indication to the subscribed client. The function only sends if:
1. `heart_rate_ind_status` is true (client enabled indications via CCCD write).
2. `heart_rate_chr_conn_handle_inited` is true (a valid connection handle is stored).

### 4.6 Indication vs Notification Flow

```
Client                          Server (ESP32)
  |                                  |
  |--- Write CCCD 0x0002 ----------->|  (enable indications)
  |                                  |  [BLE_GAP_EVENT_SUBSCRIBE]
  |                                  |  heart_rate_ind_status = true
  |                                  |
  |                                  |  [heart_rate_task ticks at 1 Hz]
  |                                  |  update_heart_rate()
  |                                  |  send_heart_rate_indication()
  |                                  |    ble_gatts_indicate(...)
  |<-- Indication (ATT Handle Value Indicate) ---|
  |--- Confirmation (ATT Handle Value Confirm) ->|
  |                                  |
  |--- Write CCCD 0x0000 ----------->|  (disable indications)
  |                                  |  [BLE_GAP_EVENT_SUBSCRIBE]
  |                                  |  heart_rate_ind_status = false
```

For **Notifications** (no confirmation required), the flow is identical except:
- CCCD is written with `0x0001` instead of `0x0002`.
- The server calls `ble_gatts_notify()` instead of `ble_gatts_indicate()`.
- No confirmation packet is sent by the client.

### 4.7 Attribute Table Example (NimBLE_GATT_Server)

The complete attribute table as registered in the NimBLE server:

| Handle | UUID | Type | Access | Description |
|---|---|---|---|---|
| 0x0001 | `0x2800` | Service Declaration | Read | GATT Service (`0x1801`) |
| ... | ... | ... | ... | ... |
| N | `0x2800` | Service Declaration | Read | Heart Rate Service (`0x180D`) |
| N+1 | `0x2803` | Characteristic Declaration | Read | HR Measurement properties |
| N+2 | `0x2A37` | Characteristic Value | Read, Indicate | Heart Rate Measurement data |
| N+3 | `0x2902` | CCCD | Read, Write | Notification/Indication enable |
| M | `0x2800` | Service Declaration | Read | Automation IO Service (`0x1815`) |
| M+1 | `0x2803` | Characteristic Declaration | Read | LED properties |
| M+2 | (128-bit) | Characteristic Value | Write | LED control data |

---

## 5. Prerequisites and Tools

- **Hardware**: ESP32 development board
- **SDK**: ESP-IDF development environment
- **Mobile App**: nRF Connect for Mobile (iOS/Android) for testing

## 6. Key API Reference Summary

| API | Purpose |
|---|---|
| `nvs_flash_init()` | Initialize NVS flash storage |
| `nimble_port_init()` | Initialize NimBLE host stack |
| `nimble_port_run()` | Run NimBLE host (blocking, call from FreeRTOS task) |
| `ble_hs_util_ensure_addr(0)` | Verify a Bluetooth address is available |
| `ble_hs_id_infer_auto(0, &type)` | Infer optimal address type |
| `ble_hs_id_copy_addr(type, buf, NULL)` | Copy device address to buffer |
| `ble_gap_adv_set_fields(&fields)` | Set advertising data fields |
| `ble_gap_adv_rsp_set_fields(&fields)` | Set scan response data fields |
| `ble_gap_adv_start(type, addr, dur, params, cb, arg)` | Start advertising |
| `ble_gap_conn_find(handle, &desc)` | Retrieve connection descriptor |
| `ble_gap_update_params(handle, &params)` | Request connection parameter update |
| `ble_svc_gatt_init()` | Initialize GATT service (UUID 0x1801) |
| `ble_gatts_count_cfg(svcs)` | Count and validate service table configuration |
| `ble_gatts_add_svcs(svcs)` | Register service table with the stack |
| `ble_gatts_indicate(conn, chr)` | Send Indication for a characteristic |
| `ble_gatts_notify(conn, chr)` | Send Notification for a characteristic |
| `os_mbuf_append(om, data, len)` | Append data to an mbuf for characteristic reads |

## 7. Connection Modes and Discoverable Modes

| Constant | Value | Meaning |
|---|---|---|
| `BLE_GAP_CONN_MODE_NON` | — | Non-connectable (beacon only) |
| `BLE_GAP_CONN_MODE_UND` | — | Undirected connectable |
| `BLE_GAP_CONN_MODE_DIR` | — | Directed connectable (to specific peer) |
| `BLE_GAP_DISC_MODE_NON` | — | Non-discoverable |
| `BLE_GAP_DISC_MODE_LTD` | — | Limited discoverable |
| `BLE_GAP_DISC_MODE_GEN` | — | General discoverable |

## 8. Characteristic Access Operation Codes

| Constant | Direction | Description |
|---|---|---|
| `BLE_GATT_ACCESS_OP_READ_CHR` | Client -> Server | Read characteristic value |
| `BLE_GATT_ACCESS_OP_WRITE_CHR` | Client -> Server | Write characteristic value |
| `BLE_GATT_ACCESS_OP_READ_DSC` | Client -> Server | Read descriptor value |
| `BLE_GATT_ACCESS_OP_WRITE_DSC` | Client -> Server | Write descriptor value |

## 9. GAP Event Types

| Event Constant | Trigger |
|---|---|
| `BLE_GAP_EVENT_CONNECT` | Connection established or failed |
| `BLE_GAP_EVENT_DISCONNECT` | Connection terminated |
| `BLE_GAP_EVENT_CONN_UPDATE` | Connection parameters updated |
| `BLE_GAP_EVENT_SUBSCRIBE` | Client wrote to a CCCD (subscribe/unsubscribe) |
| `BLE_GAP_EVENT_ADV_COMPLETE` | Advertising period ended |
