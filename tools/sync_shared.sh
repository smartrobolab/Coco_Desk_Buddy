#!/usr/bin/env bash
# Each version folder has its own complete firmware, so some files exist in
# several places. Edit them in the master copy, then run this to copy them out:
#   face engine (kibo_*.cpp/h)  master: coco_touch/firmware/coco_touch/
#   voice + behaviour code      master: coco_voice_c3/firmware/shared/
#   installer + wake sound      master: coco_voice_c3/firmware/
#
#   tools/sync_shared.sh          copy
#   tools/sync_shared.sh --check  only report differences (exit 1 if any)
set -euo pipefail
[ "${1:-}" = --check ] && MODE=check
cd "$(dirname "$0")/.."
TOUCH=coco_touch/firmware/coco_touch
C3=coco_voice_c3/firmware
S3=coco_voice_s3/firmware
FACE="kibo_canvas.cpp kibo_canvas.h kibo_faces.cpp kibo_faces.h kibo_font.h kibo_screens.cpp kibo_screens.h kibo_qr.cpp kibo_qr.h"
VOICE="kibo_behavior.cpp kibo_behavior.h kibo_board.inc kibo_display.cpp kibo_display.h kibo_sounds.h kibo_time.h"
pairs=()
for f in $FACE; do pairs+=("$TOUCH/$f $C3/shared/$f" "$TOUCH/$f $S3/shared/$f"); done
for f in $VOICE; do pairs+=("$C3/shared/$f $S3/shared/$f"); done
pairs+=("$C3/tools/install_coco.py $S3/tools/install_coco.py" "$C3/sounds/wake.ogg $S3/sounds/wake.ogg")
diffs=0
for p in "${pairs[@]}"; do
  set -- $p
  if ! cmp -s "$1" "$2"; then
    diffs=$((diffs + 1))
    if [ "${MODE:-copy}" = check ]; then echo "differs: $2 (master $1)"; else cp "$1" "$2"; echo "updated $2"; fi
  fi
done
if [ "${MODE:-copy}" = check ]; then
  [ $diffs -eq 0 ] && echo "all shared copies identical" || exit 1
fi
