# Troubleshooting

## “Project root is incomplete”

Pass the directory containing the project’s `CMakeLists.txt` and use `--build-dir` if the build directory is not `build/`. The script does not search outside the root.

## “Could not identify exactly one display call/configuration”

Inspect the source and pass `--main-file` or `--config-file`. If the project does not use `sdl_hal_init(width, height)`, add a backend adapter rather than broadening the regex to edit unrelated code.

## Build failure

Run the project’s normal CMake configure step manually after reviewing it, then rerun the preview. Check CMake, compiler, SDL2, LVGL version, and generated configuration. The script restores its touched files after a failed build.

## Preview exits early

Read `.lvgl-preview/logs/<run-id>.log`, run the executable directly only after authorization, and check missing SDL libraries, display initialization, and assertion output. A process that exits within the startup window is rejected automatically.

## Wrong orientation or colors

The desktop preview verifies composition only. Confirm the physical controller’s MADCTL/rotation, RGB/BGR order, byte order, offsets, pixel format, and touch transform in the hardware bring-up procedure.

## Physical-display request

Stop the preview workflow and route to hardware bring-up. No desktop process can prove panel wiring, signal integrity, power sequencing, backlight behavior, touch calibration, or refresh timing.
