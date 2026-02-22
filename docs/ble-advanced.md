# ESP32 BLE Advanced Features

Sources:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/ble-multiconnection-guide.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/low-power-mode/low-power-mode-ble.html

---

## Part 1: BLE Multi-Connection Guide

### Overview

The ESP32 BLE stack supports simultaneous operation as both a central and peripheral device across multiple concurrent connections. "Maximum connections" is defined as the maximum number of simultaneous active connections that the device can maintain, whether operating as a central or peripheral.

Both supported BLE host stacks (ESP-Bluedroid and ESP-NimBLE) support a maximum of **9 concurrent connections**.

### Simultaneous Central + Peripheral Roles

The ESP32 can operate as a central (GATT client, initiates connections) and peripheral (GATT server, advertises) at the same time across different connections. The controller manages time-division multiplexing of the radio across all active connections. No connections need to be of a single role type; any combination of central and peripheral connections counts toward the limit.

### Connection Limits

| Host Stack    | Max Concurrent Connections | SDKconfig Keys                                      | Example Code  |
|---------------|---------------------------|-----------------------------------------------------|---------------|
| ESP-Bluedroid | 9                         | `BT_MULTI_CONNECTION_ENBALE`, `BT_ACL_CONNECTIONS`  | `multi_conn`  |
| ESP-NimBLE    | 9                         | `BT_NIMBLE_MAX_CONNECTIONS`                         | `multi_conn`  |

The controller-level limit is set separately via `BTDM_CTRL_BLE_MAX_CONN`, which specifies the maximum number of BLE connections the controller can support concurrently. **This value must match the host-side configuration exactly.** A mismatch between host and controller limits can cause undefined behavior or connection failures.

### Configuration

#### ESP-Bluedroid (menuconfig)

```
Component config → Bluetooth → Bluedroid Options
  [*] Enable BLE multi-connection support  (BT_MULTI_CONNECTION_ENBALE)
  (9) BLE ACL Max Connections              (BT_ACL_CONNECTIONS)

Component config → Bluetooth → Controller Options
  (9) BLE Max Connections                  (BTDM_CTRL_BLE_MAX_CONN)
```

#### ESP-NimBLE (menuconfig)

```
Component config → Bluetooth → NimBLE Options
  (9) Maximum number of concurrent connections  (BT_NIMBLE_MAX_CONNECTIONS)

Component config → Bluetooth → Controller Options
  (9) BLE Max Connections                       (BTDM_CTRL_BLE_MAX_CONN)
```

The example applications are located at:
- `examples/bluetooth/bluedroid/ble/ble_multi_conn/`
- `examples/bluetooth/nimble/multi_conn/`

### Connection Parameter Management

Connection parameters must be configured appropriately in multi-connection scenarios. The key guideline is:

> "As the number of connections increases, the connection interval should be increased accordingly."

This is because each active connection requires dedicated radio time slots. As more connections are added, the controller must schedule advertising, scanning, and data events for each connection within each interval. If the connection interval is too short with many connections, the scheduler cannot fit all events and some will be skipped, causing instability or disconnections.

Practical guidelines:

- **2-3 connections**: Standard connection intervals (7.5 ms - 30 ms) are generally viable.
- **4-7 connections**: Increase intervals to 50 ms - 100 ms or higher to reduce scheduling pressure.
- **8-9 connections**: Intervals of 100 ms or more are recommended; background advertising and scanning should be minimized.
- When operating as a **peripheral**, the central device controls the connection interval negotiation. The peripheral can request parameter updates via the L2CAP connection parameter update procedure, but the central may reject them. This means connection stability depends on the peer's behavior.

Connection parameters relevant to multi-connection scenarios:

| Parameter          | Description                                                  | Multi-connection impact                        |
|--------------------|--------------------------------------------------------------|------------------------------------------------|
| Connection interval| Time between consecutive connection events (1.25 ms units)   | Must increase with more connections            |
| Slave latency      | Number of events peripheral may skip                         | Reduces peripheral wake-ups; reduce scheduler load |
| Supervision timeout| Maximum time without a packet before disconnect (10 ms units)| Should be long enough to tolerate missed events|

### Resource Constraints

#### Memory

The ability to support multiple connections **highly depends on the application's overall memory usage**. Each additional connection requires:

- Additional ACL buffer memory in the controller
- Connection state structures in the host stack
- GATT/ATT context if acting as a server or client per connection

