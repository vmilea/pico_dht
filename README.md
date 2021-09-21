# DHT sensor library for the Raspberry Pi Pico

This is a C library for working with DHT temperature & humidity sensors on the Raspberry Pi Pico and similar RP2040 based boards. It supports DHT11, DHT12, DHT21, and DHT22 through the one-wire interface.

A PIO state machine is used to communicate with the sensor, leaving the CPU cores available for other tasks. Sounds like overkill, but hey: it's bit banging and what the PIOs are designed for!

## Example

The example program prints temperature and humidity every 2 seconds.

### Wiring

| DHT pin | Raspberry Pi Pico pin |
| ------- | --------------------- |
| VDD     | 3V3(OUT)              |
| SDA     | GP15                  |
| GND     | GND                   |

Most DHT modules have a built-in pull-up. The example program also enables the internal pull-up, so an external resistor is not required.

### Setup

Follow the instructions in [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf) to setup your build environment. Then:

- `git clone https://github.com/vmilea/pico_dht`
- `cd pico_dht`
- change `DHT_MODEL` in `dht_example.c` if needed (default is DHT22)
- `mkdir build`, `cd build`, `cmake ..`, `make`
- copy `dht_example.uf2` to Raspberry Pico
- open a serial connection and check output

## Authors

Valentin Milea <valentin.milea@gmail.com>
