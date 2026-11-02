#!/usr/bin/env python3
"""Draws Coco's wiring diagrams, brass case drawings and assembly steps.

    python3 tools/make_diagrams.py

Each version gets its own folder, and every file name starts with the version:
  coco_touch/images/     coco_touch_wiring.png, _case.png, _assembly.png, _bending_template.pdf
  coco_voice_c3/images/  coco_voice_c3_wiring.png, _case.png, _assembly.png, _bending_template.pdf
  coco_voice_s3/images/  coco_voice_s3_wiring.png, _hw747_esp32s3_wiring.png, _devkit_wiring.png, _case.png, _assembly.png, _bending_template.pdf
  shared/images/         coco_overview.png (all versions side by side)
Needs matplotlib.
"""
import math
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, FancyBboxPatch, Rectangle
from mpl_toolkits.mplot3d.art3d import Line3DCollection, Poly3DCollection

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSIONS = ("coco_touch", "coco_voice_c3", "coco_voice_s3")


def out_path(name):
    """coco_touch_x.png -> coco_touch/images/, coco_voice_c3_x -> coco_voice_c3/images/, else shared/images/."""
    folder = next((v for v in VERSIONS if name.startswith(v + "_")), "shared")
    path = os.path.join(ROOT, folder, "images", name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return path


# ---------------------------------------------------------------- colours
BG = "#fbfaf6"
INK = "#22252b"
BRASS = "#c9962c"
BRASS_DARK = "#8a6414"
W_RED = "#d62828"      # battery +
W_BLACK = "#222222"    # GND
W_ORANGE = "#f08c00"   # 3V3
W_BLUE = "#1c7ed6"     # SDA
W_YELLOW = "#e0b000"   # SCL
W_GREEN = "#2b9348"    # touch
W_PURPLE = "#7048e8"   # battery sense
W_CYAN = "#0c8599"     # I2S clocks
W_PINK = "#d6336c"     # I2S data
W_GREY = "#868e96"
PCB_BLUE = "#2f5f9e"
PCB_GREEN = "#2d7a46"
PCB_BLACK = "#2b2b2b"
PCB_RED = "#b03030"
PCB_PURPLE = "#5b3d99"


# ======================================================== wiring diagrams

class Sheet:
    def __init__(self, title, subtitle, w=170, h=112):
        self.fig = plt.figure(figsize=(w / 10, h / 10), dpi=150, facecolor=BG)
        self.ax = self.fig.add_axes([0, 0, 1, 1])
        self.ax.set_xlim(0, w)
        self.ax.set_ylim(0, h)
        self.ax.set_aspect("equal")
        self.ax.axis("off")
        self.w, self.h = w, h
        self.ax.text(4, h - 5, title, fontsize=19, weight="bold", color=INK, va="center")
        self.ax.text(4, h - 10.5, subtitle, fontsize=10.5, color="#555", va="center")

    def module(self, x, y, w, h, name, sub, color, pins, text_color="white"):
        """pins: list of (label, side, pos) side in L/R/T/B, pos = offset along that side.
        Returns {label: (px, py)} endpoints just outside the board."""
        ax = self.ax
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=1.6", fc=color, ec="#111",
                                    lw=1.2, zorder=3))
        ax.text(x + w / 2, y + h / 2 + (1.6 if sub else 0), name, ha="center", va="center", fontsize=9.5,
                weight="bold", color=text_color, zorder=4)
        if sub:
            ax.text(x + w / 2, y + h / 2 - 2.2, sub, ha="center", va="center", fontsize=6.8, color=text_color,
                    zorder=4)
        out = {}
        for label, side, pos in pins:
            if side == "L":
                px, py, tx, ty, ha, va = x, y + pos, x + 1.0, y + pos, "left", "center"
            elif side == "R":
                px, py, tx, ty, ha, va = x + w, y + pos, x + w - 1.0, y + pos, "right", "center"
            elif side == "T":
                px, py, tx, ty, ha, va = x + pos, y + h, x + pos, y + h - 1.0, "center", "top"
            else:
                px, py, tx, ty, ha, va = x + pos, y, x + pos, y + 1.0, "center", "bottom"
            ax.add_patch(Circle((px, py), 0.75, fc="#e9d18a", ec="#6b5414", lw=0.8, zorder=5))
            rot = 90 if side in "TB" else 0
            ax.text(tx, ty, label, ha=ha, va=va, fontsize=6.3, color=text_color, zorder=5, rotation=rot,
                    family="DejaVu Sans Mono", weight="bold")
            out[label] = (px, py)
        return out

    def wire(self, pts, color, lw=2.2, dot_end=False, dots=()):
        xs, ys = zip(*pts)
        self.ax.plot(xs, ys, color=color, lw=lw, solid_capstyle="round", solid_joinstyle="round", zorder=2)
        for d in dots:
            self.ax.add_patch(Circle(d, 0.9, fc=color, ec=color, zorder=6))

    def gnd(self, x, y, down=True):
        """Ground symbol = solder to the 1.2 mm brass frame."""
        ax = self.ax
        s = -1 if down else 1
        ax.plot([x, x], [y, y + s * 2.2], color=W_BLACK, lw=2, zorder=2)
        for i, half in enumerate([2.0, 1.3, 0.6]):
            yy = y + s * (2.2 + i * 0.8)
            ax.plot([x - half, x + half], [yy, yy], color=BRASS_DARK, lw=1.8, zorder=2)

    def flag(self, x, y, text, color, side="R"):
        """Net label: same name = connected."""
        ax = self.ax
        L = 1.8
        ex = x + L if side == "R" else x - L
        ax.plot([x, ex], [y, y], color=color, lw=2.2, zorder=2)
        ax.text(ex + (0.5 if side == "R" else -0.5), y, text, ha="left" if side == "R" else "right", va="center",
                fontsize=7, weight="bold", color="white", zorder=6,
                bbox=dict(boxstyle="round,pad=0.25", fc=color, ec="none"))

    def resistor(self, x, y0, y1, label):
        ax = self.ax
        ym = (y0 + y1) / 2
        ax.plot([x, x], [y0, ym - 3], color=W_PURPLE, lw=2, zorder=2)
        ax.plot([x, x], [ym + 3, y1], color=W_PURPLE, lw=2, zorder=2)
        ax.add_patch(Rectangle((x - 1.2, ym - 3), 2.4, 6, fc="#e8d5a8", ec="#333", lw=1, zorder=3))
        for i, c in enumerate(["#7a4a1a", "#111", "#2b9348"]):
            ax.add_patch(Rectangle((x - 1.2, ym - 2 + i * 1.5), 2.4, 0.5, fc=c, ec="none", zorder=4))
        ax.text(x + 2, ym, label, fontsize=7, va="center", color=INK, zorder=4)

    def capacitor(self, x, y0, y1, label):
        ax = self.ax
        ym = (y0 + y1) / 2
        ax.plot([x, x], [y0, ym - 0.6], color=W_PURPLE, lw=2, zorder=2)
        ax.plot([x, x], [ym + 0.6, y1], color=W_PURPLE, lw=2, zorder=2)
        ax.plot([x - 1.6, x + 1.6], [ym - 0.6, ym - 0.6], color="#333", lw=2, zorder=3)
        ax.plot([x - 1.6, x + 1.6], [ym + 0.6, ym + 0.6], color="#333", lw=2, zorder=3)
        ax.text(x + 2, ym, label, fontsize=7, va="center", color=INK, zorder=4)

    def note(self, x, y, w, lines, title=None, fc="#fff8e1", ec=BRASS):
        ax = self.ax
        h = 3.2 * len(lines) + (5 if title else 2)
        ax.add_patch(FancyBboxPatch((x, y - h), w, h, boxstyle="round,pad=0,rounding_size=1.2", fc=fc, ec=ec,
                                    lw=1.2, zorder=1))
        yy = y - 3
        if title:
            ax.text(x + 2, yy, title, fontsize=8.5, weight="bold", color=INK, va="center", zorder=2)
            yy -= 4
        for ln in lines:
            ax.text(x + 2, yy, ln, fontsize=7, color=INK, va="center", zorder=2)
            yy -= 3.2

    def save(self, name):
        self.fig.savefig(out_path(name), facecolor=BG)
        plt.close(self.fig)
        print("wrote", name)


C3_LEFT = ["5", "6", "7", "8", "9", "10", "20", "21"]
C3_RIGHT = ["5V", "GND", "3V3", "4", "3", "2", "1", "0"]


def c3_board(s, x, y):
    """ESP32-C3 Super Mini, top view, USB-C up. 26 x 46 units."""
    pins = []
    for i, n in enumerate(C3_LEFT):
        pins.append((("G" + n), "L", 40 - i * 5))
    for i, n in enumerate(C3_RIGHT):
        pins.append(((n if not n.isdigit() else "G" + n), "R", 40 - i * 5))
    p = s.module(x, y, 26, 46, "ESP32-C3", "Super Mini", PCB_BLACK, pins)
    s.ax.add_patch(Rectangle((x + 9, y + 44), 8, 4, fc="#bbb", ec="#555", lw=1, zorder=4))
    s.ax.text(x + 13, y + 49.5, "USB-C (code)", ha="center", fontsize=6.5, color=INK)
    return p


XIAO_LEFT = [("G1", "D0"), ("G2", "D1"), ("G3", "D2"), ("G4", "D3"), ("G5", "D4"), ("G6", "D5"), ("G43", "D6")]
XIAO_RIGHT = ["5V", "GND", "3V3", "G9", "G8", "G7", "G44"]
XIAO_PAD = {"G9": "D10", "G8": "D9", "G7": "D8", "G44": "D7"}


def xiao_board(s, x, y, name="XIAO", sub="ESP32S3  8MB+8MB", pads=True):
    """Seeed XIAO ESP32S3, top view, USB-C up. Pins at the same heights as c3_board.
    pads=False: any 4 MB ESP32-S3 board with the same GPIOs, labelled by GPIO only."""
    pins = [((f"{g} {d}" if pads else g), "L", 40 - i * 5) for i, (g, d) in enumerate(XIAO_LEFT)]
    pins += [((f"{n} {XIAO_PAD[n]}" if pads and n in XIAO_PAD else n), "R", 40 - i * 5)
             for i, n in enumerate(XIAO_RIGHT)]
    p = s.module(x, y + 5, 26, 41, name, sub, PCB_BLACK, [(a, b, c - 5) for a, b, c in pins])
    s.ax.add_patch(Rectangle((x + 9, y + 44), 8, 4, fc="#bbb", ec="#555", lw=1, zorder=4))
    s.ax.text(x + 13, y + 49.5, "USB-C (code)", ha="center", fontsize=6.5, color=INK)
    # short names: "G2 D1" -> also reachable as "G2"
    for k in list(p):
        p[k.split(" ")[0]] = p[k]
    return p