It is recommended to disable unnecessary Bluetooth features to free memory for multi-connection use. Specifically:

- Disable Classic Bluetooth if not needed (`BT_CLASSIC_ENABLED = n`)
- Disable unused BLE profiles and services
- Minimize ATT MTU size if large payloads are not required
- Reduce HCI event and ACL buffer counts to the minimum needed

#### Radio Scheduling

The BLE controller uses a time-division scheduler. With N connections each requiring a connection event per interval, plus advertising and scanning events, the total radio duty cycle rises. At 9 connections with short intervals, the radio may be active nearly continuously, which:

- Increases average current consumption significantly
- Reduces time available for advertising/scanning between connection events
- Increases latency for data transmission on each individual connection

#### Peripheral Role Constraints

When the device operates in the peripheral role, connection stability and overall performance are influenced by the central device and the negotiated connection parameters. The peripheral cannot unilaterally control:

- Connection interval (the central decides whether to accept parameter updates)
- Timing of connection events
- Whether the central responds within the supervision timeout

This means robust multi-peripheral implementations must handle unexpected disconnections gracefully and implement reconnection logic.

---

## Part 2: BLE Low Power Modes

### Overview

The ESP32 BLE stack supports several power-saving modes that trade off radio availability and wakeup latency for reduced current consumption. The three main modes are:

1. **Active mode** - RF, PHY, and baseband fully operational
2. **Modem sleep** - Periodic switching between active and sleep states; RF/PHY/baseband powered down between connection events
3. **Light sleep** - Deep power reduction with the CPU and most peripherals halted between BLE events; requires a low-power clock source

Deep sleep disconnects BLE entirely and is not a BLE operational mode; it is used when BLE is stopped and the device sleeps until woken by an external event.

### Power Consumption Figures

The following figures are representative averages across a standard BLE connection workload. Actual values depend on connection interval, data rate, and application behavior.

| Chip       | Peak (Tx)  | Modem Sleep avg | Light Sleep (Main XTAL) | Light Sleep (32 kHz XTAL) |
|------------|------------|-----------------|-------------------------|---------------------------|
| ESP32      | 231 mA     | 14.1 mA         | Not supported           | 1.9 mA                    |
| ESP32-C3   | 262 mA     | 12 mA           | 2.3 mA                  | 140 µA                    |
| ESP32-S3   | 240 mA     | 17.9 mA         | 3.3 mA                  | 230 µA                    |
| ESP32-C6   | 240 mA     | 22 mA           | 3.3 mA                  | 34 µA                     |
| ESP32-H2   | 82 mA      | 16 mA           | 4.0 mA                  | 24 µA                     |

Key observations:
- **ESP32** does not support light sleep with main XTAL; an external 32 kHz crystal is required for light sleep.
- The 32 kHz XTAL path achieves dramatically lower average current (34 µA on ESP32-C6) compared to modem sleep (22 mA), a reduction of roughly 650x.
- Peak transmit current is chip-dependent and cannot be reduced by sleep modes (the radio must transmit at full power during connection events).

### Sleep Clock Requirements

Per the Bluetooth specification, **sleep clock accuracy must be within 500 PPM**. This constraint drives clock source selection. Insufficient clock accuracy causes:

- ACL connection establishment failures
- ACL connection timeouts and unexpected disconnections
- Increased reconnection overhead

### Clock Source Options

#### Main XTAL (e.g., 40 MHz)

- Config: `CONFIG_BTDM_CTRL_LOW_POWER_CLOCK` = `Main crystal` (`CONFIG_BTDM_CTRL_LPCLK_SEL_MAIN_XTAL`)
- The main XTAL remains **powered on** during light sleep, which is why light-sleep current is higher when this source is selected (2.3-4.0 mA range vs sub-200 µA with 32 kHz)
- Accuracy is well within 500 PPM
- No external component required
- **Not supported on ESP32 for light sleep**

#### 32 kHz External Crystal (Recommended for Low Power)

Two configuration paths are available:

**Path 1** (Bluetooth-specific config):
```
CONFIG_BTDM_CTRL_LOW_POWER_CLOCK = External 32 kHz crystal/oscillator
(CONFIG_BTDM_CTRL_LPCLK_SEL_EXT_32K_XTAL)
```

**Path 2** (RTC clock config, also affects BLE):
```
CONFIG_RTC_CLK_SRC = External 32 kHz crystal
(CONFIG_RTC_CLK_SRC_EXT_CRYS)
```

