#!/usr/bin/env python3
"""Test sheets for Coco Voice S3 HW-747: where to put the multimeter probes on each part, and the
bench test order. Same look as make_diagrams.py.

  python3 tools/make_test_diagrams.py
  -> coco_voice_s3/images/coco_voice_s3_hw747_esp32s3_test_multimeter.png, _test_order.png
"""
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyArrowPatch, FancyBboxPatch, Rectangle

from make_diagrams import (BG, BRASS, INK, PCB_BLACK, PCB_BLUE, PCB_PURPLE, PCB_RED, W_BLUE, W_GREEN, W_ORANGE,
                           W_RED, out_path)

PROBE_RED = "#e03131"
PROBE_BLACK = "#212529"


def module(ax, x, y, w, h, name, color, pins, side="B"):
    """A module with pins along one edge; returns {pin: (x, y)} of the pin ends."""
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=1.2", fc=color, ec="#111", lw=1))
    ax.text(x + w / 2, y + h / 2 + (1.5 if side == "B" else 0), name, ha="center", va="center", color="white",
            fontsize=8.5, weight="bold")
    out = {}
    n = len(pins)
    for i, p in enumerate(pins):
        if side == "B":
            px, py = x + w * (i + 1) / (n + 1), y
            ax.add_patch(Circle((px, py), 1.1, fc="#e9c46a", ec="#7a5c12", lw=0.8, zorder=4))
            ax.text(px, py + 2.6, p, ha="center", fontsize=6.2, color="white", weight="bold")
            out[p] = (px, py)
        else:  # top
            px, py = x + w * (i + 1) / (n + 1), y + h
            ax.add_patch(Circle((px, py), 1.1, fc="#e9c46a", ec="#7a5c12", lw=0.8, zorder=4))
            ax.text(px, py - 3.4, p, ha="center", fontsize=6.2, color="white", weight="bold")
            out[p] = (px, py)
    return out


def probe(ax, at, colour, to):
    """Probe lead from the meter (to) to a pin (at), with a pointed tip."""
    ax.plot([to[0], at[0]], [to[1], at[1] - 3], color=colour, lw=2.4, solid_capstyle="round", zorder=5)
    ax.add_patch(plt.Polygon([(at[0] - 0.9, at[1] - 3), (at[0] + 0.9, at[1] - 3), (at[0], at[1] - 0.6)],
                             fc="#adb5bd", ec="#495057", lw=0.6, zorder=6))


def meter(ax, x, y, mode, reading):
    ax.add_patch(FancyBboxPatch((x, y), 30, 15, boxstyle="round,pad=0,rounding_size=2", fc="#ffd43b", ec="#5c4400",
                                lw=1.2))
    ax.add_patch(Rectangle((x + 2.5, y + 7), 25, 6, fc="#c5d6b8", ec="#333", lw=0.8))
    ax.text(x + 15, y + 10, reading, ha="center", va="center", fontsize=7.5, family="DejaVu Sans Mono",
            weight="bold", color="#111")
    ax.text(x + 15, y + 3.6, mode, ha="center", va="center", fontsize=7, weight="bold", color="#3d2e00")
    return (x + 9, y + 15), (x + 21, y + 15)  # COM, V/Ω sockets


def panel(fig, rect, n, title, draw, expect, fail):
    ax = fig.add_axes(rect)
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.add_patch(FancyBboxPatch((1, 1), 98, 98, boxstyle="round,pad=0,rounding_size=3", fc="white", ec="#ced4da",
                                lw=1.2))
    ax.add_patch(Circle((8, 91), 4.2, fc=BRASS, ec="none"))
    ax.text(8, 91, str(n), ha="center", va="center", color="white", fontsize=10, weight="bold")
    ax.text(15, 91, title, va="center", fontsize=10.5, weight="bold", color=INK)
    draw(ax)
    ax.text(5, 17, expect, fontsize=7.8, color="#1b5e20", weight="bold", va="top", wrap=True)
    ax.text(5, 8.5, fail, fontsize=7.2, color="#b02a37", va="top")
    return ax


