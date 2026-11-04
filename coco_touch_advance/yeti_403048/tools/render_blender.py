#!/usr/bin/env python3
"""Photo-style renders of the Coco Yeti (403035 version) with Blender (Cycles).

    BLENDER=/path/to/blender python3 coco_touch_advance/yeti_403035/tools/render_blender.py \\
        [--zip path/to/yeti_v1.2_stls.zip] [--samples 128] [--res 1600x1200]

The yeti is printed white and the screen ring (bezel) blue; it stands on the plain
base on the same green cutting mat as the other Coco renders, and the screen shows
Coco's neutral face from the real firmware (sim/). The geometry is exactly the STL
files make_yeti.py writes. Needs Blender 4.2+ and the packages of make_yeti.py.
Writes ../images/coco_yeti_blender_front.png, _back.png and _1_3_and_0_96.png.
"""
import argparse
import json
import os

import numpy as np
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_yeti as Y  # noqa: E402

REPO = Y.ROOT
sys.path.insert(0, os.path.join(REPO, "tools"))
from render_realistic import cutting_mat, face_texture  # noqa: E402

IMG = os.path.join(os.path.dirname(HERE), "images")
LOOK = {}
WHITE = (0.80, 0.81, 0.83)
BLUE = (0.004, 0.07, 0.50)
# body coordinates -> standing yeti (face to -Y, head up, feet on the base top at z = 3.5)
TW = Y.TW()


def export(r, tmp, tag, dx=0.0):
    objs = []
    for key, color, rough in (("body", WHITE, 0.55), ("cover", WHITE, 0.55), ("bezel", BLUE, 0.4),
                              ("base", WHITE, 0.5)):
        m = Y.base_world(r[key]) if key == "base" else r[key].transform(TW)
        path = os.path.join(tmp, f"{tag}_{key}.stl")
        Y.to_stl(m.translate([dx, 0, 0]), path)
        look = LOOK.get(key)
        objs.append(dict(path=path, color=color, rough=rough, name=f"{tag}_{key}", look=look))
    o = Y.OLEDS[r["size"]]
    gx, gy = Y.win_c()
    # screen: black glass and the glowing face (active area), just behind the bezel
    def wpt(x, y, z):
        t = np.array(TW)
        return list(t[:, :3] @ np.array([x, y, z]) + t[:, 3] + np.array([dx, 0, 0]))
    screen = dict(glass_c=wpt(gx, gy, Y.z_bezel() - 0.15), glass_s=list(o["glass"][:2]),
                  face_c=wpt(gx, gy, Y.z_bezel() - 0.08), face_s=list(o["active"]))
    return objs, screen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zip", default=os.path.join(REPO, "yeti_v1.2_stls.zip"))
    ap.add_argument("--samples", type=int, default=128)
    ap.add_argument("--res", default="1600x1200")
    ap.add_argument("--ring", default="silk_blue", choices=["silk_blue", "white"])
    ap.add_argument("--look", default=None, choices=["white", "marble", "rainbow", "beige", "blue"],
                    help="filament look of body, cover and base (bezel in eSilk blue); one front picture")
    args = ap.parse_args()
    blender = os.environ.get("BLENDER", "blender")
    src = Y.load_zip(args.zip)
    R = {size: Y.build(src, size) for size in ("13", "096")}
    tmp = tempfile.mkdtemp(prefix="coco_yeti_render_")
    face, mat = os.path.join(tmp, "face.png"), os.path.join(tmp, "mat.png")
    face_texture(face)
    cutting_mat(mat)
    os.makedirs(IMG, exist_ok=True)
    o13, s13 = export(R["13"], tmp, "y13")
    o096, s096 = export(R["096"], tmp, "y096", dx=95.0 * Y.S)
    if args.look:
        global LOOK
        body_look = None if args.look == "white" else args.look
        ring = "silk_blue" if args.ring == "silk_blue" else "pla_white"
        LOOK = {"body": body_look, "cover": body_look, "base": body_look, "bezel": ring}
        o13, s13 = export(R["13"], tmp, "y13")
        out = os.path.join(IMG, f"coco_yeti_look_{args.look}{'' if args.ring == 'silk_blue' else '_whitering'}.png")
        spec = os.path.join(tmp, "look.json")
        json.dump(dict(objects=o13, screens=[s13], face=face, mat=mat, out=out, samples=args.samples,
                       res=args.res, cam=dict(az=-28, el=14, dist=330 * Y.S, target=[0, 0, 35 * Y.S])), open(spec, "w"))
        r = subprocess.run([blender, "-b", "--factory-startup", "-noaudio", "-P", os.path.join(HERE, "blender_yeti.py"),
                            "--", spec], capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(out):
            print(r.stdout[-3000:], r.stderr[-3000:])
            sys.exit("Blender failed")
        print("->", out)
        return
    jobs = [
        ("front", [(o13, s13)], os.path.join(IMG, "coco_yeti_blender_front.png"), dict(az=-28, el=14, dist=330 * Y.S, target=[0, 0, 35 * Y.S])),
        ("back", [(o13, s13)], os.path.join(IMG, "coco_yeti_blender_back.png"), dict(az=152, el=18, dist=330 * Y.S, target=[0, 0, 35 * Y.S])),
        ("pair", [(o13, s13), (o096, s096)], os.path.join(IMG, "coco_yeti_blender_1_3_and_0_96.png"),
         dict(az=-14, el=12, dist=470 * Y.S, target=[47.5 * Y.S, 0, 35 * Y.S])),
    ]
    for name, scene, out, cam in jobs:
        spec = os.path.join(tmp, f"{name}.json")
        json.dump(dict(objects=[o for objs, _ in scene for o in objs], screens=[s for _, s in scene], face=face,
                       mat=mat, out=out, samples=args.samples, res=args.res, cam=cam), open(spec, "w"))
        print(f"rendering {os.path.relpath(out, REPO)} ...", flush=True)
        r = subprocess.run([blender, "-b", "--factory-startup", "-noaudio", "-P", os.path.join(HERE, "blender_yeti.py"),
                            "--", spec], capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(out):
            print(r.stdout[-3000:], r.stderr[-3000:])
            sys.exit(f"Blender failed for {out}")
        print("  done", flush=True)


if __name__ == "__main__":
    main()
