#!/usr/bin/env python3
"""Coco Yeti: the yeti from yeti_v1.2_stls.zip, with room inside for Coco's parts.

    python3 coco_touch_advance/yeti/tools/make_yeti.py [path/to/yeti_v1.2_stls.zip]

The outside of the yeti is NOT changed: the body, bezel, back cover and base come
from the zip (default: yeti_v1.2_stls.zip in the repository root). This script
only adds what Coco needs inside, with 1 mm clearance round every part:

  body        + two screw bosses (M2 x 8 self-tapping) for the back cover
  back cover  + a slot for the power switch (the battery is taped to its flat inside)
  bezel 1.3"  the zip's bezel with a bigger window for the 1.3" screen (the zip's
              own bezel is for the 0.96")
  chassis     new: holds the screen against the bezel, the vibration motor, the
              MPU6050 and the top of the ESP32-C3; clamped by the back cover
  base        the zip's base without the spikes (a small plain base)

Needs numpy, trimesh, manifold3d (pip install numpy trimesh manifold3d).
Writes ../cad/*.stl, ../cad/coco_yeti_stl.zip and ../cad/parts_<size>.json, then
runs a fit check: every part is moved 0.95 mm in each free direction and must
not hit anything; parts must not overlap each other.

Body coordinates (mm), as the body sits in the zip: X = the yeti's right,
Y = towards the feet (the head is at -Y), Z = from the back (0, where the back
cover goes) to the face (36 at the bezel).
"""
import io
import json
import os
import sys
import zipfile

import manifold3d as mf
import numpy as np
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
OUT = os.path.join(os.path.dirname(HERE), "cad")
M = mf.Manifold
mf.set_circular_segments(40)

GAP = 1.0        # clearance round every part, each free side
RIB = 0.8        # locating ribs (thin, so the chassis still passes the back opening)
PLATE = 2.0      # chassis plate

SRC = {  # files in the zip
    "body": "obj_7_corp+face.stl_1.stl",
    "bezel": "obj_8_corp+face.stl_2.stl",
    "cover": "obj_4_Yeti Full.stl_3.stl",
    "base": "obj_2_Yeti Full.stl_7.stl",
}

# measured on the zip's body: window, face wall, screw holes, slots
WIN_C = (0.02, -3.97)       # centre of the face window = bezel outline 37.4 x 28.5
Z_BEZEL = 32.0              # back of the bezel / front of the screen glass
Z_FACE = 36.0               # front of the bezel
HOLES = [(-22.49, 2.61), (22.51, 2.61)]   # back cover screw holes in the body
USB = (0.0, 15.3)           # back cover USB-C slot (9.2 x 3.8) in body coordinates
TOUCH_CEIL = -30.75         # ceiling of the touch slot in the head (~2 mm wall above)

OLEDS = {  # glass (w, h, t), pcb (w, h, t of the back part), glass centre below the pcb centre
    "096": dict(glass=(26.7, 19.3, 1.5), pcb=(27.3, 27.8, 2.9), glass_dy=1.2, active=(21.7, 10.9),
                label='0.96" OLED SSD1306'),
    "13": dict(glass=(34.5, 23.0, 1.5), pcb=(35.4, 33.5, 3.1), glass_dy=0.6, active=(29.4, 14.7),
               label='1.3" OLED SH1106'),
}
BATTERY = dict(size=(30.0, 30.0, 5.0), label="LiPo 503030 (5 x 30 x 30 mm, ~400 mAh)")
ESP32 = (18.0, 4.5, 22.5)        # ESP32-C3 Super Mini standing: w, thickness, length (USB-C at the back)
CHARGER = (17.0, 12.0, 4.0)      # small TP4056 USB-C module with protection (as in Coco Touch)
TOUCH = (15.0, 2.6, 11.0)        # TTP223 small: w, thickness (pad up), length
IMU = (21.2, 3.4, 16.4)          # GY-521 MPU6050 standing: w, thickness, height
VIBE = (30.0, 21.0, 6.0)         # vibration motor module lying flat: w, h, thickness
SWITCH = (3.6, 8.6, 3.5)         # SS12D00 body: knob through the back cover


