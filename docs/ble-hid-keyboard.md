# BLE HID Keyboard on ESP32-C6

Reference for implementing a Bluetooth Low Energy keyboard using the HID-over-GATT Profile (HOGP) with NimBLE on ESP32-C6.

See the working example at [`../examples/ble_hid_keyboard/`](../examples/ble_hid_keyboard/).

---

## HID-over-GATT Profile (HOGP) Overview

HOGP is the BLE profile that allows peripheral devices (keyboards, mice, gamepads) to appear as standard HID devices to a host OS without custom drivers. It runs on top of standard BLE GATT services.

The keyboard acts as a **GATT server** (peripheral / advertiser). The host OS acts as the **GATT client** (central / initiator / HID host).

Key requirement: the connection **must be encrypted** before the host can subscribe to keyboard report notifications. This means pairing is mandatory per the HOGP specification.

---

## Required GATT Services

HOGP mandates three services coexist on the peripheral:

| Service | UUID | Role |
|---|---|---|
| Device Information Service (DIS) | 0x180A | Identifies the device to the host |
| Battery Service (BAS) | 0x180F | Reports battery level (required by HOGP) |
| HID Service | 0x1812 | Contains keyboard reports and configuration |

---

## Device Information Service (0x180A)

Minimal required characteristics for a HID device:

| Characteristic | UUID | Properties | Value |
|---|---|---|---|
| Manufacturer Name String | 0x2A29 | READ | UTF-8 string, e.g. `"Espressif"` |
| PnP ID | 0x2A50 | READ | 7 bytes: see below |

**PnP ID byte layout** (7 bytes, little-endian):

```
Byte 0:   Vendor ID Source  — 0x01=Bluetooth SIG, 0x02=USB-IF
Bytes 1-2: Vendor ID         — e.g. 0x303A (Espressif USB-IF VID), LE
Bytes 3-4: Product ID        — application-defined, LE
Bytes 5-6: Product Version   — application-defined, LE
```

Example: `{0x02, 0x3A, 0x30, 0x4B, 0x4F, 0x01, 0x00}`

---

## Battery Service (0x180F)

| Characteristic | UUID | Properties | Value |
|---|---|---|---|
| Battery Level | 0x2A19 | READ, NOTIFY | uint8, 0–100 (percent) |

---

## HID Service (0x1812)

### Characteristics

| Characteristic | UUID | Properties | Mandatory |
|---|---|---|---|
| HID Information | 0x2A4A | READ | Yes |
| Report Map | 0x2A4B | READ | Yes |
| HID Control Point | 0x2A4C | WRITE\_NO\_RSP | Yes |
| Protocol Mode | 0x2A4E | READ, WRITE\_NO\_RSP | If boot protocol supported |
| Input Report | 0x2A4D | READ, NOTIFY | Yes (one per input report) |
| Output Report | 0x2A4D | READ, WRITE, WRITE\_NO\_RSP | Yes for keyboards (LED state) |
| Boot Keyboard Input Report | 0x2A22 | READ, NOTIFY | If boot protocol supported |
| Boot Keyboard Output Report | 0x2A32 | READ, WRITE, WRITE\_NO\_RSP | If boot protocol supported |

**Both Input and Output Reports share UUID 0x2A4D.** They are distinguished by their Report Reference descriptor (0x2908).

### HID Information (0x2A4A)

4 bytes:

```
Bytes 0-1: bcdHID         — HID spec version in BCD, e.g. 0x0111 (1.11) → {0x11, 0x01}
Byte 2:    bCountryCode   — 0x00 = not localized
Byte 3:    Flags          — bit 0: RemoteWake, bit 1: NormallyConnectable
```

Typical value: `{0x11, 0x01, 0x00, 0x02}` (HID 1.11, no country, normally connectable).

### HID Control Point (0x2A4C)

Write-only. Host writes:
- `0x00` = Suspend (device may enter low power)
- `0x01` = Exit Suspend

