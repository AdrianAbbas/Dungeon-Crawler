"""Blender scene library for DUNGEON CRAWLER's painted art (run with Blender's Python
module, `bpy`, see tools/painted/README.md).

Everything is built from code: silhouette-friendly geometry (jagged ridges, gothic
spires, arches, columns, rocks, dead trees, cave walls), materials, red rim and key
lights, volumetric fog and ember particles. Each object belongs to one depth layer
(sky, far, mid, near, fog); render.py renders the layers separately so the plug-in can
move them at different speeds (parallax) and drift the fog.

Coordinates: metres, Z up. Virtual frame: 960 x 560 (the plug-in's view).
"""
import math
import random

import bpy
import bmesh
from mathutils import Vector, Matrix, noise
from bpy_extras.object_utils import world_to_camera_view

VIRTUAL_W, VIRTUAL_H = 960.0, 560.0
LAYERS = ("sky", "far", "mid", "near", "fog")


# --------------------------------------------------------------------------- scene
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    try:
        import addon_utils
        addon_utils.enable("cycles", default_set=True)
    except Exception:
        pass
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.world = bpy.data.worlds.new("World")
    world_colour((0.004, 0.004, 0.006), 1.0)
    return sc


def world_colour(colour, strength):
    w = bpy.context.scene.world
    w.use_nodes = True
    bg = w.node_tree.nodes.get("Background")
    if bg is None:
        bg = w.node_tree.nodes.new("ShaderNodeBackground")
        w.node_tree.links.new(bg.outputs[0], w.node_tree.nodes["World Output"].inputs[0])
    bg.inputs[0].default_value = (*colour, 1.0)
    bg.inputs[1].default_value = strength


def collection(layer):
    name = "L_" + layer
    c = bpy.data.collections.get(name)
    if c is None:
        c = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(c)
    return c


def put(obj, layer):
    """Moves an object into a depth layer's collection."""
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    collection(layer).objects.link(obj)
    obj["layer"] = layer
    return obj


def mesh_object(name, verts, faces, layer, material=None, smooth=False):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces])
    me.validate()
    me.update()
    if smooth:
        for p in me.polygons:
            p.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    if material is not None:
        me.materials.append(material)
    return put(ob, layer)


def camera(location, look_at, lens=35.0, shift=(0.0, 0.0)):
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = lens
    cam_data.sensor_fit = "HORIZONTAL"
    cam_data.sensor_width = 36.0
    cam_data.shift_x, cam_data.shift_y = shift
    cam_data.clip_start = 0.1
    cam_data.clip_end = 20000.0
    cam = bpy.data.objects.new("Camera", cam_data)
    bpy.context.scene.collection.objects.link(cam)
    cam.location = Vector(location)
    direction = Vector(look_at) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = cam
    return cam


def project(point):
    """World point -> virtual 960 x 560 coordinates (and depth)."""
    sc = bpy.context.scene
    bpy.context.view_layer.update()
    v = world_to_camera_view(sc, sc.camera, Vector(point))
    return (v.x * VIRTUAL_W, (1.0 - v.y) * VIRTUAL_H, v.z)


def screen_ray(x, y):
    """Virtual screen point -> (origin, direction) of the camera ray."""
    sc = bpy.context.scene
    cam = sc.camera
    bpy.context.view_layer.update()
    frame = cam.data.view_frame(scene=sc)   # top-right, bottom-right, bottom-left, top-left (camera space)
    tr, br, bl, tl = frame
    u, v = x / VIRTUAL_W, y / VIRTUAL_H
    top = tl.lerp(tr, u)
    bottom = bl.lerp(br, u)
    p = top.lerp(bottom, v)
    world = cam.matrix_world @ p
    origin = cam.matrix_world.translation.copy()
    return origin, (world - origin).normalized()


# ----------------------------------------------------------------------- materials
def _nodes(mat):
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        if n.type != "OUTPUT_MATERIAL":
            nt.nodes.remove(n)
    return nt, nt.nodes["Material Output"]


_materials = {}


def mat_stone(name="stone", colour=(0.035, 0.034, 0.04), rough=0.85, bump=0.6, scale=3.0, haze=0.0, haze_colour=(0.5, 0.48, 0.62)):
    """Dark stone. `haze` > 0 adds a faint self-lit mist colour: aerial perspective for
    distant ranges (each farther range a little paler)."""
    key = (name, colour, rough, bump, scale, haze, haze_colour)
    if key in _materials:
        return _materials[key]
    m = bpy.data.materials.new(name)
    nt, out = _nodes(m)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*colour, 1)
    bsdf.inputs["Roughness"].default_value = rough
    tex = nt.nodes.new("ShaderNodeTexNoise")
    tex.inputs["Scale"].default_value = scale
    tex.inputs["Detail"].default_value = 8.0
    tex.inputs["Roughness"].default_value = 0.65
    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.inputs["Scale"].default_value = scale * 0.7
    mix = nt.nodes.new("ShaderNodeMath"); mix.operation = "ADD"
    nt.links.new(tex.outputs["Fac"], mix.inputs[0])
    nt.links.new(vor.outputs["Distance"], mix.inputs[1])
    b = nt.nodes.new("ShaderNodeBump")
    b.inputs["Strength"].default_value = bump
    nt.links.new(mix.outputs[0], b.inputs["Height"])
    nt.links.new(b.outputs["Normal"], bsdf.inputs["Normal"])
    # Slight value variation in the colour (painterly breakup).
    ramp = nt.nodes.new("ShaderNodeMix"); ramp.data_type = "RGBA"
    ramp.inputs["A"].default_value = (*[c * 0.55 for c in colour], 1)
    ramp.inputs["B"].default_value = (*[c * 1.5 for c in colour], 1)
    nt.links.new(tex.outputs["Fac"], ramp.inputs["Factor"])
    nt.links.new(ramp.outputs["Result"], bsdf.inputs["Base Color"])
    if haze > 0:
        bsdf.inputs["Emission Color"].default_value = (*haze_colour, 1)
        bsdf.inputs["Emission Strength"].default_value = haze
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    _materials[key] = m
    return m


def mat_emission(colour, strength, name="glow"):
    key = ("emit", colour, strength)
    if key in _materials:
        return _materials[key]
    m = bpy.data.materials.new(name)
    nt, out = _nodes(m)
    e = nt.nodes.new("ShaderNodeEmission")
    e.inputs["Color"].default_value = (*colour, 1)
    e.inputs["Strength"].default_value = strength
    nt.links.new(e.outputs[0], out.inputs["Surface"])
    _materials[key] = m
    return m


def mat_metal(colour=(0.5, 0.35, 0.15), rough=0.3, name="metal"):
    key = ("metal", colour, rough)
    if key in _materials:
        return _materials[key]
    m = bpy.data.materials.new(name)
    nt, out = _nodes(m)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*colour, 1)
    bsdf.inputs["Metallic"].default_value = 1.0
    bsdf.inputs["Roughness"].default_value = rough
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    _materials[key] = m
    return m


