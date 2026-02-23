#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"

#include "hid_dev.h"

static const char *TAG = "hid_dev";

/*
 * g_led_queue is owned by main.c (declared there, extern here).
 * The output-report access callback posts the LED byte to this queue so
 * ps2_task can pick it up without blocking the BLE stack.
 */
extern QueueHandle_t g_led_queue;
extern TaskHandle_t  g_ps2_task;

/* ---------- Static characteristic data ---------- */

/*
 * HID keyboard report descriptor.
 * Input:  8 bytes — modifier(1) + reserved(1) + keycodes(6)
 * Output: 1 byte  — LED state (Num/Caps/Scroll/Compose/Kana) + 3-bit pad
 */
static const uint8_t s_report_map[] = {
    0x05, 0x01,   /* Usage Page (Generic Desktop)      */
    0x09, 0x06,   /* Usage (Keyboard)                  */
    0xA1, 0x01,   /* Collection (Application)          */

    0x05, 0x07,   /*   Usage Page (Keyboard/Keypad)    */
    0x19, 0xE0,   /*   Usage Minimum (Left Control)    */
    0x29, 0xE7,   /*   Usage Maximum (Right GUI)       */
    0x15, 0x00,   /*   Logical Minimum (0)             */
    0x25, 0x01,   /*   Logical Maximum (1)             */
    0x75, 0x01,   /*   Report Size (1 bit)             */
    0x95, 0x08,   /*   Report Count (8)                */
    0x81, 0x02,   /*   Input (Data, Variable, Absolute)*/

    0x95, 0x01,   /*   Report Count (1)                */
    0x75, 0x08,   /*   Report Size (8 bits)            */
    0x81, 0x01,   /*   Input (Constant)                */

    0x95, 0x05,   /*   Report Count (5)                */
    0x75, 0x01,   /*   Report Size (1 bit)             */
    0x05, 0x08,   /*   Usage Page (LEDs)               */
    0x19, 0x01,   /*   Usage Minimum (Num Lock)        */
    0x29, 0x05,   /*   Usage Maximum (Kana)            */
    0x91, 0x02,   /*   Output (Data, Variable, Absolute)*/

    0x95, 0x01,   /*   Report Count (1)                */
    0x75, 0x03,   /*   Report Size (3 bits)            */
    0x91, 0x01,   /*   Output (Constant)               */

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

static const uint8_t s_hid_info[]   = { 0x11, 0x01, 0x00, 0x02 };
static const uint8_t s_rrd_input[]  = { 0x00, 0x01 };
static const uint8_t s_rrd_output[] = { 0x00, 0x02 };
static uint8_t       s_proto_mode   = 0x01;
static uint8_t       s_battery_level = 100;
static const uint8_t s_pnp_id[]     = { 0x02, 0x3A, 0x30, 0x4B, 0x4F, 0x01, 0x00 };
static const char    s_manufacturer[] = "Espressif";

static hid_keyboard_report_t s_last_report;

/* ---------- GATT attribute handles ---------- */

static uint16_t s_input_report_handle;

/* ---------- UUID declarations ---------- */

static const ble_uuid16_t UUID_DIS      = BLE_UUID16_INIT(0x180A);
static const ble_uuid16_t UUID_BAS      = BLE_UUID16_INIT(0x180F);
static const ble_uuid16_t UUID_HID      = BLE_UUID16_INIT(0x1812);
static const ble_uuid16_t UUID_MANUF    = BLE_UUID16_INIT(0x2A29);
static const ble_uuid16_t UUID_PNP_ID   = BLE_UUID16_INIT(0x2A50);
static const ble_uuid16_t UUID_BATT_LVL = BLE_UUID16_INIT(0x2A19);
static const ble_uuid16_t UUID_HID_INFO = BLE_UUID16_INIT(0x2A4A);
static const ble_uuid16_t UUID_RPT_MAP  = BLE_UUID16_INIT(0x2A4B);
static const ble_uuid16_t UUID_HID_CTRL = BLE_UUID16_INIT(0x2A4C);
static const ble_uuid16_t UUID_REPORT   = BLE_UUID16_INIT(0x2A4D);
static const ble_uuid16_t UUID_PROTO_MD = BLE_UUID16_INIT(0x2A4E);
static const ble_uuid16_t UUID_RPT_REF  = BLE_UUID16_INIT(0x2908);

/* ---------- Helper: static read-only data ---------- */

typedef struct { const void *data; uint16_t len; } rodata_t;

static const rodata_t RD_MANUF    = { s_manufacturer, sizeof(s_manufacturer) - 1 };
static const rodata_t RD_PNP_ID   = { s_pnp_id,       sizeof(s_pnp_id) };
static const rodata_t RD_HID_INFO = { s_hid_info,     sizeof(s_hid_info) };
static const rodata_t RD_RPT_MAP  = { s_report_map,   sizeof(s_report_map) };
static const rodata_t RD_RRD_IN   = { s_rrd_input,    sizeof(s_rrd_input) };
static const rodata_t RD_RRD_OUT  = { s_rrd_output,   sizeof(s_rrd_output) };

static int ro_access(uint16_t conn_handle, uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const rodata_t *rd = (const rodata_t *)arg;
    int rc = os_mbuf_append(ctxt->om, rd->data, rd->len);
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int bas_access(uint16_t conn_handle, uint16_t attr_handle,
                      struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc = os_mbuf_append(ctxt->om, &s_battery_level, 1);
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int hid_ctrl_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    uint8_t cmd = 0;
    os_mbuf_copydata(ctxt->om, 0, 1, &cmd);
    ESP_LOGD(TAG, "HID Control Point: %s", cmd == 0 ? "Suspend" : "Exit Suspend");
    return 0;
}

static int proto_mode_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: {
        int rc = os_mbuf_append(ctxt->om, &s_proto_mode, 1);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        os_mbuf_copydata(ctxt->om, 0, 1, &s_proto_mode);
        ESP_LOGD(TAG, "Protocol Mode: %s", s_proto_mode == 0 ? "Boot" : "Report");
        return 0;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static int input_rpt_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;
    int rc = os_mbuf_append(ctxt->om, &s_last_report, sizeof(s_last_report));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static uint8_t s_led_state = 0;

static int output_rpt_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: {
        int rc = os_mbuf_append(ctxt->om, &s_led_state, 1);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        os_mbuf_copydata(ctxt->om, 0, 1, &s_led_state);
        ESP_LOGI(TAG, "LED state: NumLk=%d CapsLk=%d ScrollLk=%d",
                 (s_led_state >> 0) & 1, (s_led_state >> 1) & 1, (s_led_state >> 2) & 1);
        /* Forward to ps2_task; non-blocking (ps2_task drains this queue) */
        xQueueSend(g_led_queue, &s_led_state, 0);
        if (g_ps2_task)
            xTaskNotifyGive(g_ps2_task);
        return 0;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* ---------- GATT service table ---------- */

static const struct ble_gatt_svc_def s_gatt_svcs[] = {

    /* Device Information Service (0x180A) */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_DIS.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid = &UUID_MANUF.u,  .access_cb = ro_access, .arg = (void *)&RD_MANUF,  .flags = BLE_GATT_CHR_F_READ },
            { .uuid = &UUID_PNP_ID.u, .access_cb = ro_access, .arg = (void *)&RD_PNP_ID, .flags = BLE_GATT_CHR_F_READ },
            { 0 }
        },
    },

    /* Battery Service (0x180F) */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_BAS.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid = &UUID_BATT_LVL.u, .access_cb = bas_access, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY },
            { 0 }
        },
    },

    /* HID Service (0x1812) */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_HID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid      = &UUID_HID_INFO.u,
                .access_cb = ro_access,
                .arg       = (void *)&RD_HID_INFO,
                .flags     = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid      = &UUID_RPT_MAP.u,
                .access_cb = ro_access,
                .arg       = (void *)&RD_RPT_MAP,
                .flags     = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid      = &UUID_HID_CTRL.u,
                .access_cb = hid_ctrl_access,
                .flags     = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid      = &UUID_PROTO_MD.u,
                .access_cb = proto_mode_access,
                .flags     = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                /* Input Report (keyboard → host); encryption mandatory per HOGP */
                .uuid       = &UUID_REPORT.u,
                .access_cb  = input_rpt_access,
                .val_handle = &s_input_report_handle,
                .flags      = BLE_GATT_CHR_F_READ |
                              BLE_GATT_CHR_F_NOTIFY |
                              BLE_GATT_CHR_F_READ_ENC,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    { .uuid = &UUID_RPT_REF.u, .att_flags = BLE_ATT_F_READ, .access_cb = ro_access, .arg = (void *)&RD_RRD_IN },
                    { 0 }
                },
            },
            {
                /* Output Report (host → keyboard: LED state) */
                .uuid      = &UUID_REPORT.u,
                .access_cb = output_rpt_access,
                .flags     = BLE_GATT_CHR_F_READ |
                             BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_WRITE_NO_RSP |
                             BLE_GATT_CHR_F_READ_ENC |
                             BLE_GATT_CHR_F_WRITE_ENC,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    { .uuid = &UUID_RPT_REF.u, .att_flags = BLE_ATT_F_READ, .access_cb = ro_access, .arg = (void *)&RD_RRD_OUT },
                    { 0 }
                },
            },
            { 0 }
        },
    },

    { 0 }
};

/* ---------- Public API ---------- */

int hid_dev_init(void)
{
    int rc;
    rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "count_cfg: %d", rc); return rc; }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "add_svcs: %d", rc); return rc; }

    return 0;
}

int hid_keyboard_send(uint16_t conn_handle, const hid_keyboard_report_t *report)
{
    memcpy(&s_last_report, report, sizeof(*report));
    struct os_mbuf *om = ble_hs_mbuf_from_flat(report, sizeof(*report));
    if (!om) return BLE_HS_ENOMEM;
    return ble_gattc_notify_custom(conn_handle, s_input_report_handle, om);
}
