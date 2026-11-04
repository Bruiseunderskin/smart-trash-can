# Upstream Patches

This file records changes outside a board directory so they can be reviewed
when synchronizing with upstream XiaoZhi firmware.

## ESP32-S31-Korvo-1 STC text-query test

| File | Change | Reason | Upstream sync |
| --- | --- | --- | --- |
| `main/Kconfig.projbuild` | Adds `BOARD_TYPE_ESP32_S31_KORVO_1_STC` for `IDF_TARGET_ESP32S31` and a board-scoped `STC_MCP_CAMERA` option, default `n`. | Makes the isolated variant selectable. Keeping `GetCamera()` null by default preserves local camera initialization while avoiding an MCP camera/image round trip. | Keep both entries while the variant exists; re-add them if an upstream sync replaces the board choice block. |
| `main/CMakeLists.txt` | Maps `CONFIG_BOARD_TYPE_ESP32_S31_KORVO_1_STC` to `boards/espressif/esp32-s31-korvo-1-stc`, retains the official 800x480 font/emoji defaults, and packages the board-local `assets` directory. | The board selector requires an explicit mapping, and `phrases.json` must be available through the existing assets partition. | Reapply these lines after upstream CMake selection or asset packaging changes. The issue's original estimate of one Kconfig-only core change omitted the board mapping. |

No changes are made to `main/protocols/*` or `main/application.*`. The official
device-to-server protocol has no TTS-only message, and `listen/detect.text` is
rejected above roughly 16 Unicode characters. S therefore remains a constrained
Q fallback; arbitrary long text needs a new server-supported text/TTS message.
