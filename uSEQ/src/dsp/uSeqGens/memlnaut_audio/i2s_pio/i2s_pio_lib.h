#pragma once
#include "i2s_sck.pio.h"
#include "i2s_out_master.pio.h"
#include "i2s_bidi_slave.pio.h"
#include "i2s_in_slave.pio.h"

// These constants are the I2S clock to pio clock ratio
const int i2s_sck_program_pio_mult = 2;
const int i2s_out_master_program_pio_mult = 2;

/*
 * System ClocK (SCK) is only required by some I2S peripherals.
 * This outputs it at 1 SCK per 2 PIO clocks, so scale the dividers correctly
 * first.
 * NOTE: Most peripherals require that this is *perfectly* aligned in ratio,
 *       if not phase, to the bit and word clocks of any master peripherals.
 *       It is up to you to ensure that the divider config is set up for a
 *       precise (not approximate) ratio between the BCK, LRCK, and SCK outputs.
 */
static void i2s_sck_program_init(PIO pio, uint8_t sm, uint8_t offset, uint8_t sck_pin) {
    pio_gpio_init(pio, sck_pin);
    pio_sm_config sm_config = i2s_sck_program_get_default_config(offset);
    sm_config_set_set_pins(&sm_config, sck_pin, 1);

    uint pin_mask = (1u << sck_pin);
    pio_sm_set_pins_with_mask(pio, sm, 0, pin_mask);  // zero output
    pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask);

    pio_sm_init(pio, sm, offset, &sm_config);
}

static inline void i2s_out_master_program_init(PIO pio, uint8_t sm, uint8_t offset, uint8_t bit_depth, uint8_t dout_pin, uint8_t clock_pin_base) {
    pio_gpio_init(pio, dout_pin);
    pio_gpio_init(pio, clock_pin_base);
    pio_gpio_init(pio, clock_pin_base + 1);

    pio_sm_config sm_config = i2s_out_master_program_get_default_config(offset);
    sm_config_set_out_pins(&sm_config, dout_pin, 1);
    sm_config_set_sideset_pins(&sm_config, clock_pin_base);
    sm_config_set_out_shift(&sm_config, false, false, bit_depth);
    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX);
    pio_sm_init(pio, sm, offset, &sm_config);

    uint32_t pin_mask = (1u << dout_pin) | (3u << clock_pin_base);
    pio_sm_set_pins_with_mask(pio, sm, 0, pin_mask);  // zero output
    pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask);
}

static inline void i2s_bidi_slave_program_init(PIO pio, uint8_t sm, uint8_t offset, uint8_t dout_pin, uint8_t in_pin_base) {
    pio_gpio_init(pio, dout_pin);
    pio_gpio_init(pio, in_pin_base);
    pio_gpio_init(pio, in_pin_base + 1);
    pio_gpio_init(pio, in_pin_base + 2);

    pio_sm_config sm_config = i2s_bidi_slave_program_get_default_config(offset);
    sm_config_set_out_pins(&sm_config, dout_pin, 1);
    sm_config_set_in_pins(&sm_config, in_pin_base);
    sm_config_set_jmp_pin(&sm_config, in_pin_base + 2);
    sm_config_set_out_shift(&sm_config, false, false, 0);
    sm_config_set_in_shift(&sm_config, false, false, 0);
    pio_sm_init(pio, sm, offset, &sm_config);

    // Setup output pins
    uint32_t pin_mask = (1u << dout_pin);
    pio_sm_set_pins_with_mask(pio, sm, 0, pin_mask);  // zero output
    pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask);

    // Setup input pins
    pin_mask = (7u << in_pin_base);  // Three input pins
    pio_sm_set_pindirs_with_mask(pio, sm, 0, pin_mask);
}

/*
 *  Designed to be used with output master module, requiring overlapping pins:
 *    din_pin_base + 0 = input pin
 *    din_pin_base + 1 = out_master clock_pin_base
 *    din_pin_base + 2 = out_master clock_pin_base + 1
 *
 *  Intended to be run at SCK rate (4x BCK), so clock same as SCK module if using
 *  it, or 4x the BCK frequency (BCK is 64x fs, so 256x fs).
 */
static inline void i2s_in_slave_program_init(PIO pio, uint8_t sm, uint8_t offset, uint8_t din_pin_base) {
    pio_gpio_init(pio, din_pin_base);
    gpio_set_pulls(din_pin_base, false, false);
    gpio_set_dir(din_pin_base, GPIO_IN);

    pio_sm_config sm_config = i2s_in_slave_program_get_default_config(offset);
    sm_config_set_in_pins(&sm_config, din_pin_base);
    sm_config_set_in_shift(&sm_config, false, false, 0);
    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_RX);
    sm_config_set_jmp_pin(&sm_config, din_pin_base + 2);
    pio_sm_init(pio, sm, offset, &sm_config);

    uint32_t pin_mask = (7u << din_pin_base);  // Three input pins
    pio_sm_set_pindirs_with_mask(pio, sm, 0, pin_mask);
}
