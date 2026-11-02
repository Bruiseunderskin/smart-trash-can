# 智能分类垃圾桶 (smart-trash-can)

基于 `bread-compact-wifi` 扩展的项目板型：在面包板 ESP32-S3 方案上增加 4 路舵机。
用语音询问「这个垃圾扔哪个桶」时，AI 判断分类并打开对应桶盖，8 秒后自动关闭，
同时在 OLED 上显示分类名。

## 硬件接线

舵机信号线（MG90S）：

- 可回收物 -> GPIO9
- 厨余垃圾 -> GPIO10
- 有害垃圾 -> GPIO11
- 其他垃圾 -> GPIO12

舵机红线接**独立 5V 电源**（不要用 ESP32-S3 的 3.3V），棕/黑线接该电源负极，
并与 ESP32-S3 共地。舵机堵转电流可达 1A 以上，务必独立供电。

音频、OLED（I2C，SDA=GPIO41 / SCL=GPIO42）、按键等与 `bread-compact-wifi` 完全一致。

## 角度约定

- `0` 度 = 关盖，`90` 度 = 开盖
- 开盖后 `LID_AUTO_CLOSE_MS`（默认 8000ms）自动回位
- 以上常量都在 `config.h` 中，可按机械结构调整

## MCP 工具

| 工具 | 参数 | 说明 |
| --- | --- | --- |
| `self.trash_can.open_lid` | `category`（必填） | 打开指定分类的桶盖，取值 `可回收物` / `厨余垃圾` / `有害垃圾` / `其他垃圾` |
| `self.trash_can.close_lid` | `category`（默认 `all`） | 关闭指定桶，`all` 表示关闭全部 |
| `self.trash_can.get_status` | 无 | 返回四个桶盖的开合状态 |

## 编译与烧录

```bash
python scripts/build.py smart-trash-can
idf.py -p COM8 flash monitor
```