# ------------------------------------------------------------------ helpers
def to_manifold(tm):
    tm = tm.copy()
    tm.merge_vertices()
    m = M(mf.Mesh(vert_properties=np.asarray(tm.vertices, np.float32), tri_verts=np.asarray(tm.faces, np.uint32)))
    if m.is_empty():
        raise RuntimeError("could not make a solid from the mesh")
    return m


def load_zip(path):
    z = zipfile.ZipFile(path)
    out = {}
    for key, name in SRC.items():
        tm = trimesh.load(io.BytesIO(z.read(name)), file_type="stl", force="mesh")
        m = to_manifold(tm)
        b = m.bounding_box()
        out[key] = m.translate([-(b[0] + b[3]) / 2, -(b[1] + b[4]) / 2, -b[2]])  # centred in x, y; z from 0
    return out


def box(x0, x1, y0, y1, z0, z1):
    return M.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def box_c(c, s):
    return box(c[0] - s[0] / 2, c[0] + s[0] / 2, c[1] - s[1] / 2, c[1] + s[1] / 2, c[2] - s[2] / 2, c[2] + s[2] / 2)


def rim(c, s, height_axis, h0, h1, t=RIB, open_side=None):
    """Thin wall round a pocket (pocket = part + gaps) of size s at c, from h0 to h1 along height_axis."""
    i = "xyz".index(height_axis)
    so, si = list(s), list(s)
    for k in range(3):
        if k != i:
            so[k] += 2 * t
    so[i] = si[i] = h1 - h0
    cc = list(c)
    cc[i] = (h0 + h1) / 2
    r = box_c(cc, so) - box_c(cc, [si[k] + (1 if k == i else 0) for k in range(3)])
    if open_side:  # leave the middle of one side open (wires)
        k = "xyz".index(open_side[1])
        sg = 1 if open_side[0] == "+" else -1
        oc = list(cc)
        oc[k] += sg * (s[k] / 2 + t / 2)
        os_ = list(so)
        os_[k] = t + 0.4
        for j in range(3):
            if j != k and j != i:
                os_[j] = s[j] - 8
        r = r - box_c(oc, os_)
    return r


def to_stl(m, path):
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3].astype(np.float32)
    t = np.asarray(mesh.tri_verts).astype(np.int64)
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    n = np.cross(b - a, c - a)
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    rec = np.zeros(len(t), dtype=[("n", "<f4", 3), ("a", "<f4", 3), ("b", "<f4", 3), ("c", "<f4", 3), ("x", "<u2")])
    rec["n"], rec["a"], rec["b"], rec["c"] = n, a, b, c
    with open(path, "wb") as f:
        f.write(b"Coco Yeti".ljust(80, b" "))
        f.write(np.uint32(len(t)).tobytes())
        f.write(rec.tobytes())


def cavity_section(body, z):
    """The inside of the body at height z, as a CrossSection (big holes of the slice)."""
    inner = []
    for p in body.slice(z).to_polygons():
        p = np.asarray(p)
        area = 0.5 * np.sum(p[:, 0] * np.roll(p[:, 1], -1) - np.roll(p[:, 0], -1) * p[:, 1])
        if area < -200:  # big holes only (not the screw holes)
            inner.append(p[::-1].tolist())
    return mf.CrossSection(inner)


