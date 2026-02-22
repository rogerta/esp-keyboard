#pragma once

/*
 * ps2_kbd — PS/2 scan-code decoder (Set 2).
 *
 * Reads raw bytes from ps2_proto's rx_queue and decodes them into
 * (keycode, press/release) events placed on an internal event queue.
 *
 * Call ps2_kbd_process() from task context to drain the proto byte queue
 * and refill the event queue.  Then call ps2_kbd_read_event() to consume
 * decoded events one at a time.
 *
 * Keycode values are USB HID keyboard usage IDs (0x00–0xFF), matching the
 * PS2Keyboard::KeyCode enum from the original Arduino library.  Modifier
 * keys are in the range 0xE0–0xE7.
 */

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ps2_proto.h"

/*
 * USB HID keyboard usage IDs (Usage Page 0x07).
 * Values match the PS2Keyboard::KeyCode enum from the original Arduino library,
 * which was deliberately aligned with USB HID so keycodes could be used
 * directly in HID reports.
 */
#define KC_NO_EVENT          0x00
#define KC_ERROR_ROLL_OVER   0x01
#define KC_ESC               0x29
#define KC_TAB               0x2B
#define KC_SPACE             0x2C
#define KC_CAPS_LOCK         0x39
#define KC_F1                0x3A
#define KC_F2                0x3B
#define KC_F3                0x3C
#define KC_F4                0x3D
#define KC_F5                0x3E
#define KC_F6                0x3F
#define KC_F7                0x40
#define KC_F8                0x41
#define KC_F9                0x42
#define KC_F10               0x43
#define KC_F11               0x44
#define KC_F12               0x45
#define KC_SCROLL_LOCK       0x47
#define KC_PAUSE             0x48
#define KC_KP_NUM_LOCK       0x53
/* Modifier keycodes — must be in 0xE0–0xE7 range */
#define KC_LCTRL             0xE0
#define KC_LSHFT             0xE1
#define KC_LALT              0xE2
#define KC_LGUI              0xE3
#define KC_RCTRL             0xE4
#define KC_RSHFT             0xE5
#define KC_RALT              0xE6
#define KC_RGUI              0xE7
#define KC_INVALID           0xFF
/* Bounds for the non-modifier keycode range iterated in ps2_mgr */
#define KC_FIRST_NON_MODIFIER 0x00u
#define KC_COUNT_NON_MODIFIER 0xA5u  /* exclusive upper bound */

typedef enum {
    PS2_KEY_PRESSED,
    PS2_KEY_RELEASED,
} ps2_key_event_type_t;

typedef struct {
    uint8_t               keycode;
    ps2_key_event_type_t  type;
} ps2_key_event_t;

typedef enum {
    PS2K_WAIT_START,
    PS2K_WAIT_EXTENDED,
    PS2K_WAIT_BREAK,
    PS2K_WAIT_EXTENDED_BREAK,
    PS2K_WAIT_FIRST_77,   /* Pause/Break: waiting for first 0x77  */
    PS2K_WAIT_SECOND_77,  /* Pause/Break: waiting for second 0x77 */
} ps2_kbd_state_t;

typedef struct {
    QueueHandle_t    event_queue;  /* ps2_key_event_t, depth 16 */
    ps2_kbd_state_t  state;
} ps2_kbd_t;

/* Allocate the event queue and reset the state machine. */
void ps2_kbd_init(ps2_kbd_t *kbd);

/*
 * Drain all bytes currently in proto->rx_queue and decode them into events
 * on kbd->event_queue.  Call from task context.
 */
void ps2_kbd_process(ps2_kbd_t *kbd, ps2_proto_t *proto);

/*
 * Pop one event from the event queue.
 * Returns true and fills *out if an event was available; false otherwise.
 */
bool ps2_kbd_read_event(ps2_kbd_t *kbd, ps2_key_event_t *out);

void ps2_kbd_deinit(ps2_kbd_t *kbd);
