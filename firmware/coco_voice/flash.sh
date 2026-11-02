#!/usr/bin/env bash
# Flashes the last build (bootloader, partition table, app and assets) over USB.
#
#   firmware/coco_voice/flash.sh [/dev/ttyACM0]
#
# Uses esptool on the host (pip install esptool). The ESP32-C3 USB port shows
# up as /dev/ttyACM0 on Linux, /dev/cu.usbmodem* on macOS, COMx on Windows.
# If the port does not appear: hold BOOT, plug in USB, release BOOT.
set -euo pipefail
cd "$(dirname "$0")/xiaozhi-esp32/build"
PORT="${1:-/dev/ttyACM0}"
python3 -m esptool --chip esp32c3 -p "$PORT" -b 460800 --before default-reset --after hard-reset \
  write-flash "@flash_args"
echo "Flashed. Open the serial log with:  python3 -m serial.tools.miniterm $PORT 115200"
