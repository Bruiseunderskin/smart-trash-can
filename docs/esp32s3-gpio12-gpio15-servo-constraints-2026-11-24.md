# ESP32-S3 GPIO12 / GPIO15 servo constraints

Date: 2026-11-24

## Question

On the ESP32-S3 hand controller, GPIO12 and GPIO15 do not move their
servos, while GPIO13 and GPIO14 do. The same servo and 5 V/GND wiring
work when driven from GPIO13. Is there an ESP32-S3 datasheet or
board-level reason that GPIO12/GPIO15 cannot be used as 50 Hz PWM servo
outputs?

## Result

No. On a standard ESP32-S3-DevKitC-1, GPIO12 and GPIO15 have no documented
restriction preventing normal GPIO, LED PWM, or MCPWM operation.

The official ESP32-S3 Series Datasheet v2.2 lists both pins as IO pins
powered by `VDD3P3_RTC`. Table 2-9 classifies GPIO12 and GPIO15 as
Priority 2 (`P2`) for LED PWM and MCPWM, meaning they can be freely used
through the GPIO matrix. Neither pin appears in the strapping-pin table.

Observed test state:

```text
GPIO13: servo moves
GPIO14: servo moves
GPIO12: no movement, including 1000-2000 us test pulses
GPIO15: no movement, including 1000-2000 us test pulses
```

The PWM API reports no errors. This points away from a generic software
or library fault and toward the GPIO12/15 signal path, external loading,
or pin damage.

Follow-up hardware result: after switching from the single-channel test
to simultaneous OPEN_ALL/CLOSE_ALL cycling, all four servos moved. This
rules out permanent GPIO12/GPIO15 damage and confirms that the current
GPIO12/15 signal paths can drive the servos. The earlier two-servo result
therefore points to a temporary harness/contact issue or a problem in the
single-channel test setup rather than an ESP32-S3 GPIO restriction.

## Datasheet findings

| Topic | GPIO12 | GPIO15 | Consequence |
|---|---|---|---|
| Boot strapping | Not a strapping pin | Not a strapping pin | Cannot block boot mode |
| Octal flash/PSRAM reservation | Not reserved | Not reserved | Safe from the documented 8-line SPI allocation |
| LED PWM / MCPWM | Supported, P2 | Supported, P2 | Valid servo output in principle |
| USB / JTAG | Not a fixed USB or JTAG pin | Not a fixed USB or JTAG pin | No normal runtime conflict |
| Default drive | 20 mA | 20 mA | More than sufficient for a servo signal input |
| Power domain | `VDD3P3_RTC` | `VDD3P3_RTC` | Normal 3.0-3.6 V IO domain |
| Power-up behaviour | 60 us low glitch | 60 us low glitch | May twitch at reset, not prevent later PWM |
| Reset MUX | GPIO | `XTAL_32K_P` | GPIO15 can be selected as GPIO after reset |

The documented strapping pins are GPIO0, GPIO3, GPIO45, and GPIO46.
The pins reserved for in-package/8-line flash or PSRAM are GPIO26-GPIO37.
Neither GPIO12 nor GPIO15 belongs to those groups.

GPIO15 being `XTAL_32K_P` at reset is only relevant if the actual board
has an external 32.768 kHz crystal or oscillator connected there. The
official DevKitC-1 v1.1 schematic does not add such a circuit; GPIO15 is
brought directly to header J1, as is GPIO12.

## Plausible physical faults

Ranked possibilities after the software and datasheet checks:

1. Signal path from the ESP32-S3 header to the two servo connectors is
   open, intermittent, or connected to the wrong header pin.
2. GPIO12 or GPIO15 has been electrically damaged. A servo signal pin
   must never be exposed to 5 V; the datasheet absolute maximum for IO
   power is 3.6 V. A missing servo ground can also make current return
   through the signal pin.
3. A custom board has a board-level load on GPIO12 (for example an
   alternate SPI device) or GPIO15 (for example a 32 kHz crystal). This
   is not present on the official DevKitC-1 schematic.
4. The two individual servo connectors or crimps are faulty. Replacing
   the signal source with GPIO13 demonstrated that the servo itself and
   its 5 V supply can work, but does not prove the GPIO12/15 harness.

## Pin-level test

Power off and check continuity from the ESP32-S3 GPIO12/GPIO15 header
pins to the corresponding servo signal pins. Then power on with a
focused 1 ms/2 ms test and measure the signal pin:

```text
1 ms pulse at 50 Hz  -> about 0.165 V average
2 ms pulse at 50 Hz  -> about 0.330 V average
```

Use an oscilloscope if available. A pin that remains at 0 V or 3.3 V,
or has very low resistance to GND/3.3 V with power off, is suspect.

A second isolation step is to connect the known-working servo signal
lead directly to the GPIO12 and GPIO15 header pins, while the existing
firmware drives those pins. If it still does not move, the pin or the
short jumper is faulty. If it moves, the original GPIO12/15 harness is
faulty.

If the pins are suspect, remap bin 0 and bin 3 to spare GPIOs such as
GPIO16 and GPIO17 and re-run the same test. The datasheet marks both as
GPIO-capable, but the actual carrier board and attached peripherals
must still be checked.

## Existing servo-control libraries

- ESP-IDF LEDC driver: already used by `game_hand`; appropriate for
  generating 50 Hz servo pulses.
- ESP-IDF MCPWM driver: official servo-control example exists and can be
  used as an independent peripheral-level test.
- Arduino-ESP32 `ESP32Servo`: suitable if moving to the Arduino core.
- PCA9685 driver/library: suitable only if the servo rail is moved to a
  PCA9685 board.

Switching libraries is not expected to repair a damaged pin or broken
signal wire, because LEDC already works on GPIO13/14 in the same build.

## Sources

- Espressif, ESP32-S3 Series Datasheet v2.2:
  <https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf>
  - Table 2-1 and Table 2-2: pins 16-18
  - Table 2-4 and Table 2-8: IO MUX and analog functions, pages 21-24
  - Section 2.3.4-2.3.5 and Table 2-9: restrictions and peripheral
    priorities, pages 25-27
  - Table 2-14: flash/PSRAM pin mapping, page 31
  - Section 3 and Table 3-1: strapping pins, page 32
  - Table 5-1: absolute maximum ratings, page 64
- Espressif, ESP32-S3-DevKitC-1 v1.1 user guide:
  <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html>
- Espressif, ESP32-S3-DevKitC-1 v1.1 schematic:
  <https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-DevKitC-1_V1.1_20221130.pdf>

## Confidence

High confidence that GPIO12/GPIO15 have no documented ESP32-S3
restriction for servo PWM on an unmodified DevKitC-1.

Medium confidence that the current failure is physical: the exact fault
is not identifiable without continuity and voltage measurements on the
actual board.
