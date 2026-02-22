# ESP32 Bluetooth Architecture

Source: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/bt-architecture/overview.html

---

## Overview

Bluetooth wireless technology is a short-range communication standard known for reliability, low power consumption, and cost efficiency. The ESP-IDF Bluetooth stack divides Bluetooth into two categories:

- **Bluetooth Classic (BR/EDR):** Optimized for continuous, high-throughput data streaming; suitable for audio applications.
- **Bluetooth Low Energy (BLE):** Designed for low-power, intermittent data transmission; ideal for sensors and wearables.

The original ESP32 is the only variant in the family that supports dual-mode Bluetooth (both Classic and BLE simultaneously). All newer variants support BLE only.

---

## Chip Bluetooth Capabilities

| Chip Series | Bluetooth Classic (BR/EDR) | Bluetooth LE |
|-------------|:--------------------------:|:------------:|
| ESP32       | Y                          | Y            |
| ESP32-S3    | N                          | Y            |
| ESP32-C2    | N                          | Y            |
| ESP32-C3    | N                          | Y            |
| ESP32-C5    | N                          | Y            |
| ESP32-C6    | N                          | Y            |
| ESP32-C61   | N                          | Y            |
| ESP32-H2    | N                          | Y            |

The ESP32 supports Dual-Mode Bluetooth 4.2, enabling both Classic and Low Energy operation concurrently.

---

## Bluetooth Protocol Stack Architecture

The stack is split into two major components that communicate via the HCI (Host Controller Interface):

```
+-------------------------------+
|          Application          |
+-------------------------------+
|     Host Stack (Software)     |
|  GAP | GATT | SMP | L2CAP    |
|  ATT | SDP                    |
+-------------------------------+
|    HCI (Transport Layer)      |
+-------------------------------+
|   Controller Stack (Hardware) |
|  Device Mgr | Link Manager   |
|  Link Controller | Baseband  |
|  PHY                          |
+-------------------------------+
         Hardware
```

### Controller Stack

The controller runs on the chip hardware and handles all real-time radio operations. It is accessed via a library (closed-source binary blob) and is not directly modifiable.

| Layer           | Responsibility                                                                                   |
|-----------------|--------------------------------------------------------------------------------------------------|
| PHY             | Manages Bluetooth signal transmission and reception in the 2.4 GHz ISM band                     |
| Baseband        | Low-level timing, frequency hopping, packet formatting, error correction                         |
| Link Controller | State machine operations for connection/disconnection, flow control, retransmissions             |
| Link Manager    | Link setup, authentication, encryption, power control                                            |
| Device Manager  | Oversees device states, paging, inquiry processes, stored link keys                              |
| HCI             | Interface between the controller and the host; exposes standardized commands and events          |

### Host Stack

The host stack runs in software on the application processor. ESP-IDF provides two host stack implementations:

| Stack     | BT Classic | BLE | Notes                                         |
|-----------|:----------:|:---:|-----------------------------------------------|
| Bluedroid | Y          | Y   | Default; supports dual-mode; larger footprint |
| NimBLE    | N          | Y   | Lightweight; BLE-only; smaller code and RAM   |

---

## Controller vs. Host Split Architecture

The controller and host are decoupled and communicate exclusively through the HCI layer. This split enables three deployment scenarios:

### Scenario 1 - Default (Integrated, VHCI)

```
[Application]
     |
[Bluedroid Host Stack]
     |
[VHCI - Virtual HCI (software)]
     |
[ESP32 Bluetooth Controller]
```

The Bluedroid (or NimBLE) host and the controller both run on the same ESP32 chip. Instead of a physical HCI transport, a software-implemented Virtual HCI (VHCI) layer bridges them. This is the standard configuration used in most ESP-IDF applications.

### Scenario 2 - Controller Only (External Host)

```
[External Host: Linux / Android / Other]
     |
[Physical HCI: UART]
     |
[ESP32 Bluetooth Controller only]
```

The ESP32 acts exclusively as a Bluetooth controller. An external host device (e.g., a Linux system or Android device) manages the host stack over a physical HCI UART connection. Useful for offloading Bluetooth to an ESP32 module attached to a more capable processor.

### Scenario 3 - Testing / Certification (BQB)

Similar to Scenario 2. The controller is connected to Bluetooth Qualification Body (BQB) test tools via UART for Bluetooth certification testing.

---