def mat_lava(scale=1.5, strength=14.0, crack=0.06, base=(0.02, 0.016, 0.016)):
    """Dark crust with glowing red cracks (Voronoi edges)."""
    key = ("lava", scale, strength, crack, base)
    if key in _materials:
        return _materials[key]
    m = bpy.data.materials.new("lava")
    nt, out = _nodes(m)
    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.feature = "DISTANCE_TO_EDGE"
    vor.inputs["Scale"].default_value = scale
    warp = nt.nodes.new("ShaderNodeTexNoise")
    warp.inputs["Scale"].default_value = scale * 1.3
    warp.inputs["Detail"].default_value = 6.0
    coord = nt.nodes.new("ShaderNodeTexCoord")
    add = nt.nodes.new("ShaderNodeVectorMath"); add.operation = "ADD"
    sc_ = nt.nodes.new("ShaderNodeVectorMath"); sc_.operation = "SCALE"; sc_.inputs["Scale"].default_value = 0.9 / scale
    nt.links.new(warp.outputs["Color"], sc_.inputs[0])
    nt.links.new(coord.outputs["Object"], add.inputs[0])
    nt.links.new(sc_.outputs[0], add.inputs[1])
    nt.links.new(add.outputs[0], vor.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeMapRange")
    ramp.inputs["From Min"].default_value = 0.0
    ramp.inputs["From Max"].default_value = crack
    ramp.inputs["To Min"].default_value = 1.0
    ramp.inputs["To Max"].default_value = 0.0
    nt.links.new(vor.outputs["Distance"], ramp.inputs["Value"])
    pw = nt.nodes.new("ShaderNodeMath"); pw.operation = "POWER"; pw.inputs[1].default_value = 2.0
    nt.links.new(ramp.outputs[0], pw.inputs[0])
    emis = nt.nodes.new("ShaderNodeEmission")
    emis.inputs["Color"].default_value = (1.0, 0.11, 0.03, 1)
    st = nt.nodes.new("ShaderNodeMath"); st.operation = "MULTIPLY"; st.inputs[1].default_value = strength
    nt.links.new(pw.outputs[0], st.inputs[0])
    nt.links.new(st.outputs[0], emis.inputs["Strength"])
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*base, 1)
    bsdf.inputs["Roughness"].default_value = 0.9
    addsh = nt.nodes.new("ShaderNodeAddShader")
    nt.links.new(bsdf.outputs[0], addsh.inputs[0])
    nt.links.new(emis.outputs[0], addsh.inputs[1])
    nt.links.new(addsh.outputs[0], out.inputs["Surface"])
    _materials[key] = m
    return m


def mat_fog(colour=(0.55, 0.55, 0.65), density=0.05, noise_scale=0.15, z0=0.0, z1=10.0, anisotropy=0.3, emission=0.0, emit_colour=(1, 0.1, 0.05),
            centre=None, radius=None):
    """Volumetric fog whose density thins out above z1 and is broken up by noise. With
    `centre` and `radius` (x, y), it also fades out towards the edges of a bank."""
    m = bpy.data.materials.new("fog")
    nt, out = _nodes(m)
    vol = nt.nodes.new("ShaderNodeVolumePrincipled")
    vol.inputs["Color"].default_value = (*colour, 1)
    vol.inputs["Anisotropy"].default_value = anisotropy
    coord = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    # World-space height: Object coords of a box at the origin equal world coords.
    nt.links.new(coord.outputs["Object"], sep.inputs[0])
    h = nt.nodes.new("ShaderNodeMapRange")
    h.inputs["From Min"].default_value = z0
    h.inputs["From Max"].default_value = z1
    h.inputs["To Min"].default_value = 1.0
    h.inputs["To Max"].default_value = 0.0
    nt.links.new(sep.outputs["Z"], h.inputs["Value"])
    tex = nt.nodes.new("ShaderNodeTexNoise")
    tex.inputs["Scale"].default_value = noise_scale
    tex.inputs["Detail"].default_value = 4.0
    nt.links.new(coord.outputs["Object"], tex.inputs["Vector"])
    shape = nt.nodes.new("ShaderNodeMapRange")
    shape.inputs["From Min"].default_value = 0.4
    shape.inputs["From Max"].default_value = 0.7
    nt.links.new(tex.outputs["Fac"], shape.inputs["Value"])
    mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"
    nt.links.new(h.outputs[0], mul.inputs[0])
    nt.links.new(shape.outputs[0], mul.inputs[1])
    if centre is not None:
        # Elliptic fall-off: 1 at the centre, 0 at the bank's edge.
        sub = nt.nodes.new("ShaderNodeVectorMath"); sub.operation = "SUBTRACT"
        sub.inputs[1].default_value = (centre[0], centre[1], 0)
        nt.links.new(coord.outputs["Object"], sub.inputs[0])
        scl = nt.nodes.new("ShaderNodeVectorMath"); scl.operation = "MULTIPLY"
        scl.inputs[1].default_value = (1.0 / radius[0], 1.0 / radius[1], 0.0)
        nt.links.new(sub.outputs[0], scl.inputs[0])
        ln = nt.nodes.new("ShaderNodeVectorMath"); ln.operation = "LENGTH"
        nt.links.new(scl.outputs[0], ln.inputs[0])
        fall = nt.nodes.new("ShaderNodeMapRange")
        fall.inputs["From Min"].default_value = 0.3
        fall.inputs["From Max"].default_value = 1.0
        fall.inputs["To Min"].default_value = 1.0
        fall.inputs["To Max"].default_value = 0.0
        nt.links.new(ln.outputs["Value"], fall.inputs["Value"])
        mul2 = nt.nodes.new("ShaderNodeMath"); mul2.operation = "MULTIPLY"
        nt.links.new(mul.outputs[0], mul2.inputs[0])
        nt.links.new(fall.outputs[0], mul2.inputs[1])
        mul = mul2
    dens = nt.nodes.new("ShaderNodeMath"); dens.operation = "MULTIPLY"; dens.inputs[1].default_value = density
    nt.links.new(mul.outputs[0], dens.inputs[0])
    nt.links.new(dens.outputs[0], vol.inputs["Density"])
    if emission > 0:
        vol.inputs["Emission Color"].default_value = (*emit_colour, 1)
        em = nt.nodes.new("ShaderNodeMath"); em.operation = "MULTIPLY"; em.inputs[1].default_value = emission
        nt.links.new(mul.outputs[0], em.inputs[0])
        nt.links.new(em.outputs[0], vol.inputs["Emission Strength"])
    nt.links.new(vol.outputs[0], out.inputs["Volume"])
    return m


def mat_cloth(colour=(0.08, 0.008, 0.01)):
    return mat_stone("cloth", colour, rough=0.95, bump=0.2, scale=8.0)


