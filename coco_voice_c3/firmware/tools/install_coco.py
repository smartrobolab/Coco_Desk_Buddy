#!/usr/bin/env python3
"""Adds this Coco Voice board (coco/c3 or coco/s3) to a XiaoZhi (xiaozhi-esp32) checkout.

    python3 tools/install_coco.py [path/to/xiaozhi-esp32]

The board is whichever one this firmware folder contains (boards/coco/c3 or
boards/coco/s3). Safe to run again after every change:
  1. copies boards/coco/<v> and shared/ -> xiaozhi-esp32/main/boards/coco/
  2. copies partitions/* (S3 only: 8 MB layout) -> xiaozhi-esp32/partitions/
  3. registers the board in main/Kconfig.projbuild and main/CMakeLists.txt, and adds the
     COCO_OLED_SH1106 screen option (on = 1.3" SH1106, off = 0.96" SSD1306)
  4. replaces the "start listening" popup sound with the "aaaa?" wake sound (original kept as .orig)
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FW = os.path.dirname(HERE)
DEFAULT_XZ = os.path.join(FW, "xiaozhi-esp32")

BOARDS = {
    "c3": ("BOARD_TYPE_COCO_C3", "Coco Voice C3 desk buddy (ESP32-C3 Super Mini)", "IDF_TARGET_ESP32C3"),
    "s3": ("BOARD_TYPE_COCO_S3", "Coco Voice S3 desk buddy (XIAO ESP32S3 + PSRAM, Hey Coco)", "IDF_TARGET_ESP32S3"),
}


def variant():
    found = [v for v in BOARDS if os.path.isdir(os.path.join(FW, "boards", "coco", v))]
    if len(found) != 1:
        sys.exit(f"expected exactly one of boards/coco/c3 or boards/coco/s3 in {FW}, found {found}")
    return found[0]


def patch_kconfig(path, v):
    sym, title, target = BOARDS[v]
    text = open(path, encoding="utf-8").read()
    if f"config {sym}\n" in text:
        return "already registered"
    start = text.index("choice BOARD_TYPE")
    end = text.index("endchoice", start)
    block = f"    config {sym}\n        bool \"{title}\"\n        depends on {target}\n"
    text = text[:end] + block + text[end:]
    open(path, "w", encoding="utf-8").write(text)
    return "registered"


def patch_screen_option(path):
    """Adds the screen choice after the board list. A build turns it on with
    CONFIG_COCO_OLED_SH1106=y in its config.json sdkconfig_append; config.h reads it."""
    text = open(path, encoding="utf-8").read()
    if "config COCO_OLED_SH1106\n" in text:
        return "already there"
    start = text.index("choice BOARD_TYPE")
    end = text.index("endchoice", start) + len("endchoice")
    block = ('\n\nconfig COCO_OLED_SH1106\n'
             '    bool "Coco: 1.3\\" SH1106 OLED (off: 0.96\\" SSD1306)"\n'
             '    default n\n'
             '    help\n'
             '        The 1.3" OLED modules use the SH1106 controller, the 0.96" ones the SSD1306.\n')
    text = text[:end] + block + text[end:]
    open(path, "w", encoding="utf-8").write(text)
    return "added"


def patch_cmake(path, v):
    sym = BOARDS[v][0]
    text = open(path, encoding="utf-8").read()
    if f"CONFIG_{sym})" in text:
        return "already registered"
    block = (f"elseif(CONFIG_{sym})\n    set(BOARD_DIR \"coco/{v}\")\n"
             "    set(BUILTIN_TEXT_FONT font_noto_sans_basic_14_1)\n"
             "    set(BUILTIN_ICON_FONT font_material_symbols_14_1)\n")
    anchor = "elseif(CONFIG_BOARD_TYPE_XMINI_C3)"
    if anchor not in text:
        anchor = text[text.index("elseif(CONFIG_BOARD_TYPE_"):].split("\n", 1)[0]
    i = text.index(anchor)
    text = text[:i] + block + text[i:]
    open(path, "w", encoding="utf-8").write(text)
    return "registered"


def main():
    v = variant()
    xz = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_XZ)
    if not os.path.isfile(os.path.join(xz, "main", "Kconfig.projbuild")):
        sys.exit(f"{xz} does not look like a xiaozhi-esp32 checkout. Run build.sh, it clones it for you.")

    dst = os.path.join(xz, "main", "boards", "coco")
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(os.path.join(FW, "boards", "coco", v), os.path.join(dst, v))
    shutil.copytree(os.path.join(FW, "shared"), os.path.join(dst, "shared"))
    print(f"[1/4] copied coco/{v} + shared -> {dst}")

    parts = os.path.join(FW, "partitions")
    if os.path.isdir(parts):
        for name in os.listdir(parts):
            shutil.copy(os.path.join(parts, name), os.path.join(xz, "partitions", name))
        print("[2/4] copied partitions")
    else:
        print("[2/4] no custom partitions (uses XiaoZhi's 4 MB layout)")

    kconfig = os.path.join(xz, "main", "Kconfig.projbuild")
    print("[3/4] Kconfig.projbuild:", patch_kconfig(kconfig, v), "| screen option:", patch_screen_option(kconfig),
          "| CMakeLists.txt:", patch_cmake(os.path.join(xz, "main", "CMakeLists.txt"), v))

    popup = os.path.join(xz, "main", "assets", "common", "popup.ogg")
    if not os.path.exists(popup + ".orig"):
        shutil.copy(popup, popup + ".orig")
    shutil.copy(os.path.join(FW, "sounds", "wake.ogg"), popup)
    print("[4/4] popup.ogg replaced with the 'aaaa?' wake sound (restore: popup.ogg.orig)")


if __name__ == "__main__":
    main()
