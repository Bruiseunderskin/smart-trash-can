# ESP-NOW spike main component

這個 component 只在 ESP32-S31 controller 與 ESP32-S3 node 之間傳送既有的
16-byte `stc_lidmsg` 幀。角色由 `Kconfig.projbuild` 選擇；transport 行為、
建置命令與上板判準見[上層指南](../README.md)。

本 component 不包含相機、伺服機、TOF、UI 或遊戲控制器。

[上層指南](../README.md) · [專案入口](../../../README.md)
