#!/usr/bin/env python3
"""Adds the Coco Voice board to a XiaoZhi (xiaozhi-esp32) checkout.

    python3 firmware/coco_voice/tools/install_coco.py [path/to/xiaozhi-esp32]

What it does (safe to run again after every change):
  1. copies boards/coco (c3, s3) and the shared Coco/Kibo sources -> xiaozhi-esp32/main/boards/coco
     (face engine from firmware/coco_touch, voice/behaviour code from firmware/coco_voice/shared)
  2. copies partitions/* (8 MB layout for the S3) -> xiaozhi-esp32/partitions/
  3. registers "Coco Voice C3" / "Coco Voice S3" in main/Kconfig.projbuild and main/CMakeLists.txt
  4. replaces the "start listening" popup sound with the "aaaa?" wake sound (original kept as .orig)
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
VOICE = os.path.dirname(HERE)
FW = os.path.dirname(VOICE)
DEFAULT_XZ = os.path.join(VOICE, "xiaozhi-esp32")
FACE_FILES = ["kibo_canvas.cpp", "kibo_canvas.h", "kibo_faces.cpp", "kibo_faces.h", "kibo_font.h",
              "kibo_screens.cpp", "kibo_screens.h", "kibo_qr.cpp", "kibo_qr.h"]

KCONFIG_BLOCK = """    config BOARD_TYPE_COCO_C3
        bool "Coco Voice desk buddy (ESP32-C3 Super Mini)"
        depends on IDF_TARGET_ESP32C3
    config BOARD_TYPE_COCO_S3
        bool "Coco Voice S3 desk buddy (XIAO ESP32S3 + PSRAM, Hey Coco)"
        depends on IDF_TARGET_ESP32S3
"""

CMAKE_BLOCK = """elseif(CONFIG_BOARD_TYPE_COCO_C3)
    set(BOARD_DIR "coco/c3")
    set(BUILTIN_TEXT_FONT font_noto_sans_basic_14_1)
    set(BUILTIN_ICON_FONT font_material_symbols_14_1)
elseif(CONFIG_BOARD_TYPE_COCO_S3)
    set(BOARD_DIR "coco/s3")
    set(BUILTIN_TEXT_FONT font_noto_sans_basic_14_1)
    set(BUILTIN_ICON_FONT font_material_symbols_14_1)
"""

MARK = "BOARD_TYPE_COCO_S3"  # newest board: re-registers checkouts patched before it existed


def patch_kconfig(path):
    text = open(path, encoding="utf-8").read()
    if MARK in text:
        return "already registered"
    start = text.index("choice BOARD_TYPE")
    end = text.index("endchoice", start)
    text = text[:end] + KCONFIG_BLOCK + text[end:]
    open(path, "w", encoding="utf-8").write(text)
    return "registered"


def patch_cmake(path):
    text = open(path, encoding="utf-8").read()
    if MARK in text:
        return "already registered"
    anchor = "elseif(CONFIG_BOARD_TYPE_XMINI_C3)"
    if anchor not in text:
        anchor = text[text.index("elseif(CONFIG_BOARD_TYPE_"):].split("\n", 1)[0]
    i = text.index(anchor)
    text = text[:i] + CMAKE_BLOCK + text[i:]
    open(path, "w", encoding="utf-8").write(text)
    return "registered"


def main():
    xz = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_XZ)
    if not os.path.isfile(os.path.join(xz, "main", "Kconfig.projbuild")):
        sys.exit(f"{xz} does not look like a xiaozhi-esp32 checkout.\n"
                 "Run firmware/coco_voice/build.sh, it clones it for you.")

    dst = os.path.join(xz, "main", "boards", "coco")
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(os.path.join(VOICE, "boards", "coco"), dst)
    shared = os.path.join(dst, "shared")
    shutil.copytree(os.path.join(VOICE, "shared"), shared)
    for name in FACE_FILES:
        shutil.copy(os.path.join(FW, "coco_touch", name), shared)
    print(f"[1/4] copied boards       -> {dst}")

    for name in os.listdir(os.path.join(VOICE, "partitions")):
        shutil.copy(os.path.join(VOICE, "partitions", name), os.path.join(xz, "partitions", name))
    print("[2/4] copied partitions")

    print("[3/4] Kconfig.projbuild:", patch_kconfig(os.path.join(xz, "main", "Kconfig.projbuild")),
          "| CMakeLists.txt:", patch_cmake(os.path.join(xz, "main", "CMakeLists.txt")))

    popup = os.path.join(xz, "main", "assets", "common", "popup.ogg")
    if not os.path.exists(popup + ".orig"):
        shutil.copy(popup, popup + ".orig")
    shutil.copy(os.path.join(VOICE, "sounds", "wake.ogg"), popup)
    print("[4/4] popup.ogg replaced with the 'aaaa?' wake sound (restore: popup.ogg.orig)")
    print("\nDone. Build with:  firmware/coco_voice/build.sh c3   (or s3)")


if __name__ == "__main__":
    main()
