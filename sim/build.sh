#!/usr/bin/env bash
# Builds the PC renderer from the same sources the Coco Touch firmware uses.
set -euo pipefail
cd "$(dirname "$0")"
SRC=../coco_touch/firmware/coco_touch
mkdir -p build
g++ -std=c++17 -O2 -Wall -Wextra -I"$SRC" render.cpp \
  "$SRC/kibo_canvas.cpp" "$SRC/kibo_faces.cpp" "$SRC/kibo_screens.cpp" "$SRC/kibo_qr.cpp" "$SRC/coco_cards.cpp" \
  -o build/render
echo "built sim/build/render"