# --------------------------------------------------------------------------- lights
def light(kind, location, colour, energy, size=1.0, look_at=None, name="light", spot=None):
    ld = bpy.data.lights.new(name, kind)
    ld.color = colour
    ld.energy = energy
    if kind == "AREA":
        ld.shape = "DISK"
        ld.size = size
    elif kind in ("POINT", "SPOT"):
        ld.shadow_soft_size = size
    elif kind == "SUN":
        ld.angle = math.radians(size)
    if kind == "SPOT" and spot:
        ld.spot_size = math.radians(spot)
        ld.spot_blend = 0.6
    ob = bpy.data.objects.new(name, ld)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = Vector(location)
    if look_at is not None:
        d = Vector(look_at) - ob.location
        ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    return ob


def sun(direction_to_light, colour, strength, angle=3.0, name="sun"):
    ob = light("SUN", (0, 0, 0), colour, strength, size=angle, name=name)
    d = -Vector(direction_to_light).normalized()
    ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    return ob


# ------------------------------------------------------------------------- geometry
def _fbm(p, octaves=5, seed=0):
    off = Vector((seed * 17.3, seed * 9.1, seed * 3.7))
    return noise.fractal(p + off, 0.6, 2.1, octaves)


def ridge(x0, x1, y, base_z, height, depth, seed, layer, material, segments=160, jag=1.0, rows=6, sharp=1.6, slope=None, freq=None):
    """A jagged mountain ridge running along X at distance y: a height field strip whose
    crest is ridged noise. Silhouette-friendly: sharp peaks, steep flanks."""
    rnd = random.Random(seed)
    verts, faces = [], []
    crest = []
    for i in range(segments + 1):
        t = i / segments
        x = x0 + (x1 - x0) * t
        f_ = freq if freq is not None else 0.24 / max(height, 1)
        n = noise.ridged_multi_fractal(Vector((x * f_, seed * 3.1, 0)), 0.9, 2.2, 6, 1.0, 2.0)
        n2 = noise.noise(Vector((x * 3.0 / max(height, 1), seed * 1.7, 4.0)))
        h = height * (0.35 + 0.45 * (n / 2.0) ** sharp * 2.0 + 0.25 * n2) * jag
        if slope:
            h *= slope(t)
        crest.append(max(h, height * 0.05))
    for r in range(rows + 1):
        f = r / rows                         # 0 = crest line, 1 = foot (in front)
        for i in range(segments + 1):
            t = i / segments
            x = x0 + (x1 - x0) * t
            jitter = noise.noise(Vector((x * 15.0 / max(height, 1), r * 0.7, seed))) * depth * 0.15
            yy = y - depth * f + jitter
            z = base_z + crest[i] * (1.0 - f) ** 1.3 + noise.noise(Vector((x * 24.0 / max(height, 1), r * 1.3, seed + 5))) * height * 0.05 * f
            verts.append((x, yy + (rnd.random() - 0.5) * depth * 0.02, z))
    for r in range(rows):
        for i in range(segments):
            a = r * (segments + 1) + i
            faces.append((a, a + 1, a + segments + 2, a + segments + 1))
    # A back skirt so the silhouette is closed against the sky.
    off = len(verts)
    for i in range(segments + 1):
        t = i / segments
        x = x0 + (x1 - x0) * t
        verts.append((x, y + depth * 0.3, base_z - height * 0.2))
    for i in range(segments):
        faces.append((i + 1, i, off + i, off + i + 1))
    return mesh_object("ridge", verts, faces, layer, material)


def rock(location, size, seed, layer, material, squash=(1, 1, 1), detail=3.5, subdiv=5):
    """An angular rock: an icosphere displaced by fractal noise, flat shaded."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    off = Vector((seed * 3.3, seed * 1.9, seed * 7.1))
    for v in bm.verts:
        p = v.co.copy()
        d = 1.0 + 0.45 * noise.fractal(p * detail * 0.5 + off, 0.7, 2.0, 4) + 0.15 * noise.noise(p * detail * 2 + off)
        v.co = Vector((p.x * squash[0], p.y * squash[1], p.z * squash[2])) * d
    me = bpy.data.meshes.new("rock")
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("rock", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = Vector(location)
    ob.scale = (size, size, size)
    ob.rotation_euler = (0, 0, seed * 0.77)
    me.materials.append(material)
    return put(ob, layer)


def crag(location, radius, height, seed, layer, material, sides=40, rings=16, spikes=0.45, mesa=0.0):
    """A steep rocky outcrop (a cone of noise) for castles and towers to stand on."""
    verts, faces = [], []
    off = seed * 13.7
    for r in range(rings + 1):
        f = r / rings
        z = height * f
        rad = radius * ((1.0 - f) ** 0.85 * (1 - mesa) + (1.0 - f ** 3) * mesa) + radius * 0.08
        for s in range(sides):
            a = 2 * math.pi * s / sides
            n = noise.fractal(Vector((math.cos(a) * 2 + off, math.sin(a) * 2, f * 3)), 0.7, 2.0, 4)
            n += 0.5 * noise.ridged_multi_fractal(Vector((math.cos(a) * 5 + off, math.sin(a) * 5, f * 0.8)), 0.9, 2.0, 3, 1.0, 2.0) - 0.4
            rr = rad * (1 + spikes * n)
            verts.append((location[0] + math.cos(a) * rr, location[1] + math.sin(a) * rr, location[2] + z + n * height * 0.04))
    for r in range(rings):
        for s in range(sides):
            a = r * sides + s
            b = r * sides + (s + 1) % sides
            faces.append((a, b, b + sides, a + sides))
    top = len(verts)
    verts.append((location[0], location[1], location[2] + height * 1.02))
    for s in range(sides):
        faces.append((rings * sides + s, rings * sides + (s + 1) % sides, top))
    return mesh_object("crag", verts, faces, layer, material)


def box(location, size, layer, material, rotation=(0, 0, 0), name="box"):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = Vector(location)
    ob.rotation_euler = rotation
    me.materials.append(material)
    return put(ob, layer)


def cylinder(location, radius, height, layer, material, sides=12, top_radius=None, name="cyl", rotation=(0, 0, 0)):
    top_radius = radius if top_radius is None else top_radius
    verts, faces = [], []
    for s in range(sides):
        a = 2 * math.pi * s / sides
        verts.append((math.cos(a) * radius, math.sin(a) * radius, 0))
    for s in range(sides):
        a = 2 * math.pi * s / sides
        verts.append((math.cos(a) * top_radius, math.sin(a) * top_radius, height))
    for s in range(sides):
        faces.append((s, (s + 1) % sides, sides + (s + 1) % sides, sides + s))
    faces.append(tuple(range(sides - 1, -1, -1)))
    if top_radius > 1e-4:
        faces.append(tuple(range(sides, 2 * sides)))
    ob = mesh_object(name, verts, faces, layer, material)
    ob.location = Vector(location)
    ob.rotation_euler = rotation
    return ob


def cone(location, radius, height, layer, material, sides=8, name="cone"):
    verts = [(math.cos(2 * math.pi * s / sides) * radius, math.sin(2 * math.pi * s / sides) * radius, 0) for s in range(sides)]
    verts.append((0, 0, height))
    faces = [(s, (s + 1) % sides, sides) for s in range(sides)]
    faces.append(tuple(range(sides - 1, -1, -1)))
    ob = mesh_object(name, verts, faces, layer, material)
    ob.location = Vector(location)
    return ob


def arch_outline(width, height, pointed=True, steps=16):
    """The opening's outline (x, z), left foot to right foot, through the apex."""
    pts = []
    spring = height - (width * 0.85 if pointed else width * 0.5)
    spring = max(spring, 0)
    pts.append((-width / 2, 0))
    if pointed:
        r = width * 0.85
        # Two arcs centred on the opposite springing points.
        cx_l, cx_r = width / 2 - r, -width / 2 + r
        apex_z = math.sqrt(max(r * r - (width / 2) ** 2, 0))
        a_end = math.atan2(apex_z, -cx_l)
        for i in range(steps + 1):
            a = math.pi - (math.pi - a_end) * i / steps
            pts.append((cx_l + r * math.cos(a), spring + r * math.sin(a)))
        for i in range(1, steps + 1):
            a = (math.pi - a_end) - (math.pi - a_end) * i / steps
            pts.append((cx_r + r * math.cos(a), spring + r * math.sin(a)))
    else:
        r = width / 2
        for i in range(steps * 2 + 1):
            a = math.pi - math.pi * i / (steps * 2)
            pts.append((r * math.cos(a), spring + r * math.sin(a)))
    pts.append((width / 2, 0))
    return pts


