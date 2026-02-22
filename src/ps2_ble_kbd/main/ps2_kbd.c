#include "ps2_kbd.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "ps2_kbd";

/*
 * PS/2 Set-2 scan code → USB HID usage ID tables.
 *
 * Ported from PS2Utils/ps2_keyboard.cpp; PROGMEM/pgm_read_byte_near removed.
 * Indices are PS/2 scan-code bytes; values are USB HID usage IDs (KC_xxx).
 * Unknown entries are KC_INVALID (0xFF).
 */

#define KC(x) (KC_##x)
#define KCI   KC(INVALID)

/* One-byte make codes */
static const uint8_t s_scancode[256] = {
    /* 0x */ KCI,    KC(F9),    KCI,  KC(F5),   KC(F3),      KC(F1),           KC(F2),  KC(F12),
    /* 0x */ KCI,   KC(F10),  KC(F8),  KC(F6),   KC(F4),     KC(TAB), /* BQ */ 0x35,     KCI,

    /* 1x */ KCI,  KC(LALT), KC(LSHFT), KCI,  KC(LCTRL),  /* Q */0x14, /* 1 */0x1E,     KCI,
    /* 1x */ KCI,     KCI,   /* Z */0x1D, /* S */0x16, /* A */0x04, /* W */0x1A, /* 2 */0x1F, KCI,

    /* 2x */ KCI,  /* C */0x06, /* X */0x1B, /* D */0x07, /* E */0x08, /* 4 */0x21, /* 3 */0x20, KCI,
    /* 2x */ KCI,  KC(SPACE), /* V */0x19, /* F */0x09, /* T */0x17, /* R */0x15, /* 5 */0x22, KCI,

    /* 3x */ KCI, /* N */0x11, /* B */0x05, /* H */0x0B, /* G */0x0A, /* Y */0x1C, /* 6 */0x23, KCI,
    /* 3x */ KCI,     KCI,   /* M */0x10, /* J */0x0D, /* U */0x18, /* 7 */0x24, /* 8 */0x25, KCI,

    /* 4x */ KCI, /* , */0x36, /* K */0x0E, /* I */0x0C, /* O */0x12, /* 0 */0x27, /* 9 */0x26, KCI,
    /* 4x */ KCI, /* . */0x37, /* / */0x38, /* L */0x0F, /* ; */0x33, /* P */0x13, /* - */0x2D, KCI,

    /* 5x */          KCI,               KCI,  /* ' */0x34,               KCI,
    /* 5x */ /* [ */0x2F,   /* = */0x2E,               KCI,               KCI,
    /* 5x */ KC(CAPS_LOCK), KC(RSHFT),  /* ENT */0x28, /* ] */0x30,
    /* 5x */               KCI,    /* \ */0x31,               KCI,               KCI,

    /* 6x */ KCI,      KCI,      KCI,          KCI,       KCI, KCI, /* BS */0x2A, KCI,
    /* 6x */ KCI, /* KP1 */0x59, KCI, /* KP4 */0x5C, /* KP7 */0x5F, KCI, KCI, KCI,

    /* 7x */  /* KP0 */0x62, /* KP. */0x63, /* KP2 */0x5A, /* KP5 */0x5D,
    /* 7x */  /* KP6 */0x5E, /* KP8 */0x60, KC(ESC), KC(KP_NUM_LOCK),
    /* 7x */  /* F11 */0x44, /* KP+ */0x57, /* KP3 */0x5B, /* KP- */0x56,
    /* 7x */  /* KP* */0x55, /* KP9 */0x61, KC(SCROLL_LOCK), KCI,

    /* 8x */ KCI, KCI, KCI, /* F7 */0x40, KCI, KCI, KCI, KCI,
    /* 8x */ KCI, KCI, KCI,          KCI, KCI, KCI, KCI, KCI,

    /* 9x–Fx */ KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
};

/* Extended (0xE0-prefixed) make codes */
static const uint8_t s_ext_scancode[256] = {
    /* 0x–0x */ KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
    /* 1x */ KCI, KC(RALT), KCI, KCI, KC(RCTRL), KCI, KCI,       KCI,
    /* 1x */ KCI,      KCI, KCI, KCI,        KCI, KCI, KCI, KC(LGUI),

    /* 2x */ KCI,      KCI,      KCI, KCI, KCI, KCI, KCI, KC(RGUI),
    /* 2x */ KCI,      KCI,      KCI, KCI, KCI, KCI, KCI, /* APPS */0x65,

    /* 3x */ KCI, KCI, KCI, KCI, KCI, KCI, KCI, /* POWER */0x66,
    /* 3x */ KCI, KCI, KCI, KCI, KCI, KCI, KCI,            KCI,

    /* 4x */ KCI, KCI,              KCI, KCI, KCI, KCI, KCI, KCI,
    /* 4x */ KCI, KCI, /* KP/ */0x54, KCI, KCI, KCI, KCI, KCI,

    /* 5x */ KCI, KCI,               KCI,           KCI, KCI, KCI, KCI, KCI,
    /* 5x */ KCI, KCI, /* KPENT */0x58,             KCI, KCI, KCI, KCI, KCI,

    /* 6x */ KCI,        KCI,           KCI,           KCI,           KCI, KCI, KCI, KCI,
    /* 6x */ KCI, /* END */0x4D, KCI, /* LEFT */0x50, /* HOME */0x4A, KCI, KCI, KCI,

    /* 7x */ /* INS */0x49, /* DEL */0x4C, /* DOWN */0x51, KCI, /* RIGHT */0x4F, /* UP */0x52, KCI, KCI,
    /* 7x */           KCI,           KCI, /* PGDN */0x4E,   KCI, /* PRTSC */0x46, /* PGUP */0x4B, KCI, KCI,

    /* 8x–Fx */ KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
                KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,  KCI, KCI, KCI, KCI, KCI, KCI, KCI, KCI,
};

