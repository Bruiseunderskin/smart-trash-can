# PCA9685 four-servo test (ESP32-S3 / ESP-IDF)

Tested for compilation with ESP-IDF 6.0.2. Change `SDA_GPIO`, `SCL_GPIO`, and, if needed, `PCA_ADDRESS` in `main/main.c` to match your wiring. GPIO 8 and 9 are examples, not guaranteed to be available on your board. Default PCA9685 address is 0x40.

Connect ESP32-S3 3.3 V to PCA9685 VCC, SDA to SDA, SCL to SCL, and GND to GND. Connect a separate regulated supply matching the servos (typically 5 V) to V+ and GND, with sufficient current for four motors, and connect its ground to ESP32-S3 ground. Connect servo signal/power/ground to channels 0–3 according to your board's labels. Use external pull-ups to 3.3 V on SDA/SCL if your module does not already have them. Test with mechanical linkages disconnected; stop if a servo binds.

With ESP-IDF installed and its environment activated:

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p YOUR_PORT flash monitor
```

The test runs each servo through 90, 60, 120, 90 degrees, then all four together. It repeats. Pulse limits 1000–2000 µs are conservative starting values; actual angle and safe range depend on the servo. Serial logging is 115200 baud by default. ESP-IDF normally logs through UART0; use `idf.py menuconfig` → Component config → ESP System Settings → Channel for console output to select USB Serial/JTAG if using that connection. Use the correct corresponding port and reset after opening the monitor. USB Serial/JTAG depends on board wiring.

`ESP-ROM`, `rst:0x1 (POWERON)` and `SPI_FAST_FLASH_BOOT` are boot messages, not sufficient evidence of an error. If no application logs appear, verify the port, console setting, baud rate, flash result and reset; capture all following output. If the app reports an I2C error, check the pins, power, common ground, pull-ups and address. Hardware operation has not been verified.