def wall_with_arch(location, wall_w, wall_h, depth, arch_w, arch_h, layer, material, pointed=True, rotation_z=0.0, name="wall"):
    """A wall slab with an arched opening (built from the outline, no booleans)."""
    outline = arch_outline(arch_w, arch_h, pointed)
    # Outer rectangle points matched to the outline: left edge, top, right edge.
    n = len(outline)
    outer = []
    for i, (x, z) in enumerate(outline):
        t = i / (n - 1)
        # Map along the outer boundary: left side bottom->top, top left->right, right side top->bottom.
        if t < 0.25:
            outer.append((-wall_w / 2, wall_h * t / 0.25))
        elif t < 0.75:
            outer.append((-wall_w / 2 + wall_w * (t - 0.25) / 0.5, wall_h))
        else:
            outer.append((wall_w / 2, wall_h * (1 - (t - 0.75) / 0.25)))
    verts, faces = [], []
    for y in (0.0, depth):
        for (x, z) in outline:
            verts.append((x, y, z))
        for (x, z) in outer:
            verts.append((x, y, z))
    def idx(side, ring, i):
        return side * 2 * n + ring * n + i
    for side in (0, 1):
        for i in range(n - 1):
            q = (idx(side, 0, i), idx(side, 0, i + 1), idx(side, 1, i + 1), idx(side, 1, i))
            faces.append(q if side == 1 else q[::-1])
    for ring in (0, 1):
        for i in range(n - 1):
            faces.append((idx(0, ring, i), idx(0, ring, i + 1), idx(1, ring, i + 1), idx(1, ring, i)))
    ob = mesh_object(name, verts, faces, layer, material)
    ob.location = Vector(location)
    ob.rotation_euler = (0, 0, rotation_z)
    return ob


def arch_shape(location, width, height, layer, material, pointed=True, rotation_z=0.0, name="opening", thickness=0.0):
    """A flat arched shape (a window pane, a glowing doorway), facing -Y."""
    outline = arch_outline(width, height, pointed)
    verts = [(x, 0.0, z) for (x, z) in outline]
    faces = [tuple(range(len(verts)))]
    ob = mesh_object(name, verts, faces, layer, material)
    ob.location = Vector(location)
    ob.rotation_euler = (0, 0, rotation_z)
    return ob


def spire(location, radius, height, layer, material, roof_material=None, roof_h=None, sides=8, windows=None, seed=0, pinnacles=True, taper=0.9):
    """A gothic tower: a tapering shaft, a steep pointed roof, corner pinnacles and
    optional glowing slit windows (a material)."""
    roof_material = roof_material or material
    x, y, z = location
    cylinder((x, y, z), radius, height, layer, material, sides=sides, top_radius=radius * taper, name="tower")
    rh = roof_h if roof_h is not None else radius * 4.5
    cone((x, y, z + height), radius * taper * 1.15, rh, layer, roof_material, sides=sides, name="roof")
    # A thin needle on top.
    cylinder((x, y, z + height + rh * 0.95), radius * 0.04, rh * 0.35, layer, roof_material, sides=4, top_radius=0.0, name="needle")
    if pinnacles:
        for s in range(4):
            a = 2 * math.pi * (s + 0.5) / 4
            px, py = x + math.cos(a) * radius * taper * 1.05, y + math.sin(a) * radius * taper * 1.05
            cone((px, py, z + height), radius * 0.16, rh * 0.45, layer, roof_material, sides=4, name="pinnacle")
    if windows is not None:
        rnd = random.Random(seed)
        count = max(1, int(height / (radius * 2.2)))
        for k in range(count):
            if rnd.random() < 0.6:
                continue
            wz = z + height * (0.25 + 0.65 * k / max(count, 1))
            a = -math.pi / 2 + (rnd.random() - 0.5) * 0.9
            wx, wy = x + math.cos(a) * radius * 0.98, y + math.sin(a) * radius * 0.98
            arch_shape((wx, wy - 0.01, wz), radius * 0.13, radius * 0.38, layer, windows, rotation_z=a + math.pi / 2, name="window")


def castle(location, scale, seed, layer, stone, roof, windows, towers=9, spread=1.0, with_crag=True):
    """A cluster of gothic spires of many heights on a crag: the dark-fantasy castle."""
    rnd = random.Random(seed)
    x, y, z = location
    if with_crag:
        crag((x, y, z - 2 * scale), 9 * scale * spread, 10 * scale, seed, layer, stone, spikes=0.25)
    base = z + 7 * scale
    # The keep: the tallest spire in the middle.
    spire((x, y, base), 1.6 * scale, 22 * scale, layer, stone, roof, roof_h=10 * scale, windows=windows, seed=seed)
    for k in range(towers):
        a = rnd.random() * 2 * math.pi
        d = (2.5 + rnd.random() * 5.0) * scale * spread
        h = (6 + rnd.random() * 14) * scale * (1.0 - 0.35 * d / (7.5 * scale * spread))
        r = (0.6 + rnd.random() * 0.9) * scale
        spire((x + math.cos(a) * d, y + math.sin(a) * d * 0.6, base - rnd.random() * 2 * scale), r, h, layer, stone, roof,
              roof_h=r * (3.5 + rnd.random() * 3), windows=windows, seed=seed + k)
    # Curtain walls between.
    for k in range(5):
        a = rnd.random() * 2 * math.pi
        d = 3 * scale * spread
        box((x + math.cos(a) * d, y + math.sin(a) * d * 0.6, base + 1.5 * scale), (5 * scale, 1.0 * scale, 4 * scale), layer, stone,
            rotation=(0, 0, a + math.pi / 2), name="curtain")