S3MINI_LEFT = ["G43 TX", "G44 RX", "G1", "G2", "G3", "G4", "G5", "G6", "G7"]
S3MINI_RIGHT = ["5V", "GND", "3V3", "G13", "G12", "G11", "G10", "G9", "G8"]


def s3_supermini_board(s, x, y):
    """ESP32-S3 Super Mini (18 header pins, 4 MB flash + 2 MB PSRAM), top view, USB-C up."""
    pins = [(n, "L", 42 - i * 5) for i, n in enumerate(S3MINI_LEFT)]
    pins += [(n, "R", 42 - i * 5) for i, n in enumerate(S3MINI_RIGHT)]
    p = s.module(x, y, 26, 46, "ESP32-S3", "Super Mini  4MB+2MB", PCB_BLACK, pins)
    s.ax.add_patch(Rectangle((x + 9, y + 44), 8, 4, fc="#bbb", ec="#555", lw=1, zorder=4))
    s.ax.text(x + 13, y + 49.5, "USB-C (code)", ha="center", fontsize=6.5, color=INK)
    for k in list(p):
        p[k.split(" ")[0]] = p[k]
    return p


DEVKIT_LEFT = ["3V3", "3V3", "RST", "G4", "G5", "G6", "G7", "G15", "G16", "G17", "G18", "G8", "G3", "G46", "G9",
               "G10", "G11", "G12", "G13", "G14", "5V", "GND"]
DEVKIT_RIGHT = ["GND", "G43", "G44", "G1", "G2", "G42", "G41", "G40", "G39", "G38", "G37", "G36", "G35", "G0", "G45",
                "G48", "G47", "G21", "G20", "G19", "GND", "GND"]


def devkit_board(s, x, y):
    """ESP32-S3-DevKitC-1 (44 pins), top view, USB ports down. Header pins in board order."""
    pins = [(n, "L", 72.5 - i * 3.3) for i, n in enumerate(DEVKIT_LEFT)]
    pins += [(n, "R", 72.5 - i * 3.3) for i, n in enumerate(DEVKIT_RIGHT)]
    p = s.module(x, y, 26, 76, "ESP32-S3", "DevKitC-1  N8R8", PCB_BLACK, pins)
    for ux in (x + 4, x + 15):
        s.ax.add_patch(Rectangle((ux, y - 2.5), 7, 2.5, fc="#bbb", ec="#555", lw=1, zorder=4))
    s.ax.text(x + 7.5, y - 4.5, "UART", ha="center", fontsize=6, color=INK)
    s.ax.text(x + 18.5, y - 4.5, "USB", ha="center", fontsize=6, color=INK)
    return p


def power_chain(s, p, x0, sense="G1", v3_flag=False):
    """Battery, TP4056, slide switch, 3.3 V LDO and battery divider on the right of the board.
    sense: board pin for the battery divider, wired directly; None = net label BAT_SENSE.
    v3_flag: end the LDO output in a 3V3 net label instead of wiring it to the board's 3V3 pin."""
    ax = s.ax
    # TP4056 (pins on top edge)
    tp = s.module(x0 + 22, 30, 26, 18, "TP4056", "1A charger + protection", PCB_BLUE,
                  [("OUT+", "T", 3), ("B+", "T", 9.5), ("B-", "T", 16.5), ("OUT-", "T", 23)])
    ax.add_patch(Rectangle((x0 + 31, 27.5), 8, 2.5, fc="#bbb", ec="#555", lw=1, zorder=4))
    ax.text(x0 + 35, 25, "USB-C (charge)", ha="center", fontsize=6.5, color=INK)
    # battery, horizontal, + on the left
    bx, by = x0 + 22, 72
    ax.add_patch(FancyBboxPatch((bx, by), 28, 10, boxstyle="round,pad=0,rounding_size=2", fc="#5c7cfa", ec="#111",
                                lw=1.2, zorder=3))
    ax.add_patch(Rectangle((bx - 1.4, by + 3), 1.4, 4, fc="#ccc", ec="#111", lw=1, zorder=3))
    ax.text(bx + 14, by + 6.2, "3.7 V Li-ion 16340", ha="center", fontsize=8, weight="bold", color="white", zorder=4)
    ax.text(bx + 14, by + 2.8, "16 x 34 mm  (~700 mAh)", ha="center", fontsize=6.5, color="white", zorder=4)
    ax.text(bx + 2.2, by + 5, "+", fontsize=11, weight="bold", color="white", va="center", zorder=4)
    ax.text(bx + 25, by + 5, "-", fontsize=13, weight="bold", color="white", va="center", zorder=4)
    plus, minus = (bx - 1.4, by + 5), (bx + 28, by + 5)

    # battery -> TP4056 B+/B-
    s.wire([plus, (bx - 4, plus[1]), (bx - 4, 64), (tp["B+"][0], 64), tp["B+"]], W_RED)
    s.wire([minus, (minus[0] + 3, minus[1]), (minus[0] + 3, 61), (tp["B-"][0], 61), tp["B-"]], W_BLACK)
    # OUT- -> frame
    s.wire([tp["OUT-"], (tp["OUT-"][0], 54), (tp["OUT-"][0] + 5, 54)], W_BLACK)
    s.gnd(tp["OUT-"][0] + 5, 54)

    # slide switch on the BAT_SW rail
    sx, sy = x0 + 8, 66
    ax.add_patch(FancyBboxPatch((sx - 4, sy - 2.2), 8, 4.4, boxstyle="round,pad=0,rounding_size=0.8", fc="#ddd",
                                ec="#111", lw=1.1, zorder=3))
    ax.add_patch(Rectangle((sx - 1.8, sy - 1.2), 2, 2.4, fc="#444", zorder=4))
    ax.text(sx, sy + 3.8, "slide switch", ha="center", fontsize=6.8, color=INK)
    # OUT+ -> switch
    s.wire([tp["OUT+"], (tp["OUT+"][0], sy), (sx + 4, sy)], W_RED)
    # 3.3 V LDO between BAT_SW and the board's 3V3 pin (the board's own regulator is bypassed)
    lx = x0 - 16
    ldo = s.module(lx, sy - 2.5, 10, 7, "3.3V LDO", None, "#495057",
                   [("IN", "R", 2.5), ("OUT", "L", 2.5), ("G", "B", 5)])
    ax.texts[-4].set_fontsize(6.2)
    ax.text(lx + 5, sy + 5.6, "ME6211 / RT9080", ha="center", fontsize=6, color=INK)
    if not v3_flag:  # the DevKit's G2 label sits here; that sheet's pin map says it instead
        ax.text(lx + 5, sy + 8.2, "+ 1 µF on IN and OUT", ha="center", fontsize=6, color="#666")
    s.wire([(sx - 4, sy), ldo["IN"]], W_RED, dots=[(x0 + 1, sy)])
    ax.text(x0 + 1.8, sy - 2.2, "BAT_SW", fontsize=6.8, color=W_RED, weight="bold", ha="left")
    if v3_flag:
        s.wire([ldo["OUT"], (lx - 1.5, sy), (lx - 1.5, sy - 6)], W_ORANGE)
        s.flag(lx - 1.5, sy - 6, "3V3", W_ORANGE)
    else:
        vx = p["3V3"][0] + 3
        s.wire([ldo["OUT"], (vx, sy), (vx, p["3V3"][1]), p["3V3"]], W_ORANGE)
        ax.text(vx + 0.6, (sy + p["3V3"][1]) / 2, "3V3", fontsize=6.5, color=W_ORANGE, weight="bold", va="center")
        ax.text(p["5V"][0] + 0.6, p["5V"][1] + 1.5, "nc", fontsize=5.8, color="#777")
    s.gnd(ldo["G"][0], ldo["G"][1])

    # divider: BAT_SW -> 1M -> sense pin -> 330k -> GND, 100nF across 330k
    dx = x0 + 1
    ny = 36  # divider middle
    s.resistor(dx, ny, sy, "1 MΩ")
    if sense:
        s.wire([p[sense], (dx, ny)], W_PURPLE, dots=[(dx, ny)])
    else:
        ax.add_patch(Circle((dx, ny), 0.9, fc=W_PURPLE, ec=W_PURPLE, zorder=6))
        s.flag(dx, ny + 4, "BAT_SENSE", W_PURPLE, side="L")
        s.wire([(dx, ny), (dx, ny + 4)], W_PURPLE)
    s.resistor(dx, 18, ny, "330 kΩ")
    s.capacitor(dx + 9, 18, ny, "100 nF")
    s.wire([(dx, ny), (dx + 9, ny)], W_PURPLE, dots=[(dx + 9, ny)])
    s.wire([(dx, 18), (dx + 9, 18)], W_BLACK)
    s.gnd(dx + 4.5, 18)
    return tp, (sx, sy)


def touch_wiring():
    s = Sheet("COCO TOUCH  -  circuit diagram",
              "ESP32-C3 Super Mini  +  0.96\" SSD1306 OLED  +  TTP223 touch  +  TP4056  +  16340 Li-ion", h=120)
    p = c3_board(s, 58, 26)

    oled = s.module(14, 84, 30, 16, "0.96\" OLED", "SSD1306 128x64 I2C", PCB_BLUE,
                    [("GND", "B", 5), ("VCC", "B", 11.5), ("SCL", "B", 18.5), ("SDA", "B", 25)])
    s.gnd(oled["GND"][0], oled["GND"][1])
    s.wire([oled["VCC"], (oled["VCC"][0], 79)], W_ORANGE)
    s.flag(oled["VCC"][0], 79, "3V3", W_ORANGE)
    s.wire([oled["SCL"], (oled["SCL"][0], 66), p["G5"]], W_YELLOW)
    s.wire([oled["SDA"], (oled["SDA"][0], 80), (101.5, 80), (101.5, p["G4"][1]), p["G4"]], W_BLUE)

    ttp = s.module(14, 40, 22, 15, "TTP223", "touch pad 15x11", PCB_RED,
                   [("VCC", "B", 4), ("I/O", "B", 11), ("GND", "B", 18)])
    s.wire([ttp["VCC"], (ttp["VCC"][0], 35)], W_ORANGE)
    s.flag(ttp["VCC"][0], 35, "3V3", W_ORANGE, side="L")
    s.gnd(ttp["GND"][0], ttp["GND"][1])
    s.wire([ttp["I/O"], (ttp["I/O"][0], 17), (92, 17), (92, p["G0"][1]), p["G0"]], W_GREEN)

    s.wire([p["GND"], (p["GND"][0] + 1.3, p["GND"][1])], W_BLACK)
    s.gnd(p["GND"][0] + 1.3, p["GND"][1])
    power_chain(s, p, 104)

    s.note(118, 112, 50, ["GND symbol = solder to the 1.2 mm brass", "      frame (the whole frame is GND)",
                          "coloured lines = 0.9 mm brass wire", "dot = joint, crossing = no joint",
                          "same label (3V3) = same net"], title="How to read")
    s.note(118, 22, 50, ["G0  TTP223 I/O   (HIGH = touched, wakes)", "G1  battery sense (1M / 330k)",
                         "G4  OLED SDA        G5  OLED SCL", "G8  on-board LED   G9  BOOT button",
                         "3V3 <- 3.3 V LDO (BAT_SW in); 5V pin unused"], title="Pin map", fc="#eef6ff", ec=W_BLUE)
    s.note(4, 13, 112, ["Switch OFF before plugging the C3's own USB-C (code): its USB regulator also drives 3V3.",
                        "TP4056 charges at 1 A by default: swap its PROG resistor (R3, 1.2k) for 3.3k (~350 mA) for a small 16340 cell."],
           fc="#fff0f0", ec=W_RED)
    s.save("coco_touch_wiring.png")


