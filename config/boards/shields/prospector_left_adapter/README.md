# Prospector left-hand display adapter

This adapter drives the Prospector ST7789 display directly from the Sofle left
controller. The left half remains the ZMK split central; no dongle is used.

| Display signal | nice!nano pin | nRF52840 pin |
| --- | --- | --- |
| SCK | D3 | P0.20 |
| MOSI | D2 | P0.17 |
| MISO | Not used | Not used |
| CS | D1 | P0.06 |
| DC | E73 pad | P0.13 |
| RESET | D5 | P0.24 |
| Backlight PWM | D14 | P1.11 |
| VCC | 3V3 | 3V3 |
| GND | GND | GND |

## Required matrix rewiring

The Cake display assignment consumes two pins used by the original Sofle left
matrix. Rewire the following two left-half columns:

| Matrix signal | Old pin | New pin |
| --- | --- | --- |
| COL2 | P1.13 | P0.22 (D4) |
| COL5 | P1.11 | P1.13 (D15) |

The APDS9960 ambient-light sensor is unavailable with this wiring because its
former I2C pins P0.17 and P0.20 are now used by the display SPI bus. Brightness
therefore uses the fixed value from `config/sofle_left.conf`. P0.13 is normally
the nice!nano v2 external-power control pin, so that devicetree node is disabled
before P0.13 is assigned to the display DC signal.