def cathedral(location, scale, seed, layer, stone, roof, windows, rotation_z=0.0):
    """Twin west towers, a long nave with a steep roof, a rose window."""
    x, y, z = location
    rot = Matrix.Rotation(rotation_z, 4, "Z")
    def at(dx, dy, dz):
        p = rot @ Vector((dx, dy, dz))
        return (x + p.x, y + p.y, z + p.z)
    box(at(0, 6 * scale, 4 * scale), (4 * scale, 12 * scale, 8 * scale), layer, stone, rotation=(0, 0, rotation_z), name="nave")
    # Roof ridge: a prism.
    hw, ln, rh = 2.3 * scale, 12 * scale, 5 * scale
    verts = [(-hw, 0, 0), (hw, 0, 0), (0, 0, rh), (-hw, ln, 0), (hw, ln, 0), (0, ln, rh)]
    faces = [(0, 1, 2), (3, 5, 4), (0, 3, 4, 1), (1, 4, 5, 2), (0, 2, 5, 3)]
    ob = mesh_object("naveroof", verts, faces, layer, roof)
    ob.location = Vector(at(0, 0, 8 * scale)); ob.rotation_euler = (0, 0, rotation_z)
    for side in (-1, 1):
        spire(at(side * 2.6 * scale, -0.5 * scale, 0), 1.3 * scale, 15 * scale, layer, stone, roof, roof_h=9 * scale,
              windows=windows, seed=seed + side, sides=8)
    # Rose window between the towers, and lancet windows.
    arch_shape(at(0, -1.25 * scale, 9 * scale), 2.2 * scale, 2.4 * scale, layer, windows, pointed=False, rotation_z=rotation_z, name="rose")
    arch_shape(at(0, -1.25 * scale, 0.5 * scale), 1.6 * scale, 4.5 * scale, layer, windows, rotation_z=rotation_z, name="door")
    spire(at(0, 10 * scale, 8 * scale), 0.7 * scale, 4 * scale, layer, stone, roof, roof_h=7 * scale, pinnacles=False)


def dead_tree(location, height, seed, layer, material, branches=4, depth=3, lean=0.15):
    """A gnarled leafless tree as bevelled curves (thick trunk, thin twigs)."""
    rnd = random.Random(seed)
    cu = bpy.data.curves.new("tree", "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = height * 0.035
    cu.bevel_resolution = 1
    cu.use_fill_caps = True
    def grow(start, direction, length, radius, level):
        sp = cu.splines.new("POLY")
        n = 5
        sp.points.add(n - 1)
        p = Vector(start)
        d = Vector(direction).normalized()
        for i in range(n):
            sp.points[i].co = (*p, 1)
            sp.points[i].radius = radius * (1 - 0.7 * i / (n - 1))
            d = (d + Vector(((rnd.random() - 0.5) * 0.5, (rnd.random() - 0.5) * 0.5, (rnd.random() - 0.4) * 0.3))).normalized()
            p = p + d * (length / (n - 1))
        if level < depth:
            for b in range(branches if level == 0 else rnd.randint(2, 3)):
                t = 0.45 + 0.5 * rnd.random()
                idx = int(t * (n - 1))
                origin = Vector(sp.points[idx].co[:3])
                nd = (d + Vector(((rnd.random() - 0.5) * 1.6, (rnd.random() - 0.5) * 1.6, rnd.random() * 0.6))).normalized()
                grow(origin, nd, length * (0.45 + 0.25 * rnd.random()), sp.points[idx].radius * 0.6, level + 1)
    grow(Vector((0, 0, 0)), Vector((lean * (rnd.random() - 0.5), lean * (rnd.random() - 0.5), 1)), height * 0.55, 1.0, 0)
    ob = bpy.data.objects.new("tree", cu)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = Vector(location)
    cu.materials.append(material)
    return put(ob, layer)


def tube(points, radius, layer, material, name="tube", taper=None):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 2
    cu.use_fill_caps = True
    sp = cu.splines.new("POLY")
    sp.points.add(len(points) - 1)
    for i, p in enumerate(points):
        sp.points[i].co = (*p, 1)
        if taper is not None:
            sp.points[i].radius = taper(i / max(1, len(points) - 1))
    ob = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(ob)
    cu.materials.append(material)
    return put(ob, layer)


def embers(centre, spread, count, seed, layer, strength=30.0, size=0.04, colour=(1.0, 0.25, 0.05)):
    """Glowing ember particles frozen in the air (the plug-in adds drifting ones)."""
    rnd = random.Random(seed)
    m = mat_emission(colour, strength, "ember")
    for k in range(count):
        p = Vector(centre) + Vector(((rnd.random() - 0.5) * spread[0], (rnd.random() - 0.5) * spread[1], rnd.random() * spread[2]))
        s = size * (0.4 + rnd.random())
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=s)
        me = bpy.data.meshes.new("ember")
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new("ember", me)
        bpy.context.scene.collection.objects.link(ob)
        ob.location = p
        me.materials.append(m)
        put(ob, layer)
        ob.visible_shadow = False


def fog_box(location, size, layer, material):
    """A fog volume. Its mesh sits at the origin so the material's Object coordinates
    are world coordinates (the height gradient uses world Z)."""
    ob = box((0, 0, 0), size, layer, material, name="fog")
    ob.data.transform(Matrix.Translation(Vector(location)))
    ob.visible_shadow = False
    return ob


def ground(z, size, layer, material, location=(0, 0)):
    s = size / 2
    return mesh_object("ground", [(location[0] - s, location[1] - s, z), (location[0] + s, location[1] - s, z),
                                  (location[0] + s, location[1] + s, z), (location[0] - s, location[1] + s, z)],
                       [(0, 1, 2, 3)], layer, material)


def heightfield(fn, x0, x1, y0, y1, nx, ny, layer, material, name="terrain"):
    verts, faces = [], []
    for j in range(ny + 1):
        for i in range(nx + 1):
            x = x0 + (x1 - x0) * i / nx
            y = y0 + (y1 - y0) * j / ny
            verts.append((x, y, fn(x, y)))
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i
            faces.append((a, a + 1, a + nx + 2, a + nx + 1))
    return mesh_object(name, verts, faces, layer, material)


# --------------------------------------------------------------------------- render
def configure(width, height, samples):
    sc = bpy.context.scene
    sc.render.resolution_x = width
    sc.render.resolution_y = height
    sc.render.resolution_percentage = 100
    sc.cycles.samples = samples
    sc.cycles.use_adaptive_sampling = True
    sc.cycles.adaptive_threshold = 0.02
    sc.cycles.use_denoising = False          # OIDN runs in paint.py with albedo + normal
    sc.cycles.max_bounces = 4
    sc.cycles.diffuse_bounces = 2
    sc.cycles.glossy_bounces = 2
    sc.cycles.transmission_bounces = 2
    sc.cycles.volume_bounces = 1
    sc.cycles.transparent_max_bounces = 8
    try:
        sc.cycles.volume_step_rate = 2.0
        sc.cycles.volume_max_steps = 256
    except Exception:
        pass
    sc.render.film_transparent = True
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    sc.render.use_compositing = False
    vl = sc.view_layers[0]
    vl.use_pass_z = True
    vl.cycles.denoising_store_passes = True
    try:
        sc.render.image_settings.media_type = "MULTI_LAYER_IMAGE"
    except Exception:
        pass
    sc.render.image_settings.file_format = "OPEN_EXR_MULTILAYER"
    sc.render.image_settings.color_depth = "16"
    sc.render.image_settings.exr_codec = "ZIP"