def voice_wiring():
    s = Sheet("COCO VOICE C3  -  circuit diagram",
              "Coco Touch  +  INMP441 I2S mic  +  MAX98357A I2S amp  +  20 mm speaker   (XiaoZhi voice AI)",
              h=124)
    ax = s.ax
    p = c3_board(s, 58, 26)

    oled = s.module(14, 92, 30, 16, "0.96\" OLED", "SSD1306 I2C", PCB_BLUE,
                    [("GND", "B", 5), ("VCC", "B", 11.5), ("SCL", "B", 18.5), ("SDA", "B", 25)])
    s.gnd(oled["GND"][0], oled["GND"][1])
    s.wire([oled["VCC"], (oled["VCC"][0], 87)], W_ORANGE)
    s.flag(oled["VCC"][0], 87, "3V3", W_ORANGE)
    s.wire([oled["SCL"], (oled["SCL"][0], 82), (54, 82), (54, p["G5"][1]), p["G5"]], W_YELLOW)
    s.wire([oled["SDA"], (oled["SDA"][0], 88), (101.5, 88), (101.5, p["G4"][1]), p["G4"]], W_BLUE)

    # INMP441
    mic = s.module(10, 58, 24, 18, "INMP441", "I2S mic", PCB_PURPLE,
                   [("SD", "R", 15), ("SCK", "R", 11.5), ("WS", "R", 8), ("L/R", "R", 4.5), ("VDD", "L", 12),
                    ("GND", "L", 5)])
    s.wire([mic["SCK"], (46, mic["SCK"][1]), (46, p["G6"][1]), p["G6"]], W_CYAN, dots=[(46, p["G6"][1])])
    s.wire([mic["WS"], (43, mic["WS"][1]), (43, p["G7"][1]), p["G7"]], W_CYAN, lw=1.6, dots=[(43, p["G7"][1])])
    s.flag(mic["SD"][0], mic["SD"][1], "MIC_SD (G3)", W_PINK)
    s.wire([mic["L/R"], (36, mic["L/R"][1]), (36, 55)], W_BLACK)
    s.gnd(36, 55)
    s.flag(mic["VDD"][0], mic["VDD"][1], "3V3", W_ORANGE, side="L")
    s.wire([mic["GND"], (6, mic["GND"][1]), (6, 54)], W_BLACK)
    s.gnd(6, 54)

    # MAX98357A
    amp = s.module(10, 19, 24, 30, "MAX98357A", "I2S 3W amp", PCB_PURPLE,
                   [("LRC", "R", 26), ("BCLK", "R", 22), ("DIN", "R", 18), ("GAIN", "R", 14), ("SD", "R", 10),
                    ("GND", "R", 6), ("VIN", "R", 2.5), ("OUT+", "L", 20), ("OUT-", "L", 12)])
    s.wire([amp["BCLK"], (46, amp["BCLK"][1]), (46, p["G6"][1])], W_CYAN)
    s.wire([amp["LRC"], (43, amp["LRC"][1]), (43, p["G7"][1])], W_CYAN, lw=1.6)
    s.wire([amp["DIN"], (49, amp["DIN"][1]), (49, p["G10"][1]), p["G10"]], W_PINK)
    s.wire([amp["SD"], (51, amp["SD"][1]), (51, p["G20"][1]), p["G20"]], W_GREY)
    ax.text(36, amp["GAIN"][1], "nc (9 dB)", fontsize=6, color="#777", va="center")
    s.wire([amp["GND"], (39, amp["GND"][1]), (39, 18)], W_BLACK)
    s.gnd(39, 18)
    s.flag(amp["VIN"][0], amp["VIN"][1], "BAT_SW", W_RED)
    # speaker
    ax.add_patch(Circle((-0.5 + 3, 32), 0.1, fc=BG))
    spk_x, spk_y = 3.5, 32
    ax.add_patch(Circle((spk_x, spk_y), 3.2, fc="#444", ec="#111", lw=1.2, zorder=3))
    ax.add_patch(Circle((spk_x, spk_y), 1.5, fc="#888", ec="#111", lw=0.8, zorder=4))
    ax.text(spk_x, spk_y - 5.5, "20 mm\nspeaker\n8Ω 1W", ha="center", va="top", fontsize=6, color=INK)
    s.wire([amp["OUT+"], (spk_x + 2.4, amp["OUT+"][1]), (spk_x + 1.6, spk_y + 2.6)], W_RED, lw=1.6)
    s.wire([amp["OUT-"], (spk_x + 2.4, amp["OUT-"][1]), (spk_x + 1.6, spk_y - 2.6)], W_BLACK, lw=1.6)

    # TTP223 with net labels
    s.flag(p["G3"][0], p["G3"][1], "MIC_SD (G3)", W_PINK)
    ttp = s.module(58, 93, 18, 14, "TTP223", "touch", PCB_RED, [("VCC", "R", 11), ("I/O", "R", 7), ("GND", "R", 3)])
    s.flag(ttp["VCC"][0], ttp["VCC"][1], "3V3", W_ORANGE)
    s.flag(ttp["I/O"][0], ttp["I/O"][1], "TOUCH (G0)", W_GREEN)
    s.wire([ttp["GND"], (ttp["GND"][0] + 3, ttp["GND"][1])], W_BLACK)
    s.gnd(ttp["GND"][0] + 3, ttp["GND"][1])
    s.flag(p["G0"][0], p["G0"][1], "TOUCH (G0)", W_GREEN)

    s.wire([p["GND"], (p["GND"][0] + 1.3, p["GND"][1])], W_BLACK)
    s.gnd(p["GND"][0] + 1.3, p["GND"][1])
    power_chain(s, p, 104)

    s.note(118, 22, 50, ["G0 touch   G1 battery   G4 SDA   G5 SCL", "G6 I2S BCLK  (mic SCK + amp BCLK)",
                         "G7 I2S WS    (mic WS  + amp LRC)", "G3 mic SD     G10 amp DIN   G20 amp SD",
                         "mic L/R -> GND (left channel)"], title="Pin map", fc="#eef6ff", ec=W_BLUE)
    s.note(118, 116, 50, ["GND symbol = solder to the brass frame", "coloured = 0.9 mm brass wire",
                          "same label = same net, dot = joint", "keep I2S wires short and apart"],
           title="How to read")
    s.note(4, 10, 112, ["Mic faces out of the side, speaker faces up: keep them apart.  Amp runs from BAT_SW (3.0-4.2 V), "
                        "not 3V3.  LDO: >= 500 mA, low dropout (not AMS1117).  Switch OFF before using USB."],
           fc="#fff0f0", ec=W_RED)
    s.save("coco_voice_c3_wiring.png")