def p_cell(ax):
    ax.add_patch(FancyBboxPatch((25, 52), 50, 18, boxstyle="round,pad=0,rounding_size=4", fc="#5c7cfa", ec="#222"))
    ax.text(50, 61, "16340  3.7 V", ha="center", va="center", color="white", weight="bold", fontsize=9)
    ax.add_patch(Rectangle((75, 57), 3, 8, fc="#adb5bd", ec="#333"))
    ax.text(80, 61, "+", fontsize=12, weight="bold", va="center", color=W_RED)
    ax.text(19, 61, "−", fontsize=12, weight="bold", va="center", color=INK)
    com, v = meter(ax, 35, 21, "DC V  (20 V)", "3.0 - 4.2 V")
    ax.plot([v[0], v[0], 79, 79], [v[1], 46, 46, 57], color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 24, 24], [com[1], 46, 46, 57], color=PROBE_BLACK, lw=2.4)


def p_tp4056(ax):
    p = module(ax, 20, 50, 60, 24, "TP4056", PCB_BLUE, ["B+", "B-", "OUT+", "OUT-"], side="T")
    ax.text(50, 47, "cell on B+ / B-", ha="center", fontsize=7, color="#555")
    com, v = meter(ax, 35, 20, "DC V  (20 V)", "= cell volts")
    ax.plot([v[0], v[0], 90, 90, p["OUT+"][0], p["OUT+"][0]], [v[1], 40, 40, 80, 80, p["OUT+"][1]],
            color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 93, 93, p["OUT-"][0], p["OUT-"][0]], [com[1], 38, 38, 84, 84, p["OUT-"][1]],
            color=PROBE_BLACK, lw=2.4)


def p_switch(ax):
    ax.add_patch(Rectangle((32, 56), 36, 14, fc="#dee2e6", ec="#333"))
    ax.add_patch(Rectangle((38, 70), 9, 5, fc="#495057"))
    pins = {}
    for i, x in enumerate((38, 50, 62)):
        ax.plot([x, x], [56, 50], color="#868e96", lw=2.5)
        pins[i] = (x, 50)
    ax.text(50, 63, "slide switch", ha="center", va="center", fontsize=8, weight="bold", color=INK)
    com, v = meter(ax, 35, 20, "continuity  ))) ", "BEEP / ---")
    ax.plot([v[0], v[0], pins[1][0]], [v[1], 44, 44], color=PROBE_RED, lw=2.4)
    ax.plot([pins[1][0], pins[1][0]], [44, 50], color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], pins[0][0], pins[0][0]], [com[1], 41, 41, 50], color=PROBE_BLACK, lw=2.4)


def p_resistor(ax):
    ax.plot([20, 34], [62, 62], color="#868e96", lw=2.2)
    ax.plot([66, 80], [62, 62], color="#868e96", lw=2.2)
    ax.add_patch(FancyBboxPatch((34, 57), 32, 10, boxstyle="round,pad=0,rounding_size=3", fc="#e8d5a8", ec="#333"))
    for i, c in enumerate(["#f08c00", "#f08c00", "#fcc419", "#c9a227"]):
        ax.add_patch(Rectangle((39 + i * 6, 57), 2.6, 10, fc=c, ec="none"))
    ax.text(50, 73, "330 kΩ = orange-orange-yellow", ha="center", fontsize=7.5, color=INK)
    com, v = meter(ax, 35, 20, "Ω  (2 MΩ)", "314 - 347 k")
    ax.plot([v[0], v[0], 80, 80], [v[1], 46, 46, 62], color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 20, 20], [com[1], 46, 46, 62], color=PROBE_BLACK, lw=2.4)


def p_ldo(ax):
    p = module(ax, 28, 52, 44, 20, "3.3 V LDO", "#495057", ["IN", "GND", "OUT"], side="T")
    ax.text(50, 48, "IN on BAT_SW, switch ON", ha="center", fontsize=7, color="#555")
    com, v = meter(ax, 35, 20, "DC V  (20 V)", "3.25 - 3.35")
    ax.plot([v[0], v[0], 86, 86, p["OUT"][0], p["OUT"][0]], [v[1], 40, 40, 80, 80, p["OUT"][1]],
            color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 14, 14, p["GND"][0], p["GND"][0]], [com[1], 40, 40, 80, 80, p["GND"][1]],
            color=PROBE_BLACK, lw=2.4)