def render_layer(layer, path):
    """Renders one depth layer: only its objects are seen by the camera; every other
    object still casts shadows and blocks light."""
    sc = bpy.context.scene
    for ob in sc.objects:
        if ob.type in ("MESH", "CURVE", "META"):
            ob.visible_camera = ob.get("layer") == layer
            ob.hide_render = False
            # Fog volumes of other layers must not dim or tint this layer's light.
            if ob.name.startswith("fog") and ob.get("layer") != layer:
                ob.hide_render = True
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


# ------------------------------------------------------------------ interiors
def layer_by_depth(y, cuts):
    """Picks a layer from a distance along the view: cuts = (near_end, mid_end)."""
    return "near" if y < cuts[0] else ("mid" if y < cuts[1] else "far")


def gothic_hall(length, width, height, bays, cuts, stone, rib_mat, window_mat=None, aisle=4.0, column_r=0.9,
                end_wall=True, end_window=None, floor_mat=None, vault=True, side_windows=True, seed=0, window_every=1):
    """A vaulted gothic interior along +Y from y=0: a floor, two rows of columns joined by
    pointed arches, outer walls with tall lancet windows, rib vaults and an end wall.
    Layers follow the distance (cuts). Returns the bay positions."""
    floor_mat = floor_mat or stone
    half = width / 2
    spring = height * 0.55
    ys = [length * (k + 0.5) / bays for k in range(bays)]
    # Floor in three pieces so each depth layer has its own.
    edges = [0, cuts[0], cuts[1], length + 20]
    for k in range(3):
        y0, y1 = edges[k], edges[k + 1]
        if y1 <= y0:
            continue
        w = half + aisle + 2
        mesh_object("floor", [(-w, y0 - 30 * (k == 0), 0), (w, y0 - 30 * (k == 0), 0), (w, y1, 0), (-w, y1, 0)], [(0, 1, 2, 3)],
                    ("near", "mid", "far")[k], floor_mat)
    for side in (-1, 1):
        x = side * half
        wx = side * (half + aisle)
        for i, y in enumerate(ys):
            L = layer_by_depth(y, cuts)
            cylinder((x, y, 0), column_r, spring, L, stone, sides=10, name="column")
            box((x, y, 0.4), (column_r * 2.6, column_r * 2.6, 0.8), L, stone, name="plinth")
            box((x, y, spring), (column_r * 2.4, column_r * 2.4, 0.6), L, stone, name="capital")
            # Outer wall bay with a lancet window.
            box((wx + side * 0.5, y, height * 0.5), (1.0, length / bays + 0.05, height), L, stone, name="wall")
            if side_windows and window_mat is not None and i % window_every == 0 and y > cuts[0] * 0.7:
                ww, wh, wz = (length / bays) * 0.2, height * 0.5, height * 0.25
                arch_shape((wx - side * 0.02, y, wz), ww, wh, L, window_mat, rotation_z=side * math.pi / 2, name="lancet")
                # Mullion and transoms: the window reads as tracery, not a slab of light.
                mull = mat_stone("mullion", (0.01, 0.01, 0.012), bump=0.1)
                tube([(wx - side * 0.12, y, wz), (wx - side * 0.12, y, wz + wh)], ww * 0.06, L, mull, name="mullion")
                for f in (0.33, 0.62):
                    tube([(wx - side * 0.12, y - ww / 2, wz + wh * f), (wx - side * 0.12, y + ww / 2, wz + wh * f)], ww * 0.04, L, mull, name="transom")
            # Arcade: a pointed arch between this column and the next.
            if i + 1 < len(ys):
                y2 = ys[i + 1]
                span = y2 - y
                pts = []
                for t in range(13):
                    u = t / 12
                    yy = y + span * u
                    zz = spring + math.sin(math.pi * u) ** 0.7 * span * 0.62
                    pts.append((x, yy, zz))
                tube(pts, column_r * 0.45, L, rib_mat, name="arcade")
                box((x, (y + y2) / 2, spring + span * 0.62 + (height - spring - span * 0.62) / 2 + 0.3),
                    (0.8, span, max(0.5, height - spring - span * 0.62)), L, stone, name="clerestory")
            if vault:
                # Transverse rib across the nave (pointed).
                pts = []
                for t in range(17):
                    u = t / 16
                    xx = -half + width * u
                    zz = spring + (1 - abs(2 * u - 1) ** 1.6) * (height - spring)
                    pts.append((xx, y, zz))
                if side == 1:
                    tube(pts, column_r * 0.35, L, rib_mat, name="rib")
    if vault:
        # The vault web: a ceiling over the nave (a pointed barrel) per layer.
        for k in range(3):
            y0, y1 = edges[k], min(edges[k + 1], length)
            if y1 <= y0:
                continue
            verts, faces = [], []
            n = 16
            for j, yy in enumerate((y0 - (6 if k == 0 else 0), y1)):
                for t in range(n + 1):
                    u = t / n
                    verts.append((-half + width * u, yy, spring + (1 - abs(2 * u - 1) ** 1.6) * (height - spring) + 0.4))
            for t in range(n):
                faces.append((t, t + 1, n + 2 + t, n + 1 + t))
            mesh_object("vault", verts, faces, ("near", "mid", "far")[k], stone)
            for side in (-1, 1):
                box((side * (half + aisle * 0.5), (y0 + y1) / 2, height * 0.8), (aisle + 1, y1 - y0, 0.6), ("near", "mid", "far")[k], stone, name="aislevault")
    if end_wall:
        box((0, length + 0.5, height / 2), (width + 2 * aisle + 4, 1.0, height + 4), "far", stone, name="endwall")
        if end_window is not None:
            end_window(length)
    return ys


def cave_tunnel(length, radius, cuts, material, seed, wobble=0.35, rings=48, sides=40, flatten=0.55, start=-10.0, ceiling=1.0):
    """A rough cave interior along +Y: a noisy tube seen from inside, its floor flattened.
    Split into near/mid/far sections by distance."""
    sections = [(start, cuts[0], "near"), (cuts[0], cuts[1], "mid"), (cuts[1], length, "far")]
    for y0, y1, L in sections:
        if y1 <= y0:
            continue
        n = max(4, int(rings * (y1 - y0) / (length - start)))
        verts, faces = [], []
        for r in range(n + 1):
            y = y0 + (y1 - y0) * r / n
            for s in range(sides):
                a = 2 * math.pi * s / sides
                p = Vector((math.cos(a), y * 0.05, math.sin(a)))
                d = 1 + wobble * noise.fractal(Vector((math.cos(a) * 1.5, y * 0.04, math.sin(a) * 1.5)) + Vector((seed, 0, 0)), 0.7, 2.0, 5)
                d += 0.15 * noise.ridged_multi_fractal(Vector((math.cos(a) * 4, y * 0.15, math.sin(a) * 4 + seed)), 0.9, 2.0, 3, 1.0, 2.0)
                z = math.sin(a) * radius * d * (ceiling if math.sin(a) > 0 else 1.0)
                z = max(z, -radius * flatten)
                verts.append((math.cos(a) * radius * d * 1.25, y, z + radius * flatten))
        for r in range(n):
            for s in range(sides):
                a = r * sides + s
                b = r * sides + (s + 1) % sides
                faces.append((a, a + sides, b + sides, b))
        mesh_object("cave", verts, faces, L, material, smooth=True)