### Protocol Mode (0x2A4E)

- `0x00` = Boot Protocol (fixed 8-byte report format, no descriptor required)
- `0x01` = Report Protocol (default; uses the Report Map descriptor)

Devices not supporting boot protocol can omit this characteristic and the boot report characteristics.

### Report Reference Descriptor (0x2908)

Each Report characteristic (0x2A4D) must have a Report Reference descriptor that identifies it:

```
Byte 0: Report ID    — matches the Report ID in the HID report descriptor, or 0 if unused
Byte 1: Report Type  — 0x01=Input, 0x02=Output, 0x03=Feature
```

---

## HID Report Descriptor (Standard 6KRO Keyboard)

The Report Map characteristic contains the HID report descriptor — a binary structure that describes the format of all HID reports the device sends and receives.

This descriptor defines:
- An **8-byte keyboard input report** (modifier + reserved + 6 keycodes)
- A **1-byte LED output report** (Num Lock, Caps Lock, Scroll Lock, Compose, Kana)
- No Report IDs (single report per direction)

```c
static const uint8_t hid_report_map[] = {
    0x05, 0x01,   /* Usage Page (Generic Desktop)      */
    0x09, 0x06,   /* Usage (Keyboard)                  */
    0xA1, 0x01,   /* Collection (Application)          */

    /* Modifier keys: 8 bits, one bit per key */
    0x05, 0x07,   /*   Usage Page (Keyboard/Keypad)    */
    0x19, 0xE0,   /*   Usage Minimum (Left Control)    */
    0x29, 0xE7,   /*   Usage Maximum (Right GUI)       */
    0x15, 0x00,   /*   Logical Minimum (0)             */
    0x25, 0x01,   /*   Logical Maximum (1)             */
    0x75, 0x01,   /*   Report Size (1 bit)             */
    0x95, 0x08,   /*   Report Count (8)                */
    0x81, 0x02,   /*   Input (Data, Variable, Absolute)*/

    /* Reserved byte */
    0x95, 0x01,   /*   Report Count (1)                */
    0x75, 0x08,   /*   Report Size (8 bits)            */
    0x81, 0x01,   /*   Input (Constant)                */

    /* LED output: 5 bits */
    0x95, 0x05,   /*   Report Count (5)                */
    0x75, 0x01,   /*   Report Size (1 bit)             */
    0x05, 0x08,   /*   Usage Page (LEDs)               */
    0x19, 0x01,   /*   Usage Minimum (Num Lock)        */
    0x29, 0x05,   /*   Usage Maximum (Kana)            */
    0x91, 0x02,   /*   Output (Data, Variable, Absolute)*/

    /* LED padding: 3 bits to fill byte */
    0x95, 0x01,   /*   Report Count (1)                */
    0x75, 0x03,   /*   Report Size (3 bits)            */
    0x91, 0x01,   /*   Output (Constant)               */

    /* Key codes: 6-byte array */
    0x95, 0x06,   /*   Report Count (6)                */
    0x75, 0x08,   /*   Report Size (8 bits)            */
    0x15, 0x00,   /*   Logical Minimum (0)             */
    0x25, 0x65,   /*   Logical Maximum (101)           */
    0x05, 0x07,   /*   Usage Page (Keyboard/Keypad)    */
    0x19, 0x00,   /*   Usage Minimum (0)               */
    0x29, 0x65,   /*   Usage Maximum (101)             */
    0x81, 0x00,   /*   Input (Data, Array, Absolute)   */

    0xC0          /* End Collection                    */
};
```

---

## Input Report Format (8 bytes)

Sent from the keyboard to the host via BLE NOTIFY on the Input Report characteristic.

