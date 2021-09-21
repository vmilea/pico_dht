/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <dht.h>
#include <dht.pio.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <pico/stdlib.h>
#include <math.h>
#include <string.h>

static const uint PIO_SM_CLOCK_FREQUENCY = 1000000; // 1MHz
static const uint DHT_LONG_PULSE_THRESHOLD_US = 50;
static const uint DHT_MEASUREMENT_TIMEOUT_US = 6000;

//
// misc
//

static uint get_start_pulse_duration_us(dht_model_t model) {
    return (model == DHT21 || model == DHT22) ? 1000 : 18000;
}

static uint get_pio_sm_clocks(uint us) {
    float clocks_per_microsecond = PIO_SM_CLOCK_FREQUENCY / 1000000.0f;
    return roundf(us * clocks_per_microsecond);
}

static bool pio_sm_is_enabled(PIO pio, uint sm) {
    return (pio->ctrl & (1 << sm)) != 0;
}

static void dht_program_init(PIO pio, uint sm, uint offset, dht_model_t model, uint data_pin) {
    pio_sm_config c = dht_program_get_default_config(offset);
    uint32_t sys_clock_frequency = clock_get_hz(clk_sys);
    sm_config_set_clkdiv(&c, sys_clock_frequency / (float)PIO_SM_CLOCK_FREQUENCY);
    sm_config_set_set_pins(&c, data_pin, 1);
    // configuring jmp pin is enough, we don't need any other input pins
    sm_config_set_jmp_pin(&c, data_pin);
    // bits arrive in MSB order and are shifted to the left; autopush every 8 bits
    sm_config_set_in_shift(&c, false /* shift_right */, true /* autopush */, 8 /* push_threshold */);
    pio_sm_init(pio, sm, offset, &c);

    // push timing values
    pio_sm_put_blocking(pio, sm, get_pio_sm_clocks(get_start_pulse_duration_us(model) / dht_start_signal_clocks_per_loop));
    pio_sm_put_blocking(pio, sm, get_pio_sm_clocks(DHT_LONG_PULSE_THRESHOLD_US / dht_pulse_measurement_clocks_per_loop));
    // drive the data pin low to wake sensor
    pio_sm_exec(pio, sm, pio_encode_set(pio_pindirs, 1));
    // pull the start-signal duration
    pio_sm_exec(pio, sm, pio_encode_pull(/* if_empty */ false, /* block */ true));
    // store it in Y register for the loop
    pio_sm_exec(pio, sm, pio_encode_mov(pio_y, pio_osr));
    // pull the long pulse threshold
    pio_sm_exec(pio, sm, pio_encode_pull(/* if_empty */ false, /* block */ true));
    // start executing the PIO program
    pio_sm_set_enabled(pio, sm, true);
}

static void configure_dma_channel(uint chan, PIO pio, uint sm, uint8_t *write_addr) {
    dma_channel_config c = dma_channel_get_default_config(chan);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false /* is_tx */));
    channel_config_set_irq_quiet(&c, true);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(chan, &c, write_addr, &pio->rxf[sm], 5, true /* trigger */);
}

static float decode_temperature(dht_model_t model, uint8_t b0, uint8_t b1) {
    float temperature;
    switch (model) {
    case DHT11:
        if (b1 & 0x80) {
            // below-zero temperature not supported
            temperature = 0.0f;
        } else {
            temperature = b0 + 0.1f * (b1 & 0x7F);
        }
        break;
    case DHT12:
        temperature = b0 + 0.1f * (b1 & 0x7F);
        if (b1 & 0x80) {
            temperature = -temperature;
        }
        break;
    case DHT21:
    case DHT22:
        temperature = 0.1f * (((b0 & 0x7F) << 8) + b1);
        if (b0 & 0x80) {
            temperature = -temperature;
        }
        break;
    default:
        assert(false); // invalid model
    }
    return temperature;
}

static float decode_humidity(dht_model_t model, uint8_t b0, uint8_t b1) {
    float humidity;
    switch (model) {
    case DHT11:
    case DHT12:
        humidity = b0 + 0.1f * b1;
        break;
    case DHT21:
    case DHT22:
        humidity = 0.1f * ((b0 << 8) + b1);
        break;
    default:
        assert(false); // invalid model
    }
    return humidity;
}

//
// public interface
//

void dht_init(dht_t *dht, dht_model_t model, PIO pio, uint8_t data_pin, bool pull_up) {
    assert(pio == pio0 || pio == pio1);

    memset(dht, 0, sizeof(dht_t));
    dht->model = model;
    dht->pio = pio;
    dht->pio_program_offset = pio_add_program(pio, &dht_program);
    dht->sm = pio_claim_unused_sm(pio, true /* required */);
    dht->dma_chan = dma_claim_unused_channel(true /* required */);
    dht->data_pin = data_pin;

    pio_gpio_init(pio, data_pin);
    gpio_set_pulls(data_pin, pull_up, false /* down */);
}

void dht_deinit(dht_t *dht) {
    assert(dht->pio != NULL); // not initialized

    dma_channel_abort(dht->dma_chan);
    dma_channel_unclaim(dht->dma_chan);

    pio_sm_set_enabled(dht->pio, dht->sm, false);
    // make sure pin is left in hi-z mode; original pin function & pulls are not restored
    pio_sm_set_consecutive_pindirs(dht->pio, dht->sm, dht->data_pin, 1, false /* is_out */);
    pio_sm_unclaim(dht->pio, dht->sm);
    pio_remove_program(dht->pio, &dht_program, dht->pio_program_offset);

    dht->pio = NULL;
}

void dht_start_measurement(dht_t *dht) {
    assert(dht->pio != NULL); // not initialized
    assert(!pio_sm_is_enabled(dht->pio, dht->sm)); // another measurement in progress

    memset(dht->data, 0, sizeof(dht->data));
    configure_dma_channel(dht->dma_chan, dht->pio, dht->sm, dht->data);
    dht_program_init(dht->pio, dht->sm, dht->pio_program_offset, dht->model, dht->data_pin);
    dht->start_time = time_us_32();
}

dht_result_t dht_finish_measurement_blocking(dht_t *dht, float *humidity, float *temperature_c) {
    assert(dht->pio != NULL); // not initialized
    assert(pio_sm_is_enabled(dht->pio, dht->sm)); // no measurement in progress

    uint32_t timeout = get_start_pulse_duration_us(dht->model) + DHT_MEASUREMENT_TIMEOUT_US;
    while (dma_channel_is_busy(dht->dma_chan) && time_us_32() - dht->start_time < timeout) {
        tight_loop_contents();
    }
    pio_sm_set_enabled(dht->pio, dht->sm, false);
    // make sure pin is left in hi-z mode
    pio_sm_exec(dht->pio, dht->sm, pio_encode_set(pio_pindirs, 0));

    if (dma_channel_is_busy(dht->dma_chan)) {
        dma_channel_abort(dht->dma_chan);
        return DHT_RESULT_TIMEOUT;
    }
    uint8_t checksum = dht->data[0] + dht->data[1] + dht->data[2] + dht->data[3];
    if (dht->data[4] != checksum) {
        return DHT_RESULT_BAD_CHECKSUM;
    }
    if (humidity != NULL) {
        *humidity = decode_humidity(dht->model, dht->data[0], dht->data[1]);
    }
    if (temperature_c != NULL) {
        *temperature_c = decode_temperature(dht->model, dht->data[2], dht->data[3]);
    }
    return DHT_RESULT_OK;
}