def torch(location, layer, strength=900.0, flame=8.0, wall_dir=(0, 0)):
    x, y, z = location
    wood = mat_stone("wood", (0.02, 0.012, 0.008), bump=0.2)
    cylinder((x, y, z - 0.9), 0.06, 0.9, layer, wood, sides=6, top_radius=0.1, name="torchstick")
    f = cone((x, y, z), 0.13, 0.42, layer, mat_emission((1.0, 0.25, 0.05), flame, "flame"), sides=6, name="flame")
    f.visible_shadow = False
    light("POINT", (x + wall_dir[0], y + wall_dir[1], z + 0.35), (1.0, 0.24, 0.07), strength, size=0.25, name="torchlight")


def brazier(location, layer, strength=3000.0, size=1.0):
    x, y, z = location
    iron = mat_metal((0.08, 0.07, 0.07), 0.6, "iron")
    cylinder((x, y, z), 0.5 * size, 0.45 * size, layer, iron, sides=12, top_radius=0.75 * size, name="bowl")
    for k in range(3):
        a = 2 * math.pi * k / 3
        tube([(x + math.cos(a) * 0.5 * size, y + math.sin(a) * 0.5 * size, z), (x + math.cos(a) * 0.7 * size, y + math.sin(a) * 0.7 * size, z - 1.1 * size)], 0.05 * size, layer, iron)
    coals = cylinder((x, y, z + 0.42 * size), 0.68 * size, 0.05 * size, layer, mat_emission((1.0, 0.18, 0.04), 12.0, "coals"), sides=12, name="coals")
    coals.visible_shadow = False
    for k in range(4):
        fl = cone((x + (k - 1.5) * 0.18 * size, y + ((k * 7) % 3 - 1) * 0.12 * size, z + 0.45 * size), 0.17 * size, (0.6 + 0.3 * (k % 2)) * size, layer,
                  mat_emission((1.0, 0.22, 0.05), 10.0, "flame2"), sides=5, name="flame")
        fl.visible_shadow = False
    light("POINT", (x, y, z + 1.1 * size), (1.0, 0.2, 0.06), strength, size=0.4 * size, name="brazierlight")


def candle(location, layer, height=0.4, strength=60.0):
    x, y, z = location
    wax = mat_stone("wax", (0.35, 0.3, 0.26), bump=0.05)
    cylinder((x, y, z), 0.05, height, layer, wax, sides=8, name="candle")
    f = cone((x, y, z + height), 0.03, 0.12, layer, mat_emission((1.0, 0.35, 0.1), 12.0, "candleflame"), sides=5, name="flame")
    f.visible_shadow = False
    if strength > 0:
        light("POINT", (x, y, z + height + 0.15), (1.0, 0.3, 0.1), strength, size=0.05, name="candlelight")


def bars(x0, x1, y, z0, z1, layer, count, radius=0.05, rotation_z=0.0):
    iron = mat_metal((0.09, 0.08, 0.08), 0.5, "iron")
    for k in range(count):
        t = k / max(1, count - 1)
        cylinder((x0 + (x1 - x0) * t, y, z0), radius, z1 - z0, layer, iron, sides=6, name="bar")
    box(((x0 + x1) / 2, y, z1), (abs(x1 - x0) + 0.2, 0.12, 0.12), layer, iron, name="crossbar")
    box(((x0 + x1) / 2, y, z0 + (z1 - z0) * 0.55), (abs(x1 - x0) + 0.2, 0.08, 0.1), layer, iron, name="crossbar")


def chain(top, length, layer, links=None, sway=0.0):
    iron = mat_metal((0.08, 0.07, 0.07), 0.45, "iron")
    pts = []
    n = 12
    for k in range(n + 1):
        t = k / n
        pts.append((top[0] + sway * math.sin(t * math.pi), top[1], top[2] - length * t))
    tube(pts, 0.045, layer, iron, name="chain")