```
Byte 0:   Modifier bitmask
            bit 0: Left Control
            bit 1: Left Shift
            bit 2: Left Alt
            bit 3: Left GUI (Windows/Command key)
            bit 4: Right Control
            bit 5: Right Shift
            bit 6: Right Alt
            bit 7: Right GUI
Byte 1:   Reserved — always 0x00
Bytes 2-7: Key codes — up to 6 simultaneously pressed non-modifier keys
            0x00 = no key in this slot
```

To type the letter 'A' (uppercase): `{0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}`
- byte 0 = 0x02 (Left Shift)
- byte 2 = 0x04 (HID usage for 'a'/'A')

Always follow a key-press report with a key-release report (all zeros).

---

## Output Report Format (1 byte)

Sent from the host to the keyboard via WRITE to the Output Report characteristic. Carries LED state.

```
Bit 0: Num Lock LED
Bit 1: Caps Lock LED
Bit 2: Scroll Lock LED
Bit 3: Compose LED
Bit 4: Kana LED
Bits 5-7: Reserved (padding, always 0)
```

---

## Key Codes (USB HID Usage IDs)

Keyboard keycodes are USB HID Page 0x07 usage IDs. Selected values:

| Key | Code | Key | Code |
|---|---|---|---|
| a–z | 0x04–0x1D | 1–9 | 0x1E–0x26 |
| 0 | 0x27 | Enter | 0x28 |
| Escape | 0x29 | Backspace | 0x2A |
| Tab | 0x2B | Space | 0x2C |
| Caps Lock | 0x39 | F1–F12 | 0x3A–0x45 |
| Right Arrow | 0x4F | Left Arrow | 0x50 |
| Down Arrow | 0x51 | Up Arrow | 0x52 |
| Delete | 0x4C | Home | 0x4A |
| End | 0x4D | Page Up | 0x4B |
| Page Down | 0x4E | | |

Complete table in [`../examples/ble_hid_keyboard/main/keycodes.h`](../examples/ble_hid_keyboard/main/keycodes.h).

---

## GAP Advertising

The device must advertise the HID Service UUID (0x1812) and set the GAP Appearance to keyboard (0x03C1) so host OSes recognize it as a keyboard before connecting.

```c
fields.appearance = 0x03C1;          /* Keyboard */
fields.appearance_is_present = 1;
fields.uuids16 = &hid_svc_uuid;      /* UUID 0x1812 */
fields.num_uuids16 = 1;
fields.uuids16_is_complete = 1;
```

The full device name is typically placed in the scan response to save space in the primary advertisement packet.

---

## Pairing and Bonding

### Why pairing is required

HOGP mandates encrypted connections. The Input Report characteristic and its CCCD (subscription descriptor) must have `BLE_GATT_CHR_F_READ_ENC` set, requiring an encrypted link before the host can subscribe to reports.

### Security parameters

```c
ble_hs_cfg.sm_io_cap       = BLE_SM_IO_CAP_NO_IO;  /* Just Works pairing */
ble_hs_cfg.sm_bonding      = 1;                     /* store keys in NVS */
ble_hs_cfg.sm_mitm         = 0;                     /* no MITM with NO_IO */
ble_hs_cfg.sm_sc           = 1;                     /* Secure Connections (LE SC) */
ble_hs_cfg.sm_our_key_dist  = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
```

With `BLE_SM_IO_CAP_NO_IO`, pairing is "Just Works" — no passkey displayed or entered. Use `BLE_SM_IO_CAP_DISP_ONLY` to show a passkey and get MITM protection.

### Key distribution

- `BLE_SM_PAIR_KEY_DIST_ENC`: distributes LTK/EDIV/Rand for re-encryption on reconnect
- `BLE_SM_PAIR_KEY_DIST_ID`: distributes IRK for resolving random private addresses

### NVS persistence

Enable `CONFIG_BT_NIMBLE_NVS_PERSIST=y` so bonding keys survive reboots. Without this, the host must re-pair every power cycle, which causes Windows/macOS to show repeated pairing dialogs.

### Handling `BLE_GAP_EVENT_REPEAT_PAIRING`

