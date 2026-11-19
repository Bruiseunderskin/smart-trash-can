# ESP32-S3 four-TOF probe

This probe assumes four **TOF050C / VL6180X** boards.

| Signal | GPIO |
|---|---|
| I2C SDA | 8 |
| I2C SCL | 9 |
| XSHUT sensor 0 | 4 |
| XSHUT sensor 1 | 5 |
| XSHUT sensor 2 | 6 |
| XSHUT sensor 3 | 7 |

At boot all sensors are held in reset. They are released one at a time and
assigned addresses `0x30`–`0x33`. The probe verifies model ID `0xB4`, scans the
I2C bus, then benchmarks four sensors in parallel continuous-ranging mode at
50, 30, 20 and 10 ms maximum convergence settings.

Output markers:

- `TOF_READY` — one sensor was addressed and identified.
- `TOF_I2C_SCAN` — addresses found on the bus.
- `TOF_ROUND` — one four-sensor sample round.
- `TOF_BENCH` — min/p50/p95/max latency and ready/valid counts.
- `TOF_FASTEST_READY` — fastest four-sensor ready round, including status errors.
- `TOF_FASTEST_VALID` — fastest round where all four status values were zero.
- `TOF4_PROBE=PASS` — all four produced at least one valid round.
- `TOF4_PROBE=PARTIAL` — all four communicated, but at least one range status
  was non-zero in every round.

This probe measures sensor-to-I2C latency only. Accuracy, offset, crosstalk,
black-card detection and mechanical mounting are not calibrated here.
