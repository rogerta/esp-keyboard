# PS/2 → BLE HID Keyboard (ESP32-C6)

Bridges a PS/2 keyboard to Bluetooth Low Energy HID using an ESP32-C6.
The host sees a standard wireless keyboard with encryption and bond persistence.

## Hardware

### Components

- ESP32-C6 development board (e.g. ESP32-C6-DevKitC-1)
- PS/2 keyboard (mini-DIN 6 connector)
- Jumper wires or breadboard
- (Optional) BSS138-based bidirectional level shifter if powering the keyboard at 5 V

### PS/2 Connector Pinout

Mini-DIN 6, viewed from the **plug** (cable) side:

```
     ┌─────┐
    / 6   5 \
   | 4     3 |
    \ 2   1 /
     └─────┘
```

| Pin | Signal |
|-----|--------|
| 1   | DATA   |
| 3   | GND    |
| 4   | VCC    |
| 5   | CLK    |
| 2, 6 | (n/c) |

### Wiring

```mermaid
graph LR
    subgraph PS/2 Keyboard
        CLK[Pin 5 — CLK]
        DATA[Pin 1 — DATA]
        VCC[Pin 4 — VCC]
        GND_K[Pin 3 — GND]
    end

    subgraph ESP32-C6
        GPIO6[GPIO 6]
        GPIO7[GPIO 7]
        V33[3.3 V]
        GND_E[GND]
    end

    CLK --- GPIO6
    DATA --- GPIO7
    VCC --- V33
    GND_K --- GND_E
```

