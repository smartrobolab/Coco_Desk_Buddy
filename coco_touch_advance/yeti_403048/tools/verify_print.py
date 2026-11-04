#!/usr/bin/env python3
"""Checks the printable files of a Coco Yeti version before you send them to a print service.

    python3 coco_touch_advance/<version>/tools/verify_print.py

1. every STL in ../cad that is meant for printing is a closed, valid solid (no holes,
   no self-intersections) and fits a 200 x 200 x 200 mm print bed
2. the parts still fit: the fit check of make_yeti.py (1 mm round every part)
3. the yeti stands on the base: no overlap, both M3 screw holes of the base line up
   with the pilot holes in the soles, the legs are solid
4. stability: centre of mass of the finished yeti (printed parts + electronics) and
   how far it can tip before it falls, on its feet alone and screwed to the base
Writes ../cad/verify_report.txt.
"""
import glob
import math
import os
import sys

import numpy as np
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_yeti as Y  # noqa: E402

CAD = os.path.join(os.path.dirname(HERE), "cad")
PLA = 1.24          # g/cm3
FILL = 0.85         # thin walled parts print mostly as solid walls
PART_G = {"battery": 11.0, "oled": 6.0, "oled_glass": 0.0, "esp32": 2.5, "charger": 1.0, "imu": 2.0, "vibe": 3.0,
          "touch": 0.5, "switch": 0.5}
WIRES_G = 3.0
lines = []


def say(s=""):
    print(s)
    lines.append(s)


