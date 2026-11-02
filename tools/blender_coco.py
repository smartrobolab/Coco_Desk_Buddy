"""Blender scene for Coco's photo-style renders. Run by tools/render_realistic.py:

    blender -b --factory-startup -P tools/blender_coco.py -- scene.json

scene.json: {"designs": [...], "view": "front|back|family", "face": png, "mat": png,
             "out": png, "samples": int, "res": "WxH"}
Design coordinates are mm with x = width, y = height, z = depth (front = 0).
Blender uses metres with Z up and the front facing -Y.
"""
import json
import math
import sys

import bmesh
import bpy
from mathutils import Vector

S = 0.001  # mm -> m


def V(x, y, z, ox=0.0):
    """Design (x, height, depth) in mm -> Blender vector in m."""
    return Vector(((x + ox) * S, z * S, y * S))


# ------------------------------------------------------------------ materials

MATS = {}


def principled(name, color, metallic=0.0, rough=0.5, coat=0.0, emission=None, strength=0.0, image=None,
               aniso=0.0):
    if name in MATS:
        return MATS[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    b.inputs["Coat Weight"].default_value = coat
    b.inputs["Anisotropic"].default_value = aniso
    if image:
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(image)
        tex.interpolation = "Closest"
        nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"])
        b.inputs["Base Color"].default_value = (0.004, 0.004, 0.005, 1)
        b.inputs["Emission Strength"].default_value = strength
    elif emission:
        b.inputs["Emission Color"].default_value = (*emission, 1)
        b.inputs["Emission Strength"].default_value = strength
    MATS[name] = m
    return m


def materials(face, mat_png):
    principled("brass", (0.78, 0.50, 0.16), metallic=1.0, rough=0.26, aniso=0.3)
    principled("solder", (0.78, 0.78, 0.76), metallic=1.0, rough=0.28)
    principled("pcb_blue", (0.008, 0.045, 0.16), rough=0.4, coat=0.35)
    principled("pcb_black", (0.012, 0.012, 0.014), rough=0.4, coat=0.35)
    principled("pcb_red", (0.32, 0.02, 0.015), rough=0.4, coat=0.35)
    principled("pcb_purple", (0.06, 0.015, 0.12), rough=0.4, coat=0.35)
    principled("gold", (1.0, 0.78, 0.35), metallic=1.0, rough=0.25)
    principled("steel", (0.8, 0.8, 0.82), metallic=1.0, rough=0.2)
    principled("chip", (0.03, 0.03, 0.03), rough=0.45)
    principled("ceramic", (0.85, 0.83, 0.78), rough=0.5)
    principled("glass", (0.003, 0.003, 0.004), rough=0.04, coat=1.0)
    principled("led_red", (0.9, 0.1, 0.05), emission=(1.0, 0.1, 0.05), strength=6)
    principled("led_blue", (0.1, 0.3, 1.0), emission=(0.1, 0.35, 1.0), strength=6)
    principled("oled", (0, 0, 0), image=face, strength=1.8)
    principled("wrap", (0.02, 0.06, 0.24), rough=0.35, coat=0.4)
    principled("wrap_band", (0.7, 0.7, 0.68), rough=0.35, coat=0.4)
    principled("plastic_black", (0.015, 0.015, 0.015), rough=0.4)
    principled("cone", (0.12, 0.12, 0.13), rough=0.7)
    principled("knob", (0.02, 0.02, 0.02), rough=0.5)
    principled("silk", (0.9, 0.9, 0.9), rough=0.5)
    # cutting mat
    m = bpy.data.materials.new("mat")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(mat_png)
    nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.75
    MATS["mat"] = m


# ------------------------------------------------------------------ geometry

COLL = None


def link(obj):
    COLL.objects.link(obj)
    return obj


def mesh_obj(name, bm, mat, bevel=0.0, smooth=False):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    if smooth:
        for p in me.polygons:
            p.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    ob.data.materials.append(MATS[mat])
    link(ob)
    if bevel > 0:
        mod = ob.modifiers.new("bevel", "BEVEL")
        mod.width = bevel * S
        mod.segments = 2
        mod.limit_method = "ANGLE"
    return ob


def box(name, x, y, z, w, h, d, mat, ox=0.0, bevel=0.15):
    """Axis box from a design corner (x, y=height, z=depth) and size (w, h, d) in mm."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x + 0.5, v.co.y + 0.5, v.co.z + 0.5))
        v.co = Vector(((x + ox + v.co.x * w) * S, (z + v.co.y * d) * S, (y + v.co.z * h) * S))
    return mesh_obj(name, bm, mat, bevel=min(bevel, w / 3, h / 3, d / 3))


def cyl(name, cx, y0, cz, r, h, mat, ox=0.0, seg=48, axis="y"):
    """Cylinder: axis 'y' = vertical (design height) from y0, 'z' = along depth from z=y0, 'x' = across."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r * S, radius2=r * S, depth=h * S)
    for v in bm.verts:
        a, b, c = v.co.x, v.co.y, v.co.z + h * S / 2  # c in 0..h
        if axis == "y":
            v.co = Vector(((cx + ox) * S + a, cz * S + b, y0 * S + c))
        elif axis == "z":
            v.co = Vector(((cx + ox) * S + a, y0 * S + c, cz * S + b))  # cz used as height here
        else:
            v.co = Vector(((y0 + ox) * S + c, cz * S + b, cx * S + a))
    return mesh_obj(name, bm, mat, smooth=True)


def wire(name, pts, r, mat, ox=0.0):
    """Round wire through design points (mm)."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = r * S
    cu.bevel_resolution = 4
    cu.use_fill_caps = True
    sp = cu.splines.new("POLY")
    sp.points.add(len(pts) - 1)
    for p, q in zip(sp.points, pts):
        p.co = (*V(*q, ox=ox), 1)
    ob = bpy.data.objects.new(name, cu)
    ob.data.materials.append(MATS[mat])
    return link(ob)


def ring(name, cx, y, cz, R, r, mat, ox=0.0):
    bm = bmesh.new()
    seg, sub = 64, 10
    verts = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        row = []
        for j in range(sub):
            b = 2 * math.pi * j / sub
            rr = R + r * math.cos(b)
            row.append(bm.verts.new(V(cx + rr * math.cos(a), y + r * math.sin(b), cz + rr * math.sin(a), ox=ox)))
        verts.append(row)
    for i in range(seg):
        for j in range(sub):
            bm.faces.new((verts[i][j], verts[(i + 1) % seg][j], verts[(i + 1) % seg][(j + 1) % sub],
                          verts[i][(j + 1) % sub]))
    return mesh_obj(name, bm, mat, smooth=True)


def blob(x, y, z, r=0.95, ox=0.0):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=r * S)
    for v in bm.verts:
        v.co = v.co + V(x, y, z, ox=ox)
    return mesh_obj("solder", bm, "solder", smooth=True)


# ------------------------------------------------------------------ parts

def header_pins(x0, y, z, n, ox, along="x", length=2.5):
    for i in range(n):
        px = x0 + i * 2.54
        box("pin", px - 0.32, y - 0.32, z, 0.64, 0.64, length, "gold", ox=ox, bevel=0.05)


def oled(p, ox):
    x, y, z, w, h, d = p["x"], p["y"], p["z"], p["w"], p["h"], p["d"]
    box("oled_pcb", x, y, z + d - 1.2, w, h, 1.2, "pcb_blue", ox=ox)
    box("oled_glass", x + 0.6, y + 3.5, z + 0.5, w - 1.2, h - 7.5, d - 1.7, "glass", ox=ox, bevel=0.2)
    # active area with Coco's face (128x64 => 21.7 x 10.9 mm)
    aw, ah = 21.7, 10.9
    ax0, ay0 = x + (w - aw) / 2, y + 3.5 + (h - 7.5 - ah) / 2 + 1.5
    bm = bmesh.new()
    vs = [bm.verts.new(V(ax0, ay0, z + 0.48, ox)), bm.verts.new(V(ax0 + aw, ay0, z + 0.48, ox)),
          bm.verts.new(V(ax0 + aw, ay0 + ah, z + 0.48, ox)), bm.verts.new(V(ax0, ay0 + ah, z + 0.48, ox))]
    f = bm.faces.new(vs)
    uv = bm.loops.layers.uv.new()
    for loop, (u, v) in zip(f.loops, [(0, 0), (1, 0), (1, 1), (0, 1)]):
        loop[uv].uv = (u, v)
    mesh_obj("oled_face", bm, "oled")
    # flex ribbon at the bottom, header on top
    box("ribbon", x + w / 2 - 4, y + 1.2, z + d - 1.6, 8, 2.6, 0.3, "chip", ox=ox, bevel=0.05)
    header_pins(x + w / 2 - 3.81, y + h - 1.6, z + d, 4, ox)
    for hx in (2, w - 2):
        for hy in (2, h - 2):
            cyl("hole", x + hx, z + d - 1.25, y + hy, 1.1, 1.3, "steel", ox=ox, seg=20, axis="z")


def mcu(p, ox, xiao=False):
    x, y, z, w, h, d = p["x"], p["y"], p["z"], p["w"], p["h"], p["d"]
    t = 1.2
    box("mcu_pcb", x, y, z + d - t, w, h, t, "pcb_blue" if xiao else "pcb_black", ox=ox)
    fz = z + d - t  # component side faces the front (inside the cage)
    if xiao:
        box("shield", x + 2, y + 2.5, fz - 2.0, 13.5, h - 5, 2.0, "steel", ox=ox, bevel=0.3)
    else:
        box("c3_chip", x + 7, y + 6.5, fz - 0.9, 5, 5, 0.9, "chip", ox=ox, bevel=0.1)
        box("antenna", x + 1.5, y + 7, fz - 1.0, 3.2, 6, 1.0, "ceramic", ox=ox, bevel=0.1)
        box("xtal", x + 13.5, y + 4, fz - 0.8, 2.5, 2, 0.8, "steel", ox=ox, bevel=0.1)
        box("ldo_onboard", x + 13.5, y + 11, fz - 1.0, 3, 1.6, 1.0, "chip", ox=ox, bevel=0.1)
        box("led", x + 17, y + 14.5, fz - 0.5, 1.0, 0.6, 0.5, "led_blue", ox=ox, bevel=0.05)
        box("btn", x + 13, y + 14.5, fz - 1.2, 3, 2.5, 1.2, "steel", ox=ox, bevel=0.2)
    # USB-C to the right
    box("usbc", x + w - 6.5, y + h / 2 - 4.5, fz - 3.2, 7.3, 9, 3.2, "steel", ox=ox, bevel=1.0)
    # castellated pads along the long edges
    for i in range(8 if not xiao else 7):
        px = x + 1.8 + i * (w - 3.6) / (7 if not xiao else 6)
        for py in (y + 0.4, y + h - 1.6):
            box("pad", px - 0.6, py, fz - 0.05, 1.2, 1.2, t + 0.1, "gold", ox=ox, bevel=0.05)


def tp4056(p, ox):
    x, y, z, w, h, d = p["x"], p["y"], p["z"], p["w"], p["h"], p["d"]
    # lies flat on the floor bars: board w x d, components on top
    t = 1.2
    box("tp_pcb", x, y, z, w, t, d, "pcb_blue", ox=ox)
    top = y + t
    long_x = w > d
    box("tp_ic", x + w * 0.45, top, z + d * 0.3, 4.9 if long_x else 3.9, 1.5, 3.9 if long_x else 4.9, "chip", ox=ox)
    box("dw01", x + w * 0.15, top, z + d * 0.55, 2.9, 1.1, 1.6, "chip", ox=ox)
    box("fet", x + w * 0.15, top, z + d * 0.2, 2.9, 1.1, 1.6, "chip", ox=ox)
    box("led1", x + w * 0.75, top, z + d * 0.75, 1.0, 0.5, 0.6, "led_red", ox=ox, bevel=0.05)
    box("led2", x + w * 0.62, top, z + d * 0.75, 1.0, 0.5, 0.6, "led_blue", ox=ox, bevel=0.05)
    port = p.get("port") or ["x+", ""]
    if port[0] == "x+":
        box("usbc", x + w - 6.5, top, z + d / 2 - 4.5, 7.3, 3.2, 9, "steel", ox=ox, bevel=1.0)
    else:
        box("usbc", x + w / 2 - 4.5, top, z + d - 6.5, 9, 3.2, 7.3, "steel", ox=ox, bevel=1.0)
    for i, py in enumerate((z + 1.5, z + d - 2.5)):
        box("pad", x + 0.4, top - 0.05, py, 2, 0.1, 1.2, "gold", ox=ox, bevel=0.02)


def generic_board(p, ox, mat, chip=(4, 4)):
    x, y, z, w, h, d = p["x"], p["y"], p["z"], p["w"], p["h"], p["d"]
    if h <= min(w, d):  # lying flat
        box(p["label"], x, y, z, w, 1.2, d, mat, ox=ox)
        box("chip", x + w / 2 - chip[0] / 2, y + 1.2, z + d / 2 - chip[1] / 2, chip[0], 0.9, chip[1], "chip", ox=ox)
        header_pins(x + 2.5, y + 1.2, z + d - 2.0, int((w - 4) / 2.54), ox, length=0.1)
    elif w <= min(h, d):  # standing on the side wall (INMP441)
        box(p["label"], x, y, z, 1.2, h, d, mat, ox=ox)
        cyl("mic_can", y + h / 2, x - 1.4, z + d / 2, 2.0, 1.4, "steel", ox=ox, seg=32, axis="x")
    else:
        box(p["label"], x, y, z, w, h, d, mat, ox=ox)


def battery(p, ox):
    x, y, z, w, h = p["x"], p["y"], p["z"], p["w"], p["h"]
    r = w / 2
    cx, cz = x + r, z + r
    cyl("bat_wrap", cx, y + 0.6, cz, r, h - 1.2, "wrap", ox=ox, seg=64)
    cyl("bat_band", cx, y + h * 0.62, cz, r + 0.02, 5, "wrap_band", ox=ox, seg=64)
    cyl("bat_top", cx, y + h - 0.6, cz, r - 0.4, 0.6, "steel", ox=ox, seg=64)
    cyl("bat_plus", cx, y + h, cz, 2.6, 0.9, "steel", ox=ox, seg=32)
    cyl("bat_bottom", cx, y, cz, r - 0.4, 0.6, "steel", ox=ox, seg=64)


def speaker(p, ox):
    x, y, z, w, h = p["x"], p["y"], p["z"], p["w"], p["h"]
    r = w / 2
    cx, cz = x + r, z + r
    cyl("spk_frame", cx, y, cz, r, h - 0.6, "plastic_black", ox=ox, seg=64)
    cyl("spk_cone", cx, y + h - 0.6, cz, r * 0.82, 0.4, "cone", ox=ox, seg=64)
    cyl("spk_cap", cx, y + h - 0.2, cz, r * 0.3, 0.6, "plastic_black", ox=ox, seg=48)


def switch(p, ox):
    x, y, z, w, h, d = p["x"], p["y"], p["z"], p["w"], p["h"], p["d"]
    box("sw_body", x, y, z, w, h, d, "steel", ox=ox, bevel=0.2)
    kx = x + w if x > 10 else x - 1.6
    box("sw_knob", kx, y + 1, z + d / 2 - 1.2, 1.6, h - 2, 2.4, "knob", ox=ox, bevel=0.2)


def ttp223(p, ox):
    x, y, z, w, h, d = p["x"], p["y"], p["z"], p["w"], p["h"], p["d"]
    box("ttp_pcb", x, y, z, w, h, d, "pcb_red", ox=ox)
    cyl("ttp_pad", x + w / 2 - 1.5, y + h, z + d / 2, 3.6, 0.04, "silk", ox=ox, seg=40)
    cyl("ttp_pad_in", x + w / 2 - 1.5, y + h + 0.01, z + d / 2, 3.3, 0.05, "pcb_red", ox=ox, seg=40)
    header_pins(x + w - 2.0, y + h, z + d / 2 - 2.54, 3, ox, length=0.1)  # pads


def ldo(p, ox):
    x, y, z, w, h, d = p["x"], p["y"], p["z"], p["w"], p["h"], p["d"]
    box("ldo_body", x, y, z, w, h, d, "chip", ox=ox, bevel=0.1)
    for i in range(3):
        box("ldo_leg", x + 0.6 + i * 1.5, y - 1.2, z + d / 2 - 0.2, 0.45, 1.3, 0.4, "steel", ox=ox, bevel=0.05)


def build(design, ox=0.0):
    W, H, D = design["W"], design["H"], design["D"]
    for i, (a, b, step) in enumerate(design["bars"]):
        thin = step != 1 and min(a[0], b[0]) >= 21 and a[1] == H and a[1] == b[1]
        wire(f"bar{i}", [a, b], 0.45 if thin else 0.6, "brass", ox=ox)
    joints = set()
    for a, b, step in design["bars"]:
        for p in (a, b):
            joints.add(tuple(round(c, 2) for c in p))
    for j in joints:
        blob(*j, r=0.85, ox=ox)
    for cx, y, cz, r, step in design["rings"]:
        ring("ring", cx, y, cz, r, 0.6, "brass", ox=ox)
    parts = {p["label"]: p for p in design["parts"]}
    for p in design["parts"]:
        lab, kind = p["label"], p["kind"]
        if kind == "oled":
            oled(p, ox)
        elif kind == "battery":
            battery(p, ox)
        elif kind == "speaker":
            speaker(p, ox)
        elif lab in ("ESP32-C3", "XIAO S3"):
            mcu(p, ox, xiao=lab == "XIAO S3")
        elif lab == "TP4056":
            tp4056(p, ox)
        elif lab == "switch":
            switch(p, ox)
        elif lab == "TTP223":
            ttp223(p, ox)
        elif lab == "LDO":
            ldo(p, ox)
        elif lab == "MAX98357A":
            generic_board(p, ox, "pcb_purple", chip=(3, 3))
        elif lab == "INMP441":
            generic_board(p, ox, "pcb_purple")
        else:
            box(lab, p["x"], p["y"], p["z"], p["w"], p["h"], p["d"], "chip", ox=ox)
    signal_wires(design, parts, ox)


def signal_wires(design, parts, ox):
    """The 0.9 mm brass signal wires: a representative routing, as in the circuit diagram."""
    W, H, D = design["W"], design["H"], design["D"]
    o = parts["OLED"]
    m = parts.get("ESP32-C3") or parts.get("XIAO S3")
    r = 0.45
    top_pin_y = o["y"] + o["h"] - 1.6
    mz = m["z"] + m["d"] - 1.2
    for i in range(4):  # OLED header -> board top row
        px = o["x"] + o["w"] / 2 - 3.81 + i * 2.54
        hy = top_pin_y + 2.2 + i * 0.9
        tx = m["x"] + 3 + i * 2.6
        wire(f"oled_w{i}", [(px, top_pin_y, o["z"] + o["d"] + 2.4), (px, hy, o["z"] + o["d"] + 2.4),
                            (px, hy, mz - 2.5), (tx, hy, mz - 2.5), (tx, m["y"] + m["h"] - 1.0, mz - 0.1)],
             r, "brass", ox=ox)
        blob(tx, m["y"] + m["h"] - 1.0, mz - 0.1, r=0.7, ox=ox)
        blob(px, top_pin_y, o["z"] + o["d"] + 2.4, r=0.6, ox=ox)
    t = parts["TTP223"]
    for i in range(3):  # touch pad -> board
        px = t["x"] + t["w"] - 2.0
        pz = t["z"] + t["d"] / 2 - 2.54 + i * 2.54
        tx = m["x"] + m["w"] - 8 + i * 2.2 if t["x"] > 19 else m["x"] + 12 + i * 2.2
        wire(f"ttp_w{i}", [(px, t["y"] + t["h"], pz), (px, H - 1.5 - i * 0.8, pz), (px, H - 1.5 - i * 0.8, mz - 1.5),
                           (tx, H - 1.5 - i * 0.8, mz - 1.5), (tx, m["y"] + m["h"] - 1.0, mz - 0.1)],
             r, "brass", ox=ox)
    b = parts["16340"]
    tp = parts["TP4056"]
    bx, bz = b["x"] + b["w"] / 2, b["z"] + b["w"] / 2
    btop = b["y"] + b["h"] + 0.9
    wire("bat_plus", [(bx, btop, bz), (bx, btop + 1.2, bz), (18.3, btop + 1.2, bz), (18.3, 5.5, bz),
                      (tp["x"] + 1.5, 5.5, bz), (tp["x"] + 1.5, tp["y"] + 1.25, tp["z"] + 2.0)], r, "brass", ox=ox)
    wire("bat_minus", [(bx, b["y"] - 0.2, bz), (bx, 1.6, bz), (tp["x"] + 1.5, 1.6, tp["z"] + tp["d"] - 2.0),
                       (tp["x"] + 1.5, tp["y"] + 1.25, tp["z"] + tp["d"] - 2.0)], r, "brass", ox=ox)
    if "MAX98357A" in parts:  # I2S + power to the amp
        a = parts["MAX98357A"]
        for i in range(5):
            ax_ = a["x"] + 2.5 + i * 2.54
            az = a["z"] + a["d"] - 2.0
            bxp = m["x"] + 2 + i * 2.6
            wire(f"amp_w{i}", [(bxp, m["y"] + 1.0, mz - 0.1), (bxp, 7.5 + i * 0.6, mz - 2.0),
                               (ax_, 7.5 + i * 0.6, az), (ax_, a["y"] + 1.3, az)], r, "brass", ox=ox)
        mic = parts["INMP441"]
        for i in range(4):
            my = mic["y"] + 3 + i * 2.5
            wire(f"mic_w{i}", [(mic["x"] - 0.1, my, mic["z"] + mic["d"] - 1.5),
                               (m["x"] + m["w"] + 2.0, my, mic["z"] + mic["d"] - 1.5),
                               (m["x"] + m["w"] + 2.0, my, mz - 1.0),
                               (m["x"] + m["w"] - 2.5 - i * 2.6, m["y"] + 1.0, mz - 0.1)], r, "brass", ox=ox)
        s = parts["speaker"]
        for i in range(2):
            wire(f"spk_w{i}", [(a["x"] + 1.0, a["y"] + 1.3, a["z"] + 3 + i * 3), (a["x"] + 1.0, s["y"] + 1.0, a["z"] + 3 + i * 3),
                               (s["x"] + 3.0, s["y"] + 1.0, s["z"] + s["d"] / 2 - 1.5 + i * 3)], r, "brass", ox=ox)


# ------------------------------------------------------------------ scene

def scene(spec):
    global COLL
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    COLL = sc.collection
    materials(spec["face"], spec["mat"])
    designs = spec["designs"]
    view = spec["view"]
    offsets, x = [], 0.0
    for d in designs:
        offsets.append(x)
        x += d["W"] + 22
    for d, ox in zip(designs, offsets):
        build(d, ox)
    span = x - 22
    cx = span / 2
    Hmax = max(d["H"] for d in designs)
    Dmax = max(d["D"] for d in designs)

    # cutting mat: 400 x 400 mm, 3 mm thick, under everything
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector(((v.co.x * 400 + cx) * S, (v.co.y * 400 + 60) * S, (v.co.z * 3 - 1.5 - 0.6 - 0.05) * S))
    uvl = bm.loops.layers.uv.new()
    for f in bm.faces:
        for loop in f.loops:
            co = loop.vert.co
            loop[uvl].uv = ((co.x / S - cx) / 400 + 0.5, (co.y / S - 60) / 400 + 0.5)
    mesh_obj("cutting_mat", bm, "mat")

    # lights
    def area(name, loc, power, size, color=(1, 1, 1)):
        ld = bpy.data.lights.new(name, "AREA")
        ld.energy = power
        ld.size = size
        ld.color = color
        ob = bpy.data.objects.new(name, ld)
        ob.location = loc
        target = Vector((cx * S, Dmax / 2 * S, Hmax / 2 * S))
        ob.rotation_euler = (target - ob.location).to_track_quat("-Z", "Y").to_euler()
        COLL.objects.link(ob)

    k = span / 56.0
    area("key", (cx * S - 0.20 * k, -0.30 * k, 0.32 * k), 7 * k * k, 0.22 * k, (1.0, 0.95, 0.88))
    area("fill", (cx * S + 0.28 * k, -0.18 * k, 0.12 * k), 2.2 * k * k, 0.3 * k, (0.88, 0.94, 1.0))
    area("rim", (cx * S + 0.05 * k, 0.32 * k, 0.26 * k), 7 * k * k, 0.2 * k)
    world = bpy.data.worlds.new("world")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.045, 0.045, 0.05, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.25
    sc.world = world

    # camera
    cam = bpy.data.cameras.new("cam")
    cam.lens = 90 if view != "family" else 135  # long lens: all three at true relative size
    cam.clip_start = 0.005
    co = bpy.data.objects.new("cam", cam)
    COLL.objects.link(co)
    target = Vector((cx * S, Dmax * 0.45 * S, Hmax * 0.45 * S))
    dist = (0.25 if view != "family" else 0.80) * (span / 56.0 if view != "family" else 1.0)
    az = math.radians({"front": -28, "back": 150, "family": -6}[view])
    el = math.radians({"front": 20, "back": 24, "family": 14}[view])
    co.location = target + Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el))) * dist
    co.rotation_euler = (target - co.location).to_track_quat("-Z", "Y").to_euler()
    cam.dof.use_dof = True
    cam.dof.focus_distance = (target - co.location).length
    cam.dof.aperture_fstop = 11 if view != "family" else 14
    sc.camera = co

    # render
    w, h = (int(v) for v in spec["res"].split("x"))
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = spec["samples"]
    sc.cycles.use_denoising = True
    sc.render.resolution_x = w
    sc.render.resolution_y = h
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "PNG"
    sc.render.filepath = spec["out"]
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    spec = json.load(open(sys.argv[sys.argv.index("--") + 1]))
    scene(spec)