# ------------------------------------------------------------------ build
def build(src, size):
    body, bezel0, cover0, base0 = src["body"], src["bezel"], src["cover"], src["base"]
    o = OLEDS[size]

    # ---- the zip's parts, in body coordinates
    # back cover: turned onto the back (screw holes and outline match), outside of z = 0
    cv = cover0.transform([[0, -1, 0, 0], [-1, 0, 0, -0.5], [0, 0, -1, 0]])
    # bezel: front face at z = 36, centred on the window
    bz = bezel0.transform([[1, 0, 0, WIN_C[0]], [0, -1, 0, WIN_C[1]], [0, 0, -1, Z_FACE]])

    parts = []

    def part(name, label, c, s, color, contact, holder):
        parts.append(dict(name=name, label=label, c=[round(float(v), 3) for v in c], s=[float(v) for v in s],
                          color=color, contact=contact, holder=holder))

    gw, gh, gt = o["glass"]
    pw, ph, pt = o["pcb"]
    gy = WIN_C[1]
    py = gy - o["glass_dy"]                       # header at the top (towards the head)
    seat = 0.0                                    # the glass rests flat against the back of the bezel
    z_glass0 = Z_BEZEL + seat - gt
    z_pcb0 = z_glass0 - pt
    part("oled_glass", o["label"] + " glass", (WIN_C[0], gy, z_glass0 + gt / 2), (gw, gh, gt), "#1c2230", ["+z"],
         "bezel")
    part("oled", o["label"], (WIN_C[0], py, z_glass0 - pt / 2), (pw, ph, pt), "#3a6fd8", ["-z", "+z"], "chassis")
    z_plate1 = z_pcb0                              # chassis plate front = back of the screen
    z_plate0 = z_plate1 - PLATE

    # ---- on the back cover: battery flat, charger on the battery, switch
    bw, bh, bt = BATTERY["size"]
    part("battery", BATTERY["label"], (0, -22.0 + bh / 2, bt / 2), (bw, bh, bt), "#e0b030", ["-z"], "cover")
    part("charger", "TP4056 mini charger (on the battery)", (0, -14.0, bt + CHARGER[2] / 2), CHARGER, "#2f6fb0",
         ["-z"], "battery")
    part("switch", "slide switch (knob through the back cover)", (12.8, 16.1, SWITCH[2] / 2), SWITCH, "#b0b0b0",
         ["-z"], "cover")
    # ---- ESP32-C3 standing in the USB slot, components towards the feet
    part("esp32", "ESP32-C3 Super Mini (USB-C in the back cover slot)",
         (USB[0], USB[1] - 2.6 + ESP32[1] / 2, ESP32[2] / 2), ESP32, "#2a2a30", ["-z"], "cover")
    # ---- on the chassis: vibration motor on the back of the plate, MPU6050 on a fin
    vw, vh, vt = VIBE
    part("vibe", "vibration motor module", (0, -22.5 + vh / 2, z_plate0 - vt / 2), VIBE, "#30a060", ["+z"],
         "chassis")
    fin_y0, fin_y1 = 4.6, 4.6 + 1.2
    part("imu", "MPU6050 GY-521 (chip towards the feet)", (0, fin_y1 + IMU[1] / 2, 7.5 + IMU[2] / 2), IMU, "#7040b0",
         ["-y"], "chassis")
    # ---- head: touch sensor pad up against the slot ceiling
    part("touch", "TTP223 touch sensor (pad to the head)", (0, TOUCH_CEIL + 0.1 + TOUCH[1] / 2, 15.5), TOUCH,
         "#d03030", ["-y"], "body")
    P = {p["name"]: p for p in parts}

    # ---- body: two screw bosses for the back cover (pilot 1.8 mm for M2 self-tapping)
    hull = M.batch_hull([body.trim_by_plane([0, 0, -1], -9.5)])  # rough outside, keeps the bosses in the yeti
    bosses = M.batch_boolean([M.cylinder(9.0, 2.6, 2.6).translate([hx, hy, 0]) for hx, hy in HOLES], mf.OpType.Add)
    body_m = body + (bosses ^ hull)
    # widen the zip's screen pocket (it is exactly the 1.3" module size) and the window
    # corners under the bezel by GAP on every side; inside surfaces only
    o13 = OLEDS["13"]
    p13y = WIN_C[1] - o13["glass_dy"]
    carve = box_c((WIN_C[0], p13y, (Z_BEZEL - o13["glass"][2] - o13["pcb"][2] + Z_BEZEL - o13["glass"][2]) / 2),
                  (o13["pcb"][0] + 2 * GAP, o13["pcb"][1] + 2 * GAP, o13["pcb"][2] + 0.2))
    carve = carve + box_c((WIN_C[0], WIN_C[1], Z_BEZEL - o13["glass"][2] / 2 + 0.05),
                          (o13["glass"][0] + 2 * GAP, o13["glass"][1] + 2 * GAP, o13["glass"][2] + 0.1))
    body_m = body_m - carve
    for hx, hy in HOLES:
        body_m = body_m - M.cylinder(8.0, 0.9, 0.9).translate([hx, hy, -0.01])

    # ---- chassis: plate behind the screen, cut to the inside of the body (1 mm away)
    opening = cavity_section(body, 0.5).offset(-0.3, mf.JoinType.Round)      # must pass the back opening
    inside = cavity_section(body, z_plate0 + 0.2) ^ cavity_section(body, z_plate1 - 0.2)
    outline = inside.offset(-GAP, mf.JoinType.Round) ^ opening
    scr_pocket = (pw + 2 * GAP, ph + 2 * GAP)
    plate_rect = mf.CrossSection.square([scr_pocket[0] + 2 * RIB, scr_pocket[1] + 2 * RIB + 6], True).translate(
        [WIN_C[0], py + 1.0])
    ch = [M.extrude(outline ^ plate_rect, PLATE).translate([0, 0, z_plate0])]
    # screen ribs (front of the plate), open at the top for the header wires
    ch.append(rim((WIN_C[0], py, 0), (scr_pocket[0], scr_pocket[1], 0), "z", z_plate1, z_plate1 + pt - 0.6,
                  open_side="-y"))
    # motor rim (back of the plate)
    ch.append(rim(P["vibe"]["c"], (vw + 2 * GAP, vh + 2 * GAP, 0), "z", z_plate0 - 2.5, z_plate0, open_side="+y"))
    # fin for the MPU6050
    ch.append(box(-IMU[0] / 2 - 0.6, IMU[0] / 2 + 0.6, fin_y0, fin_y1, 6.0, z_plate0))
    # cup round the top end of the ESP32-C3
    ec = P["esp32"]["c"]
    ch.append(rim((ec[0], ec[1], 0), (ESP32[0] + 2 * GAP, ESP32[1] + 2 * GAP, 0), "z", 19.5, z_plate0))
    # four legs down to the back cover: the cover clamps chassis and screen against the bezel
    for lx in (-18.4, 18.4):
        for ly in (-12.0, 11.0):
            ch.append(M.cylinder(z_plate0, 1.2, 1.2).translate([lx, ly, 0]))
    chassis = M.batch_boolean(ch, mf.OpType.Add)
    chassis = chassis ^ M.extrude(opening, 40).translate([0, 0, -1])        # nothing wider than the opening

    # ---- back cover: switch slot
    ps = P["switch"]
    cover_m = cv - box_c((ps["c"][0], ps["c"][1], -2.0), (2.4, 5.4, 6.0))
    # (the battery is held by double-sided tape: the cover's inside stays flat for printing)

    # ---- bezel: the zip's for 0.96"; for 1.3" a bigger window and a seat for the glass
    if size == "13":
        aw, ah = o["active"]
        bezel_m = bz - box_c((WIN_C[0], gy, Z_FACE - 2), (aw + 2.0, ah + 2.0, 6))
    else:
        bezel_m = bz

    # ---- plain base: the zip's base without the spikes
    base_m = base0 ^ box(-60, 60, -60, 60, -1, 3.5)
    return dict(body=body_m, cover=cover_m, bezel=bezel_m, chassis=chassis, base=base_m, parts=parts, size=size,
                z_plate=(z_plate0, z_plate1))


