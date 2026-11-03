#!/usr/bin/env bash
# Builds the Coco Touch Arduino sketch with arduino-cli (esp32 core 3.x) for each
# board + screen, and copies one merged binary per variant (flash at 0x0) to release/.
#
#   coco_touch/firmware/build.sh              # all four
#   coco_touch/firmware/build.sh c3_13        # only one (several names are fine)
#
# Variants:
#   c3_096      ESP32-C3 Super Mini + 0.96" SSD1306
#   c3_13       ESP32-C3 Super Mini + 1.3" SH1106
#   hw747_096   HW-747 ESP32-S3 Super Mini (4 MB, 2 MB PSRAM) + 0.96" SSD1306
#   hw747_13    HW-747 ESP32-S3 Super Mini (4 MB, 2 MB PSRAM) + 1.3" SH1106
#
# The board pins follow from the chip (coco_config.h); the screen is OLED_SH1106,
# passed here as -DOLED_SH1106=1.
set -euo pipefail
cd "$(dirname "$0")"
FW_DIR="$(pwd)"
SKETCH="$FW_DIR/coco_touch"
OUT="$FW_DIR/release"
BUILD="${BUILD_DIR:-${TMPDIR:-/tmp}/coco_touch_build}"
ALL="c3_096 c3_13 hw747_096 hw747_13"

FQBN_C3="esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app"
FQBN_S3="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=4M,PSRAM=enabled,PartitionScheme=huge_app"

build() {
  local name=$1 fqbn sh1106
  case $name in
    c3_096)    fqbn=$FQBN_C3; sh1106=0 ;;
    c3_13)     fqbn=$FQBN_C3; sh1106=1 ;;
    hw747_096) fqbn=$FQBN_S3; sh1106=0 ;;
    hw747_13)  fqbn=$FQBN_S3; sh1106=1 ;;
    *) echo "unknown variant '$name' (use: $ALL)" >&2; exit 1 ;;
  esac
  echo "== $name ($fqbn, OLED_SH1106=$sh1106)"
  arduino-cli compile -b "$fqbn" --warnings all \
    --build-path "$BUILD/$name" \
    --build-property "compiler.cpp.extra_flags=-DOLED_SH1106=$sh1106" \
    "$SKETCH"
  mkdir -p "$OUT"
  # The core pads the merged image with 0xFF to the full flash size; drop that
  # tail (unused SPIFFS area) so the release files stay small. Still flashed at 0x0.
  python3 - "$BUILD/$name/coco_touch.ino.merged.bin" "$OUT/coco_touch_${name}-merged.bin" <<'EOF'
import sys
data = open(sys.argv[1], "rb").read()
end = len(data.rstrip(b"\xff"))
end = (end + 4095) // 4096 * 4096  # keep a whole 4 KB flash sector
open(sys.argv[2], "wb").write(data[:end])
EOF
  echo "-> $OUT/coco_touch_${name}-merged.bin"
}

for v in ${@:-$ALL}; do build "$v"; done