def p_short(ax):
    ax.add_patch(FancyBboxPatch((14, 54), 72, 22, boxstyle="round,pad=0,rounding_size=2", fc="#fff3bf",
                                ec="#e0b000"))
    ax.text(50, 70, "battery OUT, switch ON, USB unplugged", ha="center", fontsize=7.4, color=INK, weight="bold")
    ax.text(50, 63, "BAT_SW -> GND :  NOT near 0 Ω", ha="center", fontsize=7.6, color=INK, family="DejaVu Sans Mono")
    ax.text(50, 57.5, "3V3    -> GND :  NOT near 0 Ω", ha="center", fontsize=7.6, color=INK,
            family="DejaVu Sans Mono")
    meter(ax, 35, 20, "Ω  (200 kΩ)", "> 100 Ω")


def p_board(ax):
    p = module(ax, 30, 48, 40, 30, "HW-747", PCB_BLACK, ["5V", "GND", "3V3"], side="T")
    ax.add_patch(Rectangle((44, 78), 12, 5, fc="#bbb", ec="#555"))
    ax.text(50, 86, "USB-C plugged in", ha="center", fontsize=7, color="#555")
    com, v = meter(ax, 35, 18, "DC V  (20 V)", "3.3 / 5.0 V")
    ax.plot([v[0], v[0], 86, 86, p["3V3"][0]], [v[1], 40, 40, 82, 82], color=PROBE_RED, lw=2.4)
    ax.plot([p["3V3"][0], p["3V3"][0]], [82, p["3V3"][1]], color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 14, 14, p["GND"][0], p["GND"][0]], [com[1], 40, 40, 84, 84, p["GND"][1]],
            color=PROBE_BLACK, lw=2.4)


def p_touch(ax):
    p = module(ax, 30, 50, 40, 24, "TTP223", PCB_RED, ["I/O", "GND", "VCC"], side="T")
    ax.add_patch(Circle((62, 58.5), 4, fc="none", ec="white", lw=1.2))
    ax.text(50, 46, "VCC 3.3 V, GND", ha="center", fontsize=7, color="#555")
    com, v = meter(ax, 35, 20, "DC V  (20 V)", "0 V / 3.3 V")
    ax.plot([v[0], v[0], 86, 86, p["I/O"][0], p["I/O"][0]], [v[1], 40, 40, 82, 82, p["I/O"][1]],
            color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 14, 14, p["GND"][0], p["GND"][0]], [com[1], 40, 40, 85, 85, p["GND"][1]],
            color=PROBE_BLACK, lw=2.4)


def p_divider(ax):
    ax.plot([50, 50], [80, 44], color="#7048e8", lw=2)
    for y0 in (66, 50):
        ax.add_patch(Rectangle((47, y0), 6, 10, fc="#e8d5a8", ec="#333"))
        ax.text(56, y0 + 5, "330k", fontsize=7, va="center")
    ax.text(50, 82, "BAT_SW", ha="center", fontsize=7, color=W_RED, weight="bold")
    ax.text(50, 40.5, "GND (frame)", ha="center", fontsize=7, color=INK)
    ax.add_patch(Circle((50, 63), 1.3, fc="#7048e8"))
    ax.text(38, 63, "GPIO2", fontsize=7, va="center", color="#7048e8", weight="bold", ha="right")
    com, v = meter(ax, 62, 20, "DC V  (20 V)", "cell / 2")
    ax.plot([v[0], v[0], 74, 74, 50], [v[1], 40, 40, 63, 63], color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 50], [com[1], 44, 44], color=PROBE_BLACK, lw=2.4)


def p_speaker(ax):
    ax.add_patch(Circle((50, 64), 11, fc="#444", ec="#111", lw=1.2))
    ax.add_patch(Circle((50, 64), 5, fc="#888", ec="#111", lw=0.8))
    ax.text(50, 79, "20 mm speaker (off the amp)", ha="center", fontsize=7, color="#555")
    com, v = meter(ax, 35, 20, "Ω  (200 Ω)", "6 - 9 Ω")
    ax.plot([v[0], v[0], 62, 62], [v[1], 44, 44, 58], color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 38, 38], [com[1], 44, 44, 58], color=PROBE_BLACK, lw=2.4)


def p_modules(ax):
    module(ax, 6, 52, 27, 20, "OLED", PCB_BLUE, ["GND", "VCC"], side="T")
    module(ax, 37, 52, 27, 20, "INMP441", PCB_PURPLE, ["GND", "VDD"], side="T")
    module(ax, 68, 52, 27, 20, "MAX98357", PCB_PURPLE, ["GND", "VIN"], side="T")
    meter(ax, 35, 20, "Ω  (200 kΩ)", "> 1 kΩ")