def voice_s3_wiring(four_mb=False, devkit=False):
    """four_mb: the simple build (coco-s3-4mb) for 4 MB ESP32-S3 boards with a 1.3" SH1106 OLED, "Hi ESP".
    devkit: the full-size ESP32-S3-DevKitC-1 (N8R8 / N16R8), same firmware as the XIAO."""
    if devkit:
        s = Sheet("COCO VOICE S3 DevKitC  -  circuit diagram  (\"Hey Coco\")",
                  "ESP32-S3-DevKitC-1 N8R8 / N16R8 (8+ MB flash, 8 MB PSRAM)  +  INMP441  +  MAX98357A  +  20 mm speaker"
                  "  +  OLED  +  TTP223", h=124)
        p = devkit_board(s, 58, 16)
    elif four_mb:
        s = Sheet("COCO VOICE S3 4MB  -  circuit diagram  (\"Hi ESP\")",
                  "ESP32-S3 Super Mini (4 MB flash + 2 MB PSRAM)  +  INMP441  +  MAX98357A  +  20 mm speaker"
                  "  +  1.3\" OLED  +  TTP223", h=124)
        p = s3_supermini_board(s, 58, 26)
    else:
        s = Sheet("COCO VOICE S3  -  circuit diagram  (\"Hey Coco\")",
                  "Seeed XIAO ESP32S3 (8 MB flash + 8 MB PSRAM)  +  INMP441  +  MAX98357A  +  20 mm speaker  +  OLED"
                  "  +  TTP223", h=124)
        p = xiao_board(s, 58, 26)
    ax = s.ax
    # every signal by net label: name (pin) at the board, same name at the module
    nets = {"G1": ("TOUCH", W_GREEN), "G2": ("BAT_SENSE", W_PURPLE), "G4": ("AMP_DIN", W_PINK),
            "G5": ("SDA", W_BLUE), "G6": ("SCL", W_YELLOW), "G43": ("AMP_SD", W_GREY),
            "G9": ("MIC_SD", W_PINK), "G8": ("WS", W_CYAN), "G7": ("BCLK", W_CYAN)}
    for pin, (net, col) in nets.items():
        x, y = p[pin]
        s.flag(x, y, net, col, side="L" if x < 70 else "R")
    if devkit:
        s.flag(*p["3V3"], "3V3", W_ORANGE, side="L")
        ax.text(p["5V"][0] - 1.5, p["5V"][1], "nc", fontsize=5.8, color="#777", ha="right", va="center")
    s.wire([p["GND"], (p["GND"][0] + 1.3, p["GND"][1])], W_BLACK)
    s.gnd(p["GND"][0] + 1.3, p["GND"][1])

    oled = s.module(14, 94, 30, 14, "1.3\" OLED" if four_mb else "0.96\" OLED",
                    "SH1106 I2C" if four_mb else "SSD1306 I2C", PCB_BLUE,
                    [("GND", "B", 5), ("VCC", "B", 11.5), ("SCL", "B", 18.5), ("SDA", "B", 25)])
    s.gnd(oled["GND"][0], oled["GND"][1])
    for pin, net, col, dy in [("VCC", "3V3", W_ORANGE, 8), ("SCL", "SCL", W_YELLOW, 4.5), ("SDA", "SDA", W_BLUE, 1)]:
        x, y = oled[pin]
        s.wire([(x, y), (x, y - dy)], col)
        s.flag(x, y - dy, net, col)
    ttp = s.module(66, 94, 18, 14, "TTP223", "touch", PCB_RED, [("VCC", "R", 11), ("I/O", "R", 7), ("GND", "R", 3)])
    s.flag(ttp["VCC"][0], ttp["VCC"][1], "3V3", W_ORANGE)
    s.flag(ttp["I/O"][0], ttp["I/O"][1], "TOUCH", W_GREEN)
    s.wire([ttp["GND"], (ttp["GND"][0] + 3, ttp["GND"][1])], W_BLACK)
    s.gnd(ttp["GND"][0] + 3, ttp["GND"][1])

    mic = s.module(6, 60, 22, 18, "INMP441", "I2S mic", PCB_PURPLE,
                   [("SD", "R", 15), ("SCK", "R", 11.5), ("WS", "R", 8), ("L/R", "R", 4.5), ("VDD", "L", 12),
                    ("GND", "L", 5)])
    for pin, net, col in [("SD", "MIC_SD", W_PINK), ("SCK", "BCLK", W_CYAN), ("WS", "WS", W_CYAN)]:
        s.flag(*mic[pin], net, col)
    s.wire([mic["L/R"], (31, mic["L/R"][1]), (31, 60)], W_BLACK)
    s.gnd(31, 60)
    s.flag(*mic["VDD"], "3V3", W_ORANGE, side="L")
    s.wire([mic["GND"], (3, mic["GND"][1]), (3, 58)], W_BLACK)
    s.gnd(3, 58)

    amp = s.module(10, 19, 22, 30, "MAX98357A", "I2S amp", PCB_PURPLE,
                   [("LRC", "R", 26), ("BCLK", "R", 22), ("DIN", "R", 18), ("GAIN", "R", 14), ("SD", "R", 10),
                    ("GND", "R", 6), ("VIN", "R", 2.5), ("OUT+", "L", 20), ("OUT-", "L", 12)])
    for pin, net, col in [("LRC", "WS", W_CYAN), ("BCLK", "BCLK", W_CYAN), ("DIN", "AMP_DIN", W_PINK),
                          ("SD", "AMP_SD", W_GREY), ("VIN", "BAT_SW", W_RED)]:
        s.flag(*amp[pin], net, col)
    ax.text(34, amp["GAIN"][1], "nc (9 dB)", fontsize=6, color="#777", va="center")
    s.wire([amp["GND"], (amp["GND"][0] + 3, amp["GND"][1])], W_BLACK)
    s.gnd(amp["GND"][0] + 3, amp["GND"][1])
    spk_x, spk_y = 3.5, 32
    ax.add_patch(Circle((spk_x, spk_y), 3.2, fc="#444", ec="#111", lw=1.2, zorder=3))
    ax.add_patch(Circle((spk_x, spk_y), 1.5, fc="#888", ec="#111", lw=0.8, zorder=4))
    ax.text(spk_x, spk_y - 5.5, "20 mm\nspeaker", ha="center", va="top", fontsize=6, color=INK)
    s.wire([amp["OUT+"], (spk_x + 2.4, amp["OUT+"][1]), (spk_x + 1.6, spk_y + 2.6)], W_RED, lw=1.6)
    s.wire([amp["OUT-"], (spk_x + 2.4, amp["OUT-"][1]), (spk_x + 1.6, spk_y - 2.6)], W_BLACK, lw=1.6)

    power_chain(s, p, 104, sense=None, v3_flag=devkit)
    if devkit:
        s.note(118, 22, 50, ["G1 touch          G2 battery sense", "G4 amp DIN        G43 amp SD (TX)",
                             "G5 SDA            G6 SCL", "G7 BCLK   G8 WS   G9 mic SD",
                             "3V3 <- LDO (+ 1 µF on IN and OUT)", "5V unused   mic L/R -> GND"],
               title="Pin map (GPIO number)", fc="#eef6ff", ec=W_BLUE)
        s.note(118, 116, 50, ["GND symbol = solder to the brass frame", "same label = same net (wire them together)",
                              "coloured = 0.9 mm brass wire", "too big for the brass case: desk / breadboard build"],
               title="How to read")
        s.note(4, 10, 112, ["Same firmware as the XIAO (\"Hey Coco\").  Needs 8 MB flash + PSRAM: N8R8 or N16R8, "
                            "not N8 or N4R2.  Switch OFF before using USB."], fc="#fff0f0", ec=W_RED)
        s.save("coco_voice_s3_devkit_wiring.png")
        return
    if four_mb:
        s.note(118, 22, 50, ["G1 touch          G2 battery sense", "G4 amp DIN        TX (G43) amp SD",
                             "G5 SDA            G6 SCL", "G7 BCLK   G8 WS   G9 mic SD",
                             "3V3 <- LDO   5V unused   mic L/R -> GND"], title="Pin map (GPIO number)",
               fc="#eef6ff", ec=W_BLUE)
        s.note(118, 116, 50, ["GND symbol = solder to the brass frame", "same label = same net (wire them together)",
                              "coloured = 0.9 mm brass wire", "numbers = the GPIO numbers printed on the board"],
               title="How to read")
        s.note(4, 10, 112, ["4 MB flash: wake word \"Hi ESP\" (or double-tap).  \"Hey Coco\" needs 8 MB.  "
                            "Check the 1.3\" OLED pin order: some are VCC-GND.  Switch OFF before using USB."],
               fc="#fff0f0", ec=W_RED)
        s.save("coco_voice_s3_hw747_esp32s3_wiring.png")
        return
    s.note(118, 22, 50, ["D0 G1 touch      D1 G2 battery sense", "D3 G4 amp DIN   D6 G43 amp SD",
                         "D4 G5 SDA        D5 G6 SCL", "D8 G7 BCLK   D9 G8 WS   D10 G9 mic SD",
                         "3V3 <- LDO   5V unused   mic L/R -> GND"], title="Pin map (XIAO pad / GPIO)", fc="#eef6ff",
           ec=W_BLUE)
    s.note(118, 116, 50, ["GND symbol = solder to the brass frame", "same label = same net (wire them together)",
                          "coloured = 0.9 mm brass wire", "XIAO is smaller than the C3: same case"],
           title="How to read")
    s.note(4, 10, 112, ["Needs the PSRAM XIAO ESP32S3 (8 MB flash) for \"Hey Coco\".  ESP32-S3 Super Minis with 4 MB flash "
                        "are too small.  Switch OFF before using USB."], fc="#fff0f0", ec=W_RED)
    s.save("coco_voice_s3_wiring.png")


# =========================================================== brass cases
# Coordinates in mm: x = width (left->right), y = height (up), z = depth (front->back).

