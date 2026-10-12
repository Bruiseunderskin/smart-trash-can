# Smart Trash Can（智能垃圾桶）

基于 ESP32-S3 的智能垃圾桶项目：使用舵机控制桶盖开合，并结合 xiaozhi-esp32 语音助手固件实现语音交互。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `arduino/MG90_LidServo_ESP32S3/` | Arduino 示例：MG90S 舵机控制桶盖开合（GPIO9，支持串口命令 `o`/`c`/`s`/角度） |
| `xiaozhi-esp32/` | 语音助手固件源码，来源为 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) |
| `.vscode/`、`.clangd` | 编辑器 / 语言服务器配置 |

## 硬件接线

- 舵机信号线（橙/黄） → ESP32-S3 `GPIO9`
- 舵机电源（红） → 外部 5V 电源正极（不要用 ESP32-S3 的 3.3V）
- 舵机地线（棕/黑） → 外部 5V 电源负极，并与 ESP32-S3 共地

## 关于构建产物

以下内容**未纳入版本控制**（可通过官方工具链重新生成）：

- `xiaozhi-esp32/build/`
- `xiaozhi-esp32/managed_components/`
- `xiaozhi-esp32/sdkconfig`、`xiaozhi-esp32/dependencies.lock` 等本地配置

编译 `xiaozhi-esp32` 时，请参考上游仓库的说明安装 ESP-IDF 并运行 `idf.py build`。