def fit_check(r):
    shells = r["body"] + r["cover"] + r["bezel"] + r["chassis"]
    rep, ok = [], True
    boxes = {p["name"]: box_c(p["c"], p["s"]) for p in r["parts"]}

    def test_box(p):  # the sides that rest on something give up 0.3 mm
        c, s = list(p["c"]), list(p["s"])
        for d in p["contact"]:
            i = "xyz".index(d[1])
            s[i] -= 0.3
            c[i] += -0.15 if d[0] == "+" else 0.15
        return box_c(c, s)

    for p in r["parts"]:
        b = test_box(p)
        hit = (b ^ shells).volume()
        ok &= hit < 0.01
        free = []
        for ax in "xyz":
            for sg in "+-":
                d = sg + ax
                if d in p["contact"]:
                    continue
                v = [0, 0, 0]
                v["xyz".index(ax)] = (GAP - 0.05) * (1 if sg == "+" else -1)
                h = (b.translate(v) ^ shells).volume()
                free.append(f"{d}:{'ok' if h < 0.01 else 'TIGHT'}")
                ok &= h < 0.01
        rep.append(f'{p["name"]:10s} collision {hit:6.3f} mm3  gaps ' + " ".join(free))
    names = list(boxes)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            v = (boxes[names[i]] ^ boxes[names[j]]).volume()
            if v > 0.01:
                ok = False
                rep.append(f"PARTS OVERLAP {names[i]} / {names[j]}: {v:.2f} mm3")
    pr = [("body", r["body"]), ("cover", r["cover"]), ("bezel", r["bezel"]), ("chassis", r["chassis"])]
    for i in range(len(pr)):
        for j in range(i + 1, len(pr)):
            v = (pr[i][1] ^ pr[j][1]).volume()
            if v > 0.05:
                if {pr[i][0], pr[j][0]} == {"body", "bezel"} and v < 3:
                    rep.append(f"body / bezel: {v:.2f} mm3 press fit (as in the original yeti)")
                    continue
                ok = False
                rep.append(f"PRINTED PARTS OVERLAP {pr[i][0]} / {pr[j][0]}: {v:.2f} mm3")
    return ok, rep