def box(x0, y0, z0, w, h, d):
    x1, y1, z1 = x0 + w, y0 + h, z0 + d
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1),
         (x0, y1, z1)]
    faces = [(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (3, 2, 6, 7), (0, 3, 7, 4), (1, 2, 6, 5)]
    return [[v[i] for i in f] for f in faces]


class Design:
    """One Coco case: frame bars (1.2 mm), posts (0.9 mm) and parts."""

    def __init__(self, name, W, H, D):
        self.name, self.W, self.H, self.D = name, W, H, D
        self.bars = []    # (p0, p1, step)   step = assembly step where it appears
        self.rings = []   # (cx, y, cz, r, step)  battery clips, horizontal circles
        self.parts = []   # dict(kind, label, x, y, z, w, h, d, color, step, ...)

    def rect_front(self, z, step=1):
        W, H = self.W, self.H
        pts = [(0, 0, z), (W, 0, z), (W, H, z), (0, H, z), (0, 0, z)]
        for a, b in zip(pts, pts[1:]):
            self.bars.append((a, b, step))

    def bar(self, a, b, step):
        self.bars.append((a, b, step))

    def part(self, step, kind, label, x, y, z, w, h, d, color, **kw):
        self.parts.append(dict(step=step, kind=kind, label=label, x=x, y=y, z=z, w=w, h=h, d=d, color=color, **kw))


BAT_D, BAT_L = 16.0, 34.0


def touch_design():
    d = Design("COCO TOUCH", 52, 40, 20)
    d.slug = "coco_touch"
    W, H, D = d.W, d.H, d.D
    d.rect_front(0)
    d.rect_front(D)
    for x in (0, W):
        for y in (0, H):
            d.bar((x, y, 0), (x, y, D), 1)
    d.bar((19.5, 0, 0), (19.5, H, 0), 1)            # front divider between battery and screen
    d.bar((19.5, 0, D), (19.5, H, D), 1)            # back divider
    # step 2: seats
    for z in (6, 14):
        d.bar((19.5, 0, z), (W, 0, z), 2)           # floor bars (TP4056 seat)
        d.bar((19.5, H, z), (W, H, z), 2)           # top bars (TTP223 seat)
    d.rings += [(10, 7, 10, BAT_D / 2 + 0.6, 2), (10, 33, 10, BAT_D / 2 + 0.6, 2)]
    for y in (7, 33):                                # ring tabs to the frame
        d.bar((10, y, 1.4), (10, y, 0), 2)
        d.bar((1.4, y, 10), (0, y, 10), 2)
    # parts
    d.part(3, "box", "TP4056", 31, 0.6, 3.5, 17, 3.6, 12, PCB_BLUE, port=("x+", "USB-C charge"))
    d.part(3, "box", "switch", W - 0.6, 10, 5, 3.6, 4.0, 8.6, "#cfcfcf", knob=True)
    d.part(4, "box", "ESP32-C3", 24, 19, D - 4.2, 22.5, 18, 3.6, PCB_BLACK, port=("x+", "USB-C code"))
    d.part(4, "box", "LDO", 27, 11, D - 2.6, 4.5, 2.5, 1.6, "#495057")
    d.part(5, "oled", "OLED", 21.5, 6, 0.6, 27.3, 27.8, 4.1, PCB_BLUE)
    d.part(6, "box", "TTP223", 27, H + 0.6, 4.5, 15, 1.6, 11, PCB_RED, pad=True)
    d.part(6, "battery", "16340", 10 - BAT_D / 2, 3, 10 - BAT_D / 2, BAT_D, BAT_L, BAT_D, "#5c7cfa")
    d.steps = [
        ("Bend the cage", "Two 52 x 40 mm rectangles + a 40 mm divider each (1.2 mm). Join with four 20 mm struts."),
        ("Seats + battery clips", "Floor and top bars at z = 6 / 14 mm. Two 17 mm rings hold the 16340 cell."),
        ("Charger + switch", "TP4056 on the floor bars: B- and OUT- pads straight onto the frame (GND). Switch on the right."),
        ("ESP32-C3 + LDO", "C3 GND pad onto the back bar, USB-C right. 3.3 V LDO below it: switch -> LDO IN, LDO OUT -> 3V3."),
        ("OLED on the front", "Four 0.9 mm posts through its holes. SDA/SCL/VCC up the back to G4/G5/3V3."),
        ("Touch pad + battery", "TTP223 on the top bars (I/O -> G0). Battery in, check + / -, switch on, test."),
    ]
    return d


def voice_design(s3=False):
    d = Design("COCO VOICE S3" if s3 else "COCO VOICE C3", 56, 44, 30)
    d.slug = "coco_voice_s3" if s3 else "coco_voice_c3"
    W, H, D = d.W, d.H, d.D
    d.rect_front(0)
    d.rect_front(D)
    for x in (0, W):
        for y in (0, H):
            d.bar((x, y, 0), (x, y, D), 1)
    d.bar((19.5, 0, 0), (19.5, H, 0), 1)
    d.bar((19.5, 0, D), (19.5, H, D), 1)
    for z in (7, 20):
        d.bar((19.5, 0, z), (W, 0, z), 2)
    d.bar((0, H, 13), (19.5, H, 13), 2)              # TTP223 seat over the battery
    d.bar((0, H, 3), (19.5, H, 3), 2)
    for z in (12, 16, 20, 24):                       # speaker grille (0.9 mm, on top)
        d.bar((22, H, z), (W, H, z), 2)
    d.rings += [(10, 8, 10, BAT_D / 2 + 0.6, 2), (10, 34, 10, BAT_D / 2 + 0.6, 2)]
    for y in (8, 34):
        d.bar((10, y, 1.4), (10, y, 0), 2)
        d.bar((1.4, y, 10), (0, y, 10), 2)
    d.part(3, "box", "TP4056", 42, 0.6, 11.5, 12, 3.6, 17, PCB_BLUE, port=("z+", "USB-C charge"))
    d.part(3, "box", "MAX98357A", 21, 0.6, 3.5, 19.3, 3.0, 17.8, PCB_PURPLE)
    d.part(3, "box", "switch", -3.0, 10, 21, 3.6, 4.0, 8.6, "#cfcfcf", knob=True)
    if s3:
        d.part(4, "box", "XIAO S3", 27, 14, D - 4.2, 21, 17.8, 3.4, PCB_BLACK, port=("x+", "USB-C code"))
    else:
        d.part(4, "box", "ESP32-C3", 26, 14, D - 4.2, 22.5, 18, 3.6, PCB_BLACK, port=("x+", "USB-C code"))
    d.part(4, "box", "LDO", 29, 8, D - 2.6, 4.5, 2.5, 1.6, "#495057")
    d.part(4, "box", "INMP441", W - 2.2, 20, 5, 1.6, 14, 14, PCB_PURPLE, mic=True)
    d.part(5, "oled", "OLED", 22, 8, 0.6, 27.3, 27.8, 4.1, PCB_BLUE)
    d.part(5, "speaker", "speaker", 39 - 10, H - 5.2, 18 - 10, 20, 4.6, 20, "#444")
    d.part(6, "box", "TTP223", 2.5, H + 0.6, 2.5, 15, 1.6, 11, PCB_RED, pad=True)
    d.part(6, "battery", "16340", 10 - BAT_D / 2, 4, 10 - BAT_D / 2, BAT_D, BAT_L, BAT_D, "#5c7cfa")
    d.steps = [
        ("Bend the cage", "Two 56 x 44 mm rectangles + a 44 mm divider each (1.2 mm). Join with four 30 mm struts."),
        ("Seats, clips, grille", "Floor bars, TTP223 seat over the battery, four 0.9 mm grille bars over the speaker."),
        ("Charger, amp, switch", "TP4056 (USB-C to the back) and MAX98357A on the floor bars, GND pads to the frame."),
        (("XIAO S3" if s3 else "ESP32-C3") + ", LDO, mic", ("XIAO" if s3 else "C3") + " and the 3.3 V LDO on the back bars. "
         "INMP441 on the right side, sound hole facing OUT, away from the speaker."),
        ("OLED + speaker", "OLED on four posts. Speaker under the grille, facing up; OUT+/OUT- from the amp."),
        ("Touch pad + battery", "TTP223 on top of the battery bay. Battery in, check polarity, switch on, flash, test."),
    ]
    return d


def cylinder_surface(cx, y0, cz, r, L, n=28):
    th = np.linspace(0, 2 * np.pi, n)
    yy = np.array([y0, y0 + L])
    T, Y = np.meshgrid(th, yy)
    return cx + r * np.cos(T), Y, cz + r * np.sin(T)


def draw3d(ax, d, upto=99, highlight=None, show_labels=True):
    """Draws design d into a 3D axis. Plot axes: X = width, Y = depth, Z = height."""
    ax.set_proj_type("ortho")
    m = 6
    ax.set_xlim(-m, d.W + m)
    ax.set_ylim(-m, d.D + m)
    ax.set_zlim(-m, d.H + m)
    ax.set_box_aspect((d.W + 2 * m, d.D + 2 * m, d.H + 2 * m))
    ax.view_init(elev=22, azim=-58)
    ax.axis("off")
    ax.computed_zorder = False  # draw in our own back-to-front order

    def P(p):
        return (p[0], p[2], p[1])

    # parts, back (large z) to front
    for zo, pt in enumerate(sorted(d.parts, key=lambda q: -(q["z"] + q["d"] / 2)), start=10):
        if pt["step"] > upto:
            continue
        hl = highlight is not None and pt["step"] == highlight
        alpha = 0.95 if (highlight is None or hl) else 0.35
        x, y, z, w, h, dd = pt["x"], pt["y"], pt["z"], pt["w"], pt["h"], pt["d"]
        if pt["kind"] == "battery":
            r = w / 2
            X, Y, Z = cylinder_surface(x + r, y, z + r, r, h)
            ax.plot_surface(X, Z, Y, color=pt["color"], alpha=alpha, linewidth=0, shade=True, zorder=zo)
            continue
        if pt["kind"] == "speaker":
            r = w / 2
            X, Y, Z = cylinder_surface(x + r, y, z + r, r, h)
            ax.plot_surface(X, Z, Y, color="#555", alpha=alpha, linewidth=0, shade=True, zorder=zo)
            th = np.linspace(0, 2 * np.pi, 40)
            ax.plot(x + r + r * 0.55 * np.cos(th), z + r + r * 0.55 * np.sin(th), np.full(40, y + h), color="#bbb",
                    lw=1, alpha=alpha, zorder=zo)
            continue
        faces = [[P(v) for v in f] for f in box(x, y, z, w, h, dd)]
        pc = Poly3DCollection(faces, facecolor=pt["color"], edgecolor="#111", linewidths=0.4, alpha=alpha, zorder=zo)
        ax.add_collection3d(pc)
        if pt["kind"] == "oled":
            # glass + Coco's eyes on the front face
            g = [P(v) for v in [(x + 1.5, y + 6, z - 0.05), (x + w - 1.5, y + 6, z - 0.05),
                                 (x + w - 1.5, y + h - 4, z - 0.05), (x + 1.5, y + h - 4, z - 0.05)]]
            ax.add_collection3d(Poly3DCollection([g], facecolor="#081218", edgecolor="#000", alpha=alpha,
                                                    zorder=zo + 0.3))
            for ex in (x + w * 0.3, x + w * 0.7):
                e = [P(v) for v in [(ex - 3.6, y + 11, z - 0.1), (ex + 3.6, y + 11, z - 0.1),
                                     (ex + 3.6, y + 20, z - 0.1), (ex - 3.6, y + 20, z - 0.1)]]
                ax.add_collection3d(Poly3DCollection([e], facecolor="#28e1eb", edgecolor="none", alpha=alpha,
                                                        zorder=zo + 0.6))
        if pt.get("pad"):
            th = np.linspace(0, 2 * np.pi, 30)
            ax.plot(x + w / 2 + 3.5 * np.cos(th), z + dd / 2 + 3.5 * np.sin(th), np.full(30, y + h + 0.05),
                    color="#ffd6d6", lw=1.2, alpha=alpha, zorder=zo + 0.5)

    # frame: bars on the back face go behind the parts, the rest in front
    hl1 = highlight in (None, 1)
    hl2 = highlight in (None, 2)
    groups = {}
    for a, b, step in d.bars:
        if step > upto:
            continue
        back = a[2] == d.D and b[2] == d.D
        groups.setdefault((step == 1, back), []).append([P(a), P(b)])
    for (main, back), segs in groups.items():
        on = hl1 if main else hl2
        lw = (3.2 if on else 1.8) if main else (2.4 if on else 1.4)
        ax.add_collection3d(Line3DCollection(segs, colors=BRASS, linewidths=lw, alpha=1 if on else 0.6,
                                             zorder=1 if back else 100))
    for cx, y, cz, r, step in d.rings:
        if step > upto:
            continue
        th = np.linspace(0, 2 * np.pi, 48)
        ax.plot(cx + r * np.cos(th), cz + r * np.sin(th), np.full(48, y), color=BRASS, lw=2.2 if hl2 else 1.3,
                alpha=1 if hl2 else 0.6, zorder=100)

    if show_labels:
        for pt in d.parts:
            if pt["step"] > upto or (highlight is not None and pt["step"] != highlight):
                continue
            x = pt["x"] + pt["w"] / 2
            y = pt["y"] + pt["h"] + 1.5
            z = pt["z"] + pt["d"] / 2
            ax.text(x, z, y, pt["label"], fontsize=7.5, weight="bold", color=INK, ha="center", zorder=50,
                    bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="#999", lw=0.5, alpha=0.85))
            ax.texts[-1].set_zorder(200)


def dim(ax, a, b, off, text, vertical=False):
    """Dimension line between a and b (2D), offset perpendicular by off."""
    (x0, y0), (x1, y1) = a, b
    if vertical:
        xs = [x0 + off, x1 + off]
        ax.annotate("", xy=(xs[1], y1), xytext=(xs[0], y0),
                    arrowprops=dict(arrowstyle="<->", color="#c92a2a", lw=1))
        ax.plot([x0, x0 + off], [y0, y0], color="#c92a2a", lw=0.5, ls=":")
        ax.plot([x1, x1 + off], [y1, y1], color="#c92a2a", lw=0.5, ls=":")
        ax.text(xs[0] + (1.2 if off > 0 else -1.2), (y0 + y1) / 2, text, rotation=90, ha="center", va="center",
                fontsize=8, color="#c92a2a", backgroundcolor=BG)
    else:
        ys = [y0 + off, y1 + off]
        ax.annotate("", xy=(x1, ys[1]), xytext=(x0, ys[0]),
                    arrowprops=dict(arrowstyle="<->", color="#c92a2a", lw=1))
        ax.plot([x0, x0], [y0, y0 + off], color="#c92a2a", lw=0.5, ls=":")
        ax.plot([x1, x1], [y1, y1 + off], color="#c92a2a", lw=0.5, ls=":")
        ax.text((x0 + x1) / 2, ys[0] + (1.4 if off > 0 else -1.6), text, ha="center", va="center", fontsize=8,
                color="#c92a2a", backgroundcolor=BG)


