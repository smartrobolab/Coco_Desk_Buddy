#!/usr/bin/env python3
"""Step-by-step assembly pictures for a Coco Yeti version, from the real STL geometry.

    python3 coco_touch_advance/yeti/tools/render_steps.py [path/to/yeti_v1.2_stls.zip]
    python3 coco_touch_advance/yeti_403035/tools/render_steps.py

Writes ../images/assembly/step_NN.png (one picture per step, the new part in colour
sliding in along an arrow, the parts already fitted in grey), ../images/coco_yeti_bottom.png
(the feet holes and the square pocket in the floor) and ../ASSEMBLY.md.
Same packages as make_yeti.py / render_yeti.py.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_yeti as Y  # noqa: E402
import render_yeti as RY  # noqa: E402
from render_yeti import Scene, font, hexc, INK, BG, FUR, BASEC  # noqa: E402

FOLDER = os.path.basename(os.path.dirname(HERE))       # "yeti" or "yeti_403035"
V403 = FOLDER == "yeti_403035"
IMG = os.path.join(os.path.dirname(HERE), "images")
STEPS = os.path.join(IMG, "assembly")
GREY = (198, 203, 210)
WHITE = (238, 240, 244)
CHASSIS = (150, 190, 235)
BEZEL = (40, 90, 215)
TW = Y.TW()


def world(m):
    return m.transform(TW)


def wpt(p):
    return np.array(TW)[:, :3] @ np.asarray(p, float) + np.array(TW)[:, 3]


def wvec(v):
    return np.array(TW)[:, :3] @ np.asarray(v, float)


def arrow(d, a, b, color=(220, 60, 40)):
    d.line([a, b], fill=color, width=5)
    v = np.array(b, float) - np.array(a, float)
    n = np.linalg.norm(v)
    if n < 1:
        return
    v /= n
    p = np.array([-v[1], v[0]])
    tip = np.array(b, float)
    d.polygon([tuple(tip), tuple(tip - 18 * v + 9 * p), tuple(tip - 18 * v - 9 * p)], fill=color)


def wrap(d, x, y, w, text, f, fill=INK, lh=26):
    for para in text.split("\n"):
        line = ""
        for word in para.split():
            if d.textlength((line + " " + word).strip(), font=f) > w:
                d.text((x, y), line, font=f, fill=fill)
                y += lh
                line = word
            else:
                line = (line + " " + word).strip()
        d.text((x, y), line, font=f, fill=fill)
        y += lh
    return y


def step_image(r, n, title, text, installed, new, move, view, scale=6.0, cut=None, extra=None, base=False,
               center=None, eyes=False, layout=False):
    """installed / new: keys of printed parts ('body', 'bezel', 'chassis', 'cover', 'base') and part names.
    move: insertion offset (body coordinates, mm) for the new things; they are drawn moved by it."""
    s = Scene()
    keep = Y.box(cut, 300, -300, 300, -300, 300) if cut is not None else None

    def add(key, m, color, off=(0, 0, 0)):
        if keep is not None and key != "base":
            m = m ^ keep
        if m.is_empty():
            return
        mm = m if key == "base" else world(m.translate(list(off)))
        s.add(mm, color, 60000)

    colors = {"body": WHITE, "bezel": BEZEL, "chassis": CHASSIS, "cover": WHITE, "base": BASEC}
    if layout:  # the printed parts side by side, standing as they go into the yeti
        s.add(world(r["body"]), WHITE, 60000)
        s.add(world(r["bezel"]).translate([-62, 0, 22]), BEZEL, 30000)
        s.add(world(r["chassis"]).translate([62, 0, 12]), CHASSIS, 30000)
        s.add(world(r["cover"]).translate([118, 0, 12]), WHITE, 30000)
        s.add(Y.base_world(r["base"]).translate([30, 75, -4]), BASEC, 20000)
        installed, new = set(), set()
    for key in ("body", "bezel", "chassis", "cover"):
        if key in installed:
            add(key, r[key], colors[key] if key in ("body", "cover") else GREY)
        if key in new:
            add(key, r[key], colors[key], move)
    if base or "base" in installed or "base" in new:
        s.add(Y.base_world(r["base"]), BASEC, 20000)
    for p in r["parts"]:
        if p["name"] in installed:
            add(p["name"], Y.box_c(p["c"], p["s"]), GREY)
        if p["name"] in new:
            add(p["name"], Y.box_c(p["c"], p["s"]), hexc(p["color"]), move)
    if eyes:
        RY.eyes(s, r)
    if extra:
        extra(s)
    W, H = 1100, 760
    im = s.render(W, H, az=view[0], el=view[1], scale=scale, center=center)
    img = Image.new("RGB", (W, H + 300), BG)
    img.paste(im, (0, 70))
    d = ImageDraw.Draw(img)
    d.text((26, 14), f"Step {n}. {title}", font=font(32, True), fill=INK)
    # arrows from where the new parts start to where they go
    if new and any(abs(v) > 0.1 for v in move):
        for key in new:
            if key in ("body", "bezel", "chassis", "cover"):
                bb = r[key].bounding_box()
                c = [(bb[0] + bb[3]) / 2, (bb[1] + bb[4]) / 2, (bb[2] + bb[5]) / 2]
            else:
                p = [q for q in r["parts"] if q["name"] == key]
                if not p:
                    continue
                c = p[0]["c"]
            a = RY.project(s, W, H, wpt([c[i] + move[i] * 0.85 for i in range(3)]))
            b = RY.project(s, W, H, wpt([c[i] + move[i] * 0.15 for i in range(3)]))
            arrow(d, (a[0], a[1] + 70), (b[0], b[1] + 70))
    d.line([(26, H + 84), (W - 26, H + 84)], fill=(200, 205, 214), width=2)
    wrap(d, 30, H + 100, W - 60, text, font(21))
    os.makedirs(STEPS, exist_ok=True)
    path = os.path.join(STEPS, f"step_{n:02d}.png")
    img.save(path, optimize=True)
    return path


def bottom_picture(r):
    """The heavy legs: solid, an M3 pilot hole in each sole, screwed to the base from below."""
    S = Y.S
    W, H = 1650, 980
    img = Image.new("RGB", (W, H), BG)
    hi = (230, 120, 40)
    s = Scene()
    s.add(world(r["body"]), WHITE, 60000)
    img.paste(s.render(550, 640, az=-20, el=-55, scale=5.6 / S), (0, 100))
    # cut through a leg, with the base and the screw
    s3 = Scene()
    fx, fz = Y.FEET[1]
    keep = Y.box(fx * S, 300, -300, 300, -300, 300)
    s3.add(world(r["body"] ^ keep), WHITE, 60000)
    s3.add(Y.base_world(r["base"]) ^ Y.box(-300, -fx * S, -300, 300, -300, 300), BASEC, 20000)
    foot = wpt([fx * S, Y.SOLE_Y * S, fz * S])
    screw = (Y.M.cylinder(12.0, 1.5, 1.5, 24) + Y.M.cylinder(1.7, 2.8, 1.5, 24)).translate([foot[0], foot[1], 0.2])
    s3.add(screw ^ Y.box(-300, foot[0], -300, 300, -300, 300), hi, None)
    img.paste(s3.render(550, 640, az=-90, el=8, scale=7.0 / S, center=np.array([0.0, 24.0 * S])), (550, 100))
    # the recess in the floor (unused)
    s2 = Scene()
    k2 = Y.box(0, 300, -300, 300, -300, 300)
    s2.add(world(r["body"] ^ k2), WHITE, 60000)
    s2.add(world(Y.box_c((0, 27.25 * S, 16.0 * S), (23.0 * S, 1.5 * S, 23.0 * S)) ^ k2), (150, 150, 160), None)
    img.paste(s2.render(550, 640, az=-90, el=8, scale=6.6 / S, center=np.array([0.0, 30.0 * S])), (1100, 100))
    d = ImageDraw.Draw(img)
    d.text((28, 18), "Heavy legs and the base", font=font(34, True), fill=INK)
    d.text((30, 62), "The legs are solid; two M3 screws from under the base hold the yeti", font=font(20),
           fill=(90, 96, 110))
    for i, t in enumerate(["1. From below: solid soles", "2. Cut through a leg", "3. Cut through the middle"]):
        d.text((i * 550 + 275, 112), t, font=font(20, True), fill=INK, anchor="mm")
    texts = [
        "The zip's stepped holes in the legs are filled, so both legs are solid plastic (heavier, and they stand "
        "firm). Each sole has one 2.5 mm pilot hole for an M3 screw.",
        "An M3 x 12 flat-head screw (orange) goes up through the countersunk hole in the base into the leg. "
        "Two screws make yeti and base one piece: it stands on the 100 mm base and does not tip over.",
        "Between the legs the yeti is open underneath (an arch). Inside, the floor has a shallow 23 x 23 mm recess "
        "(grey) from the original design; it stays empty.",
    ]
    for i, t in enumerate(texts):
        wrap(d, i * 550 + 24, 760, 505, t, font(18), lh=23)
    img.save(os.path.join(IMG, "coco_yeti_bottom.png"), optimize=True)


VERSIONS = {
    "yeti_403048": dict(
        name="Coco Yeti, 403048 battery (scaled 1.12x)",
        battery="LiPo 403048 (4.0 x 30 x 48 mm)",
        battery_step="Lay the back cover inside up. Stick the 403048 LiPo flat on it with double-sided tape. The "
                     "TP4056 charger stands on its long edge right of the battery, near the head end: USB-C end down "
                     "in its own slot in the back cover, so you charge from outside; a drop of glue. The slide switch "
                     "goes in the same strip, knob through its slot. Solder: battery + / - to TP4056 B+ / B-, OUT+ "
                     "to the switch, switch to the LDO input.",
        esp_step="The ESP32-C3 stands on its long edge against the left side of the back: USB-C end down into the "
                 "back cover's slot (on the left), the other end in the chassis cup.",
        files="coco_yeti_c3_13-merged.bin (1.3\") or coco_yeti_c3_096 (0.96\")"),
    "yeti_2x502030": dict(
        name="Coco Yeti, 2 x 502030 battery (original size)",
        battery="2 x LiPo 502030 (5 x 20 x 30 mm, with protection boards)",
        battery_step="Charge both cells to the same voltage (within 0.05 V) first, then wire them in parallel: + to +, "
                     "- to -. Tape them on top of each other, 30 mm side across, on the head end of the back cover. "
                     "The TP4056 charger stands across the feet end: USB-C end down in its own slot in the back "
                     "cover, so you charge from outside; a drop of glue. The slide switch goes next to the ESP32's "
                     "slot, knob through its slot. "
                     "Solder: cells + / - to TP4056 B+ / B-, OUT+ to the switch, switch to the LDO input.",
        esp_step="The ESP32-C3 stands at the feet end of the back: USB-C end down into the back cover's slot (the "
                 "original one), the other end in the chassis cup.",
        files="coco_yeti_c3_13-merged.bin (1.3\") or coco_yeti_c3_096 (0.96\")"),
}


def main():
    zpath = sys.argv[1] if len(sys.argv) > 1 else os.path.join(Y.ROOT, "yeti_v1.2_stls.zip")
    src = Y.load_zip(zpath)
    r = Y.build(src, "13")
    bottom_picture(r)
    V = VERSIONS[FOLDER]
    S = Y.S
    back_view = (200, 32)
    wires = ("Solder all wires now (see the wiring diagram): OLED and MPU6050 SDA/SCL to GPIO4/GPIO5, touch to GPIO0, "
             "MPU6050 INT to GPIO3, vibration IN to GPIO10, battery sense (1 M / 330 k) to GPIO1, LDO 3.3 V to "
             "3V3 and to the VCC of the OLED, MPU6050 and touch. Leave the ESP32's 5V pin unconnected (the charger "
             "has its own USB-C)." + " "
             " The vibration module's "
             "VCC goes to the battery AFTER the switch (it is a 5 V motor).")
    steps = [
        ("Print and get the parts ready",
         "Printed (see PRINTING.md): body, bezel (1.3\" or 0.96\"), chassis (1.3\" or 0.96\"), back cover, base. "
         "Remove the supports and test that the bezel presses into the window. Electronics: ESP32-C3 Super Mini, "
         "OLED 1.3\" SH1106 or 0.96\" SSD1306 (I2C, no pin header), TTP223, MPU6050 GY-521, the black 21 x 23 mm "
         "vibration motor module, " + V["battery"] + ", TP4056 mini USB-C charger, slide switch SS12D00, ME6211 "
         "3.3 V LDO + 2 x 1 uF, 1 M + 330 k + 100 nF. Screws: 2 x M2 x 8 self-tapping (back cover), 2 x M3 x 12 "
         "flat-head (base). Thin wire, double-sided tape, a little glue.",
         set(), set(), (0, 0, 0), (-25, 22), dict(scale=3.6 / S, layout=True)),
        ("Bezel into the face",
         "Press the bezel into the window from the front until it sits flush (a firm press fit, as in the "
         "original yeti). Use the bezel for your screen size: its window matches the screen.",
         {"body"}, {"bezel"}, (0, 0, 22), (-35, 12), dict(scale=6.0 / S)),
        ("Touch sensor into the head",
         "Solder three wires (VCC, GND, I/O) to the TTP223. Lay the yeti face down and slide the sensor into the "
         "slot inside the head, pad side towards the top of the head, against the 2 mm wall under the hair. "
         "A drop of glue. Leave its A/B jumpers open.",
         {"body", "bezel"}, {"touch"}, (0, 14, -26), back_view, dict(scale=6.0 / S)),
        ("Screen into the face",
         "Solder four thin wires to the OLED pins. Still face down, drop the OLED in, glass towards the bezel, "
         "pins at the head end. It has 1 mm all round; the chassis presses it against the bezel in step 6.",
         {"body", "bezel", "touch"}, {"oled", "oled_glass"}, (0, 0, -34), back_view, dict(scale=6.0 / S)),
        ("Motor and motion sensor on the chassis",
         "Glue the 21 x 23 mm vibration module into the rim on the back of the chassis plate (motor side out). Glue "
         "the MPU6050 flat onto the fin: board against the fin, chip facing away from it, pins down. Solder their "
         "wires first (motor: VCC, GND, IN; MPU6050: VCC, GND, SDA, SCL, INT).",
         {"chassis"}, {"vibe", "imu"}, (0, 0, -18), (215, 30), dict(scale=7.0 / S)),
        ("Chassis into the body",
         "Slide the chassis in from the back until its plate rests on the back of the screen; the four legs point "
         "to the back cover. Lead the screen and touch wires through the notch at the head end of the plate.",
         {"body", "bezel", "touch", "oled", "oled_glass"}, {"chassis", "vibe", "imu"}, (0, 0, -32), back_view,
         dict(scale=6.0 / S)),
        ("Battery, charger and switch on the back cover", V["battery_step"],
         {"cover"}, {"battery", "charger", "switch"}, (0, 0, 14), (-15, 38), dict(scale=6.0 / S)),
        ("ESP32-C3 and wiring", V["esp_step"] + " " + wires,
         {"body", "bezel", "touch", "oled", "oled_glass", "chassis", "vibe", "imu"}, {"esp32"}, (0, 0, -30),
         back_view, dict(scale=6.0 / S)),
        ("Close the back",
         "Fold the cover with the battery onto the back: the ESP32-C3's USB-C goes into its slot, the chassis legs "
         "rest on the cover. Push the wires in so nothing is pinched. Two M2 x 8 self-tapping screws into the "
         "bosses; do not overtighten.",
         {"body", "bezel", "touch", "oled", "oled_glass", "chassis", "vibe", "imu", "esp32"},
         {"cover", "battery", "charger", "switch"}, (0, 0, -24), (150, 22), dict(scale=6.0 / S)),
        ("Screw it onto the base",
         "Stand the yeti on the base, each foot over a countersunk hole. From underneath, drive one M3 x 12 "
         "flat-head screw through each hole into the pilot hole in the sole (the legs are solid). Snug, not tight. "
         "Now yeti and base are one piece and it cannot tip over.",
         {"body", "bezel", "cover"}, {"base"}, (0, 0, 0), (-28, 14),
         dict(base=True, eyes=True, scale=5.2 / S, center=np.array([0.0, 32.0 * S]))),
        ("Flash and set up",
         "Charge: plug a USB-C cable into the charger's port on the back (the TP4056's red LED lights while "
         "charging, blue when full). Program: switch on and plug USB-C into the ESP32-C3's port. Flash " + V["files"] + " from coco_touch_advance/yeti/firmware/"
         "release at 0x0. Set up Wi-Fi from your phone (QR code), then Menu > Settings > Motion setup: stand the "
         "yeti upright and tap its head, lay it on its back and tap again. Done.",
         {"body", "bezel", "cover"}, set(), (0, 0, 0), (152, 16),
         dict(base=True, scale=5.2 / S, center=np.array([0.0, 32.0 * S]))),
    ]
    for i, (title, text, installed, new, move, view, opt) in enumerate(steps, 1):
        print("step", i, title)
        step_image(r, i, title, text, installed, new, [m * S for m in move], view, **opt)
    lines = [f"# {V['name']}: assembly, part by part", "",
             "Every step shows the new part in colour, sliding in along the red arrow; parts already fitted are "
             "grey. Pictures come from the real STL files (`tools/render_steps.py`). Printing: "
             "[PRINTING.md](PRINTING.md).", "",
             "Wiring: [images/coco_yeti_wiring.png](images/coco_yeti_wiring.png). Legs and base: "
             "[images/coco_yeti_bottom.png](images/coco_yeti_bottom.png).", ""]
    for i, (title, text, *_rest) in enumerate(steps, 1):
        lines += [f"## Step {i}. {title}", "", f"![Step {i}](images/assembly/step_{i:02d}.png)", "", text, ""]
    lines += ["## Legs and base", "", "![Legs and base](images/coco_yeti_bottom.png)", ""]
    with open(os.path.join(os.path.dirname(HERE), "ASSEMBLY.md"), "w") as f:
        f.write("\n".join(lines))
    print("done:", len(steps), "steps")


if __name__ == "__main__":
    main()