| PS/2 Pin | Signal | ESP32-C6 | Notes |
|----------|--------|----------|-------|
| 5 | CLK  | GPIO 6 (default) | Open-drain, internal pull-up. Deep sleep wake source. |
| 1 | DATA | GPIO 7 (default) | Open-drain, internal pull-up. |
| 4 | VCC  | 3.3 V | See [Power](#power) below. |
| 3 | GND  | GND | |

> **CLK must be GPIO 0–7.** Deep sleep wakeup on the ESP32-C6 requires an
> LP (low-power) GPIO. GPIOs 0–7 are LP-capable; the DATA pin has no such
> restriction but defaults to GPIO 7 for convenience. Both pins are
> configurable via `idf.py menuconfig`.

### Power

Most PS/2 keyboards work at 3.3 V despite the spec calling for 5 V.
Powering from the ESP32-C6's 3.3 V rail is the simplest option:

```mermaid
graph TD
    USB["USB / Battery<br>5 V"] -->|5 V| LDO["ESP32-C6 on-board LDO"]
    LDO -->|3.3 V| ESP[ESP32-C6]
    LDO -->|3.3 V| KBD[PS/2 Keyboard VCC]
    ESP <-->|"CLK, DATA<br>(open-drain)"| KBD
```

If your keyboard requires 5 V, add a **bidirectional level shifter**
(e.g. BSS138 module) on CLK and DATA, and power the keyboard from 5 V:

```mermaid
graph LR
    subgraph 3.3 V domain
        ESP_CLK[ESP GPIO 6]
        ESP_DATA[ESP GPIO 7]
    end

    subgraph Level Shifter
        LS["BSS138 &times; 2"]
    end

    subgraph 5 V domain
        KBD_CLK[PS/2 CLK]
        KBD_DATA[PS/2 DATA]
    end

    ESP_CLK <--> LS <--> KBD_CLK
    ESP_DATA <--> LS <--> KBD_DATA
```

> **ESP32-C6 GPIOs are NOT 5 V tolerant** (absolute max 3.6 V).
> Do not connect PS/2 lines directly when the keyboard is powered at 5 V.

## Architecture

### System Overview

```mermaid
graph LR
    KBD["PS/2 Keyboard"] <-->|"CLK + DATA<br>(open-drain bus)"| ESP["ESP32-C6<br>ps2_ble_kbd"]
    ESP <-->|"BLE HID<br>(GATT notify)"| HOST["PC / Phone / Tablet"]
```

### Software Components

```mermaid
graph TD
    subgraph ISR Context
        ISR["PS/2 CLK NEGEDGE ISR<br><i>ps2_proto.c</i>"]
    end

    subgraph "ps2_task (prio 5)"
        PROTO["ps2_proto<br>byte rx/tx"]
        KBD["ps2_kbd<br>Set 2 scan-code decoder"]
        MGR["ps2_mgr<br>key state + HID report builder"]
    end

    subgraph "ble_task (prio 4)"
        BLE_SEND["hid_keyboard_send<br>BLE NOTIFY"]
    end

    subgraph "NimBLE host task"
        GAP["GAP event handler<br>connect / encrypt / advertise"]
        GATT["GATT services<br>DIS + BAS + HID"]
    end

    ISR -->|"rx_queue<br>(uint8_t)"| PROTO
    PROTO --> KBD
    KBD -->|"event_queue<br>(keycode, press/release)"| MGR
    MGR -->|"g_report_queue<br>(8-byte HID report)"| BLE_SEND
    BLE_SEND --> GATT
    GATT -->|"g_led_queue<br>(LED output report)"| MGR
```

### Keypress Data Flow

```mermaid
sequenceDiagram
    participant KB as PS/2 Keyboard
    participant ISR as CLK ISR
    participant PS2 as ps2_task
    participant BLE as ble_task
    participant HOST as BLE Host

    KB->>ISR: CLK falling edges (11 bits)
    ISR->>PS2: rx_queue + task notify
    PS2->>PS2: ps2_kbd: decode scan code
    PS2->>PS2: ps2_mgr: update bitmask, build report
    PS2->>BLE: g_report_queue (8-byte report)
    BLE->>HOST: GATT NOTIFY (input report)

    Note over HOST,KB: LED feedback (reverse path)
    HOST->>BLE: GATT WRITE (output report)
    BLE->>PS2: g_led_queue + task notify
    PS2->>KB: PS/2 0xED + LED byte
```

### BLE GATT Services

```mermaid
graph TD
    subgraph "Device Information Service (0x180A)"
        MANUF["Manufacturer Name (0x2A29)<br>READ"]
        PNP["PnP ID (0x2A50)<br>READ"]
    end

    subgraph "Battery Service (0x180F)"
        BATT["Battery Level (0x2A19)<br>READ | NOTIFY"]
    end

    subgraph "HID Service (0x1812)"
        INFO["HID Info (0x2A4A)<br>READ"]
        RMAP["Report Map (0x2A4B)<br>READ"]
        CTRL["HID Control Point (0x2A4C)<br>WRITE_NO_RSP"]
        PMOD["Protocol Mode (0x2A4E)<br>READ | WRITE_NO_RSP"]
        INP["Input Report (0x2A4D)<br>READ | NOTIFY | READ_ENC<br><i>Report Ref: id=0 type=Input</i>"]
        OUT["Output Report (0x2A4D)<br>READ | WRITE | READ_ENC | WRITE_ENC<br><i>Report Ref: id=0 type=Output</i>"]
    end
```

## Power Management

Three power states, entered automatically:

```mermaid
stateDiagram-v2
    [*] --> Active : Boot / deep sleep wake
    Active --> LightSleep : All tasks idle
    LightSleep --> Active : PS/2 CLK edge
    Active --> DeepSleep : No keypress for N seconds
    DeepSleep --> [*] : Keypress (CLK low)

    note right of LightSleep
        Automatic via tickless idle
        CPU at 10 MHz min
        BLE connection maintained
    end note

    note right of DeepSleep
        ~5 µA draw
        BLE disconnected
        Full reboot on wake
    end note
```

| State | Entry trigger | Current draw | BLE | Wake source |
|-------|---------------|-------------|-----|-------------|
| **Active** | Keypress / BLE event | ~30–80 mA | Connected | — |
| **Light sleep** | FreeRTOS idle (automatic) | ~1–3 mA | Maintained | PS/2 CLK GPIO |
| **Deep sleep** | No keypress for timeout | ~5 µA | Disconnected | PS/2 CLK GPIO (LP) |

After deep sleep wake the chip reboots. BLE re-advertises and the bonded
host reconnects automatically (bond keys persist in NVS). The first
keypress is consumed by the wake event.

## Build

```bash
. ~/esp/esp-idf/export.sh
idf.py -C src/ps2_ble_kbd build
idf.py -C src/ps2_ble_kbd flash monitor
```

## Configuration

```bash
idf.py -C src/ps2_ble_kbd menuconfig
```

Under **PS/2 Keyboard**:

| Option | Default | Range | Description |
|--------|---------|-------|-------------|
| `PS2_CLK_GPIO` | 6 | 0–7 | PS/2 clock GPIO. Must be LP-capable for deep sleep wake. |
| `PS2_DATA_GPIO` | 7 | — | PS/2 data GPIO. |
| `DEEP_SLEEP_TIMEOUT_SEC` | 300 | 30–3600 | Seconds of inactivity before deep sleep. |
