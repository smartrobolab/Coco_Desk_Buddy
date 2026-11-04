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
TW = RY.TW


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
        s.add(r["base"].translate([30, 70, -4]), BASEC, 20000)
        installed, new = set(), set()
    for key in ("body", "bezel", "chassis", "cover"):
        if key in installed:
            add(key, r[key], colors[key] if key in ("body", "cover") else GREY)
        if key in new:
            add(key, r[key], colors[key], move)
    if base or "base" in installed or "base" in new:
        s.add(r["base"], BASEC, 20000)
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
    """The feet holes, the space between the legs and the square recess in the floor, explained."""
    W, H = 1650, 980
    img = Image.new("RGB", (W, H), BG)
    hi = (230, 120, 40)
    # 1: from below
    s = Scene()
    s.add(world(r["body"]), WHITE, 60000)
    img.paste(s.render(550, 640, az=-20, el=-55, scale=5.6), (0, 100))
    # 2: cut through the middle (x = 0): the recess in the floor and its 2 mm hole
    s2 = Scene()
    s2.add(world(r["body"] ^ Y.box(0, 300, -300, 300, -300, 300)), WHITE, 60000)
    s2.add(world(Y.box_c((0, 27.25, 16.0), (23.0, 1.5, 23.0)) ^ Y.box(0, 300, -300, 300, -300, 300)), hi, None)
    s2.add(world(Y.M.cylinder(4.0, 1.0, 1.0).rotate([-90, 0, 0]).translate([0, 27.9, 16.0])
                 ^ Y.box(0, 300, -300, 300, -300, 300)), (220, 60, 40), None)
    img.paste(s2.render(550, 640, az=-90, el=8, scale=6.6, center=np.array([0.0, 30.0])), (550, 100))
    # 3: cut through the left leg (x = 15): the hole runs from the sole into the body
    s3 = Scene()
    keep = Y.box(15.0, 300, -300, 300, -300, 300)
    s3.add(world(r["body"] ^ keep), WHITE, 60000)
    chan = (Y.M.cylinder(3.5, 4.4, 4.4, 40) + Y.M.cylinder(11.0, 3.0, 3.0, 40).translate([0, 0, 3.5]))
    chan = chan.rotate([90, 0, 0]).translate([15.0, 37.36, 16.0])  # from the sole (y 37.36) up into the body
    s3.add(world(chan ^ keep), hi, None)
    img.paste(s3.render(550, 640, az=-90, el=8, scale=6.6, center=np.array([0.0, 30.0])), (1100, 100))
    d = ImageDraw.Draw(img)
    d.text((28, 18), "The bottom of the yeti", font=font(34, True), fill=INK)
    d.text((30, 62), "Features of the original yeti design; Coco does not need them", font=font(20),
           fill=(90, 96, 110))
    for i, t in enumerate(["1. From below", "2. Cut through the middle", "3. Cut through a leg"]):
        d.text((i * 550 + 275, 112), t, font=font(20, True), fill=INK, anchor="mm")
    texts = [
        "Between the legs the yeti is open underneath: an arch, so it stands on its two feet only. In each sole "
        "is a round hole; in the middle, a tiny 2 mm hole.",
        "Inside, at the feet end, the floor has a square recess 23 x 23 mm, 1.5 mm deep (orange), with the "
        "2 mm hole (red) going out through the bottom. It most likely held the 21 x 23 mm vibration motor in "
        "the original design. In Coco's layouts it stays empty.",
        "Each leg hole (orange) is 8.8 mm wide for 3.5 mm at the sole, then 6 mm wide right up into the body. "
        "They are 30 mm apart, over the two 8.8 mm marks on the base: for screws, pins or 8 x 3 mm magnets "
        "that hold the yeti on its base. Optional.",
    ]
    for i, t in enumerate(texts):
        wrap(d, i * 550 + 24, 760, 505, t, font(18), lh=23)
    img.save(os.path.join(IMG, "coco_yeti_bottom.png"), optimize=True)


