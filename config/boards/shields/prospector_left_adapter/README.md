# Prospector left-hand display adapter

This adapter drives the Prospector ST7789 display directly from the Sofle left
nice!nano. The left half remains the ZMK split central; no dongle is used. Its
display wiring matches the central receiver from the `cake_dongle` branch of
`LXF-YZP/cake-zmk-config`.

| Display signal | nice!nano pin | nRF52840 pin |
| --- | --- | --- |
| SCK | D15 | P1.13 |
| MOSI | A0/D18 | P1.15 |
| MISO | internal pad | P1.10 |
| CS | D9 | P1.06 |
| DC | D8 | P1.04 |
| RESET | D7 | P0.11 |
| Backlight PWM | D14 | P1.11 |
| APDS9960 SDA | D2 | P0.17 |
| APDS9960 SCL | D3 | P0.20 |
| APDS9960 INT | D6 | P1.00 |
| VCC | 3V3 | 3V3 |
| GND | GND | GND |

## Required matrix rewiring

The Cake display assignment consumes two pins used by the original Sofle left
matrix. Rewire the following two left-half columns:

| Matrix signal | Old pin | New pin |
| --- | --- | --- |
| COL2 | P1.13 | P0.22 (D4) |
| COL5 | P1.11 | P0.24 (D5) |

The APDS9960 ambient-light sensor is enabled. The right TPS43/TPS48 remains on
its own nice!nano at SCL P0.20 and SDA P0.17; the duplicate pin numbers are safe
because the two halves use separate microcontrollers.