def main():
    ok_all = True
    zpath = sys.argv[1] if len(sys.argv) > 1 else os.path.join(Y.ROOT, "yeti_v1.2_stls.zip")
    S = getattr(Y, "S", 1.0)
    say(f"Coco Yeti print check: {os.path.basename(os.path.dirname(HERE))} (scale {S:.2f})")
    say("")
    say("1. Printable files (cad/print/, as they go on the print bed)")
    for path in sorted(glob.glob(os.path.join(CAD, "print", "coco_yeti_*.stl"))):
        name = os.path.basename(path)
        if "_parts_" in name:
            continue
        m = trimesh.load(path, force="mesh")
        ext = m.extents
        bodies = len(m.split(only_watertight=False))
        good = m.is_watertight and m.is_winding_consistent and m.volume > 0 and bodies == 1
        fits = max(ext) <= 200
        flat = abs(m.bounds[0][2]) < 0.01
        ok_all &= good and fits
        say(f"   {name:32s} {'OK ' if good else 'BAD'} closed solid, {ext[0]:5.1f} x {ext[1]:5.1f} x {ext[2]:5.1f} mm, "
            f"{m.volume / 1000:5.1f} cm3, ~{m.volume / 1000 * PLA * FILL:4.0f} g PLA"
            f"{'' if bodies == 1 else f'  {bodies} SEPARATE PIECES'}{'' if fits else '  TOO BIG'}")
    src = Y.load_zip(zpath)
    for size in ("13", "096"):
        r = Y.build(src, size)
        ok, rep = Y.fit_check(r)
        ok_all &= ok
        if size == "13":
            say("")
            say("2. Fit (1 mm round every part)")
        say(f"   {size} screen: {'OK' if ok else 'PROBLEMS'}" + ("" if ok else ": " + "; ".join(
            l.strip() for l in rep if "TIGHT" in l or "OVERLAP" in l)))
    r = Y.build(src, "13")
    TW = Y.TW() if callable(getattr(Y, "TW", None)) else None
    tw = np.array(TW)
    yeti = (r["body"] + r["bezel"] + r["cover"]).transform(TW)
    base = Y.base_world(r["base"])
    say("")
    say("3. Yeti on the base")
    ov = (yeti ^ base).volume()
    say(f"   overlap yeti / base: {ov:.3f} mm3 ({'OK' if ov < 0.05 else 'BAD'})")
    ok_all &= ov < 0.05
    for fx, fz in Y.FEET:
        foot = tw[:, :3] @ np.array([fx * S, Y.SOLE_Y * S, fz * S]) + tw[:, 3]
        probe_up = Y.M.cylinder(10, 0.3, 0.3).translate([foot[0], foot[1], foot[2] + 0.5])   # the pilot hole: air
        solid_ring = (Y.M.cylinder(8, 4.0 * S, 4.0 * S) - Y.M.cylinder(8, 1.6, 1.6)).translate(
            [foot[0], foot[1], foot[2] + 0.3])
        air = (probe_up ^ yeti).volume() < 0.01
        fill = (solid_ring ^ yeti).volume() / solid_ring.volume()
        hole = Y.M.cylinder(3.5 * S, 0.4, 0.4).translate([foot[0], foot[1], 0.1])
        base_hole = (hole ^ base).volume() < 0.01
        say(f"   foot at x {foot[0]:6.1f}, y {foot[1]:5.1f}: pilot hole {'open' if air else 'BLOCKED'}, "
            f"leg {100 * fill:3.0f}% solid round it, base hole below it {'yes' if base_hole else 'NO'}")
        ok_all &= air and base_hole and fill > 0.97

    say("")
    say("4. Stability (1.3\" version; PLA, parts as weighed typically)")
    pieces = []
    for key in ("body", "bezel", "cover", "chassis"):
        m = r[key].transform(TW)
        mesh = m.to_mesh()
        tm = trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=False)
        pieces.append((key, tm.volume / 1000 * PLA * FILL, tm.center_mass))
    for p in r["parts"]:
        g = PART_G.get(p["name"], 0.5)
        if p["name"] == "battery" and "2 x" in p["label"]:
            g = 12.0
        if g:
            pieces.append((p["name"], g, tw[:, :3] @ np.array(p["c"]) + tw[:, 3]))
    bb = yeti.bounding_box()
    pieces.append(("wires", WIRES_G, np.array([0, 0, (bb[2] + bb[5]) / 2])))
    mass = sum(g for _, g, _ in pieces)
    com = sum(g * np.asarray(c) for _, g, c in pieces) / mass
    say(f"   total about {mass:.0f} g, centre of mass at x {com[0]:.1f}, y {com[1]:.1f} (back +), "
        f"z {com[2]:.1f} mm above the desk")
    # support: the soles (yeti alone) and the base disc (screwed on)
    sole_z = Y.BASE_T * S                     # the soles stand on the base top
    soles = yeti.slice(sole_z + 0.4)
    pts = np.concatenate([np.asarray(p) for p in soles.to_polygons()])
    from scipy.spatial import ConvexHull
    h = ConvexHull(pts)
    poly = pts[h.vertices]

    def cross2(u, v):
        return u[0] * v[1] - u[1] * v[0]

    def margin(poly, p):
        n = len(poly)
        sides = [cross2(poly[(i + 1) % n] - poly[i], p - poly[i]) for i in range(n)]
        inside = all(x >= 0 for x in sides) or all(x <= 0 for x in sides)
        dist = min(abs(sides[i]) / np.linalg.norm(poly[(i + 1) % n] - poly[i]) for i in range(n))
        return inside, dist

    inside, dist = margin(poly, com[:2])
    height = com[2] - sole_z
    ang = math.degrees(math.atan2(dist, height)) if inside else 0
    say(f"   on its feet alone: centre of mass {'inside' if inside else 'OUTSIDE'} the feet, {dist:.1f} mm from the "
        f"nearest edge, tips over at ~{ang:.0f} deg of tilt")
    bb2 = base.bounding_box()
    rad = (bb2[3] - bb2[0]) / 2
    bc = np.array([(bb2[0] + bb2[3]) / 2, (bb2[1] + bb2[4]) / 2])
    dist_b = rad - np.linalg.norm(com[:2] - bc)
    ang_b = math.degrees(math.atan2(dist_b, com[2]))
    say(f"   screwed to the base: {dist_b:.1f} mm to the base edge, tips over at ~{ang_b:.0f} deg of tilt")
    ok_all &= inside and ang > 8
    say("")
    say("RESULT: " + ("all checks passed" if ok_all else "SOME CHECKS FAILED"))
    with open(os.path.join(CAD, "verify_report.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    sys.exit(0 if ok_all else 1)


if __name__ == "__main__":
    main()