def main():
    zpath = sys.argv[1] if len(sys.argv) > 1 else os.path.join(Y.ROOT, "yeti_v1.2_stls.zip")
    src = Y.load_zip(zpath)
    r = Y.build(src, "13")
    P = {p["name"]: p for p in r["parts"]}
    bottom_picture(r)

    back_view = (200, 32)     # looking into the open back
    bat = "LiPo 403035 (4.0 x 30 x 35 mm)" if V403 else "LiPo 503030 (5.0 x 30 x 30 mm)"
    vibe = "the black 21 x 23 mm vibration motor module" if V403 else "the vibration motor module"
    esp_where = "at the head end of the back" if V403 else "in the middle, at the feet end of the back"
    sw_where = "at the feet end" if V403 else "next to the ESP32-C3"
    vibe_vcc = ("VCC of the vibration module: to the battery AFTER the switch (it is a 5 V motor); IN to GPIO10; "
                "GND to GND." if V403 else "VCC of the vibration module to 3V3, IN to GPIO10, GND to GND.")

    steps = [
        ("Print and get the parts ready",
         "Print: body (white, back down, supports for arms and horns), bezel (blue, front down), chassis (screen "
         "ribs down), back cover (white, flat inside down), plain base (flat). Remove the supports and test that the "
         "bezel presses into the window. You also need: ESP32-C3 Super Mini, OLED 1.3\" or 0.96\" (I2C), TTP223, "
         "MPU6050 GY-521, " + vibe + ", " + bat + ", TP4056 mini USB-C charger, slide switch, ME6211 3.3 V "
         "LDO + 2 x 1 uF, 1 M + 330 k + 100 nF, 2 x M2 x 8 self-tapping screws, thin wire, double-sided tape, glue.",
         set(), set(), (0, 0, 0), (-25, 22), dict(scale=3.6, layout=True)),
        ("Bezel into the face",
         "Press the bezel into the window from the front until it sits flush (it is a firm press fit, as in the "
         "original yeti). Use the 1.3\" bezel (bigger window) for the 1.3\" screen, the zip's bezel for the 0.96\".",
         {"body"}, {"bezel"}, (0, 0, 22), (-35, 12), {}),
        ("Touch sensor into the head",
         "Solder three wires (VCC, GND, I/O) to the TTP223. Lay the yeti face down and slide the sensor into the "
         "slot inside the head, pad side towards the top of the head, against the 2 mm wall under the hair. "
         "A drop of glue. Leave its A/B jumpers open.",
         {"body", "bezel"}, {"touch"}, (0, 14, -26), back_view, {}),
        ("Screen into the face",
         "Solder four thin wires to the OLED pins (no pin header: it would not fit). Still face down, drop the "
         "OLED in, glass towards the bezel, pins at the head end. It sits in its pocket with 1 mm all round; the "
         "chassis presses it against the bezel in step 6.",
         {"body", "bezel", "touch"}, {"oled", "oled_glass"}, (0, 0, -34), back_view, {}),
        ("Motor and motion sensor on the chassis",
         "Glue " + vibe + " into the rim on the back of the chassis plate (motor side out). Glue the MPU6050 flat "
         "onto the fin: board against the fin, chip facing away from it, pins down. Solder its wires (VCC, GND, "
         "SDA, SCL, INT) first.",
         {"chassis"}, {"vibe", "imu"}, (0, 0, -18), (215, 30), dict(scale=7.5)),
        ("Chassis into the body",
         "Slide the chassis in from the back until its plate rests on the back of the screen. The four legs point "
         "to the back cover. Lead the screen and touch wires past the plate's open side at the head end.",
         {"body", "bezel", "touch", "oled", "oled_glass"}, {"chassis", "vibe", "imu"}, (0, 0, -32), back_view, {}),
        ("Battery, charger and switch on the back cover",
         "Lay the back cover inside up. Stick the " + bat + " flat on it with double-sided tape, the TP4056 "
         "charger on top of the battery, and the slide switch " + sw_where + " with its knob through the slot. "
         "Solder: battery + / - to TP4056 B+ / B-, OUT+ to the switch, switch to the LDO input.",
         {"cover"}, {"battery", "charger", "switch"}, (0, 0, 14), (-15, 38), dict(scale=6.5)),
        ("ESP32-C3",
         "The ESP32-C3 stands " + esp_where + ": USB-C end down into the back cover's slot, the other end in the "
         "chassis cup. Solder all wires now (see the wiring diagram): OLED and MPU6050 SDA/SCL to GPIO4/GPIO5, "
         "touch to GPIO0, MPU6050 INT to GPIO3, vibration IN to GPIO10, battery sense (1 M / 330 k) to GPIO1, "
         "ESP32 5V to TP4056 IN+, LDO 3.3 V to 3V3. " + vibe_vcc,
         {"body", "bezel", "touch", "oled", "oled_glass", "chassis", "vibe", "imu"}, {"esp32"}, (0, 0, -30),
         back_view, {}),
        ("Close the back",
         "Fold the cover with the battery onto the back: the ESP32-C3's USB-C goes into its slot, the chassis legs "
         "rest on the cover. Push the wires in so nothing is pinched. Two M2 x 8 self-tapping screws into the "
         "bosses. Do not overtighten.",
         {"body", "bezel", "touch", "oled", "oled_glass", "chassis", "vibe", "imu", "esp32"},
         {"cover", "battery", "charger", "switch"}, (0, 0, -24), (150, 22), {}),
        ("Onto the base",
         "Stand the yeti on the plain base, feet over the two round marks. Optional: fix it with the leg holes "
         "(8 x 3 mm magnets in the soles and under the base, or pins / screws; see coco_yeti_bottom.png).",
         {"body", "bezel", "cover"}, {"base"}, (0, 0, 0), (-28, 14), dict(base=True, eyes=True, scale=5.4,
                                                                         center=np.array([0.0, 32.0]))),
        ("Flash and set up",
         "Switch on and plug USB-C into the back. Flash coco_yeti_c3_13-merged.bin (1.3\") or coco_yeti_c3_096 "
         "(0.96\") at 0x0. Set up Wi-Fi from your phone (QR code), then Menu > Settings > Motion setup: stand the "
         "yeti upright and tap its head, lay it on its back and tap again. Done: pet it, shake it, play games.",
         {"body", "bezel", "cover"}, set(), (0, 0, 0), (152, 16), dict(base=True, scale=5.4,
                                                                       center=np.array([0.0, 32.0]))),
    ]
    paths = []
    for i, (title, text, installed, new, move, view, opt) in enumerate(steps, 1):
        print("step", i, title)
        paths.append(step_image(r, i, title, text, installed, new, move, view, **opt))

    # ASSEMBLY.md next to the README
    name = "Coco Yeti, 403035 battery" if V403 else "Coco Yeti"
    lines = [f"# {name}: assembly, part by part", "",
             "Every step shows the new part in colour, sliding in along the red arrow; parts already fitted are "
             "grey. Pictures come from the real STL files (`tools/render_steps.py`).", "",
             "Wiring: [images/coco_yeti_wiring.png](images/coco_yeti_wiring.png). "
             "The bottom of the yeti (feet holes, square pocket): [images/coco_yeti_bottom.png]"
             "(images/coco_yeti_bottom.png).", ""]
    for i, (title, text, *_rest) in enumerate(steps, 1):
        lines += [f"## Step {i}. {title}", "", f"![Step {i}](images/assembly/step_{i:02d}.png)", "", text, ""]
    lines += ["## The bottom of the yeti", "", "![Bottom](images/coco_yeti_bottom.png)", "",
              "The feet holes and the square pocket come from the original yeti design. Coco does not need "
              "them; see the picture for what they are for.", ""]
    with open(os.path.join(os.path.dirname(HERE), "ASSEMBLY.md"), "w") as f:
        f.write("\n".join(lines))
    print("done:", len(paths), "steps")


if __name__ == "__main__":
    main()