def view2d(ax, d, view, title):
    """Orthographic view. view: 'front' (x,y), 'top' (x,z), 'side' (z,y) from the right."""
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title(title, fontsize=11, weight="bold", color=INK, pad=4)

    def proj(p):
        x, y, z = p
        return {"front": (x, y), "top": (x, d.D - z), "side": (z, y)}[view]

    # parts first (behind the frame lines)
    order = sorted(d.parts, key=lambda pt: {"front": -pt["z"], "top": pt["y"], "side": pt["x"]}[view])
    ax.text(0, -11, "dashed = behind other parts in this view", fontsize=6.5, color="#777")
    for pt in order:
        x, y, z, w, h, dd = pt["x"], pt["y"], pt["z"], pt["w"], pt["h"], pt["d"]
        corners = [proj((x, y, z)), proj((x + w, y + h, z + dd))]
        (u0, v0), (u1, v1) = corners
        u0, u1 = sorted((u0, u1))
        v0, v1 = sorted((v0, v1))
        near = {"front": z < 6, "top": y + h > d.H - 6, "side": x + w > d.W - 6}[view]
        round_ = pt["kind"] in ("battery", "speaker") and view == "top"
        if not near:
            # hidden behind nearer parts: dashed outline, small grey label
            if round_:
                ax.add_patch(Circle(((u0 + u1) / 2, (v0 + v1) / 2), w / 2, fc="none", ec="#777", lw=0.8, ls="--"))
            else:
                ax.add_patch(Rectangle((u0, v0), u1 - u0, v1 - v0, fc="none", ec="#777", lw=0.8, ls="--"))
            ax.text(u1 - 0.6, v0 + 0.6, pt["label"], ha="right", va="bottom", fontsize=5.2, color="#666")
            continue
        if round_:
            ax.add_patch(Circle(((u0 + u1) / 2, (v0 + v1) / 2), w / 2, fc=pt["color"], ec="#111", lw=0.8, alpha=0.7))
        else:
            ax.add_patch(FancyBboxPatch((u0, v0), u1 - u0, v1 - v0, boxstyle="round,pad=0,rounding_size=0.8",
                                        fc=pt["color"], ec="#111", lw=0.8, alpha=0.8))
        if pt["kind"] == "oled" and view == "front":
            ax.add_patch(Rectangle((u0 + 1.5, v0 + 6), (u1 - u0) - 3, (v1 - v0) - 10, fc="#081218", ec="none"))
            for ex in (u0 + (u1 - u0) * 0.3, u0 + (u1 - u0) * 0.7):
                ax.add_patch(FancyBboxPatch((ex - 3.6, v0 + 11), 7.2, 9, boxstyle="round,pad=0,rounding_size=1.5",
                                            fc="#28e1eb", ec="none"))
            continue
        if (u1 - u0) > 6 and (v1 - v0) > 3:
            ax.text((u0 + u1) / 2, (v0 + v1) / 2, pt["label"], ha="center", va="center", fontsize=6.5,
                    color="white", weight="bold", rotation=90 if (v1 - v0) > 2.2 * (u1 - u0) else 0)
    # frame
    for a, b, step in d.bars:
        (u0, v0), (u1, v1) = proj(a), proj(b)
        lw = 3.0 if step == 1 else 2.0
        ax.plot([u0, u1], [v0, v1], color=BRASS, lw=lw, solid_capstyle="round", zorder=5)
    for cx, y, cz, r, step in d.rings:
        if view == "top":
            ax.add_patch(Circle((cx, d.D - cz), r, fc="none", ec=BRASS, lw=2, zorder=5))
        elif view == "front":
            ax.plot([cx - r, cx + r], [y, y], color=BRASS, lw=2, zorder=5)
        else:
            ax.plot([cz - r, cz + r], [y, y], color=BRASS, lw=2, zorder=5)

    U = {"front": d.W, "top": d.W, "side": d.D}[view]
    V = {"front": d.H, "top": d.D, "side": d.H}[view]
    dim(ax, (0, 0), (U, 0), -6, f"{U:g} mm")
    dim(ax, (U, 0), (U, V), 6, f"{V:g} mm", vertical=True)
    if view == "front":
        dim(ax, (0, V), (19.5, V), 5, "19.5")
    ax.set_xlim(-8, U + 14)
    ax.set_ylim(-12, V + 10)


def case_sheet(d, fname):
    fig = plt.figure(figsize=(16, 10.4), dpi=130, facecolor=BG)
    fig.text(0.02, 0.965, f"{d.name}  -  brass wire case", fontsize=20, weight="bold", color=INK)
    fig.text(0.02, 0.935, f"Outer frame {d.W} x {d.H} x {d.D} mm (wire centre lines).  "
             "Thick gold = 1.2 mm brass frame (also GND).  Thin gold = 1.2 mm seats / 0.9 mm grille.  "
             "Test-fit your real parts before soldering.", fontsize=10, color="#555")
    ax3 = fig.add_axes([0.0, 0.33, 0.5, 0.6], projection="3d", facecolor=BG)
    draw3d(ax3, d)
    axf = fig.add_axes([0.5, 0.48, 0.25, 0.42])
    view2d(axf, d, "front", "FRONT")
    axs = fig.add_axes([0.75, 0.48, 0.24, 0.42])
    view2d(axs, d, "side", "RIGHT SIDE")
    axt = fig.add_axes([0.5, 0.05, 0.25, 0.4])
    view2d(axt, d, "top", "TOP")

    # cut list
    W, H, D = d.W, d.H, d.D
    total_12 = 2 * (2 * W + 2 * H) + 4 * D + 2 * H
    seats = sum(np.linalg.norm(np.subtract(b, a)) for a, b, s in d.bars if s == 2)
    rings = sum(2 * np.pi * r for *_, r, s in d.rings)
    lines = [
        f"1.2 mm brass  frame:  2 x rectangle {W} x {H} (bend 4 x 90°, one joint)",
        f"                       2 x divider {H} mm  +  4 x strut {D} mm",
        f"                       ~{total_12 / 10:.0f} cm in total",
        f"seats / tabs / grille ~{seats / 10:.0f} cm   battery rings 2 x Ø{2 * d.rings[0][3]:.0f} mm (~{rings / 10:.0f} cm)",
        "0.9 mm brass  signal wires: ~30-40 cm, bend at 90°, keep >= 2 mm",
        "                       from the frame and each other; heat-shrink where they cross",
        "Solder: leaded 0.6-0.8 mm + flux; tin every joint, clamp in a jig, 1-2 s per joint",
        "Battery wrap stays ON - the 16340 can is negative and must not touch the frame bare",
    ]
    ax = fig.add_axes([0.02, 0.03, 0.46, 0.28])
    ax.axis("off")
    ax.add_patch(FancyBboxPatch((0, 0), 1, 1, boxstyle="round,pad=0,rounding_size=0.03", fc="#fff8e1", ec=BRASS,
                                transform=ax.transAxes))
    ax.text(0.03, 0.9, "Cut list + tips", fontsize=12, weight="bold", color=INK, transform=ax.transAxes)
    for i, ln in enumerate(lines):
        ax.text(0.03, 0.78 - i * 0.095, ln, fontsize=8.6, color=INK, family="DejaVu Sans Mono",
                transform=ax.transAxes)

    # parts placement table
    ax = fig.add_axes([0.76, 0.05, 0.23, 0.4])
    ax.axis("off")
    ax.add_patch(FancyBboxPatch((0, 0), 1, 1, boxstyle="round,pad=0,rounding_size=0.03", fc="#eef6ff", ec=W_BLUE,
                                transform=ax.transAxes))
    ax.text(0.05, 0.93, "Where each part sits", fontsize=12, weight="bold", color=INK, transform=ax.transAxes)
    where = {"OLED": "front, right of the divider", "16340": "front-left bay, in 2 rings",
             "ESP32-C3": "back wall, USB-C to the right", "XIAO S3": "back wall, USB-C to the right",
             "LDO": "back wall, under the board", "TP4056": "on the floor bars",
             "MAX98357A": "floor, front", "switch": "side wall, knob out", "TTP223": "on top = Coco's head",
             "INMP441": "right wall, hole facing out", "speaker": "under the top grille, up"}
    y = 0.83
    for pt in d.parts:
        ax.text(0.05, y, pt["label"], fontsize=9, weight="bold", color=INK, transform=ax.transAxes)
        ax.text(0.40, y, where.get(pt["label"], ""), fontsize=8.2, color="#333", transform=ax.transAxes)
        y -= 0.085
    fig.savefig(out_path(fname), facecolor=BG)
    plt.close(fig)
    print("wrote", fname)


def assembly_sheet(d, fname):
    fig = plt.figure(figsize=(16, 11), dpi=120, facecolor=BG)
    fig.text(0.02, 0.965, f"{d.name}  -  assembly steps", fontsize=20, weight="bold", color=INK)
    fig.text(0.02, 0.937, "New parts in each step are solid, earlier ones faded.  Follow the circuit diagram for "
             "every wire.  Insert the battery last.", fontsize=10.5, color="#555")
    for i, (title, text) in enumerate(d.steps):
        r, c = divmod(i, 3)
        x0, y0 = 0.01 + c * 0.33, 0.49 - r * 0.46
        ax = fig.add_axes([x0, y0 + 0.07, 0.32, 0.36], projection="3d", facecolor=BG)
        draw3d(ax, d, upto=i + 1, highlight=i + 1)
        fig.text(x0 + 0.01, y0 + 0.415, f"{i + 1}", fontsize=22, weight="bold", color="white",
                 bbox=dict(boxstyle="circle,pad=0.25", fc=BRASS, ec="none"))
        fig.text(x0 + 0.045, y0 + 0.42, title, fontsize=13, weight="bold", color=INK)
        words, line, out = text.split(), "", []
        for w in words:
            if len(line) + len(w) > 58:
                out.append(line)
                line = w
            else:
                line = (line + " " + w).strip()
        out.append(line)
        fig.text(x0 + 0.01, y0 + 0.055, "\n".join(out), fontsize=9.5, color="#333", va="top")
    fig.savefig(out_path(fname), facecolor=BG)
    plt.close(fig)
    print("wrote", fname)


def overview(dt, dv, ds):
    fig = plt.figure(figsize=(18, 8), dpi=120, facecolor=BG)
    fig.text(0.02, 0.94, "COCO  -  brass desk buddy, three versions", fontsize=22, weight="bold", color=INK)
    cols = [
        (dt, ["ESP32-C3 Super Mini, 0.96\" OLED, TTP223", "faces, clock, weather, status",
              "Wi-Fi set up from your phone", f"{dt.W} x {dt.H} x {dt.D} mm, ~1 day per charge"]),
        (dv, ["+ INMP441 mic, MAX98357A amp, speaker", "talks: XiaoZhi voice AI, \"Hi ESP\"",
              "faces follow the emotion of the answer", f"{dv.W} x {dv.H} x {dv.D} mm, ~3-5 h talking"]),
        (ds, ["XIAO ESP32S3 (8 MB + PSRAM) instead", "own wake word: \"Hey Coco\"",
              "calm voice + personality on xiaozhi.me", f"same {ds.W} x {ds.H} x {ds.D} mm case"]),
    ]
    for i, (d, lines) in enumerate(cols):
        ax = fig.add_axes([i / 3, 0.2, 1 / 3, 0.66], projection="3d", facecolor=BG)
        draw3d(ax, d, show_labels=False)
        fig.text(0.03 + i / 3, 0.86, d.name, fontsize=16, weight="bold", color=BRASS_DARK)
        for k, ln in enumerate(lines):
            fig.text(0.03 + i / 3, 0.16 - k * 0.037, ln, fontsize=11, color=INK)
    fig.savefig(out_path("coco_overview.png"), facecolor=BG)
    plt.close(fig)
    print("wrote coco_overview.png")


