#!/usr/bin/env python3
"""3D models of Coco as STL, from the same case data as the drawings.

    python3 tools/make_stl.py

Writes <version>/cad/<version>_full.stl (frame + all parts) and
<version>/cad/<version>_frame.stl (brass frame only) for coco_touch,
coco_voice_c3 and coco_voice_s3.
Units are mm, Z up, the front (OLED side) faces -Y. Brass bars are modelled as
round wire (1.2 mm frame, 1.0 mm seats/grille); the 0.9 mm signal wires are left
out. GitHub shows .stl files as an interactive 3D view.
"""
import math
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_diagrams as md  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def P(p):
    """Design coords (x, height, depth) -> STL (x, depth, height)."""
    return np.array([p[0], p[2], p[1]], dtype=float)


class Mesh:
    def __init__(self):
        self.tris = []

    def tri(self, a, b, c):
        self.tris.append((np.asarray(a, float), np.asarray(b, float), np.asarray(c, float)))

    def quad(self, a, b, c, d):
        self.tri(a, b, c)
        self.tri(a, c, d)

    def box(self, x, y, z, w, h, d):
        """Axis box in design coords (x, height y, depth z)."""
        c = [P(v) for v in [(x, y, z), (x + w, y, z), (x + w, y + h, z), (x, y + h, z), (x, y, z + d),
                             (x + w, y, z + d), (x + w, y + h, z + d), (x, y + h, z + d)]]
        for f in [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (3, 7, 6, 2), (0, 4, 7, 3), (1, 2, 6, 5)]:
            self.quad(*[c[i] for i in f])

    def cylinder(self, a, b, r, n=14, caps=True):
        """Cylinder between two STL-space points."""
        a, b = np.asarray(a, float), np.asarray(b, float)
        axis = b - a
        L = np.linalg.norm(axis)
        if L < 1e-6:
            return
        k = axis / L
        t = np.array([1.0, 0, 0]) if abs(k[0]) < 0.9 else np.array([0, 1.0, 0])
        u = np.cross(k, t)
        u /= np.linalg.norm(u)
        v = np.cross(k, u)
        ring = [(math.cos(2 * math.pi * i / n), math.sin(2 * math.pi * i / n)) for i in range(n)]
        A = [a + r * (c * u + s * v) for c, s in ring]
        B = [b + r * (c * u + s * v) for c, s in ring]
        for i in range(n):
            j = (i + 1) % n
            self.quad(A[i], A[j], B[j], B[i])
            if caps:
                self.tri(a, A[j], A[i])
                self.tri(b, B[i], B[j])

    def torus(self, centre, R, r, n=48, m=10):
        """Horizontal torus (ring of wire) around an STL-space centre."""
        cx, cy, cz = centre
        pts = [[(cx + (R + r * math.cos(2 * math.pi * j / m)) * math.cos(2 * math.pi * i / n),
                 cy + (R + r * math.cos(2 * math.pi * j / m)) * math.sin(2 * math.pi * i / n),
                 cz + r * math.sin(2 * math.pi * j / m)) for j in range(m)] for i in range(n)]
        for i in range(n):
            for j in range(m):
                i2, j2 = (i + 1) % n, (j + 1) % m
                self.quad(pts[i][j], pts[i2][j], pts[i2][j2], pts[i][j2])

    def save(self, path, name):
        with open(path, "wb") as f:
            f.write(name.encode()[:80].ljust(80, b" "))
            f.write(struct.pack("<I", len(self.tris)))
            for a, b, c in self.tris:
                nrm = np.cross(b - a, c - a)
                ln = np.linalg.norm(nrm)
                nrm = nrm / ln if ln > 0 else nrm
                f.write(struct.pack("<12fH", *nrm, *a, *b, *c, 0))
        print(f"wrote {os.path.relpath(path, ROOT)}  ({len(self.tris)} triangles)")


def add_frame(mesh, d):
    for a, b, step in d.bars:
        mesh.cylinder(P(a), P(b), 0.6 if step == 1 else 0.5)
    # corner balls hide the gaps where bars meet
    corners = {tuple(np.round(p, 2)) for a, b, s in d.bars if s == 1 for p in (a, b)}
    for c in corners:
        mesh.cylinder(P(c) - [0, 0, 0.6], P(c) + [0, 0, 0.6], 0.62, n=10)
    for cx, y, cz, r, step in d.rings:
        mesh.torus(P((cx, y, cz)), r, 0.6)


def add_parts(mesh, d):
    for pt in d.parts:
        x, y, z, w, h, dd = pt["x"], pt["y"], pt["z"], pt["w"], pt["h"], pt["d"]
        if pt["kind"] == "battery":
            r = w / 2
            mesh.cylinder(P((x + r, y, z + r)), P((x + r, y + h, z + r)), r, n=40)
            mesh.cylinder(P((x + r, y + h, z + r)), P((x + r, y + h + 0.8, z + r)), 2.5, n=20)  # + button
        elif pt["kind"] == "speaker":
            r = w / 2
            mesh.cylinder(P((x + r, y, z + r)), P((x + r, y + h, z + r)), r, n=40)
            mesh.cylinder(P((x + r, y + h, z + r)), P((x + r, y + h + 0.4, z + r)), r * 0.55, n=30)  # cone
        else:
            mesh.box(x, y, z, w, h, dd)
        if pt["kind"] == "oled":
            # glass panel and Coco's two eyes, raised so the face reads in any viewer
            mesh.box(x + 1.5, y + 6, z - 0.3, w - 3, h - 10, 0.3)
            for ex in (x + w * 0.3, x + w * 0.7):
                mesh.box(ex - 3.6, y + 11, z - 0.7, 7.2, 9, 0.4)
        if pt.get("pad"):
            mesh.cylinder(P((x + w / 2, y + h, z + dd / 2)), P((x + w / 2, y + h + 0.3, z + dd / 2)), 3.5, n=24)
        if pt.get("knob"):
            side = -1 if x < 0 else 1
            kx = x + (0 if side < 0 else w)
            mesh.box(kx + (side * 2.0 if side > 0 else -2.0), y + 1, z + 3, 2.0, 2.0, 2.6)
        if pt.get("port"):
            axis, _ = pt["port"]
            if axis == "x+":
                mesh.box(x + w - 0.5, y + h / 2 - 1.6, z + dd / 2 - 4.5, 1.6, 3.2, 9)
            elif axis == "z+":
                mesh.box(x + w / 2 - 4.5, y + h, z + dd - 0.5, 9, 3.2, 1.6)


def main():
    for d in (md.touch_design(), md.voice_design(), md.voice_design(s3=True),
              md.voice_design(s3=True, hw747=True)):
        slug = d.slug
        folder = next(v for v in md.VERSIONS if slug == v or slug.startswith(v + "_"))  # HW-747 -> coco_voice_s3
        out = os.path.join(ROOT, folder, "cad")
        os.makedirs(out, exist_ok=True)
        frame = Mesh()
        add_frame(frame, d)
        frame.save(os.path.join(out, f"{slug}_frame.stl"), f"{d.name} brass frame")
        full = Mesh()
        add_frame(full, d)
        add_parts(full, d)
        full.save(os.path.join(out, f"{slug}_full.stl"), f"{d.name} full model")


if __name__ == "__main__":
    main()
