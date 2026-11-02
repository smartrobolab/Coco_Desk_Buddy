#!/usr/bin/env bash
# Builds the Coco Voice C3 firmware (ESP32-C3 Super Mini, wake word "Hi ESP") inside the
# official ESP-IDF Docker image (no local ESP-IDF install needed).
#
#   coco_voice_c3/firmware/build.sh
#   WAKE=wn9s_hijason coco_voice_c3/firmware/build.sh   # another wake word (or WAKE=disabled)
#   LANG_CODE=hi-IN coco_voice_c3/firmware/build.sh     # UI / voice-prompt language
#   JOBS=6 coco_voice_c3/firmware/build.sh              # use only 6 CPU cores (less RAM)
#
# Output: xiaozhi-esp32/build/*.bin next to this script (flash with ./flash.sh)
set -euo pipefail
cd "$(dirname "$0")"
FW_DIR="$(pwd)"
IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v6.1}"
XIAOZHI_REPO="${XIAOZHI_REPO:-https://github.com/78/xiaozhi-esp32}"
# Commit the Kibo/Coco board code was tested against. Set XIAOZHI_REF=main to try the latest.
XIAOZHI_REF="${XIAOZHI_REF:-d395220a83fb7a16ea5dc761c08a4816177cc880}"
LANG_CODE="${LANG_CODE:-en-US}"
# JOBS=6 limits the build to 6 CPU cores. Use it if Docker runs out of memory
# (a full-speed build on a 16-core laptop can take more RAM than Docker Desktop has).
CPU_ARGS=()
if [ -n "${JOBS:-}" ]; then CPU_ARGS=(--cpuset-cpus "0-$((JOBS - 1))"); fi

if [ ! -d xiaozhi-esp32/.git ]; then
  git clone "$XIAOZHI_REPO" xiaozhi-esp32
  git -C xiaozhi-esp32 checkout "$XIAOZHI_REF"
fi

python3 tools/install_coco.py xiaozhi-esp32

docker run --rm "${CPU_ARGS[@]}" -u "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$FW_DIR/xiaozhi-esp32:/project" -w /project "$IDF_IMAGE" \
  bash -c "git config --global --add safe.directory '*' && \
           python3 scripts/build.py coco/c3 --language $LANG_CODE --wake-word ${WAKE:-wn9s_hiesp}"

echo
echo "Built coco/c3. Flash it with:  $FW_DIR/flash.sh /dev/ttyACM0"