def main():
    zpath = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "yeti_v1.2_stls.zip")
    src = load_zip(zpath)
    os.makedirs(OUT, exist_ok=True)
    all_ok = True
    written = []
    for size in ("096", "13"):
        print(f"== Coco Yeti, {size} screen")
        r = build(src, size)
        ok, rep = fit_check(r)
        all_ok &= ok
        print("\n".join("   " + s for s in rep))
        print("   fit check:", "OK" if ok else "PROBLEMS")
        files = {f"coco_yeti_chassis_{size}.stl": r["chassis"], f"coco_yeti_bezel_{size}.stl": r["bezel"]}
        if size == "13":
            files.update({"coco_yeti_body.stl": r["body"], "coco_yeti_back_cover.stl": r["cover"],
                          "coco_yeti_base_plain.stl": r["base"]})
        for fn, m in files.items():
            to_stl(m, os.path.join(OUT, fn))
            written.append(fn)
            bb = m.bounding_box()
            print(f"   {fn}: {m.num_tri()} triangles, {bb[3] - bb[0]:.1f} x {bb[4] - bb[1]:.1f} x {bb[5] - bb[2]:.1f} mm")
        parts_m = M.batch_boolean([box_c(p["c"], p["s"]) for p in r["parts"]], mf.OpType.Add)
        to_stl(parts_m, os.path.join(OUT, f"coco_yeti_parts_{size}.stl"))
        with open(os.path.join(OUT, f"parts_{size}.json"), "w") as f:
            json.dump(dict(parts=r["parts"], gap=GAP, plate=r["z_plate"]), f, indent=1)
    with zipfile.ZipFile(os.path.join(OUT, "coco_yeti_stl.zip"), "w", zipfile.ZIP_DEFLATED) as z:
        for fn in written:
            z.write(os.path.join(OUT, fn), f"coco_yeti/{fn}")
    print("-> cad/coco_yeti_stl.zip")
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