#undef KC
#undef KCI

/* ---------- Internal byte processor ---------- */

static void push_event(ps2_kbd_t *kbd, uint8_t keycode, ps2_key_event_type_t type)
{
    if (keycode == KC_INVALID) return;
    ps2_key_event_t ev = { .keycode = keycode, .type = type };
    if (xQueueSend(kbd->event_queue, &ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropping keycode 0x%02X", keycode);
    }
}

static void process_byte(ps2_kbd_t *kbd, uint8_t b)
{
    bool is_break    = (b == 0xF0);
    bool is_extended = (b == 0xE0);
    uint8_t kc;

    switch (kbd->state) {

    case PS2K_WAIT_START:
        if (is_break) {
            kbd->state = PS2K_WAIT_BREAK;
        } else if (is_extended) {
            kbd->state = PS2K_WAIT_EXTENDED;
        } else if (b == 0xE1) {
            /* Pause/Break: full sequence E1 14 77 E1 F0 14 F0 77 */
            kbd->state = PS2K_WAIT_FIRST_77;
        } else {
            kc = s_scancode[b];
            push_event(kbd, kc, PS2_KEY_PRESSED);
        }
        break;

    case PS2K_WAIT_EXTENDED:
        if (is_extended) {
            ESP_LOGW(TAG, "E0 while in EXT state");
            kbd->state = PS2K_WAIT_START;
        } else if (is_break) {
            kbd->state = PS2K_WAIT_EXTENDED_BREAK;
        } else {
            kc = s_ext_scancode[b];
            push_event(kbd, kc, PS2_KEY_PRESSED);
            kbd->state = PS2K_WAIT_START;
        }
        break;

    case PS2K_WAIT_BREAK:
        if (is_break || is_extended) {
            ESP_LOGW(TAG, "BRK/EXT while in BREAK state");
            kbd->state = PS2K_WAIT_START;
        } else {
            kc = s_scancode[b];
            push_event(kbd, kc, PS2_KEY_RELEASED);
            kbd->state = PS2K_WAIT_START;
        }
        break;

    case PS2K_WAIT_EXTENDED_BREAK:
        if (is_break || is_extended) {
            ESP_LOGW(TAG, "BRK/EXT while in EXT_BREAK state");
            kbd->state = PS2K_WAIT_START;
        } else {
            kc = s_ext_scancode[b];
            push_event(kbd, kc, PS2_KEY_RELEASED);
            kbd->state = PS2K_WAIT_START;
        }
        break;

    case PS2K_WAIT_FIRST_77:
    case PS2K_WAIT_SECOND_77:
        switch (b) {
        case 0x77:
            if (kbd->state == PS2K_WAIT_FIRST_77) {
                kbd->state = PS2K_WAIT_SECOND_77;
            } else {
                push_event(kbd, KC_PAUSE, PS2_KEY_PRESSED);
                kbd->state = PS2K_WAIT_START;
            }
            break;
        case 0x14: case 0xE1: case 0xF0:
            /* Expected intermediate bytes — ignore */
            break;
        default:
            ESP_LOGW(TAG, "unexpected byte 0x%02X in Pause sequence", b);
            kbd->state = PS2K_WAIT_START;
            break;
        }
        break;
    }
}

/* ---------- Public API ---------- */

void ps2_kbd_init(ps2_kbd_t *kbd)
{
    kbd->state = PS2K_WAIT_START;
    kbd->event_queue = xQueueCreate(16, sizeof(ps2_key_event_t));
}

void ps2_kbd_process(ps2_kbd_t *kbd, ps2_proto_t *proto)
{
    uint8_t b;
    while (xQueueReceive(proto->rx_queue, &b, 0) == pdTRUE) {
        process_byte(kbd, b);
    }
}

bool ps2_kbd_read_event(ps2_kbd_t *kbd, ps2_key_event_t *out)
{
    return xQueueReceive(kbd->event_queue, out, 0) == pdTRUE;
}

void ps2_kbd_deinit(ps2_kbd_t *kbd)
{
    if (kbd->event_queue) {
        vQueueDelete(kbd->event_queue);
        kbd->event_queue = NULL;
    }
    kbd->state = PS2K_WAIT_START;
}
