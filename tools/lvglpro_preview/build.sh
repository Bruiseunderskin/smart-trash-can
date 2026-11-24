#!/bin/sh
# Builds the LVGL 9.5 preview for ui/lvglpro and nothing else.
#
#   tools/lvglpro_preview/build.sh            # full build
#   LVGL_DIR=/path/to/lvgl tools/lvglpro_preview/build.sh
#
# The LVGL tree is pinned: v9.5.0 == 85aa60d18b3d5e5588d7b247abf90198f07c8a63.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BUILD="$HERE/build"
LVGL_DIR=${LVGL_DIR:-$BUILD/lvgl}
CC=${CC:-cc}
JOBS=${JOBS:-8}
PIN=85aa60d18b3d5e5588d7b247abf90198f07c8a63

if [ ! -d "$LVGL_DIR/src" ]; then
    mkdir -p "$BUILD"
    git clone --depth 1 --branch v9.5.0 https://github.com/lvgl/lvgl.git "$LVGL_DIR"
fi
if [ "$(git -C "$LVGL_DIR" rev-parse HEAD)" != "$PIN" ]; then
    echo "LVGL at $LVGL_DIR is not v9.5.0 ($PIN)" >&2
    exit 1
fi

mkdir -p "$BUILD/obj"
find "$LVGL_DIR/src" -name '*.c' > "$BUILD/sources.txt"
i=0
while IFS= read -r src; do
    name=$(printf '%s' "$src" | sed 's|/|_|g; s|\.c$||')
    "$CC" -O1 -c "$src" -o "$BUILD/obj/$name.o" \
        -I"$LVGL_DIR" -I"$HERE" -DLV_CONF_INCLUDE_SIMPLE \
        -DSTC_UI_PREVIEW=1 &
    i=$((i + 1))
    if [ $((i % JOBS)) -eq 0 ]; then
        wait
    fi
done < "$BUILD/sources.txt"
wait
rm -f "$BUILD/liblvgl.a"
ar rcs "$BUILD/liblvgl.a" "$BUILD/obj"/*.o

"$CC" -O1 -DLV_CONF_INCLUDE_SIMPLE -DSTC_UI_PREVIEW=1 -I"$HERE" -I"$LVGL_DIR" \
    -I"$ROOT/ui/lvglpro/include" -I"$ROOT/ui/lvglpro/generated" \
    -o "$BUILD/preview" \
    "$HERE/main.c" \
    "$ROOT/ui/lvglpro/src/stc_ui.c" \
    "$ROOT/ui/lvglpro/src/stc_chat_ui.c" \
    "$ROOT/ui/lvglpro/generated/"*.c \
    "$BUILD/liblvgl.a" -lm

"$CC" -O1 -DLV_CONF_INCLUDE_SIMPLE -DSTC_UI_PREVIEW=1 -I"$HERE" -I"$LVGL_DIR" \
    -I"$ROOT/ui/lvglpro/include" -I"$ROOT/ui/lvglpro/generated" \
    -o "$BUILD/sim" \
    "$HERE/sim.c" \
    "$ROOT/ui/lvglpro/src/stc_ui.c" \
    "$ROOT/ui/lvglpro/src/stc_chat_ui.c" \
    "$ROOT/ui/lvglpro/generated/"*.c \
    "$BUILD/liblvgl.a" -lm

echo "built $BUILD/preview and $BUILD/sim"
