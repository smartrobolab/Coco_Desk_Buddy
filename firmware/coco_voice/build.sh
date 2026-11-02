#!/usr/bin/env bash
# Builds Coco Voice firmware inside the official ESP-IDF Docker image (no local
# ESP-IDF install needed).
#
#   firmware/coco_voice/build.sh c3                    # ESP32-C3 Super Mini, wake word "Hi ESP"
#   firmware/coco_voice/build.sh s3                    # XIAO ESP32S3 + PSRAM, wake word "Hey Coco"
#   WAKE=wn9s_hijason firmware/coco_voice/build.sh c3  # another C3 wake word (or WAKE=disabled)
#   LANG_CODE=hi-IN firmware/coco_voice/build.sh s3    # UI / voice-prompt language
#
# Output: firmware/coco_voice/xiaozhi-esp32/build/*.bin  (flash with flash.sh)
set -euo pipefail
cd "$(dirname "$0")"
VOICE_DIR="$(pwd)"
IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v6.1}"
XIAOZHI_REPO="${XIAOZHI_REPO:-https://github.com/78/xiaozhi-esp32}"
# Commit the Kibo/Coco board code was tested against. Set XIAOZHI_REF=main to try the latest.
XIAOZHI_REF="${XIAOZHI_REF:-d395220a83fb7a16ea5dc761c08a4816177cc880}"
LANG_CODE="${LANG_CODE:-en-US}"
VARIANT="${1:-c3}"
case "$VARIANT" in
  c3) BOARD="coco/c3"; WAKE_ARGS="--wake-word ${WAKE:-wn9s_hiesp}" ;;
  s3) BOARD="coco/s3"; WAKE_ARGS="" ;;  # custom "Hey Coco" is set in boards/coco/s3/config.json
  *) echo "usage: $0 c3|s3"; exit 2 ;;
esac

if [ ! -d xiaozhi-esp32/.git ]; then
  git clone "$XIAOZHI_REPO" xiaozhi-esp32
  git -C xiaozhi-esp32 checkout "$XIAOZHI_REF"
fi

python3 tools/install_coco.py xiaozhi-esp32

docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$VOICE_DIR/xiaozhi-esp32:/project" -w /project "$IDF_IMAGE" \
  bash -c "git config --global --add safe.directory '*' && \
           python3 scripts/build.py $BOARD --language $LANG_CODE $WAKE_ARGS"

echo
echo "Built $BOARD. Flash it with:  firmware/coco_voice/flash.sh /dev/ttyACM0"
