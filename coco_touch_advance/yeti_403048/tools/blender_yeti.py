"""Runs inside Blender (see render_blender.py): builds the Coco Yeti scene and renders it.

    blender -b --factory-startup -P blender_yeti.py -- scene.json

Units: the STL files are in mm, imported at 0.001 so the scene is in metres.
"""
import json
import math
import sys

import bpy
from mathutils import Vector

spec = json.load(open(sys.argv[sys.argv.index("--") + 1]))
MM = 0.001

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = spec["samples"]
scene.cycles.use_denoising = True
w, h = (int(v) for v in spec["res"].split("x"))
scene.render.resolution_x, scene.render.resolution_y = w, h
scene.render.image_settings.file_format = "PNG"
scene.view_settings.view_transform = "AgX"
scene.view_settings.look = "AgX - Medium High Contrast"


def material(name, color, rough, sss=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    if sss:  # a little light through the plastic, like white PLA
        b.inputs["Subsurface Weight"].default_value = sss
        b.inputs["Subsurface Radius"].default_value = (1.0, 1.0, 1.0)
        b.inputs["Subsurface Scale"].default_value = 0.002
    return m


def look_material(name, look):
    """Filament looks: 'marble' (off-white with grey/black speckles), 'rainbow' (colour bands
    that change with the print height), 'silk_blue' (shiny blue silk PLA)."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    solid = {"beige": ((0.72, 0.60, 0.42), 0.5), "blue": ((0.02, 0.10, 0.62), 0.45),
             "pla_white": ((0.80, 0.81, 0.83), 0.5)}
    if look in solid:  # plain PLA colours
        b.inputs["Base Color"].default_value = (*solid[look][0], 1)
        b.inputs["Roughness"].default_value = solid[look][1]
        return m
    if look == "silk_blue":
        b.inputs["Base Color"].default_value = (0.02, 0.12, 0.75, 1)
        b.inputs["Metallic"].default_value = 0.45
        b.inputs["Roughness"].default_value = 0.22
        return m
    tc = nt.nodes.new("ShaderNodeTexCoord")
    if look == "marble":
        noise = nt.nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 700.0        # flecks of ~1-2 mm (scene in metres)
        noise.inputs["Detail"].default_value = 6.0
        noise.inputs["Roughness"].default_value = 0.75
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        cr = ramp.color_ramp
        cr.interpolation = "CONSTANT"
        cr.elements[0].position, cr.elements[0].color = 0.0, (0.03, 0.03, 0.035, 1)   # black flecks
        cr.elements[1].position, cr.elements[1].color = 0.40, (0.78, 0.77, 0.74, 1)   # off-white
        e = cr.elements.new(0.36)
        e.color = (0.30, 0.30, 0.31, 1)                                                # grey flecks
        e = cr.elements.new(0.66)
        e.color = (0.55, 0.55, 0.55, 1)                                                # light grey veins
        e = cr.elements.new(0.69)
        e.color = (0.78, 0.77, 0.74, 1)
        geo_m = nt.nodes.new("ShaderNodeNewGeometry")
        nt.links.new(geo_m.outputs["Position"], noise.inputs["Vector"])  # world position, in metres
        nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
        b.inputs["Roughness"].default_value = 0.6
    else:  # rainbow: the colour follows the height (the filament changes colour along its length)
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        mul = nt.nodes.new("ShaderNodeMath")
        mul.operation = "MULTIPLY"
        frac = nt.nodes.new("ShaderNodeMath")
        frac.operation = "FRACT"
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        cr = ramp.color_ramp
        cols = [(0.0, (0.80, 0.02, 0.02)), (0.18, (0.90, 0.30, 0.0)), (0.34, (0.85, 0.75, 0.0)),
                (0.5, (0.03, 0.55, 0.08)), (0.66, (0.0, 0.18, 0.80)), (0.83, (0.35, 0.03, 0.60)),
                (1.0, (0.80, 0.02, 0.02))]
        cr.elements[0].position, cr.elements[0].color = cols[0][0], (*cols[0][1], 1)
        cr.elements[1].position, cr.elements[1].color = cols[-1][0], (*cols[-1][1], 1)
        for pos, c in cols[1:-1]:
            e = cr.elements.new(pos)
            e.color = (*c, 1)
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        nt.links.new(geo.outputs["Position"], sep.inputs["Vector"])     # world height: same for every part
        nt.links.new(sep.outputs["Z"], mul.inputs[0])
        nt.links.new(mul.outputs["Value"], frac.inputs[0])
        nt.links.new(frac.outputs["Value"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
        b.inputs["Roughness"].default_value = 0.35
        b.inputs["Coat Weight"].default_value = 0.3        # a little shine
        mul.inputs[1].default_value = 25.0                 # one full colour cycle per ~40 mm of height
    return m


def image_material(name, path, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(path)
    nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
    if emit:
        nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"])
        b.inputs["Emission Strength"].default_value = emit
        b.inputs["Roughness"].default_value = 0.15
    else:
        b.inputs["Roughness"].default_value = 0.6
    return m


def smooth(obj, angle=35):
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    try:
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(angle))
    except Exception:
        bpy.ops.object.shade_smooth()
    obj.select_set(False)


# ---- the yeti parts
for o in spec["objects"]:
    bpy.ops.wm.stl_import(filepath=o["path"], global_scale=MM)
    obj = bpy.context.selected_objects[0]
    obj.name = o["name"]
    if o.get("look"):
        obj.data.materials.append(look_material(o["name"], o["look"]))
    else:
        obj.data.materials.append(material(o["name"], o["color"], o["rough"], sss=0.15 if o["color"][0] > 0.8 else 0))
    smooth(obj)

# ---- screens: black glass with Coco's face glowing on the active area
glass_m = material("glass", (0.01, 0.012, 0.015), 0.08)
face_m = image_material("face", spec["face"], emit=1.6)
for s in spec["screens"]:
    for key, mat, z_off in (("glass", glass_m, 0.0), ("face", face_m, 0.0)):
        c, sz = s[key + "_c"], s[key + "_s"]
        bpy.ops.mesh.primitive_plane_add(size=1, location=(c[0] * MM, c[1] * MM, c[2] * MM))
        p = bpy.context.active_object
        p.scale = (sz[0] * MM, sz[1] * MM, 1)
        p.rotation_euler = (math.radians(90), 0, 0)  # face -Y, towards the camera
        p.data.materials.append(mat)

# ---- desk: green cutting mat, 400 mm
bpy.ops.mesh.primitive_plane_add(size=0.4, location=(0.05, 0.06, 0))
desk = bpy.context.active_object
desk.data.materials.append(image_material("mat", spec["mat"]))

# ---- light: soft studio
world = bpy.data.worlds.new("world")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.78, 0.82, 0.88, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.22
target = Vector([v * MM for v in spec["cam"].get("target", [0, 0, 35])])


def area(name, loc, power, size, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, "AREA")
    d.energy, d.size, d.color = power, size, color
    ob = bpy.data.objects.new(name, d)
    scene.collection.objects.link(ob)
    ob.location = Vector(loc) + target
    ob.rotation_euler = (target - ob.location).to_track_quat("-Z", "Y").to_euler()


area("key", (-0.16, -0.22, 0.22), 4.0, 0.18, (1.0, 0.97, 0.92))
area("fill", (0.24, -0.12, 0.10), 1.2, 0.25, (0.9, 0.95, 1.0))
area("rim", (0.05, 0.25, 0.20), 2.5, 0.15)

# ---- camera: same angles as the line drawings (az 0 = from the front)
cam = spec["cam"]
a, e = math.radians(cam["az"]), math.radians(cam["el"])
d = Vector((math.sin(a) * math.cos(e), math.cos(a) * math.cos(e), -math.sin(e)))
cd = bpy.data.cameras.new("cam")
cd.lens = 85
cd.clip_start = 0.01
cd.dof.use_dof = True
cd.dof.aperture_fstop = 8
co = bpy.data.objects.new("cam", cd)
scene.collection.objects.link(co)
co.location = target - d * cam["dist"] * MM
co.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
cd.dof.focus_distance = cam["dist"] * MM
scene.camera = co

scene.render.filepath = spec["out"]
bpy.ops.render.render(write_still=True)