def bending_template(designs):
    """1:1 A4 template per version: lay the wire on the lines while bending."""
    from matplotlib.backends.backend_pdf import PdfPages

    for d in designs:
        path = out_path(f"{d.slug}_bending_template.pdf")
        with PdfPages(path) as pdf:
                fig = plt.figure(figsize=(210 / 25.4, 297 / 25.4), facecolor="white")
                ax = fig.add_axes([0, 0, 1, 1])
                ax.set_xlim(0, 210)
                ax.set_ylim(0, 297)
                ax.axis("off")
                ax.text(15, 282, f"{d.name} - 1:1 bending template", fontsize=16, weight="bold")
                ax.text(15, 275, "Print at 100 % (no 'fit to page'). Check: the 50 mm ruler below must measure 50 mm.",
                        fontsize=8.5)
                ax.plot([15, 65], [268, 268], color="k", lw=1.5)
                for x in range(15, 66, 10):
                    ax.plot([x, x], [266, 270], color="k", lw=1)
                ax.text(40, 262, "50 mm", ha="center", fontsize=8)

                def frame(x0, y0, label, divider=True):
                    ax.add_patch(Rectangle((x0, y0), d.W, d.H, fc="none", ec=BRASS, lw=3))
                    if divider:
                        ax.plot([x0 + 19.5, x0 + 19.5], [y0, y0 + d.H], color=BRASS, lw=2.4)
                    for cx, cy in [(x0, y0), (x0 + d.W, y0), (x0 + d.W, y0 + d.H), (x0, y0 + d.H)]:
                        ax.add_patch(Circle((cx, cy), 1.2, fc="#c92a2a", ec="none"))
                    ax.text(x0, y0 + d.H + 3, label, fontsize=9, weight="bold")
                    ax.text(x0 + d.W / 2, y0 - 5, f"{d.W} mm", ha="center", fontsize=8)
                    ax.text(x0 + d.W + 3, y0 + d.H / 2, f"{d.H} mm", rotation=90, va="center", fontsize=8)

                frame(20, 190, "FRONT rectangle + divider (1.2 mm)")
                frame(110, 190, "BACK rectangle + divider (1.2 mm)")
                ax.text(20, 178, "Red dots = 90° bends. Start a rectangle in the middle of the bottom side so the "
                        "joint is hidden.", fontsize=8)
                # struts and straight pieces at 1:1
                y = 165
                pieces = [(d.D, f"strut {d.D} mm  x4 (1.2 mm)"), (d.H, f"divider {d.H} mm  x2 (1.2 mm)"),
                          (d.W - 19.5, f"seat / floor bar {d.W - 19.5:g} mm (see case drawing)")]
                for L, label in pieces:
                    ax.plot([20, 20 + L], [y, y], color=BRASS, lw=3)
                    ax.plot([20, 20], [y - 2, y + 2], color="k", lw=1)
                    ax.plot([20 + L, 20 + L], [y - 2, y + 2], color="k", lw=1)
                    ax.text(25 + L, y, label, va="center", fontsize=8.5)
                    y -= 10
                # battery ring former
                r = d.rings[0][3]
                ax.add_patch(Circle((40, y - 18), r, fc="none", ec=BRASS, lw=3))
                ax.add_patch(Circle((40, y - 18), BAT_D / 2, fc="#5c7cfa", ec="none", alpha=0.25))
                ax.text(62, y - 18, f"battery ring Ø{2 * r:.1f} mm x2 - wrap the wire around a 16 mm\n"
                        "rod (a spare 16340 in its wrap works) and leave a 3 mm tab.", va="center", fontsize=8.5)
                # OLED hole posts
                ax.add_patch(Rectangle((20, y - 75), 27.3, 27.8, fc="none", ec=PCB_BLUE, lw=1.5, ls="--"))
                for hx in (2, 25.3):
                    for hy in (2, 25.8):
                        ax.add_patch(Circle((20 + hx, y - 75 + hy), 1.0, fc=PCB_BLUE))
                ax.text(52, y - 61, "OLED outline 27.3 x 27.8 mm with its 4 mounting holes.\n"
                        "Measure your module: hole spacing varies (about 23 x 24 mm).\n"
                        "Posts: 0.9 mm wire through each hole, soldered to the front frame.", va="center", fontsize=8.5)
                pdf.savefig(fig)
                plt.close(fig)
        print(f"wrote {os.path.basename(path)}")


# ============================================== size + case dimension sheets

PART_SIZES = {  # real module sizes in mm (w x h x thickness), shown on the size sheet
    "OLED": "27.3 x 27.8 x 4.1", "ESP32-C3": "22.5 x 18 x 3.6", "XIAO S3": "21 x 17.8 x 3.4",
    "TP4056": "17 x 12 x 3.6", "TTP223": "15 x 11 x 1.6", "16340": "Ø16 x 34", "switch": "8.6 x 3.6 x 4",
    "LDO": "4.5 x 2.5 (SOT-89)", "MAX98357A": "19.3 x 17.8 x 3", "INMP441": "14 x 14 x 1.6", "speaker": "Ø20 x 4.6",
}


def size_sheet(d):
    fig = plt.figure(figsize=(18, 10.5), dpi=120, facecolor=BG)
    fig.text(0.02, 0.955, f"{d.name}  -  size diagram", fontsize=22, weight="bold", color=INK)
    fig.text(0.02, 0.925, f"Outer size {d.W} x {d.H} x {d.D} mm (brass wire centre lines, add 1.2 mm for the wire "
             "itself).  All parts drawn at their real size.", fontsize=11, color="#555")
    for k, (view, title, rect) in enumerate([("front", "FRONT", [0.01, 0.40, 0.27, 0.47]),
                                             ("side", "RIGHT SIDE", [0.27, 0.40, 0.2, 0.47]),
                                             ("top", "TOP", [0.01, 0.02, 0.27, 0.36])]):
        ax = fig.add_axes(rect)
        view2d(ax, d, view, title)

    # credit card for scale
    ax = fig.add_axes([0.27, 0.03, 0.22, 0.34])
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Scale: next to a bank card (85.6 x 54 mm)", fontsize=10.5, weight="bold", color=INK)
    ax.add_patch(FancyBboxPatch((0, 0), 85.6, 54, boxstyle="round,pad=0,rounding_size=3.2", fc="#dfe7f1", ec="#7a8ca5",
                                lw=1.4))
    ax.add_patch(Rectangle((8, 30), 12, 9, fc="#d4b65a", ec="#9a7a2a", lw=0.8))  # chip
    ox, oy = 85.6 - d.W - 4, 4
    ax.add_patch(Rectangle((ox, oy), d.W, d.H, fc="#f6e7c4", ec=BRASS, lw=2.4, alpha=0.95))
    ax.plot([ox + 19.5, ox + 19.5], [oy, oy + d.H], color=BRASS, lw=1.6)
    oled = next(p for p in d.parts if p["kind"] == "oled")
    ax.add_patch(Rectangle((ox + oled["x"] + 1.5, oy + oled["y"] + 6), oled["w"] - 3, oled["h"] - 10, fc="#081218"))
    for ex in (0.3, 0.7):
        ax.add_patch(FancyBboxPatch((ox + oled["x"] + oled["w"] * ex - 3.6, oy + oled["y"] + 11), 7.2, 9,
                                    boxstyle="round,pad=0,rounding_size=1.5", fc="#28e1eb", ec="none"))
    bat = next(p for p in d.parts if p["kind"] == "battery")
    ax.add_patch(Rectangle((ox + bat["x"], oy + bat["y"]), bat["w"], bat["h"], fc="#5c7cfa", alpha=0.8))
    ax.text(ox + d.W / 2, oy + d.H + 2, f"Coco {d.W} x {d.H}", ha="center", fontsize=8.5, weight="bold", color=BRASS_DARK)
    ax.set_xlim(-2, 88)
    ax.set_ylim(-3, 60)

    # part table
    ax = fig.add_axes([0.5, 0.03, 0.48, 0.86])
    ax.axis("off")
    ax.add_patch(FancyBboxPatch((0, 0), 1, 1, boxstyle="round,pad=0,rounding_size=0.02", fc="#eef6ff", ec=W_BLUE,
                                transform=ax.transAxes))
    ax.text(0.04, 0.95, "Every part and where it sits", fontsize=14, weight="bold", color=INK, transform=ax.transAxes)
    hdr = ["Part", "Size (mm)", "Position from front-left-bottom corner (x, y, z)"]
    xs = [0.04, 0.24, 0.50]
    for x, h in zip(xs, hdr):
        ax.text(x, 0.89, h, fontsize=10.5, weight="bold", color=W_BLUE, transform=ax.transAxes)
    y = 0.84
    for p in sorted(d.parts, key=lambda q: (q["z"], q["y"])):
        ax.add_patch(Rectangle((0.02, y - 0.012), 0.012, 0.024, fc=p["color"], transform=ax.transAxes))
        ax.text(xs[0], y, p["label"], fontsize=10.5, weight="bold", color=INK, transform=ax.transAxes, va="center")
        ax.text(xs[1], y, PART_SIZES.get(p["label"], f'{p["w"]:g} x {p["h"]:g} x {p["d"]:g}'), fontsize=10,
                color=INK, transform=ax.transAxes, va="center", family="DejaVu Sans Mono")
        ax.text(xs[2], y, f'x {p["x"]:>5.1f}   y {p["y"]:>5.1f}   z {p["z"]:>5.1f}', fontsize=10, color="#333",
                transform=ax.transAxes, va="center", family="DejaVu Sans Mono")
        y -= 0.062
    notes = ["x = width (left -> right), y = height (up), z = depth (front -> back).",
             "Position = the part's corner nearest the front-left-bottom of the frame.",
             "Module sizes vary between sellers: measure yours and keep >= 1 mm clearance."]
    for i, n in enumerate(notes):
        ax.text(0.04, 0.12 - i * 0.04, n, fontsize=9.5, color="#555", transform=ax.transAxes)
    fig.savefig(out_path(f"{d.slug}_size.png"), facecolor=BG)
    plt.close(fig)
    print(f"wrote {d.slug}_size.png")


