#pragma once

/*
 * ps2_proto — low-level PS/2 protocol driver for ESP-IDF / FreeRTOS.
 *
 * Both CLK and DATA GPIOs are configured as open-drain (INPUT_OUTPUT_OD)
 * with internal pull-ups, matching the PS/2 open-collector bus.
 *
 * Receive path:  NEGEDGE ISR on CLK reads DATA bits → rx_queue (uint8_t)
 * Transmit path: ps2_proto_write_and_wait() inhibits CLK, drives start bit,
 *                releases CLK; keyboard clocks in the remaining bits via ISR;
 *                tx_done semaphore is given from ISR on ACK.
 */

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

typedef enum {
    PS2P_WAIT_R_START = 0,
    PS2P_WAIT_R_DATA0, PS2P_WAIT_R_DATA1, PS2P_WAIT_R_DATA2, PS2P_WAIT_R_DATA3,
    PS2P_WAIT_R_DATA4, PS2P_WAIT_R_DATA5, PS2P_WAIT_R_DATA6, PS2P_WAIT_R_DATA7,
    PS2P_WAIT_R_PARITY, PS2P_WAIT_R_STOP, PS2P_WAIT_R_IGNORE,
    /* Send states must be > all receive states (used as discriminant) */
    PS2P_WAIT_S_DATA0, PS2P_WAIT_S_DATA1, PS2P_WAIT_S_DATA2, PS2P_WAIT_S_DATA3,
    PS2P_WAIT_S_DATA4, PS2P_WAIT_S_DATA5, PS2P_WAIT_S_DATA6, PS2P_WAIT_S_DATA7,
    PS2P_WAIT_S_PARITY, PS2P_WAIT_S_STOP, PS2P_WAIT_S_ACK,
} ps2_proto_state_t;

typedef struct {
    gpio_num_t clk_gpio;
    gpio_num_t data_gpio;
    QueueHandle_t    rx_queue;  /* received bytes; uint8_t; depth 16 */
    SemaphoreHandle_t tx_done;  /* binary; given from ISR on ACK       */
    volatile ps2_proto_state_t state;
    volatile uint8_t current;  /* shift register for current byte      */
    volatile int     parity;   /* running odd-parity accumulator       */
} ps2_proto_t;

/*
 * Initialise the protocol driver.
 * Configures CLK and DATA as INPUT_OUTPUT_OD with pull-up, installs the
 * NEGEDGE ISR on CLK, and creates the rx_queue / tx_done primitives.
 * Safe to call if gpio_install_isr_service() was already called elsewhere.
 */
esp_err_t ps2_proto_init(ps2_proto_t *proto,
                         gpio_num_t   clk_gpio,
                         gpio_num_t   data_gpio);

/*
 * Send one byte to the keyboard and block until the keyboard ACKs or the
 * 50 ms timeout expires.  Returns true on success.
 * Must be called from task context (not ISR).
 */
bool ps2_proto_write_and_wait(ps2_proto_t *proto, uint8_t b);

/* Release resources.  Do not call while ISR may still fire. */
void ps2_proto_deinit(ps2_proto_t *proto);