Important caveat: If the external 32 kHz crystal is **not detected** during Bluetooth LE initialization, the system automatically falls back to the main XTAL. This fallback is silent at the application level unless logs are inspected, and results in higher-than-expected current consumption.

- Accuracy typically within 20-50 PPM, well within the 500 PPM limit
- Requires an external 32.768 kHz crystal on the XTAL_32K_P/N pins
- Allows the main XTAL to power down during sleep, achieving minimum current

#### 136 kHz RC Oscillator (Internal)

- **Not supported** on ESP32 as a BLE low-power clock source
- Does not meet the 500 PPM accuracy requirement

### How to Identify the Active Clock Source

The active clock source can be determined from the BLE initialization log output. The ESP-IDF logs a message during `esp_bt_controller_enable()` that maps to the clock source in use:

| Log Message                              | Active Clock Source              |
|------------------------------------------|----------------------------------|
| `BTDM_INIT: BT controller compile version...` + XTAL ref | Main XTAL                |
| References to `32kHz` or `ext_32k`      | External 32 kHz crystal          |
| References to `rc_32k` or `136kHz`      | Internal RC oscillator (not BLE-compatible on ESP32) |
| References to `32K_XP`                  | External 32 kHz oscillator on 32K_XP pin |

Verify the log at startup to confirm the expected clock source is active before deploying to production.

### Modem Sleep

Modem sleep is the baseline power-saving mode where the BLE controller powers down the RF/PHY/baseband between scheduled connection events, waking only for each connection event window.

- **No external crystal required**
- CPU continues running normally; application is not paused
- The controller wakes at each connection event to transmit/receive
- Average current is proportional to the connection interval: shorter intervals = higher average current
- On ESP32, modem sleep Mode 1 is the recommended setting

Configuration (menuconfig):
```
Component config → Bluetooth → Controller Options
  [*] Bluetooth modem sleep  (BTDM_MODEM_SLEEP)
  Bluetooth modem sleep mode (BTDM_MODEM_SLEEP_MODE)
    (X) Mode 1 (BTDM_MODEM_SLEEP_MODE_ORIG)
```

For newer chips (ESP32-C6 and later), modem sleep is configured via:
```
Component config → Bluetooth → Controller Options
  [*] Enable BLE sleep  (BT_CTRL_BLE_SLEEP_ENABLE)
```

### Light Sleep

Light sleep allows the CPU and most peripherals to halt between BLE events. The system uses FreeRTOS tickless idle to automatically enter light sleep when idle time exceeds the configured threshold.

**Requirements for light sleep with BLE:**

1. Power management enabled:
   ```
   Component config → Power Management
     [*] Support for power management  (PM_ENABLE)
   ```

2. FreeRTOS tickless idle configured:
   ```
   Component config → FreeRTOS
     [*] configUSE_TICKLESS_IDLE  (FREERTOS_USE_TICKLESS_IDLE)
     (3) Expected idle time before entering sleep  (FREERTOS_IDLE_TIME_BEFORE_SLEEP) [ms]
   ```

3. FreeRTOS tick rate set high enough:
   ```
   Component config → FreeRTOS
     (1000) configTICK_RATE_HZ  (FREERTOS_HZ)
   ```

4. Appropriate low-power clock source selected (see above)

5. For ESP32-C6: MAC/baseband power-down must be enabled:
   ```
   Component config → Bluetooth → Controller Options
     [*] Enable BLE MAC/BB power down during light-sleep  (BT_CTRL_LPCLK_SEL_EXT_32K_XTAL)
   ```

**Why light sleep may fail to engage:**

- Insufficient idle time between events (common with short connection intervals or active scanning)
- Excessive UART/log output keeping the system busy
- Continuous BLE scanning without duty-cycling
- Background tasks preventing the idle task from running

### Deep Sleep

Deep sleep powers down the CPU, RAM contents (unless placed in RTC RAM), and all radio hardware. BLE connections are lost. Deep sleep is used when the application needs maximum power savings during known idle periods and can tolerate reconnecting from scratch when it wakes.

- BLE stack must be stopped (`esp_bt_controller_disable()`, `esp_bt_controller_deinit()`) before entering deep sleep
- Wakeup sources: timer, GPIO, touch sensor, ULP coprocessor
- After wakeup, BLE stack must be re-initialized and re-connected
- Achieves the lowest possible system current (10-100 µA range for the entire SoC depending on configuration)

