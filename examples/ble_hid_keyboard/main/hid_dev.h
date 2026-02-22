#pragma once

#include <stdint.h>
#include "host/ble_hs.h"

/*
 * 8-byte keyboard input report (no Report ID prefix).
 *
 * Byte 0:   modifier  — bitmask of MOD_* flags (see keycodes.h)
 * Byte 1:   reserved  — always 0
 * Bytes 2-7: keycode  — up to 6 simultaneous non-modifier key codes
 */
typedef struct {
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycode[6];
} __attribute__((packed)) hid_keyboard_report_t;

/* Bitmasks for the 1-byte LED output report sent by the host */
#define HID_LED_NUM_LOCK    (1 << 0)
#define HID_LED_CAPS_LOCK   (1 << 1)
#define HID_LED_SCROLL_LOCK (1 << 2)
#define HID_LED_COMPOSE     (1 << 3)
#define HID_LED_KANA        (1 << 4)

/*
 * Register the DIS/BAS/HID GATT services with the NimBLE stack.
 * Call before nimble_port_freertos_init().
 */
int hid_dev_init(void);

/*
 * Send a keyboard input report via BLE NOTIFY.
 * conn_handle: obtained from BLE_GAP_EVENT_CONNECT.
 * Returns 0 on success, BLE_HS_* error otherwise.
 * Caller should send a zero report immediately after to release keys.
 */
int hid_keyboard_send(uint16_t conn_handle, const hid_keyboard_report_t *report);
