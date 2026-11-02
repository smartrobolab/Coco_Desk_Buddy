#!/usr/bin/env python3
"""Photo-style renders of each Coco version with Blender (Cycles).

    BLENDER=/path/to/blender python3 tools/render_realistic.py [--samples 160] [--only coco_touch]

Writes <version>/images/<version>_render.png (front 3/4 view) and
<version>_render_back.png (back 3/4 view, electronics side), plus
shared/images/coco_family_render.png (all three together).

The scene comes from the same case data as the drawings and STL files
(tools/make_diagrams.py). Coco's face on the OLED is rendered from the real
firmware (sim/), and the desk is a green cutting mat with a 10 mm grid.
Needs Blender 4.2+, matplotlib and Pillow.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import make_diagrams as md  # noqa: E402


def face_texture(path):
    """Coco's neutral face from the firmware renderer, as the OLED's glowing pixels."""
    render = os.path.join(ROOT, "sim", "build", "render")
    if not os.path.exists(render):
        subprocess.run([os.path.join(ROOT, "sim", "build.sh")], check=True, capture_output=True)
    raw = subprocess.run([render, "face", "neutral", "25", "40"], check=True, capture_output=True).stdout[:1024]
    s = 8
    img = Image.new("RGB", (128 * s, 64 * s), (0, 0, 0))
    d = ImageDraw.Draw(img)
    for page in range(8):
        for x in range(128):
            b = raw[page * 128 + x]
            for bit in range(8):
                if b >> bit & 1:
                    y = page * 8 + bit
                    d.rectangle([x * s, y * s, x * s + s - 2, y * s + s - 2], fill=(40, 225, 235))
    img.save(path)


def cutting_mat(path):
    """Green self-healing cutting mat: 400 x 400 mm, 10 mm grid, 2048 px."""
    n, mm = 2048, 400
    img = Image.new("RGB", (n, n), (24, 66, 50))
    d = ImageDraw.Draw(img)
    px = n / mm
    for i in range(0, mm + 1, 5):
        x = int(i * px)
        major = i % 10 == 0
        col = (92, 140, 114) if major else (52, 96, 76)
        w = 3 if i % 50 == 0 else (2 if major else 1)
        d.line([(x, 0), (x, n)], fill=col, width=w)
        d.line([(0, x), (n, x)], fill=col, width=w)
    img.save(path)


def design_json(d):
    return {"name": d.name, "slug": d.slug, "W": d.W, "H": d.H, "D": d.D,
            "bars": [[list(a), list(b), s] for a, b, s in d.bars],
            "rings": [list(r) for r in d.rings],
            "parts": [dict(p) for p in d.parts]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", type=int, default=160)
    ap.add_argument("--res", default="1600x1100")
    ap.add_argument("--only", default=None, help="coco_touch | coco_voice_c3 | coco_voice_s3 | family")
    ap.add_argument("--no-family", action="store_true")
    args = ap.parse_args()
    blender = os.environ.get("BLENDER", "blender")
    designs = [md.touch_design(), md.voice_design(), md.voice_design(s3=True)]
    tmp = tempfile.mkdtemp(prefix="coco_render_")
    face, mat = os.path.join(tmp, "face.png"), os.path.join(tmp, "mat.png")
    face_texture(face)
    cutting_mat(mat)
    jobs = []
    for d in designs:
        if args.only and d.slug != args.only:  # --only family renders just the group picture
            continue
        out = os.path.join(ROOT, d.slug, "images")
        jobs.append(([design_json(d)], "front", os.path.join(out, f"{d.slug}_render.png")))
        jobs.append(([design_json(d)], "back", os.path.join(out, f"{d.slug}_render_back.png")))
    if args.only in (None, "family") and not args.no_family:
        jobs.append(([design_json(d) for d in designs], "family",
                     os.path.join(ROOT, "shared", "images", "coco_family_render.png")))
    for scene, view, out in jobs:
        spec = os.path.join(tmp, "scene.json")
        json.dump({"designs": scene, "view": view, "face": face, "mat": mat, "out": out,
                   "samples": args.samples, "res": args.res}, open(spec, "w"))
        print(f"rendering {os.path.relpath(out, ROOT)} ...", flush=True)
        r = subprocess.run([blender, "-b", "--factory-startup", "-noaudio", "-P",
                            os.path.join(HERE, "blender_coco.py"), "--", spec], capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(out):
            print(r.stdout[-3000:], r.stderr[-3000:])
            sys.exit(f"Blender failed for {out}")
        print("  done", flush=True)


if __name__ == "__main__":
    main()