def p_frame(ax):
    ax.add_patch(Rectangle((18, 50), 64, 26, fc="none", ec=BRASS, lw=4))
    for x in (30, 50, 70):
        ax.add_patch(Circle((x, 50), 2, fc="#e9c46a", ec="#7a5c12"))
    ax.text(50, 64, "every GND pad <-> frame", ha="center", fontsize=7.6, color=INK, weight="bold")
    com, v = meter(ax, 35, 18, "continuity  )))", "BEEP")
    ax.plot([v[0], v[0], 70, 70], [v[1], 42, 42, 48], color=PROBE_RED, lw=2.4)
    ax.plot([com[0], com[0], 18, 18], [com[1], 42, 42, 63], color=PROBE_BLACK, lw=2.4)


PANELS = [
    ("Battery cell", p_cell, "3.0-4.2 V.  Under 3.6 V: charge it first.", "0 V: protection tripped - charge it on the TP4056."),
    ("Resistors (each one)", p_resistor, "Both 330 kΩ: 314-347 kΩ (±5 %).", "1 MΩ = brown-black-green: wrong part."),
    ("Slide switch", p_switch, "Middle + one side: beeps in one position only.", "Beeps both ways / never: broken switch."),
    ("TP4056 output", p_tp4056, "OUT+/OUT- = cell volts (±0.05 V). USB in: red\nLED = charging, blue/green = full.",
     "0 V with a good cell: B+/B- swapped or protection off."),
    ("Short check (BEFORE power)", p_short, "Readings climb or stay high: OK.",
     "0-10 Ω: a short. Find it before the battery goes in!"),
    ("HW-747 on USB", p_board, "3V3 pin 3.25-3.35 V, 5V pin 4.7-5.1 V.", "3V3 low / hot board: short on 3V3."),
    ("3.3 V LDO", p_ldo, "OUT 3.25-3.35 V with a 3.5-4.2 V cell.", "OUT = IN: wrong pinout.  0 V: IN/GND swapped."),
    ("Battery divider (powered)", p_divider, "GPIO2 = cell / 2: 1.5-2.1 V.", "= cell volts: bottom 330k missing.  > 2.5 V: wrong."),
    ("TTP223 touch", p_touch, "I/O 0 V free, ~3.3 V touched (red LED on).", "Inverted: pad A bridged.  Toggles: pad B bridged."),
    ("Speaker", p_speaker, "6-9 Ω, and a soft click as you touch the probes.", "Open (OL): broken coil or lead."),
    ("Module power pins", p_modules, "VCC/VDD/VIN to GND (unpowered): > 1 kΩ each.", "Near 0 Ω: damaged module - don't power it."),
    ("Brass frame = GND", p_frame, "Every GND pad and B-/OUT- beeps to the frame.", "No beep: cold solder joint - reflow it."),
]


def multimeter_sheet():
    fig = plt.figure(figsize=(18, 14.6), dpi=120, facecolor=BG)
    fig.text(0.02, 0.968, "COCO VOICE S3 HW-747  -  multimeter checks", fontsize=22, weight="bold", color=INK)
    fig.text(0.02, 0.946, "Check each part loose (1-4, 10, 11), then while building (5-9, 12).  Red probe in V/Ω, "
             "black probe in COM.  Green = good reading, red = what a bad reading means.", fontsize=11, color="#555")
    cols, rows = 4, 3
    for i, (title, draw, ok, bad) in enumerate(PANELS):
        r, c = divmod(i, cols)
        panel(fig, [0.01 + c * 0.2475, 0.635 - r * 0.31, 0.24, 0.30], i + 1, title, draw, ok, bad)
    fig.savefig(out_path("coco_voice_s3_hw747_esp32s3_test_multimeter.png"), facecolor=BG)
    plt.close(fig)
    print("wrote coco_voice_s3_hw747_esp32s3_test_multimeter.png")


