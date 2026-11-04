#!/usr/bin/env bash
# Builds the Coco Yeti sketch (ESP32-C3 Super Mini) with arduino-cli (esp32 core 3.x)
# and copies one merged binary per variant (flash at 0x0) to release/.
#
#   coco_touch_advance/yeti/firmware/build.sh            # all four
#   coco_touch_advance/yeti/firmware/build.sh c3_13      # only one
#
# Variants:
#   c3_096       Coco wiring (touch GPIO0, SDA 4, SCL 5, INT 3, vibration 10) + 0.96" SSD1306
#   c3_13        Coco wiring + 1.3" SH1106
#   leroyd_096   Desk Companion wiring (SDA 8, SCL 9, touch 7, vibration 10) + 0.96" SSD1306
#   leroyd_13    Desk Companion wiring + 1.3" SH1106
#
# Like Coco Touch: release/ is built without coco_secrets.h; PRIVATE=1 builds
# with it into private/ (ignored by git).
set -euo pipefail
cd "$(dirname "$0")"
FW_DIR="$(pwd)"
SKETCH="$FW_DIR/coco_yeti"
if [ -n "${PRIVATE:-}" ]; then
  OUT="$FW_DIR/private"
  SECRETS=""
else
  OUT="$FW_DIR/release"
  SECRETS="-DCOCO_NO_SECRETS=1"
fi
BUILD="${BUILD_DIR:-${TMPDIR:-/tmp}/coco_yeti_build}"
ALL="c3_096 c3_13 leroyd_096 leroyd_13"
FQBN="esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app"

build() {
  local name=$1 sh1106 leroyd
  case $name in
    c3_096)     sh1106=0; leroyd=0 ;;
    c3_13)      sh1106=1; leroyd=0 ;;
    leroyd_096) sh1106=0; leroyd=1 ;;
    leroyd_13)  sh1106=1; leroyd=1 ;;
    *) echo "unknown variant '$name' (use: $ALL)" >&2; exit 1 ;;
  esac
  echo "== $name (OLED_SH1106=$sh1106, YETI_PINOUT_LEROYD=$leroyd)"
  arduino-cli compile -b "$FQBN" --warnings all \
    --build-path "$BUILD/$name" \
    --build-property "compiler.cpp.extra_flags=-DOLED_SH1106=$sh1106 -DYETI_PINOUT_LEROYD=$leroyd $SECRETS" \
    "$SKETCH"
  mkdir -p "$OUT"
  # drop the 0xFF padding at the end (unused flash); still flashed at 0x0
  python3 - "$BUILD/$name/coco_yeti.ino.merged.bin" "$OUT/coco_yeti_${name}-merged.bin" <<'EOF'
import sys
data = open(sys.argv[1], "rb").read()
end = len(data.rstrip(b"\xff"))
end = (end + 4095) // 4096 * 4096
open(sys.argv[2], "wb").write(data[:end])
EOF
  echo "-> $OUT/coco_yeti_${name}-merged.bin"
}

for v in ${@:-$ALL}; do build "$v"; done
