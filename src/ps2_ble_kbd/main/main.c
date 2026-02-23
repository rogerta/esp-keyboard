#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"

#include "esp_pm.h"
#include "esp_sleep.h"
#include "sdkconfig.h"

#include "hid_dev.h"
#include "keycodes.h"
#include "ps2_proto.h"
#include "ps2_kbd.h"
#include "ps2_mgr.h"

static const char *TAG = "ps2_ble";

/* ---------- Inter-task queues ---------- */

/* HID reports from ps2_task to ble_task */
static QueueHandle_t g_report_queue;

/* LED bytes from BLE GATT callback (hid_dev.c) to ps2_task */
QueueHandle_t g_led_queue;   /* extern'd by hid_dev.c */

/* ps2_task handle — notified by ISR and BLE output-report callback */
TaskHandle_t g_ps2_task;     /* extern'd by hid_dev.c */

/* ---------- BLE connection state ---------- */

#define GAP_APPEARANCE_KEYBOARD  0x03C1

static volatile uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static volatile bool     g_encrypted   = false;

/* ---------- LED callback (called from ps2_task context) ---------- */

static void led_cb(ps2_proto_t *proto, uint8_t ps2_leds)
{
    ps2_proto_write_and_wait(proto, 0xED);    /* Set LEDs command */
    ps2_proto_write_and_wait(proto, ps2_leds);
}

/* ---------- Deep sleep ---------- */

static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "keyboard idle for %d s, entering deep sleep",
             CONFIG_DEEP_SLEEP_TIMEOUT_SEC);

    /* Release all keys so the host doesn't see stuck keys */
    if (g_conn_handle != BLE_HS_CONN_HANDLE_NONE && g_encrypted) {
        hid_keyboard_report_t empty = {0};
        hid_keyboard_send(g_conn_handle, &empty);
    }

    /* Disconnect BLE; brief delay lets the packet reach the host */
    if (g_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(g_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        ble_gap_adv_stop();
    }

    /* PS/2 CLK is idle-high; a keypress pulls it LOW → wake trigger */
    esp_deep_sleep_enable_gpio_wakeup(
        1ULL << CONFIG_PS2_CLK_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW);

    esp_deep_sleep_start();
    /* Not reached — chip resets on wakeup */
}

/* ---------- PS/2 task ---------- */

static void ps2_task(void *arg)
{
    ps2_proto_t proto;
    ps2_kbd_t   kbd;
    ps2_mgr_t   mgr;

    esp_err_t err = ps2_proto_init(&proto,
                                   CONFIG_PS2_CLK_GPIO,
                                   CONFIG_PS2_DATA_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ps2_proto_init failed: %d", err);
        vTaskDelete(NULL);
        return;
    }

    ps2_kbd_init(&kbd);
    ps2_mgr_init(&mgr, &proto, led_cb);

    /* Let ISR wake this task on each received PS/2 byte */
    proto.task_to_notify = xTaskGetCurrentTaskHandle();

    ESP_LOGI(TAG, "PS/2 task started (clk=%d data=%d)",
             CONFIG_PS2_CLK_GPIO, CONFIG_PS2_DATA_GPIO);

    TickType_t last_activity = xTaskGetTickCount();
    const TickType_t sleep_timeout =
        pdMS_TO_TICKS((uint32_t)CONFIG_DEEP_SLEEP_TIMEOUT_SEC * 1000);

    while (1) {
        /* Wake on PS/2 byte, BLE LED callback, or periodic idle check */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        /* Decode any bytes received from the keyboard */
        ps2_kbd_process(&kbd, &proto);

        /* Build HID report; post if changed */
        hid_keyboard_report_t report;
        if (ps2_mgr_read(&mgr, &kbd, &report)) {
            last_activity = xTaskGetTickCount();
            xQueueSend(g_report_queue, &report, 0);
        }

        /* Apply any LED updates sent by the BLE host */
        uint8_t led_byte;
        if (xQueueReceive(g_led_queue, &led_byte, 0) == pdTRUE) {
            ps2_mgr_apply_ble_leds(&mgr, led_byte);
        }

        /* Enter deep sleep after prolonged inactivity */
        if ((xTaskGetTickCount() - last_activity) >= sleep_timeout) {
            enter_deep_sleep();
        }
    }
}

/* ---------- BLE task ---------- */

