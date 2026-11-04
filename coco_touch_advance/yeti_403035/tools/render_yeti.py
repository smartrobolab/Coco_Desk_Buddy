#!/usr/bin/env python3
"""Pictures of the Coco Yeti (the yeti from yeti_v1.2_stls.zip with Coco inside),
made from the real STL geometry.

    python3 coco_touch_advance/yeti_403035/tools/render_yeti.py [path/to/yeti_v1.2_stls.zip]

Needs numpy, pillow, trimesh, manifold3d (same as make_yeti.py) and
fast-simplification (optional, makes it faster). Writes ../images/coco_yeti_*.png.

A small software renderer (orthographic z-buffer, soft light, ink outlines), so no
Blender or OpenGL is needed.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_yeti as Y  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(os.path.dirname(HERE), "images")
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONTB = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
BG = (246, 247, 250)
INK = (40, 44, 52)
FUR = (236, 240, 246)
FUR2 = (214, 224, 238)
BASEC = (120, 140, 170)


def font(sz, bold=False):
    try:
        return ImageFont.truetype(FONTB if bold else FONT, sz)
    except OSError:
        return ImageFont.load_default()


def hexc(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mesh_arrays(m, target=None):
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3].astype(np.float64)
    t = np.asarray(mesh.tri_verts).astype(np.int64)
    if target and len(t) > target:
        try:
            import fast_simplification
            v2, t2 = fast_simplification.simplify(v.astype(np.float32), t.astype(np.int32), 1 - target / len(t))
            v, t = v2.astype(np.float64), t2.astype(np.int64)
        except Exception:
            pass
    return v, t


class Scene:
    def __init__(self):
        self.objs = []  # (verts, tris, color, id)

    def add(self, m, color, target=60000, offset=(0, 0, 0)):
        v, t = mesh_arrays(m, target)
        self.objs.append((v + np.array(offset), t, np.array(color, float), len(self.objs) + 1))
        return self

    def add_box(self, c, s, color, offset=(0, 0, 0)):
        return self.add(Y.box_c(c, s), color, None, offset)

    def render(self, W, H, az=-35, el=20, scale=None, center=None, ss=2, outline=True, light=None):
        """Orthographic view. az: degrees around Z (0 = looking from the front, -Y),
        el: degrees above the horizon."""
        a, e = math.radians(az), math.radians(el)
        # camera basis: view direction d (from camera into the scene)
        d = np.array([math.sin(a) * math.cos(e), math.cos(a) * math.cos(e), -math.sin(e)])
        right = np.array([math.cos(a), -math.sin(a), 0.0])
        up = np.cross(right, d)
        allv = np.concatenate([o[0] for o in self.objs])
        P = np.stack([allv @ right, allv @ up], 1)
        lo, hi = P.min(0), P.max(0)
        if center is None:
            center = (lo + hi) / 2
        if scale is None:
            scale = 0.9 * min(W / (hi[0] - lo[0]), H / (hi[1] - lo[1]))
        Wi, Hi = W * ss, H * ss
        sc = scale * ss
        zbuf = np.full((Hi, Wi), np.inf)
        img = np.zeros((Hi, Wi, 3))
        img[:] = BG
        ids = np.zeros((Hi, Wi), np.int32)
        L1 = light if light is not None else np.array([-0.45, -0.8, 0.55])
        L1 = L1 / np.linalg.norm(L1)
        L2 = np.array([0.7, -0.2, 0.3])
        L2 /= np.linalg.norm(L2)
        for v, t, col, oid in self.objs:
            sx = (v @ right - center[0]) * sc + Wi / 2
            sy = -(v @ up - center[1]) * sc + Hi / 2
            sz = v @ d
            A, B, C = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
            n = np.cross(B - A, C - A)
            nl = np.linalg.norm(n, axis=1)
            ok = nl > 1e-12
            n[ok] /= nl[ok, None]
            front = (n @ d) < 0
            shade = 0.50 + 0.42 * np.clip(n @ L1, 0, 1) + 0.14 * np.clip(n @ L2, 0, 1)
            rim = np.clip(1 - np.abs(n @ d), 0, 1) ** 3 * 0.10
            for k in np.nonzero(front & ok)[0]:
                i0, i1, i2 = t[k]
                x0, y0, x1, y1, x2, y2 = sx[i0], sy[i0], sx[i1], sy[i1], sx[i2], sy[i2]
                minx, maxx = int(max(min(x0, x1, x2), 0)), int(min(max(x0, x1, x2) + 1, Wi - 1))
                miny, maxy = int(max(min(y0, y1, y2), 0)), int(min(max(y0, y1, y2) + 1, Hi - 1))
                if minx > maxx or miny > maxy:
                    continue
                den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
                if abs(den) < 1e-9:
                    continue
                xx, yy = np.meshgrid(np.arange(minx, maxx + 1) + 0.5, np.arange(miny, maxy + 1) + 0.5)
                w0 = ((y1 - y2) * (xx - x2) + (x2 - x1) * (yy - y2)) / den
                w1 = ((y2 - y0) * (xx - x2) + (x0 - x2) * (yy - y2)) / den
                w2 = 1 - w0 - w1
                inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
                if not inside.any():
                    continue
                z = w0 * sz[i0] + w1 * sz[i1] + w2 * sz[i2]
                zb = zbuf[miny:maxy + 1, minx:maxx + 1]
                m = inside & (z < zb)
                if not m.any():
                    continue
                zb[m] = z[m]
                c = np.clip(col * shade[k] + 255 * rim[k], 0, 255)
                img[miny:maxy + 1, minx:maxx + 1][m] = c
                ids[miny:maxy + 1, minx:maxx + 1][m] = oid
        if outline:
            zf = np.where(np.isinf(zbuf), 1e6, zbuf)
            edge = np.zeros_like(zf, bool)
            for dy, dx in ((0, 1), (1, 0), (1, 1)):
                z2 = np.roll(np.roll(zf, dy, 0), dx, 1)
                i2 = np.roll(np.roll(ids, dy, 0), dx, 1)
                edge |= (np.abs(zf - z2) > 1.2) | (ids != i2)
            # slightly thicker lines
            edge = edge | np.roll(edge, 1, 0) | np.roll(edge, 1, 1)
            img[edge] = img[edge] * 0.25 + np.array(INK) * 0.75
        out = Image.fromarray(img.astype(np.uint8)).resize((W, H), Image.LANCZOS)
        self.proj = (right, up, center, scale)
        return out

    def to_px(self, p):
        right, up, center, scale = self.proj
        p = np.asarray(p, float)
        return ((p @ right - center[0]) * scale + self._W / 2, -(p @ up - center[1]) * scale + self._H / 2)


def project(scene, W, H, p):
    right, up, center, scale = scene.proj
    p = np.asarray(p, float)
    return (float((p @ right - center[0]) * scale + W / 2), float(-(p @ up - center[1]) * scale + H / 2))



# ------------------------------------------------------------------ models
sys.path.insert(0, HERE)
ZIP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(Y.ROOT, "yeti_v1.2_stls.zip")
CHASSIS_C = (150, 190, 235)
BEZEL_C = (225, 232, 242)
COVER_C = (200, 212, 228)
# body coordinates -> standing yeti: face to -Y, head up (+Z), feet on the base top (z = 3.5)
TW = [[-1, 0, 0, 0], [0, 0, -1, 20.3], [0, -1, 0, 37.36 + 3.5]]


def world(m):
    return m.transform(TW)


def wpt(p):
    p = np.asarray(p, float)
    return np.array(TW)[:, :3] @ p + np.array(TW)[:, 3]


def wbox(c, s):
    return world(Y.box_c(c, s))


def load_all():
    src = Y.load_zip(ZIP)
    return {size: Y.build(src, size) for size in ("13", "096")}


def eyes(scene, r, dz=0.0):
    o = Y.OLEDS[r["size"]]
    aw, ah = o["active"]
    gx, gy = Y.WIN_C
    zg = Y.Z_BEZEL - 0.08 + dz
    scene.add(world(Y.box_c((gx, gy, zg - 0.1), (o["glass"][0], o["glass"][1], 0.1))), (18, 22, 30), None)
    ew, eh = aw * 0.2, ah * 0.6
    for s in (-1, 1):
        rr = ew * 0.25
        e = Y.M.batch_hull([Y.M.cylinder(0.1, rr, rr, 20).translate([gx + s * aw * 0.21 + dx, gy + dy, zg])
                            for dx in (-ew / 2 + rr, ew / 2 - rr) for dy in (-eh / 2 + rr, eh / 2 - rr)])
        scene.add(world(e), (70, 230, 240), None)


def add_yeti(scene, r, parts=False, base=True, explode=None, only=None, grey=()):
    ex = explode or {}

    def off(key):
        d = ex.get(key, (0, 0, 0))
        return list(d)

    def put(key, m, color, target=60000):
        if only is not None and key not in only:
            return
        scene.add(world(m.translate(off(key))), (200, 205, 212) if key in grey else color, target)

    put("body", r["body"], FUR)
    put("bezel", r["bezel"], BEZEL_C)
    put("chassis", r["chassis"], CHASSIS_C)
    put("cover", r["cover"], COVER_C)
    if base and (only is None or "base" in only):
        # the plain base lies flat under the feet, centred under the yeti
        scene.add(r["base"].translate([0, 0, 0]).translate(list(ex.get("base", (0, 0, 0)))), BASEC, 20000)
    if parts:
        for p in r["parts"]:
            key = p["holder"] if p["name"] not in ex else p["name"]
            if p["name"] in ex:
                key = p["name"]
            if only is not None and p["name"] not in only:
                continue
            c = list(p["c"])
            d = ex.get(p["name"], ex.get(p["holder"], (0, 0, 0)))
            c = [c[i] + d[i] for i in range(3)]
            col = (200, 205, 212) if p["name"] in grey else hexc(p["color"])
            scene.add(wbox(c, p["s"]), col, None)


def label(draw, xy, text, anchor_xy, f, color=INK):
    draw.line([xy, anchor_xy], fill=color, width=2)
    draw.ellipse([anchor_xy[0] - 4, anchor_xy[1] - 4, anchor_xy[0] + 4, anchor_xy[1] + 4], fill=color)
    tw = draw.textlength(text, font=f)
    x = xy[0] - tw - 6 if xy[0] < anchor_xy[0] else xy[0] + 6
    draw.text((x, xy[1] - f.size // 2 - 2), text, font=f, fill=color)


def title(img, text, sub=None):
    d = ImageDraw.Draw(img)
    d.text((28, 18), text, font=font(34, True), fill=INK)
    if sub:
        d.text((30, 62), sub, font=font(20), fill=(90, 96, 110))
    return d


def wrap(d, x, y, w, text, f, fill=(70, 76, 90), lh=24):
    line = ""
    for word in text.split():
        if d.textlength((line + " " + word).strip(), font=f) > w:
            d.text((x, y), line, font=f, fill=fill)
            y += lh
            line = word
        else:
            line = (line + " " + word).strip()
    d.text((x, y), line, font=f, fill=fill)


# ------------------------------------------------------------------ pictures
def hero(R):
    r = R["13"]
    img = Image.new("RGB", (1600, 880), BG)
    for k, (az, el) in enumerate(((-28, 14), (152, 16))):
        s = Scene()
        add_yeti(s, r)
        if k == 0:
            eyes(s, r)
        im = s.render(800, 760, az=az, el=el, scale=7.6, center=np.array([0.0, 32.0]))
        img.paste(im, (k * 800, 100))
    d = title(img, "Coco Yeti (403035 battery)", "The yeti from yeti_v1.2_stls.zip with Coco inside: ESP32-C3, OLED, "
              "touch, MPU6050, 21 x 23 vibration motor, LiPo 4.0 x 30 x 35")
    d.text((400, 860), "front: the screen behind the bezel", font=font(20), fill=INK, anchor="mm")
    d.text((1200, 860), "back: USB-C, switch, 2 x M2 screws", font=font(20), fill=INK, anchor="mm")
    img.save(os.path.join(IMG, "coco_yeti_render.png"), optimize=True)


def exploded(R):
    r = R["13"]
    ex = {"bezel": (0, 0, 22), "oled": (0, 0, 13), "oled_glass": (0, 0, 13), "body": (0, 0, 0),
          "chassis": (0, 0, -40), "vibe": (0, 0, -40), "imu": (0, 0, -40), "esp32": (0, 0, -70),
          "battery": (0, 0, -92), "charger": (0, 0, -92), "switch": (0, 0, -92), "cover": (0, 0, -112),
          "touch": (0, -30, 0), "base": (0, 0, -18)}
    s = Scene()
    add_yeti(s, r, parts=True, explode=ex)
    W, H = 1700, 1050
    im = s.render(W, H, az=-62, el=20, scale=4.6)
    img = Image.new("RGB", (W, H + 90), BG)
    img.paste(im, (0, 90))
    d = ImageDraw.Draw(img)
    f = font(19)
    names = {"oled": '1.3" OLED', "battery": "LiPo 403035", "charger": "TP4056 mini (on the battery)",
             "esp32": "ESP32-C3 Super Mini", "vibe": "vibration motor 21x23", "imu": "MPU6050 on the fin",
             "touch": "TTP223 touch (head slot)", "switch": "slide switch"}
    side = {"oled": 1, "battery": 1, "charger": -1, "esp32": -1, "vibe": -1, "imu": 1, "touch": 1, "switch": 1}
    for p in r["parts"]:
        if p["name"] not in names:
            continue
        o = ex.get(p["name"], (0, 0, 0))
        x, y = project(s, W, H, wpt([p["c"][i] + o[i] for i in range(3)]))
        sd = side[p["name"]]
        label(d, (x + sd * 190, y + 90 - 36), names[p["name"]], (x, y + 90), f)
    for key, text, pt, sd in (("bezel", "bezel (1.3\" window)", (0, -4, 34), -1), ("body", "yeti body", (-26, 0, 18), -1),
                              ("chassis", "new chassis", (-17, -20, 26), -1), ("cover", "back cover", (-20, 0, -2), 1),
                              ("base", "plain base", (30, 37.4 + 1.8, 18), 1)):
        o = ex[key]
        x, y = project(s, W, H, wpt([pt[i] + o[i] for i in range(3)]))
        label(d, (x + sd * 200, y + 90 + 40), text, (x, y + 90), font(20, True))
    title(img, "Exploded view (1.3\" screen)", "Your yeti's body, bezel, back cover and base, a new chassis inside; "
          "every part has 1 mm free on each side")
    img.save(os.path.join(IMG, "coco_yeti_exploded.png"), optimize=True)


def assembly(R):
    r = R["13"]
    steps = [
        ("1. Touch sensor", dict(only={"body", "touch"}, parts=True, view=(200, 28)),
         "TTP223 into the slot inside the head, pad side up against the 2 mm wall under the hair. A drop of glue."),
        ("2. Screen", dict(only={"body", "bezel", "oled", "oled_glass"}, parts=True, view=(200, 28)),
         "Press the bezel into the window. Lay the yeti face down and drop the OLED in, glass to the bezel, "
         "pins at the head end."),
        ("3. Chassis", dict(only={"body", "bezel", "chassis", "vibe", "imu", "oled", "oled_glass", "touch"},
                            parts=True, view=(200, 28)),
         "Glue the 21 x 23 mm vibration motor into its rim and the MPU6050 on the fin (chip away from the fin, "
         "pins down). Slide the chassis in onto the screen."),
        ("4. Back cover", dict(only={"cover", "battery", "charger", "switch"}, parts=True, view=(20, 40)),
         "Tape the 403035 LiPo to the inside of the back cover, the charger on top. Switch knob through its slot "
         "at the feet end."),
        ("5. ESP32-C3 and wiring", dict(only={"body", "bezel", "chassis", "vibe", "imu", "oled", "oled_glass", "touch",
                                              "esp32"}, parts=True, view=(200, 28)),
         "Wire everything (wiring diagram). The ESP32-C3 stands at the head end in the chassis cup, USB-C to "
         "the back."),
        ("6. Close", dict(view=(-28, 14)),
         "Back cover on: the USB-C goes into its slot. Two M2 x 8 screws. Stand the yeti on the base."),
    ]
    cols, cw, chh = 3, 620, 600
    img = Image.new("RGB", (cols * cw, 2 * chh + 100), BG)
    for i, (head, opt, text) in enumerate(steps):
        s = Scene()
        add_yeti(s, r, parts=opt.get("parts", False), base=i == 5, only=opt.get("only"))
        if i == 5:
            eyes(s, r)
        az, el = opt["view"]
        im = s.render(cw, chh - 140, az=az, el=el, scale=5.0 if i < 5 else 4.6,
                      center=np.array([0.0, 32.0]) if i == 5 else None)
        x0, y0 = (i % cols) * cw, 100 + (i // cols) * chh
        img.paste(im, (x0, y0))
        d = ImageDraw.Draw(img)
        d.text((x0 + 24, y0 + chh - 128), head, font=font(24, True), fill=INK)
        wrap(d, x0 + 24, y0 + chh - 94, cw - 48, text, font(18))
    title(img, "Assembly", "Build the inside, then close the back with two M2 x 8 screws")
    img.save(os.path.join(IMG, "coco_yeti_assembly.png"), optimize=True)


def section(R):
    W, H = 1600, 860
    img = Image.new("RGB", (W, H), BG)
    for k, (size, cutx, text) in enumerate((("13", 0.0, '1.3": cut through the middle'),
                                            ("096", 0.0, '0.96": cut through the middle'))):
        r = R[size]
        s = Scene()
        keep = Y.box(cutx, 200, -200, 200, -200, 200)
        for key, col in (("body", FUR), ("bezel", BEZEL_C), ("chassis", CHASSIS_C), ("cover", COVER_C)):
            s.add(world(r[key] ^ keep), col, 50000)
        for p in r["parts"]:
            if p["c"][0] + p["s"][0] / 2 > cutx:
                s.add(world(Y.box_c(p["c"], p["s"]) ^ keep), hexc(p["color"]), None)
        im = s.render(800, 740, az=-90, el=0, scale=9.0, light=np.array([0.9, -0.3, 0.4]))
        img.paste(im, (k * 800, 90))
        ImageDraw.Draw(img).text((k * 800 + 400, 840), text, font=font(20), fill=INK, anchor="mm")
    title(img, "Cross sections", "Face to the left. Blue = new chassis; colours = parts, each with 1 mm free round it")
    img.save(os.path.join(IMG, "coco_yeti_section.png"), optimize=True)


def print_layout(R):
    r = R["13"]
    W, H = 1600, 900
    img = Image.new("RGB", (W, H), BG)
    s = Scene()
    items = [
        ("body", r["body"], FUR, (0, 0)),                                     # as in the zip: back down
        ("bezel", r["bezel"].transform([[1, 0, 0, 0], [0, -1, 0, 0], [0, 0, -1, Y.Z_FACE]]), BEZEL_C, (62, -30)),
        ("cover", r["cover"].transform([[1, 0, 0, 0], [0, -1, 0, 0], [0, 0, -1, 0]]), COVER_C, (-68, 0)),
        ("chassis", r["chassis"].transform([[1, 0, 0, 0], [0, -1, 0, 0], [0, 0, -1, r["z_plate"][1] + 2.5]]),
         CHASSIS_C, (62, 25)),
        ("base", r["base"], BASEC, (0, 95)),
    ]
    for nm, m, col, (dx, dy) in items:
        bb = m.bounding_box()
        s.add(m.translate([dx - (bb[0] + bb[3]) / 2, dy - (bb[1] + bb[4]) / 2, -bb[2]]), col, 50000)
    s.add(Y.box(-115, 105, -55, 145, -1.0, 0), (225, 228, 232), None)
    im = s.render(1600, 680, az=-20, el=42, scale=3.3)
    img.paste(im, (0, 80))
    d = ImageDraw.Draw(img)
    rows = ["Body: as it comes in the zip (back down, supports for the arms and horns). "
            "Bezel: front down. Back cover: flat inside down.",
            "Chassis: screen ribs down, supports under the plate (2.5 mm). Base: flat. Bezel 1.3\" or the zip's "
            "0.96\" bezel, and the chassis for your screen.",
            "PLA or PETG, 0.2 mm layers, 3 walls, 15 % infill. Files: cad/coco_yeti_*.stl (all in "
            "cad/coco_yeti_stl.zip)."]
    for i, t in enumerate(rows):
        d.text((40, 776 + i * 32), t, font=font(20), fill=INK)
    title(img, "Printing", "Five parts: body, bezel, chassis, back cover, plain base")
    img.save(os.path.join(IMG, "coco_yeti_print.png"), optimize=True)


def inside_view(R):
    """The yeti from the back with the back cover off: where everything goes."""
    W, H = 1600, 900
    img = Image.new("RGB", (W, H), BG)
    for k, size in enumerate(("13", "096")):
        r = R[size]
        s = Scene()
        add_yeti(s, r, parts=True, base=False, only={"body", "bezel", "chassis", "oled", "oled_glass", "vibe", "imu",
                                                     "esp32", "touch"})
        im = s.render(800, 760, az=180, el=0, scale=8.0)
        img.paste(im, (k * 800, 90))
        d = ImageDraw.Draw(img)
        d.text((k * 800 + 400, 870), f'{"1.3" if size == "13" else "0.96"}" screen: back cover off', font=font(20),
               fill=INK, anchor="mm")
    title(img, "Inside, from the back", "Green: vibration motor, purple: MPU6050, black: ESP32-C3 (USB-C to you), "
          "red: touch sensor in the head")
    img.save(os.path.join(IMG, "coco_yeti_inside.png"), optimize=True)


def wiring():
    """Wiring diagram, drawn as blocks with pin labels (both wirings in the table)."""
    W, H = 1700, 1080
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    f, fb, fs = font(19), font(21, True), font(16)

    def block(x, y, w, h, name, color, pins, side="left"):
        d.rounded_rectangle([x, y, x + w, y + h], 14, fill=color, outline=INK, width=3)
        d.text((x + w / 2, y + 22), name, font=fb, fill=(255, 255, 255), anchor="mm")
        pos = {}
        n = len(pins)
        for i, p in enumerate(pins):
            py = y + 52 + i * ((h - 64) / max(n - 1, 1)) if n > 1 else y + h / 2
            px = x if side == "left" else x + w
            d.ellipse([px - 6, py - 6, px + 6, py + 6], fill=(255, 255, 255), outline=INK, width=2)
            tx = x + 14 if side == "left" else x + w - 14
            d.text((tx, py), p, font=f, fill=(255, 255, 255), anchor="lm" if side == "left" else "rm")
            pos[p] = (px, py)
        return pos

    esp = block(690, 250, 300, 560, "ESP32-C3 Super Mini", (40, 44, 52),
                ["GPIO4 (SDA)", "GPIO5 (SCL)", "GPIO0 (touch)", "GPIO3 (INT)", "GPIO10 (vibe)", "GPIO1 (battery)",
                 "3V3", "GND"], side="left")
    d.text((1000, 236), "5V", font=f, fill=INK)
    d.ellipse([990 - 6, 262 - 6 + 10, 990 + 6, 262 + 6 + 10], fill=(255, 255, 255), outline=INK, width=2)
    oled = block(120, 120, 300, 200, "OLED 0.96\" / 1.3\"", (58, 111, 216), ["SDA", "SCL", "VCC", "GND"], "right")
    imu = block(120, 360, 300, 230, "MPU6050 GY-521", (112, 64, 176), ["SDA", "SCL", "INT", "VCC", "GND"], "right")
    tch = block(120, 630, 300, 170, "TTP223 touch", (208, 48, 48), ["I/O", "VCC", "GND"], "right")
    vib = block(120, 840, 300, 170, "vibration module 5 V", (48, 160, 96), ["IN", "VCC", "GND"], "right")
    pw = block(1180, 250, 380, 450, "power", (176, 140, 40), ["TP4056 IN+ (from 5V)", "TP4056 B+ / B- (LiPo)",
                                                              "TP4056 OUT+", "switch", "LDO 3.3 V in", "LDO 3.3 V out",
                                                              "1M / 330k"], "left")
    colors = {"sda": (40, 120, 220), "scl": (230, 150, 30), "v": (220, 50, 50), "g": (30, 30, 30),
              "sig": (130, 60, 190)}

    def wire(a, b, c, mid=None):
        mx = mid if mid is not None else (a[0] + b[0]) / 2
        d.line([a, (mx, a[1]), (mx, b[1]), b], fill=c, width=4, joint="curve")

    wire(oled["SDA"], esp["GPIO4 (SDA)"], colors["sda"], 520)
    wire(oled["SCL"], esp["GPIO5 (SCL)"], colors["scl"], 545)
    wire(imu["SDA"], esp["GPIO4 (SDA)"], colors["sda"], 520)
    wire(imu["SCL"], esp["GPIO5 (SCL)"], colors["scl"], 545)
    wire(imu["INT"], esp["GPIO3 (INT)"], colors["sig"], 575)
    wire(tch["I/O"], esp["GPIO0 (touch)"], colors["sig"], 600)
    wire(vib["IN"], esp["GPIO10 (vibe)"], colors["sig"], 625)
    for blk in (oled, imu, tch):
        wire(blk["VCC"], esp["3V3"], colors["v"], 650)
    # the 5 V vibration motor runs from the battery after the switch (3.7-4.2 V), not 3.3 V
    d.line([vib["VCC"], (560, vib["VCC"][1]), (560, 1040), (1100, 1040), (1100, pw["switch"][1]), pw["switch"]],
           fill=(230, 120, 30), width=4)
    d.text((570, 1046), "VCC from the battery after the switch", font=fs, fill=INK)
    for blk in (oled, imu, tch, vib):
        wire(blk["GND"], esp["GND"], colors["g"], 670)
    wire(pw["LDO 3.3 V out"], (990, esp["3V3"][1]), colors["v"], 1090)
    wire(pw["1M / 330k"], (990, esp["GPIO1 (battery)"][1]), colors["sig"], 1120)
    d.line([(990, esp["GND"][1]), (1060, esp["GND"][1])], fill=colors["g"], width=4)
    wire((990, 272), pw["TP4056 IN+ (from 5V)"], (220, 120, 40), 1120)
    notes = ["One USB-C (the ESP32-C3's) programs and charges: its 5V pin feeds the TP4056 IN+ (IN- to GND).",
             "LiPo -> TP4056 B+ / B-. OUT+ -> switch -> LDO ME6211 (1 uF in and out) -> 3V3.",
             "Battery sense: after the switch -> 1 M -> GPIO1 -> 330 k -> GND, 100 nF across the 330 k.",
             "GND: all modules, the TP4056 OUT- and the LDO together."]
    yy = 732
    for n_ in notes:
        words, line = n_.split(), ""
        for w in words:
            if d.textlength((line + " " + w).strip(), font=fs) > 500:
                d.text((1180, yy), line, font=fs, fill=INK)
                yy += 21
                line = w
            else:
                line = (line + " " + w).strip()
        d.text((1180, yy), line, font=fs, fill=INK)
        yy += 27
    # pin table
    ty = 860
    d.text((690, ty), "Pins", font=fb, fill=INK)
    rows = [("", "Coco wiring (default)", "Desk Companion wiring (leroyd_*)"),
            ("OLED + MPU6050 SDA / SCL", "GPIO4 / GPIO5", "GPIO8 / GPIO9"),
            ("touch sensor", "GPIO0 (wakes from deep sleep)", "GPIO7 (light sleep only)"),
            ("vibration motor IN", "GPIO10", "GPIO10"),
            ("MPU6050 INT", "GPIO3 (wakes when picked up)", "GPIO3 (optional)"),
            ("battery sense", "GPIO1", "-")]
    for i, (a, b, c) in enumerate(rows):
        yy = ty + 32 + i * 30
        fnt = fb if i == 0 else fs
        d.text((690, yy), a, font=fnt, fill=INK)
        d.text((960, yy), b, font=fnt, fill=INK)
        d.text((1300, yy), c, font=fnt, fill=INK)
    d.text((28, 18), "Coco Yeti wiring (403035 version)", font=font(34, True), fill=INK)
    d.text((30, 62), "OLED, MPU6050, touch on 3.3 V (I2C 0x3C and 0x68); the 5 V vibration motor on the switched "
           "battery.", font=font(20),
           fill=(90, 96, 110))
    img.save(os.path.join(IMG, "coco_yeti_wiring.png"), optimize=True)



def main():
    os.makedirs(IMG, exist_ok=True)
    R = load_all()
    for fn in (hero, exploded, assembly, section, inside_view, print_layout):
        print("render", fn.__name__)
        fn(R)
    wiring()
    print("done ->", IMG)


if __name__ == "__main__":
    main()