STEPS = [
    ("Loose parts", "multimeter", "Checks 1-4, 10, 11 on the multimeter sheet.", "cell 3.0-4.2 V, 330k x2,\nspeaker 6-9 Ω",
     "#e7f5ff"),
    ("HW-747 alone", "USB", "Flash tests/coco_hw747_test.\nSerial Monitor 115200, type 1.", "[1] red, green, blue", "#e7f5ff"),
    ("+ 1.3\" OLED", "USB", "VCC 3V3, GND, SDA G5, SCL G6.\nType 2.", "device at 0x3C <- OLED\neyes on screen", "#e7f5ff"),
    ("+ TTP223", "USB", "VCC 3V3, GND, I/O G1.\nType 3, touch the pad.", "TOUCHED / free\nPASS: n touch(es)", "#e7f5ff"),
    ("+ amp + speaker", "USB +\ncell", "DIN G4, BCLK G7, LRC G8, SD G43,\nVIN BAT_SW (or 5V for now). Type 5.",
     "three rising beeps", "#fff4e6"),
    ("+ INMP441", "USB", "SD G9, SCK G7, WS G8, L/R GND,\nVDD 3V3. Type 6, clap.", "level jumps when you\nclap - PASS",
     "#e7f5ff"),
    ("Power chain", "cell", "Cell -> TP4056 -> switch -> LDO -> 3V3.\nChecks 5-9, then type 4.",
     "battery = multimeter\n(±0.1 V) - PASS", "#ebfbee"),
    ("Coco firmware", "USB", "Flash coco_voice_s3_hw747_esp32s3-\nmerged.bin at 0x0.", "QR code, then\n\"Hi ESP\" answers",
     "#ebfbee"),
]


def order_sheet():
    fig = plt.figure(figsize=(18, 9.6), dpi=120, facecolor=BG)
    fig.text(0.02, 0.94, "COCO VOICE S3 HW-747  -  bench test order", fontsize=22, weight="bold", color=INK)
    fig.text(0.02, 0.905, "Add one part at a time on a breadboard or with clip leads, run its test, then add the "
             "next.  Only then solder everything into the brass frame.", fontsize=11, color="#555")
    ax = fig.add_axes([0, 0, 1, 0.88])
    ax.set_xlim(0, 200)
    ax.set_ylim(0, 100)
    ax.axis("off")
    w, h = 44, 40
    for i, (title, power, do, result, fc) in enumerate(STEPS):
        r, c = divmod(i, 4)
        x, y = 4 + c * 49, 54 - r * 50
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=2", fc=fc, ec="#adb5bd",
                                    lw=1.2))
        ax.add_patch(Circle((x + 5, y + h - 5), 3.4, fc=BRASS, ec="none"))
        ax.text(x + 5, y + h - 5, str(i), ha="center", va="center", color="white", weight="bold", fontsize=11)
        ax.text(x + 10.5, y + h - 5, title, va="center", fontsize=12, weight="bold", color=INK)
        ax.text(x + w - 2.5, y + h - 5, power, va="center", ha="right", fontsize=8, color=W_BLUE, weight="bold")
        ax.text(x + 3, y + h - 12, do, va="top", fontsize=8.6, color=INK, linespacing=1.4)
        ax.add_patch(FancyBboxPatch((x + 3, y + 3), w - 6, 11, boxstyle="round,pad=0,rounding_size=1.5",
                                    fc="#212529", ec="none"))
        ax.text(x + 5, y + 8.5, result, va="center", fontsize=8.2, color="#8ce99a", family="DejaVu Sans Mono")
        if c < 3:
            ax.add_patch(FancyArrowPatch((x + w + 0.5, y + h / 2), (x + w + 4.5, y + h / 2), arrowstyle="-|>",
                                         mutation_scale=16, color=BRASS, lw=2))
    ax.add_patch(FancyArrowPatch((4 + 3 * 49 + w / 2, 53.5), (4 + w / 2, 44.5), arrowstyle="-|>", mutation_scale=16,
                                 color=BRASS, lw=2, connectionstyle="arc3,rad=0.06"))
    ax.text(100, 1.5, "Blue = USB only.  Orange = the amp needs more current: power VIN from the cell (BAT_SW) or "
            "the 5V pin while testing.  Green = runs from the cell.", ha="center", fontsize=9.5, color="#555")
    fig.savefig(out_path("coco_voice_s3_hw747_esp32s3_test_order.png"), facecolor=BG)
    plt.close(fig)
    print("wrote coco_voice_s3_hw747_esp32s3_test_order.png")


if __name__ == "__main__":
    multimeter_sheet()
    order_sheet()
