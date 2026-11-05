# Upstream Patches

This file records changes outside a board directory so they can be reviewed
when synchronizing with upstream XiaoZhi firmware.

## ESP32-S31-Korvo-1 STC game/chat variant

| File | Change | Reason | Upstream sync |
| --- | --- | --- | --- |
| `main/Kconfig.projbuild` | Adds `BOARD_TYPE_ESP32_S31_KORVO_1_STC` for `IDF_TARGET_ESP32S31` and a board-scoped `STC_MCP_CAMERA` option, default `n`. | Makes the isolated variant selectable and prevents the CHAT server from invoking a camera MCP tool. | Keep both entries while the variant exists; re-add them if an upstream sync replaces the board choice block. |
| `main/CMakeLists.txt` | Maps the board option to `boards/espressif/esp32-s31-korvo-1-stc`, adds the frozen C contract include directory, and packages the board-local `assets` directory. | The board needs the game contract headers and local PCM files in the existing assets partition. | Reapply these lines after upstream CMake selection or asset packaging changes. |

No changes are made to `main/protocols/*`, `main/application.*`, `main/mcp_server.*`,
or an official board directory. GAME mode starts before the upstream network
controller is enabled; CHAT reuses the existing native XiaoZhi voice path.
