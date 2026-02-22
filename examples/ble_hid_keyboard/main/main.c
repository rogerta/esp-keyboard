#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"

#include "hid_dev.h"
#include "keycodes.h"

static const char *TAG = "ble_hid";

static int gap_event_cb(struct ble_gap_event *event, void *arg); /* forward decl */

/* GAP appearance value for a keyboard (Bluetooth SIG Assigned Numbers) */
#define GAP_APPEARANCE_KEYBOARD  0x03C1

/* Connection state — written from GAP event handler, read from demo task */
static volatile uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static volatile bool     g_encrypted   = false;

/* ---------- Advertising ---------- */

static void start_advertising(void)
{
    static const ble_uuid16_t hid_svc_uuid = BLE_UUID16_INIT(0x1812);

    struct ble_hs_adv_fields fields    = {0};
    struct ble_hs_adv_fields scan_rsp  = {0};
    struct ble_gap_adv_params params   = {0};
    int rc;

    /* Primary advertisement: flags, appearance, HID service UUID */
    fields.flags                = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance           = GAP_APPEARANCE_KEYBOARD;
    fields.appearance_is_present = 1;
    fields.tx_pwr_lvl           = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.tx_pwr_lvl_is_present = 1;
    fields.uuids16              = &hid_svc_uuid;
    fields.num_uuids16          = 1;
    fields.uuids16_is_complete  = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed: %d", rc);
        return;
    }

    /* Scan response: full device name (may not fit in primary packet) */
    const char *name = ble_svc_gap_device_name();
    scan_rsp.name         = (uint8_t *)name;
    scan_rsp.name_len     = strlen(name);
    scan_rsp.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&scan_rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_rsp_set_fields failed: %d", rc);
        return;
    }

    params.conn_mode = BLE_GAP_CONN_MODE_UND;   /* undirected connectable */
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min  = BLE_GAP_ADV_ITVL_MS(40);
    params.itvl_max  = BLE_GAP_ADV_ITVL_MS(60);

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &params, gap_event_cb, NULL);
    ESP_LOGI(TAG, "advertising started (rc=%d)", rc);
}

/* ---------- GAP event handler ---------- */

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn_handle = event->connect.conn_handle;
            g_encrypted   = false;
            ESP_LOGI(TAG, "connected, handle=%d", g_conn_handle);
        } else {
            ESP_LOGI(TAG, "connection failed, status=%d", event->connect.status);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected, reason=%d", event->disconnect.reason);
        g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        g_encrypted   = false;
        start_advertising();
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /*
         * Fired after pairing completes (first connection) and after
         * re-encryption with stored LTK (subsequent connections).
         * Reports are safe to send only after this event.
         */
        if (event->enc_change.status == 0) {
            g_encrypted = true;
            ESP_LOGI(TAG, "encryption enabled");
        } else {
            ESP_LOGW(TAG, "encryption failed, status=%d", event->enc_change.status);
        }
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe: attr_handle=%d notify=%d indicate=%d",
                 event->subscribe.attr_handle,
                 event->subscribe.cur_notify,
                 event->subscribe.cur_indicate);
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGD(TAG, "connection parameters updated");
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /*
         * The peer is trying to pair but we already have a bond record.
         * This happens when the host loses its bond (e.g. OS reinstall).
         * Delete our stale record so pairing can proceed cleanly.
         */
        ESP_LOGW(TAG, "repeat pairing — deleting stale bond");
        ble_store_util_delete_peer_records(
            &event->repeat_pairing.conn_desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        /* Only reached if sm_io_cap != BLE_SM_IO_CAP_NO_IO */
        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            struct ble_sm_io pkey = { .action = BLE_SM_IOACT_DISP, .passkey = 123456 };
            ESP_LOGI(TAG, "passkey: %06" PRIu32, pkey.passkey);
            ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        }
        break;

    default:
        break;
    }
    return 0;
}

/* ---------- NimBLE host callbacks ---------- */

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE host reset, reason=%d", reason);
    g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    g_encrypted   = false;
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    start_advertising();
}

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ---------- Demo task: type "hello world" when connected ---------- */

/*
 * Each entry: {modifier_byte, keycode}
 * Modifier = 0 for lowercase; MOD_LSHIFT for uppercase or symbols.
 */
static const struct { uint8_t mod; uint8_t key; } s_demo_keys[] = {
    { 0,          KEY_H },
    { 0,          KEY_E },
    { 0,          KEY_L },
    { 0,          KEY_L },
    { 0,          KEY_O },
    { 0,          KEY_SPACE },
    { 0,          KEY_W },
    { 0,          KEY_O },
    { 0,          KEY_R },
    { 0,          KEY_L },
    { 0,          KEY_D },
    { 0,          KEY_ENTER },
};

static void demo_task(void *param)
{
    const size_t n = sizeof(s_demo_keys) / sizeof(s_demo_keys[0]);

    while (1) {
        /* Wait until connected and encrypted */
        while (g_conn_handle == BLE_HS_CONN_HANDLE_NONE || !g_encrypted) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        ESP_LOGI(TAG, "sending 'hello world'");

        hid_keyboard_report_t report = {0};

        for (size_t i = 0; i < n; i++) {
            /* Key press */
            report.modifier  = s_demo_keys[i].mod;
            report.keycode[0] = s_demo_keys[i].key;
            hid_keyboard_send(g_conn_handle, &report);
            vTaskDelay(pdMS_TO_TICKS(15));

            /* Key release */
            memset(&report, 0, sizeof(report));
            hid_keyboard_send(g_conn_handle, &report);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        /* Repeat every 30 seconds */
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* ---------- app_main ---------- */

void app_main(void)
{
    /* NVS is required for BLE bonding key storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(nimble_port_init());

    /* NimBLE host global callbacks */
    ble_hs_cfg.reset_cb         = on_reset;
    ble_hs_cfg.sync_cb          = on_sync;
    ble_hs_cfg.store_status_cb  = ble_store_util_status_rr;
    ble_hs_cfg.gatts_register_cb = NULL;

    /*
     * Security / pairing configuration.
     * NO_IO = "Just Works" — no passkey, no MITM protection.
     * For MITM protection, use BLE_SM_IO_CAP_DISP_ONLY and set sm_mitm=1.
     */
    ble_hs_cfg.sm_io_cap        = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding       = 1;
    ble_hs_cfg.sm_mitm          = 0;
    ble_hs_cfg.sm_sc            = 1;   /* LE Secure Connections (BT 4.2+) */
    ble_hs_cfg.sm_our_key_dist  = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    /* Register standard GAP and GATT services, then HID services */
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(hid_dev_init());

    /* Bonding key persistence (NVS) */
    ble_store_config_init();

    xTaskCreate(demo_task, "hid_demo", 4096, NULL, 5, NULL);

    nimble_port_freertos_init(nimble_host_task);
}