When a bonded host reconnects and tries to pair again (e.g., OS lost its bond record), the peripheral should delete the old bond and retry:

```c
case BLE_GAP_EVENT_REPEAT_PAIRING:
    ble_store_util_delete_peer_records(
        &event->repeat_pairing.conn_desc.peer_id_addr);
    return BLE_GAP_REPEAT_PAIRING_RETRY;
```

---

## Connection Flow

```
Peripheral (ESP32-C6)                Host OS
        |                                |
        |<-- Advertising (0x1812) -------|  host discovers keyboard
        |<-- CONNECT_REQ ---------------|
        |  [BLE_GAP_EVENT_CONNECT]       |
        |                                |
        |<== SMP: Pairing Exchange =====>|  Just Works or passkey
        |  [BLE_GAP_EVENT_ENC_CHANGE]    |  LTK stored in NVS
        |                                |
        |<-- Write CCCD (enable notify)--|  requires encryption
        |  [BLE_GAP_EVENT_SUBSCRIBE]     |
        |                                |
        |--- Input Report (NOTIFY) ----->|  keyboard report sent
        |--- Input Report (NOTIFY) ----->|  key release
```

### Reconnection (bonded)

On reconnect, the host re-establishes encryption using the stored LTK without re-pairing. `BLE_GAP_EVENT_ENC_CHANGE` fires again when encryption is restored. Do not send reports before this event.

---

## Sending Reports

```c
int hid_keyboard_send(uint16_t conn_handle, const hid_keyboard_report_t *report)
{
    struct os_mbuf *om = ble_hs_mbuf_from_flat(report, sizeof(*report));
    if (!om) return BLE_HS_ENOMEM;
    /* ble_gattc_notify_custom consumes the mbuf */
    return ble_gattc_notify_custom(conn_handle, input_report_handle, om);
}
```

Always send a key-release report (all zeros) after each key-press report. Aim for 10–50 ms between press and release for reliable input recognition.

---

## sdkconfig Options

```
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_NVS_PERSIST=y
CONFIG_BT_NIMBLE_SM_SC=1
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3
CONFIG_BT_NIMBLE_SVC_GAP_DEVICE_NAME="ESP32-C6 Keyboard"
```

---

## Limitations of This Descriptor

- **6KRO**: maximum 6 simultaneous non-modifier keys. For NKRO (n-key rollover), the descriptor must use a 16-byte bitmap instead of a 6-byte array.
- **No media/consumer keys**: adding volume, play/pause, etc. requires a second HID report for the Consumer Control usage page (0x0C), with its own Report ID.
- **No boot protocol**: the example omits boot keyboard characteristics (0x2A22, 0x2A32). Boot protocol provides a fixed-format fallback that works before the OS loads the HID driver (e.g., BIOS/UEFI). Add boot characteristics and Protocol Mode (0x2A4E) to support it.
- **Single connection**: only one host at a time.

---

## Testing

1. Flash the example to an ESP32-C6.
2. Open Bluetooth settings on the host OS and scan for new devices.
3. Select "ESP32-C6 Keyboard" — the OS initiates pairing automatically.
4. Accept the pairing dialog (Just Works: no PIN required).
5. Open a text editor and observe the demo task typing "hello world" periodically.
6. Use nRF Connect (iOS/Android) to inspect the GATT service table and individual characteristic values.

---

## References

- Bluetooth SIG HOGP specification: https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile-1-0/
- USB HID Usage Tables 1.4 (key codes): https://usb.org/document-library/hid-usage-tables-14
- NimBLE GATT Server API: https://mynewt.apache.org/latest/network/ble_hs/ble_gatts.html
- Community NimBLE HID example: https://github.com/olegos76/nimble_kbdhid_example
- ESP-IDF bleprph walkthrough (security/bonding patterns): https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/bleprph/tutorial/bleprph_walkthrough.md