## HCI Transport Details

### Virtual HCI (VHCI) - Integrated Mode

VHCI is a software HCI layer used when both the host and controller run on the same chip. The VHCI API:

| Function                                                             | Description                                              |
|----------------------------------------------------------------------|----------------------------------------------------------|
| `esp_vhci_host_check_send_available()`                               | Returns `true` if controller is ready to receive HCI data |
| `esp_vhci_host_send_packet(uint8_t *data, uint16_t len)`             | Transmits an HCI packet to the controller                |
| `esp_vhci_host_register_callback(const esp_vhci_host_callback_t *)` | Registers host-side notification callbacks               |

VHCI callbacks:

| Callback                                        | Description                                        |
|-------------------------------------------------|----------------------------------------------------|
| `notify_host_send_available()`                  | Called when the controller can accept more data    |
| `notify_host_recv(uint8_t *data, uint16_t len)` | Called when the controller delivers a packet       |

### UART HCI - External Host Mode

When the ESP32 is used as a standalone controller, HCI is transported over UART:

| Parameter        | Range             | Default   |
|------------------|-------------------|-----------|
| UART number      | UART1 or UART2    | -         |
| Baud rate        | 115,200–921,600   | 921,600   |

---

## Operating Environment

### FreeRTOS Task Architecture

The entire Bluetooth stack runs within FreeRTOS tasks. Task priorities are assigned by function:

- **Controller tasks** have the highest priority, reflecting their hard real-time requirements.
- On the dual-core ESP32, controller tasks are second in priority only to IPC (Inter-Processor Communication) tasks, which coordinate operations between the two CPU cores.
- **Bluedroid Host** is composed of three tasks:
  - **BTC (Bluetooth Transport Controller layer):** Provides application-level APIs and manages profiles.
  - **BTU (Bluetooth Upper Layer):** Implements core protocols (L2CAP, GATT, SMP).
  - **HCI task:** Handles HCI packet queuing between the host and the VHCI layer.

### OS Adaptation Layer (Bluedroid)

Bluedroid was ported from Android and uses the following FreeRTOS abstractions internally:

| Mechanism          | Android Origin      | FreeRTOS Equivalent                                        |
|--------------------|---------------------|------------------------------------------------------------|
| Timer (Alarm)      | POSIX timer         | FreeRTOS Timer packaged as `Alarm`                         |
| Task (Thread)      | POSIX Thread        | FreeRTOS Task using FreeRTOS Queue                         |
| Semaphore (Future) | Condition variable  | `xSemaphoreTake` as `future_await`, `xSemaphoreGive` as `future_ready` |
| Allocator          | `malloc/free`       | Wrapped `malloc/free` as Allocator function                |

---

## Bluedroid Stack

ESP-Bluedroid is a modified version of Android's Bluedroid stack. It supports both Bluetooth Classic and BLE, making it the appropriate choice for dual-mode applications.

### Two-Layer Structure

```
+------------------------------------------+
|  BTC (Bluetooth Transport Controller)     |
|  - Application-facing APIs (esp_ prefix) |
|  - Profile management                    |
+------------------------------------------+
|  BTU (Bluetooth Upper Layer)              |
|  - L2CAP, GATT, SMP                      |
|  - bta_ prefixed interfaces              |
+------------------------------------------+
```

- **BTU** implements core host protocols and exposes `bta`-prefixed interfaces.
- **BTC** wraps BTU and exposes `esp`-prefixed APIs to the application. Developers should only use `esp`-prefixed APIs.

### Directory Structure

```
components/bt/host/bluedroid/
├── api/            All public APIs (except controller-related)
├── bta/            Bluetooth adaptation layer for host protocol interfaces
├── btc/            Bluetooth control layer; upper-layer protocols and profiles
├── common/
│   └── include/
│       └── common/ Common header files shared across the protocol stack
├── config/         Protocol stack parameter configuration
├── device/         Controller device control and HCI command processing
├── external/
│   └── sbc/        SBC audio codec (non-Bluetooth code, used by A2DP)
├── hci/            HCI layer protocol implementation
├── main/           Main program for process startup and halting
├── stack/          Bottom-layer host protocols: GAP, ATT, GATT, SDP, SMP
└── Kconfig.in      Menuconfig configuration options
```

---

## NimBLE Stack

