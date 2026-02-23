#include "ps2_proto.h"

#include <string.h>

#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "ps2_proto";

/* ---------- ISR helpers ---------- */

/*
 * Receive state handler.  Advances through the bit shift-register states and
 * pushes completed bytes onto rx_queue.
 * Returns pdTRUE if a higher-priority task was woken.
 */
static BaseType_t isr_handle_receive(ps2_proto_t *proto, int bit)
{
    BaseType_t woken = pdFALSE;

    switch (proto->state) {

    case PS2P_WAIT_R_START:
        if (!bit) {
            proto->current = 0;
            proto->parity  = 1;  /* odd parity: accumulator starts at 1 */
            proto->state   = PS2P_WAIT_R_DATA0;
        }
        /* else: framing error — stay in START and wait for next byte */
        break;

    case PS2P_WAIT_R_DATA0: case PS2P_WAIT_R_DATA1:
    case PS2P_WAIT_R_DATA2: case PS2P_WAIT_R_DATA3:
    case PS2P_WAIT_R_DATA4: case PS2P_WAIT_R_DATA5:
    case PS2P_WAIT_R_DATA6: case PS2P_WAIT_R_DATA7:
        proto->current >>= 1;
        if (bit) {
            proto->current |= 0x80;
            proto->parity  ^= 1;
        }
        proto->state = (ps2_proto_state_t)(proto->state + 1);
        break;

    case PS2P_WAIT_R_PARITY:
        if (bit != proto->parity) {
            /* Parity error: discard byte */
            proto->state = PS2P_WAIT_R_START;
        } else {
            proto->state = PS2P_WAIT_R_STOP;
        }
        break;

    case PS2P_WAIT_R_STOP:
        if (bit) {
            uint8_t b = proto->current;   /* copy out of volatile */
            xQueueSendFromISR(proto->rx_queue, &b, &woken);
            if (proto->task_to_notify) {
                BaseType_t w2 = pdFALSE;
                vTaskNotifyGiveFromISR(proto->task_to_notify, &w2);
                woken |= w2;
            }
        }
        proto->state = PS2P_WAIT_R_START;
        break;

    case PS2P_WAIT_R_IGNORE:
        break;

    default:
        proto->state = PS2P_WAIT_R_START;
        break;
    }

    return woken;
}

/*
 * Send state handler.  Outputs bits onto DATA (open-drain) and signals
 * tx_done on the keyboard ACK clock pulse.
 * Returns pdTRUE if a higher-priority task was woken.
 */
static BaseType_t isr_handle_send(ps2_proto_t *proto, int ack_bit)
{
    BaseType_t woken = pdFALSE;

    switch (proto->state) {

    case PS2P_WAIT_S_DATA0: case PS2P_WAIT_S_DATA1:
    case PS2P_WAIT_S_DATA2: case PS2P_WAIT_S_DATA3:
    case PS2P_WAIT_S_DATA4: case PS2P_WAIT_S_DATA5:
    case PS2P_WAIT_S_DATA6: case PS2P_WAIT_S_DATA7: {
        int sbit = proto->current & 1;
        proto->current >>= 1;
        if (sbit) proto->parity ^= 1;
        gpio_set_level(proto->data_gpio, sbit);
        proto->state = (ps2_proto_state_t)(proto->state + 1);
        break;
    }

    case PS2P_WAIT_S_PARITY:
        gpio_set_level(proto->data_gpio, proto->parity);
        proto->state = PS2P_WAIT_S_STOP;
        break;

    case PS2P_WAIT_S_STOP:
        /* Release DATA (open-drain high = release line to pull-up) */
        gpio_set_level(proto->data_gpio, 1);
        proto->state = PS2P_WAIT_S_ACK;
        break;

    case PS2P_WAIT_S_ACK:
        /* Keyboard pulls DATA low as ACK */
        if (!ack_bit)
            xSemaphoreGiveFromISR(proto->tx_done, &woken);
        /* Reset to receive mode whether ACK was valid or not */
        proto->state = PS2P_WAIT_R_START;
        break;

    default:
        proto->state = PS2P_WAIT_R_START;
        break;
    }

    return woken;
}

