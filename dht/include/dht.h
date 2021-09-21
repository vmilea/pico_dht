/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _DHT_H_
#define _DHT_H_

#include <hardware/pio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file dht.h
 *
 * \brief DHT sensor library.
 */

/**
 * \brief DHT sensor model.
 */
typedef enum dht_model_t {
    DHT11,
    DHT12,
    DHT21,
    DHT22,
} dht_model_t;

/**
 * \brief DHT sensor.
 */
typedef struct dht_t {
    PIO pio;
    uint8_t model;
    uint8_t pio_program_offset;
    uint8_t sm;
    uint8_t dma_chan;
    uint8_t data_pin;
    uint8_t data[5];
    uint32_t start_time;
} dht_t;

/**
 * \brief Measurement result.
 */
typedef enum dht_result_t {
    DHT_RESULT_OK, /**< No error.*/
    DHT_RESULT_TIMEOUT, /**< DHT sensor not reponding. */
    DHT_RESULT_BAD_CHECKSUM, /**< Sensor data doesn't match checksum. */
} dht_result_t;

/**
 * \brief Initialize DHT sensor.
 * 
 * The library claims one state machine from the given PIO instance, and one DMA
 * channel to communicate with the sensor.
 * 
 * \param dht DHT sensor.
 * \param model DHT sensor model.
 * \param pio PIO block to use (pio0 or pio1).
 * \param data_pin Sensor data pin.
 * \param pull_up Whether to enable the internal pull-up.
 */
void dht_init(dht_t *dht, dht_model_t model, PIO pio, uint8_t data_pin, bool pull_up);

/**
 * \brief Deinitialize DHT sensor.
 *
 * \param dht DHT sensor.
 */
void dht_deinit(dht_t *dht);

/**
 * \brief Start asynchronous measurement.
 *
 * The measurement runs in the background, and may take up to 25ms depending
 * on DHT model.
 * 
 * DHT sensors typically need at least 2 seconds between measurements for
 * accurate results.
 * 
 * \param dht DHT sensor.
 */
void dht_start_measurement(dht_t *dht);

/**
 * \brief Wait for measurement to complete and get the result.
 *
 * \param dht DHT sensor.
 * \param[out] humidity Relative humidity. May be NULL.
 * \param[out] temperature_c Degrees Celsius. May be NULL.
 * \return Result status.
 */
dht_result_t dht_finish_measurement_blocking(dht_t *dht, float *humidity, float *temperature_c);

#ifdef __cplusplus
}
#endif

#endif // _DHT_H_