def classify_bar(d, a, b, step):
    """Name and wire gauge of a frame piece."""
    L = float(np.linalg.norm(np.subtract(b, a)))
    if step == 1:
        if a[2] != b[2]:
            return "strut", "1.2", L
        return ("divider" if a[0] == b[0] == 19.5 else "rectangle side"), "1.2", L
    if L < 3:
        return "ring tab", "1.2", L
    if a[1] == 0:
        return "floor bar", "1.2", L
    if min(a[0], b[0]) >= 21:
        return "speaker grille bar", "0.9", L
    if max(a[0], b[0]) <= 19.6:
        return "touch-pad seat bar", "1.2", L
    return "top bar (touch-pad seat)", "1.2", L


def cut_list(d):
    rows = [("front + back rectangle", "1.2", 2 * d.W + 2 * d.H, 2, "4 x 90°, joint at bottom")]
    groups = {}
    for a, b, step in d.bars:
        name, gauge, L = classify_bar(d, a, b, step)
        if name == "rectangle side":
            continue
        key = (name, gauge, round(L, 1))
        groups[key] = groups.get(key, 0) + 1
    for (name, gauge, L), n in sorted(groups.items(), key=lambda kv: (kv[0][1] != "1.2", -kv[0][2])):
        cut = L + 2 if name == "ring tab" else L
        rows.append((name, gauge, cut, n, "L-shape: 2 mm on the ring" if name == "ring tab" else "straight"))
    r = d.rings[0][3]
    rows.append((f"battery ring Ø{2 * r:.1f}", "1.2", round(2 * math.pi * r + 4, 1), len(d.rings),
                 "on a 16 mm rod, 4 mm overlap"))
    return rows


def case_dims_sheet(d):
    fig = plt.figure(figsize=(18, 11), dpi=120, facecolor=BG)
    fig.text(0.02, 0.955, f"{d.name}  -  case diagram (brass frame)", fontsize=22, weight="bold", color=INK)
    fig.text(0.02, 0.925, "All sizes in mm, measured to the wire centre lines.  Thick = 1.2 mm frame, thin = seats, "
             "tabs (1.2 mm) and grille (0.9 mm).  Red dots = 90° bends.", fontsize=11, color="#555")
    W, H, D = d.W, d.H, d.D
    seat_z = sorted({a[2] for a, b, s in d.bars if s == 2 and a[1] == 0 and a[2] == b[2]})
    top_z = sorted({a[2] for a, b, s in d.bars if s == 2 and a[1] == H and a[2] == b[2]})
    ring_y = sorted({y for cx, y, cz, r, s in d.rings})
    r = d.rings[0][3]
    rcx, rcz = d.rings[0][0], d.rings[0][2]

    def frame_ax(rect, title, U, V):
        ax = fig.add_axes(rect)
        ax.set_aspect("equal")
        ax.axis("off")
        ax.set_title(title, fontsize=12, weight="bold", color=INK)
        ax.set_xlim(-14, U + 16)
        ax.set_ylim(-14, V + 12)
        return ax

    def bends(ax, pts):
        for p in pts:
            ax.add_patch(Circle(p, 0.9, fc="#c92a2a", ec="none", zorder=6))

    # FRONT (x, y)
    ax = frame_ax([0.0, 0.46, 0.34, 0.42], "FRONT  (x across, y up)", W, H)
    ax.add_patch(Rectangle((0, 0), W, H, fc="none", ec=BRASS, lw=3.2))
    ax.plot([19.5, 19.5], [0, H], color=BRASS, lw=3)
    bends(ax, [(0, 0), (W, 0), (W, H), (0, H)])
    for y in ring_y:
        ax.plot([rcx - r, rcx + r], [y, y], color=BRASS, lw=2)
        ax.text(rcx, y + 1.2, f"ring @ {y:g}", ha="center", fontsize=7.5, color=BRASS_DARK)
    dim(ax, (0, 0), (W, 0), -7, f"{W:g}")
    dim(ax, (W, 0), (W, H), 7, f"{H:g}", vertical=True)
    dim(ax, (0, H), (19.5, H), 5, "19.5")
    dim(ax, (19.5, H), (W, H), 5, f"{W - 19.5:g}")
    for y in ring_y:
        dim(ax, (0, 0), (0, y), -4 - 4 * ring_y.index(y), f"{y:g}", vertical=True)

    # SIDE (z, y)
    ax = frame_ax([0.33, 0.46, 0.22, 0.42], "RIGHT SIDE  (z = depth)", D, H)
    ax.add_patch(Rectangle((0, 0), D, H, fc="none", ec=BRASS, lw=3.2))
    for z in seat_z:
        ax.add_patch(Circle((z, 0), 1.0, fc=BRASS, ec="none"))
    for z in top_z:
        ax.add_patch(Circle((z, H), 0.8 if top_z and len(top_z) > 2 else 1.0, fc=BRASS, ec="none"))
    dim(ax, (0, 0), (D, 0), -7, f"{D:g}")
    dim(ax, (D, 0), (D, H), 7, f"{H:g}", vertical=True)
    ax.text(0, H + 3, "front", fontsize=8, color="#666")
    ax.text(D, H + 3, "back", fontsize=8, color="#666", ha="right")
    ax.text(D / 2, -12.5, "dots = bars seen end-on", ha="center", fontsize=7.5, color="#666")

    # TOP (x, z) with the front edge at the bottom
    ax = frame_ax([0.0, 0.03, 0.34, 0.42], "TOP  (front edge at the bottom)", W, D)
    ax.add_patch(Rectangle((0, 0), W, D, fc="none", ec=BRASS, lw=3.2))
    ax.plot([19.5, 19.5], [0, D], color=BRASS, lw=3, alpha=0.6)
    for a, b, s in d.bars:
        if s == 2 and a[1] == H and a[2] == b[2]:
            name, gauge, L = classify_bar(d, a, b, s)
            ax.plot([a[0], b[0]], [a[2], b[2]], color=BRASS, lw=1.4 if gauge == "0.9" else 2.2)
    ax.add_patch(Circle((rcx, rcz), r, fc="none", ec=BRASS, lw=2))
    ax.text(rcx, rcz, f"Ø{2 * r:.1f}", ha="center", va="center", fontsize=8, color=BRASS_DARK)
    dim(ax, (0, 0), (W, 0), -7, f"{W:g}")
    dim(ax, (W, 0), (W, D), 7, f"{D:g}", vertical=True)
    for z in top_z:
        x0 = min(min(a[0], b[0]) for a, b, s in d.bars if s == 2 and a[1] == H and a[2] == b[2] == z)
        if x0 < 19:  # bar over the battery bay: label on the left
            ax.text(-1.5, z, f"{z:g}", fontsize=7, va="center", ha="right", color="#c92a2a")
        else:
            ax.text(W + 1.5, z, f"{z:g}", fontsize=7, va="center", color="#c92a2a")
    ax.text(W / 2, D + 3, "red numbers = z of each top bar (from the front)", fontsize=7, ha="center", color="#c92a2a")

    # floor plan (x, z) for the floor bars
    ax = frame_ax([0.33, 0.03, 0.22, 0.42], "FLOOR BARS (from below)", W, D)
    ax.add_patch(Rectangle((0, 0), W, D, fc="none", ec=BRASS, lw=3.2))
    for z in seat_z:
        ax.plot([19.5, W], [z, z], color=BRASS, lw=2.2)
        ax.text(W + 1.5, z, f"{z:g}", fontsize=7, va="center", color="#c92a2a")
    ax.plot([19.5, 19.5], [0, D], color=BRASS, lw=3, alpha=0.6)
    dim(ax, (19.5, 0), (W, 0), -7, f"{W - 19.5:g}")

    # cut list
    ax = fig.add_axes([0.56, 0.03, 0.42, 0.88])
    ax.axis("off")
    ax.add_patch(FancyBboxPatch((0, 0), 1, 1, boxstyle="round,pad=0,rounding_size=0.02", fc="#fff8e1", ec=BRASS,
                                transform=ax.transAxes))
    ax.text(0.04, 0.95, "Cut list", fontsize=15, weight="bold", color=INK, transform=ax.transAxes)
    xs = [0.04, 0.09, 0.47, 0.57, 0.70, 0.77]
    for x, h in zip(xs, ["#", "Piece", "Wire", "Length", "Qty", "How"]):
        ax.text(x, 0.89, h, fontsize=10.5, weight="bold", color=BRASS_DARK, transform=ax.transAxes)
    rows = cut_list(d)
    y = 0.84
    total = {"1.2": 0.0, "0.9": 0.0}
    for i, (name, gauge, L, n, how) in enumerate(rows, 1):
        total[gauge] += L * n
        for x, t in zip(xs, [str(i), name, f"{gauge} mm", f"{L:g}", f"x{n}", how]):
            ax.text(x, y, t, fontsize=9.6 if x != xs[5] else 8.2, color=INK, transform=ax.transAxes, va="center")
        y -= 0.055
    y -= 0.02
    ax.text(0.04, y, f"Total 1.2 mm wire: {total['1.2'] / 10:.0f} cm (buy {total['1.2'] / 10 * 1.3:.0f} cm)",
            fontsize=10.5, weight="bold", color=INK, transform=ax.transAxes)
    if total["0.9"]:
        y -= 0.045
        ax.text(0.04, y, f"Total 0.9 mm wire for the grille: {total['0.9'] / 10:.0f} cm", fontsize=10.5,
                weight="bold", color=INK, transform=ax.transAxes)
    y -= 0.045
    ax.text(0.04, y, "Plus ~40-50 cm of 0.9 mm wire for the signal wires (see the circuit diagram).", fontsize=9.5,
            color="#555", transform=ax.transAxes)
    steps = ["Order: rectangles -> dividers -> struts -> floor/top bars -> rings -> tabs.",
             "Bend on the 1:1 bending template, then tape both rectangles in place",
             f"{D:g} mm apart before soldering the struts.",
             "Check square: both diagonals of each rectangle must match (±0.5 mm)."]
    for i, t in enumerate(steps):
        ax.text(0.04, 0.12 - i * 0.035, t, fontsize=9.5, color="#555", transform=ax.transAxes)
    fig.savefig(out_path(f"{d.slug}_case_dimensions.png"), facecolor=BG)
    plt.close(fig)
    print(f"wrote {d.slug}_case_dimensions.png")


def main():
    touch_wiring()
    voice_wiring()
    voice_s3_wiring()
    voice_s3_wiring(four_mb=True)
    voice_s3_wiring(devkit=True)
    dt, dv, ds = touch_design(), voice_design(), voice_design(s3=True)
    for d in (dt, dv, ds):
        case_sheet(d, f"{d.slug}_case.png")
        assembly_sheet(d, f"{d.slug}_assembly.png")
        size_sheet(d)
        case_dims_sheet(d)
    overview(dt, dv, ds)
    bending_template([dt, dv, ds])


if __name__ == "__main__":
    main()
