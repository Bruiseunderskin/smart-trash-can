# Display profiles

Profiles are composition starting points, not electrical or driver authority. Confirm the actual module’s logical dimensions, intended rotation, color format, controller, bus, offsets, inversion, touch mapping, timing, backlight, and power constraints before hardware use.

| Board/profile | Logical size | Depth | Rotation | Status | Boundary |
|---|---:|---:|---:|---|---|
| Common ST7789 dashboard | 320×240 | 16 | 0 | Runnable in native SDL preview | Controller settings, touch, and physical orientation remain unverified. |
| Waveshare ESP32-S3-LCD-3.16 | 320×820 | 16 | 0 | Runnable in native SDL preview | The vendor page specifies 320×820 and RGB565/64K color; controller, bus timing, GPIOs, touch, offsets, and backlight remain hardware-verification items. |
| Generic ESP32-S3 RGB LCD | 480×480 | 16 | 0 | Layout starting point | RGB timing, GPIO mapping, PSRAM pressure, touch, and backlight are outside the SDL preview. |
| Generic 7-inch RGB LCD | 800×480 | 16 | 0 | Layout starting point | Timing, GPIO mapping, PSRAM pressure, touch, and backlight are outside the SDL preview. |

The official Waveshare source used for the 3.16-inch entry is the [ESP32-S3-LCD-3.16 wiki page](https://www.waveshare.com/wiki/ESP32-S3-LCD-3.16). Profiles marked “layout starting point” should not be treated as a proven runtime target without a dedicated simulator and hardware test.