/* ---------- NEGEDGE ISR on CLK ---------- */

static void ps2_proto_isr(void *arg)
{
    ps2_proto_t *proto = (ps2_proto_t *)arg;
    BaseType_t woken;

    if (proto->state < PS2P_WAIT_S_DATA0) {
        woken = isr_handle_receive(proto, gpio_get_level(proto->data_gpio));
    } else {
        int ack = (proto->state == PS2P_WAIT_S_ACK)
                  ? gpio_get_level(proto->data_gpio)
                  : 0;
        woken = isr_handle_send(proto, ack);
    }

    /* Yield at the end of the ISR if a higher-priority task was unblocked */
    portYIELD_FROM_ISR(woken);
}

/* ---------- Public API ---------- */

esp_err_t ps2_proto_init(ps2_proto_t *proto,
                         gpio_num_t   clk_gpio,
                         gpio_num_t   data_gpio)
{
    memset(proto, 0, sizeof(*proto));
    proto->clk_gpio  = clk_gpio;
    proto->data_gpio = data_gpio;
    proto->state     = PS2P_WAIT_R_START;
    proto->parity    = 1;

    proto->rx_queue = xQueueCreate(16, sizeof(uint8_t));
    if (!proto->rx_queue) return ESP_ERR_NO_MEM;

    proto->tx_done = xSemaphoreCreateBinary();
    if (!proto->tx_done) {
        vQueueDelete(proto->rx_queue);
        return ESP_ERR_NO_MEM;
    }

    /* Configure both lines as open-drain with internal pull-up.
     * In open-drain mode: gpio_set_level(n, 0) → pull low;
     *                     gpio_set_level(n, 1) → release (line floats high). */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << clk_gpio) | (1ULL << data_gpio),
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) return err;

    /* Release both lines (idle state) */
    gpio_set_level(clk_gpio,  1);
    gpio_set_level(data_gpio, 1);

    /* Install the GPIO ISR service.  If already installed, that is fine. */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service: %d", err);
        return err;
    }

    err = gpio_set_intr_type(clk_gpio, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) return err;

    err = gpio_isr_handler_add(clk_gpio, ps2_proto_isr, proto);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "init: clk=%d data=%d", clk_gpio, data_gpio);
    return ESP_OK;
}

bool ps2_proto_write_and_wait(ps2_proto_t *proto, uint8_t b)
{
    /* Disable ISR so we can atomically set up the transmit state */
    gpio_isr_handler_remove(proto->clk_gpio);

    /* Pull CLK low for >= 100 µs (host inhibit: keyboard stops transmitting) */
    gpio_set_level(proto->clk_gpio, 0);
    esp_rom_delay_us(100);

    /* Drive DATA low (start bit) */
    gpio_set_level(proto->data_gpio, 0);

    /* Set up transmit state machine */
    proto->current = b;
    proto->parity  = 1;
    proto->state   = PS2P_WAIT_S_DATA0;

    /* Re-enable ISR before releasing CLK so no clock pulse is missed */
    gpio_isr_handler_add(proto->clk_gpio, ps2_proto_isr, proto);

    /* Release CLK: keyboard takes over clocking and reads our data bits */
    gpio_set_level(proto->clk_gpio, 1);

    /* Block until ISR gives the tx_done semaphore (keyboard ACK) */
    bool ok = (xSemaphoreTake(proto->tx_done, pdMS_TO_TICKS(50)) == pdTRUE);
    if (!ok) {
        ESP_LOGW(TAG, "write 0x%02X: no ACK (timeout)", b);
        proto->state = PS2P_WAIT_R_START;
    }
    return ok;
}

void ps2_proto_deinit(ps2_proto_t *proto)
{
    gpio_isr_handler_remove(proto->clk_gpio);
    if (proto->rx_queue) vQueueDelete(proto->rx_queue);
    if (proto->tx_done)  vSemaphoreDelete(proto->tx_done);
    memset(proto, 0, sizeof(*proto));
}
