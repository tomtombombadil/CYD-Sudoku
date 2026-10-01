#!/bin/sh
# Builds the PC preview (Linux/CI only). Usage: tools/preview/build.sh <lvgl dir> <out dir>
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
LVGL=$1
OUT=$2
mkdir -p "$OUT/obj"
for f in $(find "$LVGL/src" -name '*.c'); do
  o="$OUT/obj/lv_$(echo "$f" | md5sum | cut -c1-12).o"
  [ "$o" -nt "$f" ] || gcc -c -O1 -w -DLV_CONF_INCLUDE_SIMPLE -I"$ROOT/include" -I"$LVGL" "$f" -o "$o" &
  [ $(jobs -p | wc -l) -ge 8 ] && wait
done
wait
g++ -std=c++17 -O1 -Wall -DLV_CONF_INCLUDE_SIMPLE -I"$ROOT/include" -I"$LVGL" -I"$ROOT/src" \
  "$ROOT/tools/preview/preview.cpp" "$ROOT/src/ui/game_screen.cpp" "$ROOT/src/ui/board_view.cpp" "$ROOT/src/ui/theme.cpp" \
  "$ROOT/src/game/game.cpp" "$ROOT/src/game/sudoku.cpp" "$OUT"/obj/*.o -lm -o "$OUT/preview"
