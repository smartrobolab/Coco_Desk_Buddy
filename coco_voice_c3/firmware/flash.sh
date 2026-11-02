#!/usr/bin/env bash
# Flashes the last build (bootloader, partition table, app and assets) over USB.
#
#   coco_voice_c3/firmware/flash.sh [/dev/ttyACM0]
#
# Uses esptool on the host (pip install esptool). The ESP32-C3/S3 USB port shows
# up as /dev/ttyACM0 on Linux, /dev/cu.usbmodem* on macOS, COMx on Windows.
# If the port does not appear: hold BOOT, plug in USB, release BOOT.
set -euo pipefail
cd "$(dirname "$0")/xiaozhi-esp32/build"
PORT="${1:-/dev/ttyACM0}"
CHIP=$(python3 -c "import json;print(json.load(open('project_description.json'))['target'])")
python3 -m esptool --chip "$CHIP" -p "$PORT" -b 460800 --before default-reset --after hard-reset \
  write-flash "@flash_args"
echo "Flashed. Open the serial log with:  python3 -m serial.tools.miniterm $PORT 115200"
