#!/usr/bin/env bash
# Builds Coco Voice firmware inside the official ESP-IDF Docker image (no local
# ESP-IDF install needed).
#
#   firmware/coco_voice/build.sh                     # wake word "Hi ESP"
#   WAKE=wn9s_hijason firmware/coco_voice/build.sh   # another wake word (or WAKE=disabled)
#   LANG_CODE=hi-IN firmware/coco_voice/build.sh     # UI / voice-prompt language
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

if [ ! -d xiaozhi-esp32/.git ]; then
  git clone "$XIAOZHI_REPO" xiaozhi-esp32
  git -C xiaozhi-esp32 checkout "$XIAOZHI_REF"
fi

python3 tools/install_coco.py xiaozhi-esp32

docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$VOICE_DIR/xiaozhi-esp32:/project" -w /project "$IDF_IMAGE" \
  bash -c "git config --global --add safe.directory '*' && \
           python3 scripts/build.py coco/c3 --language $LANG_CODE --wake-word ${WAKE:-wn9s_hiesp}"

echo
echo "Built coco/c3. Flash it with:  firmware/coco_voice/flash.sh /dev/ttyACM0"