def gold_pile(location, radius, height, layer, seed, coins=40):
    gold = mat_metal((0.9, 0.55, 0.22), 0.28, "gold")
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=16, radius=1.0)
    for v in bm.verts:
        p = v.co.copy()
        n = noise.fractal(p * 3 + Vector((seed, 0, 0)), 0.6, 2.0, 4)
        v.co = Vector((p.x * radius, p.y * radius, max(p.z, -0.05) * height * (1 + 0.25 * n)))
    me = bpy.data.meshes.new("gold")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("gold", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = Vector(location)
    me.materials.append(gold)
    put(ob, layer)
    rnd = random.Random(seed)
    for k in range(coins):
        a = rnd.random() * 2 * math.pi
        d = radius * (0.9 + rnd.random() * 0.8)
        c = cylinder((location[0] + math.cos(a) * d, location[1] + math.sin(a) * d, location[2] + 0.02), 0.12, 0.03, layer, gold, sides=10, name="coin")
        c.rotation_euler = (rnd.random() * 1.2, rnd.random() * 1.2, 0)
    return ob


def chest(location, layer, size=1.0, rotation_z=0.0, open_glow=True):
    wood = mat_stone("chestwood", (0.025, 0.012, 0.008), bump=0.3)
    gold = mat_metal((0.9, 0.55, 0.22), 0.3, "gold")
    x, y, z = location
    box((x, y, z + 0.35 * size), (1.4 * size, 0.8 * size, 0.7 * size), layer, wood, rotation=(0, 0, rotation_z), name="chest")
    lid = box((x, y + 0.45 * size, z + 0.95 * size), (1.4 * size, 0.12 * size, 0.7 * size), layer, wood, rotation=(-0.5, 0, rotation_z), name="lid")
    gold_pile((x, y, z + 0.65 * size), 0.6 * size, 0.25 * size, layer, int(x * 10) % 97, coins=6)
    if open_glow:
        light("POINT", (x, y - 0.2 * size, z + 1.2 * size), (1.0, 0.45, 0.15), 40 * size, size=0.3, name="chestglow")


def banner(location, width, length, layer, rotation_z=0.0, colour=(0.09, 0.006, 0.008)):
    cloth = mat_cloth(colour)
    x, y, z = location
    verts = [(-width / 2, 0, 0), (width / 2, 0, 0), (width / 2, 0, -length), (0, 0, -length - width * 0.5), (-width / 2, 0, -length)]
    ob = mesh_object("banner", verts, [(0, 1, 2, 3, 4)], layer, cloth)
    ob.location = Vector(location)
    ob.rotation_euler = (0, 0, rotation_z)
    box((x, y, z + 0.05), (width * 1.3, 0.1, 0.1), layer, mat_metal((0.1, 0.08, 0.07), 0.5, "iron"), rotation=(0, 0, rotation_z), name="pole")
    return ob


def throne(location, layer, scale=1.0, stone_mat=None):
    st = stone_mat or mat_stone("throne", (0.02, 0.018, 0.022), bump=0.3)
    x, y, z = location
    s = scale
    box((x, y, z + 0.45 * s), (1.6 * s, 1.2 * s, 0.9 * s), layer, st, name="seat")
    box((x, y + 0.55 * s, z + 2.4 * s), (1.6 * s, 0.25 * s, 3.2 * s), layer, st, name="back")
    for side in (-1, 1):
        box((x + side * 0.85 * s, y, z + 1.2 * s), (0.25 * s, 1.2 * s, 0.9 * s), layer, st, name="arm")
    # A crown of spikes above the back.
    for k in range(7):
        t = (k - 3) / 3
        h = (2.4 - abs(t) * 1.4) * s
        cone((x + t * 0.75 * s, y + 0.55 * s, z + 4.0 * s), 0.12 * s, h, layer, st, sides=4, name="spike")
    cushion = mat_cloth((0.12, 0.008, 0.01))
    box((x, y - 0.05 * s, z + 0.95 * s), (1.2 * s, 1.0 * s, 0.12 * s), layer, cushion, name="cushion")


def steps(location, width, count, rise, run, layer, material):
    x, y, z = location
    for k in range(count):
        box((x, y + k * run, z + rise * (k + 0.5)), (width - k * 0.4, run * 1.02, rise), layer, material, name="step")


def table(location, length, width, layer, height=1.0):
    wood = mat_stone("tablewood", (0.022, 0.013, 0.009), bump=0.2)
    x, y, z = location
    box((x, y, z + height), (width, length, 0.12), layer, wood, name="tabletop")
    for dx in (-1, 1):
        for dy in (-1, 1):
            box((x + dx * (width / 2 - 0.15), y + dy * (length / 2 - 0.2), z + height / 2), (0.15, 0.15, height), layer, wood, name="leg")


def pew_rows(x, y0, rows, spacing, width, layer_fn):
    wood = mat_stone("pewwood", (0.018, 0.011, 0.008), bump=0.2)
    for r in range(rows):
        y = y0 + r * spacing
        L = layer_fn(y)
        box((x, y, 0.45), (width, 0.45, 0.08), L, wood, name="pewseat")
        box((x, y + 0.25, 0.85), (width, 0.08, 0.8), L, wood, name="pewback")
        for side in (-1, 1):
            box((x + side * width / 2, y + 0.05, 0.5), (0.1, 0.6, 1.0), L, wood, name="pewend")


def horned_lord(location, scale, layer, eye_strength=60.0):
    """The final boss: a vast horned, winged silhouette, glowing eyes."""
    x, y, z = location
    s = scale
    skin = mat_stone("lordskin", (0.008, 0.007, 0.009), bump=0.8, scale=3)
    horn = mat_stone("horn", (0.015, 0.013, 0.015), bump=0.3)
    mb = bpy.data.metaballs.new("lord")
    mb.resolution = 0.25 * s
    mb.render_resolution = 0.12 * s
    def ball(px, py, pz, r, kind="BALL", size=None):
        e = mb.elements.new()
        e.type = kind
        e.co = (px, py, pz)
        e.radius = r
        if size:
            e.size_x, e.size_y, e.size_z = size
    # A gaunt torso (narrow waist, broad hunched shoulders) and a long skull.
    ball(0, 0, 3.0 * s, 2.0 * s, "ELLIPSOID", (0.8, 0.6, 2.2))
    ball(0, 0, 7.4 * s, 2.4 * s, "ELLIPSOID", (1.6, 0.8, 0.95))
    ball(0, 0, 5.0 * s, 2.0 * s, "ELLIPSOID", (1.1, 0.7, 1.2))
    for side in (-1, 1):
        ball(side * 2.9 * s, 0.2 * s, 8.6 * s, 1.3 * s)
    ball(0, -0.6 * s, 10.0 * s, 1.15 * s, "ELLIPSOID", (0.85, 1.1, 1.3))
    ob = bpy.data.objects.new("lord", mb)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = Vector(location)
    mb.materials.append(skin)
    put(ob, layer)
    for side in (-1, 1):
        # Long arms hanging down, clawed.
        arm = [(x + side * (3.8 + 1.2 * t) * s, y - 0.6 * t * s, z + (8.0 - 7.5 * t) * s) for t in [k / 10 for k in range(11)]]
        tube(arm, 0.55 * s, layer, skin, name="arm", taper=lambda u: 1.0 - 0.6 * u)
        for c in range(3):
            tube([arm[-1], (arm[-1][0] + side * (0.3 + c * 0.4) * s, arm[-1][1] - 0.8 * s, arm[-1][2] - 1.6 * s)], 0.14 * s, layer, horn, name="claw", taper=lambda u: 1 - 0.9 * u)
        # Great curving horns.
        pts = [(x + side * (0.8 + 4.2 * t) * s, y - 0.4 * s, z + (10.8 + 4.6 * math.sin(t * 2.4) - 1.8 * t * t) * s) for t in [k / 15 for k in range(16)]]
        tube(pts, 0.6 * s, layer, horn, name="horn", taper=lambda u: 1.0 - 0.95 * u)
        # Bat wings: a few spars from the shoulder with a membrane between them.
        root = Vector((x + side * 3.0 * s, y + 1.0 * s, z + 8.6 * s))
        tips = [Vector((x + side * (9 + 7 * k) * s, y + 2.0 * s, z + (17 - 5.5 * k) * s)) for k in range(4)]
        elbow = Vector((x + side * 8.0 * s, y + 1.6 * s, z + 15.5 * s))
        tube([tuple(root), tuple(elbow)], 0.45 * s, layer, skin, name="wingarm", taper=lambda u: 1 - 0.4 * u)
        for t in tips:
            tube([tuple(elbow), tuple(t)], 0.22 * s, layer, skin, name="spar", taper=lambda u: 1 - 0.8 * u)
        verts = [tuple(root), tuple(elbow)] + [tuple(t) for t in tips]
        # Membrane: scalloped between the spar tips, down to the body.
        low = Vector((x + side * 3.2 * s, y + 1.2 * s, z + 3.5 * s))
        verts.append(tuple(low))
        faces = [(1, 2, 3), (1, 3, 4), (1, 4, 5), (0, 1, 5, 6)]
        mesh_object("wing", verts, faces, layer, mat_stone("membrane", (0.012, 0.004, 0.005), bump=0.2))
        e = cone((x + side * 0.42 * s, y - 1.7 * s, z + 10.2 * s), 0.16 * s, 0.05 * s, layer, mat_emission((1.0, 0.1, 0.03), eye_strength, "eyes"), sides=6, name="eye")
        e.rotation_euler = (math.pi / 2, 0, 0)
    return ob
