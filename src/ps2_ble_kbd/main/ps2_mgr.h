#pragma once

/*
 * ps2_mgr — PS/2 keyboard state manager.
 *
 * Tracks which keys are currently pressed via a 256-bit bitmask (one bit per
 * USB HID usage ID).  On each call to ps2_mgr_read() it drains the keyboard
 * event queue, updates the bitmask, builds a hid_keyboard_report_t, and
 * returns true if the report changed since the last call.
 *
 * LED handling:
 *   - On lock-key release, the local LED state is toggled and the led_cb
 *     is called to send the PS/2 0xED command to the keyboard.
 *   - ps2_mgr_apply_ble_leds() converts a BLE HID LED byte (Num/Caps/Scroll
 *     in bits 0/1/2) to the PS/2 bit ordering (Scroll/Num/Caps in bits 0/1/2)
 *     and calls led_cb.
 */

#include <stdbool.h>
#include <stdint.h>

#include "hid_dev.h"
#include "ps2_kbd.h"
#include "ps2_proto.h"

/* PS/2 LED bit positions (sent with command 0xED) */
#define PS2_LED_SCROLL_LOCK  (1 << 0)
#define PS2_LED_NUM_LOCK     (1 << 1)
#define PS2_LED_CAPS_LOCK    (1 << 2)

/*
 * Callback invoked (in task context) when the keyboard LED state changes.
 * Implementations should call ps2_proto_write_and_wait(proto, 0xED) followed
 * by ps2_proto_write_and_wait(proto, ps2_leds).
 */
typedef void (*ps2_led_cb_t)(ps2_proto_t *proto, uint8_t ps2_leds);

#define PS2_MGR_MASK_BYTES  32  /* 256 bits, one per keycode 0x00–0xFF */

typedef struct {
    uint8_t               pressed[PS2_MGR_MASK_BYTES];
    uint8_t               leds;          /* current PS/2 LED state */
    ps2_led_cb_t          led_cb;
    ps2_proto_t          *proto;
    hid_keyboard_report_t last_report;   /* for change detection */
} ps2_mgr_t;

/*
 * Initialise the manager.  proto is used for LED writes; led_cb is called
 * whenever the LED state changes (may be NULL to skip LED updates).
 */
void ps2_mgr_init(ps2_mgr_t *mgr, ps2_proto_t *proto, ps2_led_cb_t led_cb);

/*
 * Drain pending events from kbd, update the pressed bitmask, and build a
 * HID report.  Returns true (and fills *out) when the report changed.
 * Must be called from task context.
 */
bool ps2_mgr_read(ps2_mgr_t *mgr, ps2_kbd_t *kbd, hid_keyboard_report_t *out);

/*
 * Apply a BLE HID LED output-report byte to the keyboard LEDs.
 * Converts BLE bit ordering → PS/2 bit ordering and calls led_cb.
 */
void ps2_mgr_apply_ble_leds(ps2_mgr_t *mgr, uint8_t ble_leds);
