# Prospector left-hand display adapter

This adapter drives the Prospector ST7789 display directly from the Sofle left
controller. The left half remains the ZMK split central; no dongle is used.

## Status screen

The Operator UI and portrait layout fixes are ported from `caip2` (531753e),
using Prospector's `feat/new-status-screens` branch. The display remains at
240 x 280 with a final 180-degree orientation. It shows both battery levels
with centered labels and uses the narrower modifier row and inset layer name.
The screen stays on at 80% fixed brightness with left-half deep sleep disabled.
The E73 wiring below is preserved independently of the UI port.

The left half has no battery. Its circle displays 100% whenever USB power is
detected, independently of the selected USB/BLE output. This is a display-only
power indicator, not a measured battery level. The right circle still shows the
peripheral's reported battery level.

| Display signal | nice!nano pin | nRF52840 pin |
| --- | --- | --- |
| SCK | D3 | P0.20 |
| MOSI | D2 | P0.17 |
| MISO | Not used | Not used |
| CS | D1 | P0.06 |
| DC | D5 | P0.24 |
| RESET | E73 pad | P0.13 |
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
before P0.13 is assigned to the display RESET signal.
