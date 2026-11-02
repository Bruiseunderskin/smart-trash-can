# PCA9685_4Servo_Test

ESP32-S3 的 PCA9685 四通道測試。Arduino IDE 開啟同名 .ino；使用內建 Wire，預設 SDA=8、SCL=9、位址 0x40，須核對實際接線。序列埠 115200；先逐通道 0–3，再同步測試。伺服使用獨立電源並共地。

## 本層檔案

- [PCA9685_4Servo_Test.ino](PCA9685_4Servo_Test.ino)

[上層指南](../README.md) · [專案入口](../../README.md)