Deep sleep is not covered in the BLE low-power mode documentation because it is a system-level sleep mode rather than a BLE-operational mode.

### Configuration Summary (ESP32 Light Sleep with 32 kHz XTAL)

```
# sdkconfig settings for minimum BLE light-sleep current on ESP32

# System power management
CONFIG_PM_ENABLE=y

# FreeRTOS tickless idle
CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP=3
CONFIG_FREERTOS_HZ=1000

# BLE controller
CONFIG_BTDM_CTRL_BLE_MAX_CONN=1          # adjust as needed
CONFIG_BTDM_MODEM_SLEEP=y
CONFIG_BTDM_MODEM_SLEEP_MODE_ORIG=y       # Mode 1

# Clock source: external 32 kHz crystal
CONFIG_BTDM_CTRL_LPCLK_SEL_EXT_32K_XTAL=y
# OR equivalently:
CONFIG_RTC_CLK_SRC_EXT_CRYS=y
```

Hardware requirement: a 32.768 kHz crystal must be connected to the ESP32's `XTAL_32K_P` and `XTAL_32K_N` pins.

### Trade-offs: Power vs Performance

| Mode               | Avg Current (ESP32) | BLE Connection | Wakeup Latency | External Component |
|--------------------|---------------------|----------------|----------------|-------------------|
| Active             | ~100+ mA            | Full bandwidth | None           | None              |
| Modem sleep        | ~14 mA              | Maintained     | Per conn event | None              |
| Light sleep (XTAL) | ~1.9 mA             | Maintained     | ~1-3 ms        | 32 kHz crystal    |
| Deep sleep         | ~10-100 µA (SoC)    | Disconnected   | Full reconnect | Optional          |

Key trade-off dimensions:

1. **Power vs. latency**: Light sleep with a 32 kHz crystal achieves the lowest average current while keeping BLE connected, but the CPU is halted between events. Any application logic that needs to respond quickly during sleep periods may miss events or add wakeup latency.

2. **Power vs. throughput**: Modem sleep wakes only for connection events. At a 100 ms connection interval, the radio is idle most of the time, reducing throughput to ~few kB/s. Short connection intervals increase throughput but increase average current proportionally.

3. **Power vs. connection count**: Each additional simultaneous BLE connection requires more frequent radio wakeups. Light sleep becomes less effective as connections increase, because the scheduler has fewer long idle periods. At 9 connections, the system may be unable to enter light sleep at all.

4. **Clock accuracy vs. power**: The 32 kHz external crystal is required for light sleep on ESP32 and achieves ~1.9 mA average, but requires an extra board component. Using the main XTAL for the sleep clock keeps the XTAL powered during sleep, negating the light-sleep benefit on the ESP32 (hence "not supported").

5. **Peripheral vs. central power**: As a peripheral, the device can use slave latency to skip connection events, reducing wakeup frequency and average current without disconnecting. As a central, the device controls the schedule but cannot skip events without affecting all connected peripherals.

### Common Issues and Diagnostics

#### BLE ACL Connection Fails or Disconnects in Low Power Mode

**Cause**: The active clock source does not meet the 500 PPM accuracy requirement, or the 32 kHz crystal fallback to main XTAL occurred silently.

**Fix**: Check initialization logs to confirm the expected clock source is active. If the external 32 kHz crystal is not detected, the system falls back to main XTAL. Verify crystal placement and connectivity.

#### Measured Light-Sleep Current Higher Than Expected

**Cause**: The main XTAL clock source is being used (either as primary config or as a fallback), keeping the XTAL powered during light sleep.

**Fix**: Confirm the 32 kHz external crystal path is selected and detected. Inspect the boot log for the clock source message.

#### Unable to Enter Light Sleep

**Cause**: Insufficient idle time for the FreeRTOS tickless idle mechanism to engage. The system needs a contiguous idle period exceeding `FREERTOS_IDLE_TIME_BEFORE_SLEEP` (minimum 3 ms recommended).

**Common triggers**:
- Excessive logging (`ESP_LOGI` / `printf` with UART) during BLE operation
- Continuous BLE scanning (no duty cycle)
- Short connection intervals leaving no gap between connection events
- Application tasks running at high frequency

**Fix**: Reduce logging verbosity, configure BLE scan with a duty cycle (scan window < scan interval), increase connection interval, and profile task execution times with `vTaskGetRunTimeStats()`.
