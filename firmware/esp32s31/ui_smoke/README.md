# S31 UI smoke

Build-only ESP32-S31 target project for the latest LVGL Pro XML UI in
`C:/smart_trash_can-ui-main/ui/lvgl`.

It initializes the Korvo-1 display and touch controller, renders the game UI,
cycles display states, and logs UI action intents. It does not initialize the
camera, ESP-DL, ESP-NOW, servos or TOF sensors.

```powershell
idf.py -C C:\smart_trash_can_espnow\firmware\esp32s31\ui_smoke `
  -B D:\jack\codex-build\s31-ui-smoke build
```