NimBLE is an open-source BLE-only stack from the Apache Mynewt project. ESP-IDF includes a port of NimBLE as an alternative to Bluedroid.

- **BLE only:** No support for Bluetooth Classic.
- **Smaller code size and memory footprint** compared to Bluedroid.
- **Recommended** for resource-constrained applications or devices that only require BLE.
- Uses the same underlying ESP32 Bluetooth controller; only the host stack differs.

---

## Controller Lifecycle and Configuration

### Lifecycle Functions

| Function                                      | Description                                |
|-----------------------------------------------|--------------------------------------------|
| `esp_bt_controller_init(esp_bt_controller_config_t *)` | Allocates controller tasks and resources  |
| `esp_bt_controller_enable(esp_bt_mode_t mode)` | Activates the controller in the given mode |
| `esp_bt_controller_disable()`                 | Deactivates the controller                 |
| `esp_bt_controller_deinit()`                  | Releases all controller resources          |
| `esp_bt_controller_get_status()`              | Returns IDLE, INITED, or ENABLED           |

The controller must be initialized before any other Bluetooth functions are called.

### Configuration Parameters

| Parameter                          | Range          | Default |
|------------------------------------|----------------|---------|
| BLE maximum connections            | 1–9            | 3       |
| BR/EDR ACL connections             | 1–7            | 2       |
| Scan duplicate list size (normal)  | 10–1,000       | 100     |
| Duplicate list refresh interval    | 0–100 seconds  | 0       |
| Encryption key size minimum        | 7–16 bytes     | 7       |

### TX Power Control

BLE TX power is configurable per connection handle (handles 0–8), as well as for advertising and scan modes. Power levels range from -12 dBm to +9 dBm in 3 dBm steps.

BR/EDR TX power is configurable with a global min/max range.

### BLE Sleep Clock Accuracy (SCA)

Supported SCA values: 500 ppm and 250 ppm.

---

## Memory and Resource Requirements

### Controller Memory Release

After initialization, unused Bluetooth memory can be reclaimed. The `esp_bt_controller_mem_release(esp_bt_mode_t mode)` function releases memory associated with the specified mode:

- **BLE-only mode:** Releases Classic BT memory (~70 KB recoverable in total depending on mode).
- **Classic-only mode:** Releases BLE memory.
- **Dual-mode (BTDM):** No release possible; both stacks active.

`esp_bt_mem_release(esp_bt_mode_t mode)` releases both controller and host stack memory for the given mode.

### Stack Selection Trade-offs

| Attribute          | Bluedroid         | NimBLE            |
|--------------------|-------------------|-------------------|
| BLE support        | Yes               | Yes               |
| Classic BT support | Yes               | No                |
| Code size          | Larger            | Smaller           |
| RAM usage          | Higher            | Lower             |
| Use case           | Dual-mode apps    | BLE-only, constrained devices |

---

## Bluetooth Host Protocol Layers (Detail)

### Common to Both Classic and BLE

| Layer | Full Name                   | Role                                                              |
|-------|-----------------------------|-------------------------------------------------------------------|
| L2CAP | Logical Link Control and Adaptation Protocol | Data segmentation, reassembly, channel multiplexing  |
| GAP   | Generic Access Profile      | Device discovery, connection establishment, roles and modes       |
| SMP   | Security Manager Protocol   | Authentication, encryption, secure pairing                        |

### BLE-Specific

| Layer | Full Name                            | Role                                                           |
|-------|--------------------------------------|----------------------------------------------------------------|
| ATT   | Attribute Protocol                   | Attribute-based data exchange                                  |
| GATT  | Generic Attribute Profile            | Services and characteristics structure built on top of ATT     |

### Classic BT-Specific

| Layer | Full Name                            | Role                                                           |
|-------|--------------------------------------|----------------------------------------------------------------|
| SDP   | Service Discovery Protocol           | Advertising and discovering available Bluetooth services       |

---

## References

- [ESP-IDF BT Architecture Overview](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/bt-architecture/overview.html)
- [ESP-IDF Bluetooth Classic Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/classic-bt/index.html)
- [ESP-IDF Bluetooth LE Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/index.html)
- [ESP-IDF Controller & VHCI API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/controller_vhci.html)
- [Bluetooth Core Specification (Bluetooth SIG)](https://www.bluetooth.com/specifications/specs/)
- [Apache Mynewt NimBLE](https://mynewt.apache.org/latest/network/docs/index.html)