static void ble_task(void *arg)
{
    while (1) {
        hid_keyboard_report_t report;
        if (xQueueReceive(g_report_queue, &report, portMAX_DELAY) != pdTRUE)
            continue;

        if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE || !g_encrypted) {
            /* Not connected or not yet encrypted; discard report */
            continue;
        }

        hid_keyboard_send(g_conn_handle, &report);
    }
}

/* ---------- Advertising ---------- */

static int gap_event_cb(struct ble_gap_event *event, void *arg);

static void start_advertising(void)
{
    static const ble_uuid16_t hid_svc_uuid = BLE_UUID16_INIT(0x1812);

    struct ble_hs_adv_fields fields   = {0};
    struct ble_hs_adv_fields scan_rsp = {0};
    struct ble_gap_adv_params params  = {0};
    int rc;

    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance            = GAP_APPEARANCE_KEYBOARD;
    fields.appearance_is_present = 1;
    fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.tx_pwr_lvl_is_present = 1;
    fields.uuids16               = &hid_svc_uuid;
    fields.num_uuids16           = 1;
    fields.uuids16_is_complete   = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "adv_set_fields: %d", rc); return; }

    const char *name = ble_svc_gap_device_name();
    scan_rsp.name             = (uint8_t *)name;
    scan_rsp.name_len         = strlen(name);
    scan_rsp.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&scan_rsp);
    if (rc != 0) { ESP_LOGE(TAG, "adv_rsp_set_fields: %d", rc); return; }

    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min  = BLE_GAP_ADV_ITVL_MS(100);
    params.itvl_max  = BLE_GAP_ADV_ITVL_MS(150);

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

            /* Request power-friendly connection parameters:
             * 7.5–15 ms interval, skip up to 30 events when idle,
             * 4 s supervision timeout.  Host may negotiate different values. */
            struct ble_gap_upd_params conn_params = {
                .itvl_min            = 6,    /* 7.5 ms  */
                .itvl_max            = 12,   /* 15 ms   */
                .latency             = 30,
                .supervision_timeout = 400,  /* 4000 ms */
                .min_ce_len          = 0,
                .max_ce_len          = 0,
            };
            ble_gap_update_params(g_conn_handle, &conn_params);
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
        if (event->enc_change.status == 0) {
            g_encrypted = true;
            ESP_LOGI(TAG, "encryption enabled");
        } else {
            ESP_LOGW(TAG, "encryption failed, status=%d", event->enc_change.status);
        }
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe: attr=%d notify=%d",
                 event->subscribe.attr_handle,
                 event->subscribe.cur_notify);
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGD(TAG, "connection parameters updated");
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /*
         * Peer wants to re-pair but we already have a bond.  Delete the
         * stale bond record and allow pairing to proceed.
         */
        ESP_LOGW(TAG, "repeat pairing — deleting stale bond");
        ble_store_util_delete_peer_records(
            &event->repeat_pairing.conn_desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
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

/* ---------- app_main ---------- */

void app_main(void)
{
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        ESP_LOGI(TAG, "woke from deep sleep (keypress)");
    }

    /* NVS required for BLE bonding key storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Create inter-task queues before any task or BLE callback can use them */
    g_report_queue = xQueueCreate(4, sizeof(hid_keyboard_report_t));
    g_led_queue    = xQueueCreate(2, sizeof(uint8_t));
    assert(g_report_queue && g_led_queue);

    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.reset_cb          = on_reset;
    ble_hs_cfg.sync_cb           = on_sync;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;
    ble_hs_cfg.gatts_register_cb = NULL;

    /* Just Works pairing (NO_IO); LE Secure Connections enabled */
    ble_hs_cfg.sm_io_cap         = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_mitm           = 0;
    ble_hs_cfg.sm_sc             = 1;
    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(hid_dev_init());
    ble_store_config_init();

    /* Power management: auto light sleep when all tasks are idle */
    esp_pm_config_t pm_cfg = {
        .max_freq_mhz      = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz      = 10,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_cfg));

    /* Allow PS/2 CLK falling edge to wake from light sleep */
    gpio_wakeup_enable(CONFIG_PS2_CLK_GPIO, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    xTaskCreate(ps2_task, "ps2",  4096, NULL, 5, &g_ps2_task);
    xTaskCreate(ble_task, "ble",  4096, NULL, 4, NULL);

    nimble_port_freertos_init(nimble_host_task);
}
