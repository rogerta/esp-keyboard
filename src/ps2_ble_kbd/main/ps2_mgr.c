#include "ps2_mgr.h"

#include <string.h>

#include "keycodes.h"

/* Modifier keycode → HID modifier byte bit mapping */
static const struct {
    uint8_t keycode;
    uint8_t mod_bit;
} s_mod_map[] = {
    { KC_LCTRL,  MOD_LCTRL  },
    { KC_LSHFT,  MOD_LSHIFT },
    { KC_LALT,   MOD_LALT   },
    { KC_LGUI,   MOD_LGUI   },
    { KC_RCTRL,  MOD_RCTRL  },
    { KC_RSHFT,  MOD_RSHIFT },
    { KC_RALT,   MOD_RALT   },
    { KC_RGUI,   MOD_RGUI   },
};

static inline bool is_pressed(const ps2_mgr_t *mgr, uint8_t kc)
{
    return (mgr->pressed[kc / 8] & (1u << (kc % 8))) != 0;
}

static inline void set_pressed(ps2_mgr_t *mgr, uint8_t kc, bool val)
{
    if (val)
        mgr->pressed[kc / 8] |=  (1u << (kc % 8));
    else
        mgr->pressed[kc / 8] &= ~(1u << (kc % 8));
}

static void call_led_cb(ps2_mgr_t *mgr)
{
    if (mgr->led_cb)
        mgr->led_cb(mgr->proto, mgr->leds);
}

/* ---------- Public API ---------- */

void ps2_mgr_init(ps2_mgr_t *mgr, ps2_proto_t *proto, ps2_led_cb_t led_cb)
{
    memset(mgr, 0, sizeof(*mgr));
    mgr->proto  = proto;
    mgr->led_cb = led_cb;
}

bool ps2_mgr_read(ps2_mgr_t *mgr, ps2_kbd_t *kbd, hid_keyboard_report_t *out)
{
    /* Drain all pending key events */
    bool had_events = false;
    ps2_key_event_t ev;
    while (ps2_kbd_read_event(kbd, &ev)) {
        had_events = true;
        set_pressed(mgr, ev.keycode, ev.type == PS2_KEY_PRESSED);

        /* On lock-key release, toggle the local LED state */
        if (ev.type == PS2_KEY_RELEASED) {
            uint8_t mask = 0;
            switch (ev.keycode) {
            case KC_SCROLL_LOCK:  mask = PS2_LED_SCROLL_LOCK; break;
            case KC_KP_NUM_LOCK:  mask = PS2_LED_NUM_LOCK;    break;
            case KC_CAPS_LOCK:    mask = PS2_LED_CAPS_LOCK;   break;
            default: break;
            }
            if (mask) {
                mgr->leds ^= mask;
                call_led_cb(mgr);
            }
        }
    }

    if (!had_events) return false;

    /* Build HID report from current pressed state */
    hid_keyboard_report_t report = {0};

    /* Modifier byte */
    for (size_t i = 0; i < sizeof(s_mod_map) / sizeof(s_mod_map[0]); i++) {
        if (is_pressed(mgr, s_mod_map[i].keycode))
            report.modifier |= s_mod_map[i].mod_bit;
    }

    /* Non-modifier keycodes: 6-key rollover */
    int pos = 0;
    bool overflow = false;
    for (uint8_t kc = KC_FIRST_NON_MODIFIER; kc < KC_COUNT_NON_MODIFIER; kc++) {
        if (!is_pressed(mgr, kc)) continue;
        if (pos == 6) {
            overflow = true;
            break;
        }
        report.keycode[pos++] = kc;

        /* Pause/Break has no break code; auto-release after one report */
        if (kc == KC_PAUSE)
            set_pressed(mgr, kc, false);
    }
    if (overflow)
        memset(report.keycode, KC_ERROR_ROLL_OVER, sizeof(report.keycode));

    /* Only emit the report if it actually changed */
    if (memcmp(&report, &mgr->last_report, sizeof(report)) == 0)
        return false;

    mgr->last_report = report;
    *out = report;
    return true;
}

void ps2_mgr_apply_ble_leds(ps2_mgr_t *mgr, uint8_t ble_leds)
{
    /*
     * BLE HID LED byte:   bit0=NumLock bit1=CapsLock bit2=ScrollLock
     * PS/2 LED byte:      bit0=ScrollLock bit1=NumLock bit2=CapsLock
     */
    uint8_t ps2 = 0;
    if (ble_leds & HID_LED_NUM_LOCK)    ps2 |= PS2_LED_NUM_LOCK;
    if (ble_leds & HID_LED_CAPS_LOCK)   ps2 |= PS2_LED_CAPS_LOCK;
    if (ble_leds & HID_LED_SCROLL_LOCK) ps2 |= PS2_LED_SCROLL_LOCK;

    mgr->leds = ps2;
    call_led_cb(mgr);
}
