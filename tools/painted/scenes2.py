"""The painted scenes of the other eight locations (after the prototype's approval):
Cathedral, Haunted Forest, Caverns, Village, Swamp, Mountains, Necropolis, Wizard's
Tower. Same style and helpers as scenes.py."""
import math
import random

from mathutils import Vector

import blib as B
from scenes import (scene, stone, dark_rock, roof_mat, windows, moon_lighting, hotspot, lightpt, interior_fog, shafts, door,
                    dungeon_stone, lava_strip, CRIMSON, EMBER, SLATE, WINDOW)


# ------------------------------------------------------------------ shared set-ups
def vista(moon, far=(2300, 620), near_range=(1400, 300), fog=(-150, 10, 0.012), ground_z=-240, seed=3, moon_strength=4.0):
    """The exterior frame: the moon as backlight, two hazy ranges, a valley of mist."""
    moon_lighting(moon, strength=moon_strength)
    if far:
        B.ridge(-2600, 2800, far[0], -80, far[1], 260, seed, "far", B.mat_stone("far2", (0.03, 0.028, 0.035), haze=0.05), segments=240, rows=4)
    if near_range:
        B.ridge(-2200, 2400, near_range[0], -80, near_range[1], 200, seed + 4, "far", B.mat_stone("far1", (0.02, 0.019, 0.024), haze=0.022), segments=220, rows=4)
    B.ground(ground_z, 9000, "far", B.mat_stone("valley", (0.01, 0.01, 0.012)))
    if fog:
        f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=fog[2], noise_scale=0.006, z0=fog[0], z1=fog[1], anisotropy=0.2,
                      emission=0.0009, emit_colour=SLATE)
        B.fog_box((0, 1100, (fog[0] + fog[1]) / 2), (5000, 2000, fog[1] - fog[0] + 60), "fog", f)


def ground_patch(z, x0, x1, y0, y1, layer, material, rough=0.6, seed=1, res=90):
    """Uneven ground (a small height field)."""
    from mathutils import noise as N
    ob = B.heightfield(lambda x, y: z + rough * N.fractal(Vector((x * 0.08, y * 0.08, seed)), 0.6, 2.0, 4), x0, x1, y0, y1, res, res, layer, material)
    for poly in ob.data.polygons:
        poly.use_smooth = True
    return ob


def ground_layers(z, x_half, cuts, far_y, material, rough=0.5, seed=1):
    """Ground split into near / mid / far strips."""
    edges = [-10, cuts[0], cuts[1], far_y]
    for k, layer in enumerate(("near", "mid", "far")):
        ground_patch(z, -x_half, x_half, edges[k], edges[k + 1] + 0.5, layer, material, rough=rough, seed=seed + k)


def wooden_room(width, depth, height, layer_fn, wall, beam, floor, cuts):
    """A timber room along +Y from y=0: floor, walls, ceiling beams and posts."""
    for k, (y0, y1) in enumerate(((-6, cuts[0]), (cuts[0], cuts[1]), (cuts[1], depth))):
        L = ("near", "mid", "far")[k]
        B.mesh_object("floor", [(-width / 2, y0, 0), (width / 2, y0, 0), (width / 2, y1, 0), (-width / 2, y1, 0)], [(0, 1, 2, 3)], L, floor)
        for side in (-1, 1):
            B.box((side * (width / 2 + 0.2), (y0 + y1) / 2, height / 2), (0.4, y1 - y0, height), L, wall, name="wall")
        B.box((0, (y0 + y1) / 2, height + 0.2), (width + 0.8, y1 - y0, 0.4), L, wall, name="ceiling")
    B.box((0, depth + 0.2, height / 2), (width + 1, 0.4, height + 1), "far", wall, name="endwall")
    y = 1.5
    while y < depth:
        L = layer_fn(y)
        B.box((0, y, height - 0.25), (width, 0.35, 0.35), L, beam, name="beam")
        for side in (-1, 1):
            B.box((side * (width / 2 - 0.2), y, height / 2), (0.3, 0.3, height), L, beam, name="post")
        y += 3.0


def lantern(location, layer, strength=200.0):
    x, y, z = location
    iron = B.mat_metal((0.06, 0.055, 0.05), 0.5, "iron")
    B.box((x, y, z + 0.18), (0.22, 0.22, 0.04), layer, iron, name="lanterntop")
    glow = B.box((x, y, z), (0.16, 0.16, 0.3), layer, B.mat_emission((1.0, 0.2, 0.06), 8.0, "lanternglow"), name="lanternglass")
    glow.visible_shadow = False
    B.light("POINT", (x, y - 0.2, z), (1.0, 0.22, 0.07), strength, size=0.1, name="lanternlight")


def wisps(centre, spread, count, seed, layer, strength=10.0):
    """Will-o'-wisps: pale-red orbs hanging in the mist."""
    B.embers(centre, spread, count, seed, layer, strength=strength, size=0.12, colour=(1.0, 0.35, 0.3))


def trees_row(xs, y, z, heights, seed, layer, material, lean=0.15):
    rnd = random.Random(seed)
    for k, x in enumerate(xs):
        h = heights[0] + rnd.random() * (heights[1] - heights[0])
        B.dead_tree((x, y + (rnd.random() - 0.5) * 6, z), h, seed + k, layer, material, branches=4, depth=3, lean=lean)


def hs(*points):
    return [hotspot(k, p) for k, p in enumerate(points)]


# ===================================================================== CATHEDRAL
@scene("cathedral/cloister")
def cathedral_cloister():
    B.camera((0, -6, 3.0), (0, 120, 26), lens=24)
    moon = (650, 120)
    vista(moon, far=(2300, 420), near_range=None, fog=None)
    st, rf = stone(1.4), roof_mat()
    win = B.mat_emission(WINDOW, 3.0, "dimwin")
    B.cathedral((0, 110, 0), 6.5, 7, "mid", st, rf, win)
    cuts = (12, 60)
    ground_layers(0, 60, cuts, 400, B.mat_stone("grass", (0.012, 0.012, 0.012), bump=0.6), rough=0.3)
    # The cloister's arcades on both sides: pointed arches on slim columns.
    for side in (-1, 1):
        for k in range(9):
            y = 2 + k * 6
            L = B.layer_by_depth(y, cuts)
            B.cylinder((side * 10, y, 0), 0.35, 4.2, L, st, sides=10, name="col")
            pts = [(side * 10, y + 6 * u, 4.2 + math.sin(math.pi * u) ** 0.7 * 2.6) for u in [t / 10 for t in range(11)]]
            B.tube(pts, 0.25, L, st, name="arch")
            B.box((side * 10, y + 3, 7.6), (0.8, 6.0, 1.4), L, st, name="arcadewall")
        B.box((side * 13, 30, 5), (0.6, 60, 10), "mid", st, name="walk")
    B.dead_tree((-2, 28, 0), 9, 7, "mid", dark_rock(), branches=4, depth=3)
    B.box((0, 18, 0.5), (2.4, 2.4, 1.0), "mid", st, name="well")
    door((-10.2, 9, 0), 1.6, 3.4, "near", rotation_z=-math.pi / 2, glow=1.0)                  # down to the crypt
    B.light("AREA", (0, 220, 120), CRIMSON, 1.5e6, size=60, look_at=(0, 110, 40), name="rim")
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.03, noise_scale=0.08, z0=-1, z1=2.5, anisotropy=0.3, emission=0.0006, emit_colour=SLATE)
    B.fog_box((0, 40, 1.5), (40, 90, 3), "fog", f)
    B.embers((0, 60, 1), (20, 60, 10), 25, 5, "mid", size=0.06, strength=10)
    return {"sky": {"moon": [moon[0], moon[1], 92], "horizon": 320, "clouds": 0.7, "stars": 0.6, "seed": 21},
            "haze": {"amount": 0.7},
            "anchors": hs((-2, 28, 3), (0, 103, 6), (-10.2, 9, 1.8), (13.6, 104, 80)) + [lightpt((0, 103, 30), 20, "window")]}


@scene("cathedral/nave")
def cathedral_nave():
    L_, W, H = 90.0, 15.0, 38.0
    B.camera((0, -2, 2.6), (0, 80, 12), lens=20)
    cuts = (14, 44)
    st, rib = stone(1.35), stone(1.1)
    win = B.mat_emission((1.0, 0.05, 0.03), 1.6, "lancet")
    def end_window(length):
        B.arch_shape((0, length - 0.1, 14), 14, 14, "far", B.mat_emission((1.0, 0.05, 0.03), 2.6, "rose"), pointed=False, name="rose")
        tr = stone(0.6)
        for k in range(16):
            a = 2 * math.pi * k / 16
            B.tube([(0, length - 0.3, 21), (math.cos(a) * 6.8, length - 0.3, 21 + math.sin(a) * 6.8)], 0.22, "far", tr, name="tracery")
        B.arch_shape((0, length - 0.1, 2), 5, 10, "far", B.mat_emission((1.0, 0.05, 0.03), 1.6, "lowwin"), name="lowwin")
        B.tracery(0, length - 0.1, 2, 5, 10, "far", columns=3, rows=4)
    ys = B.gothic_hall(L_, W, H, 10, cuts, st, rib, win, aisle=5.0, column_r=1.1, end_window=end_window, seed=31)
    B.pew_rows(-3.6, 8, 14, 3.0, 5.0, lambda y: B.layer_by_depth(y, cuts))
    B.pew_rows(3.6, 8, 14, 3.0, 5.0, lambda y: B.layer_by_depth(y, cuts))
    B.steps((0, 66, 0), 12, 4, 0.35, 1.0, "far", st)
    B.box((0, 72, 2.0), (5, 1.6, 1.2), "far", stone(1.7), name="altar")
    for k in range(9):
        B.candle((-2.2 + k * 0.55, 71.8, 2.6), "far", height=0.35 + 0.25 * (k % 3), strength=25)
    for side in (-1, 1):
        for k in range(4):
            B.candle((side * (4 + k * 0.5), 69, 1.4), "far", height=1.0 + 0.4 * (k % 2), strength=18)
    door((-W / 2 - 4.9, ys[1] + 4.5, 0), 3.0, 6.0, "mid", rotation_z=math.pi / 2)
    door((W / 2 + 4.9, ys[3] + 4.5, 0), 3.0, 6.0, "mid", rotation_z=-math.pi / 2)
    shafts([((-W / 2 - 16, y, 34), (3, y + 2)) for y in ys[1:8:2]], energy=6.0e5, spot=10)
    B.sun((0.2, -1, 0.8), SLATE, 0.05, name="fill")
    interior_fog(L_, W + 12, H, density=0.02, top=10)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 200, 150, 0.4]], "seed": 22},
            "haze": {"amount": 0.7},
            "anchors": hs((-W / 2 - 4.9, ys[1] + 4.5, 3), (0, 30, 1.5), (5, 66, 1), (W / 2 + 4.9, ys[3] + 4.5, 3))
            + [lightpt((0, 71.8, 3.0), 26, "candle")]}


@scene("cathedral/crypt")
def cathedral_crypt():
    L_, W, H = 36.0, 9.0, 6.0
    B.camera((0.5, -1.5, 1.7), (0, 34, 2.0), lens=20)
    cuts = (7, 20)
    st = dungeon_stone(1.3)
    ys = B.gothic_hall(L_, W, H, 7, cuts, st, dungeon_stone(1.1), None, aisle=3.0, column_r=0.65, seed=33, floor_mat=dungeon_stone(1.2))
    lid = stone(1.5)
    for k, (x, y) in enumerate(((-6.2, 8), (6.2, 12), (-6.2, 18), (6.2, 22), (0, 29))):
        L = B.layer_by_depth(y, cuts)
        B.box((x, y, 0.45), (1.3, 2.6, 0.9), L, st, name="sarcophagus")
        B.box((x, y, 0.98), (1.45, 2.75, 0.16), L, lid, name="lid")
        B.candle((x + 0.6, y - 1.2, 0.9), L, height=0.3, strength=42)
    B.box((0, 29, 1.4), (0.5, 1.6, 0.3), "far", stone(1.0), name="effigy")
    for y in ys[1::2]:
        B.torch((-W / 2 - 2.9, y, 2.3), B.layer_by_depth(y, cuts), strength=180, wall_dir=(0.3, 0))
    door((0, L_ - 0.1, 0), 2.0, 3.6, "far", glow=1.4)
    for yy, zz in ((1.0, 3.0), (6.0, 3.0), (12.0, 3.2), (18.0, 3.2), (0.0, 9.0), (0.0, 15.0)):
        B.light("POINT", (0.0, yy, zz), EMBER, 350, size=1.0, name="dimfill")
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    interior_fog(L_, W + 6, H, density=0.06, top=1.2)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 280, 90, 0.4]], "seed": 23},
            "haze": {"amount": 0.7},
            "anchors": hs((0, 3, 0.3), (0, L_, 2), (0, 29, 1.6), (-W / 2 - 2.9, ys[3], 1.6))
            + [lightpt((x + 0.6, y - 1.2, 1.25), 6, "candle") for (x, y) in ((-6.2, 8), (6.2, 12), (-6.2, 18), (6.2, 22), (0, 29))]}


@scene("cathedral/bell-tower")
def cathedral_bell_tower():
    B.camera((0, -1, 2.0), (0, 30, 6.5), lens=20)
    moon = (480, 150)
    vista(moon, far=(2300, 520), near_range=(1300, 260), fog=(-200, -20, 0.01), ground_z=-300, moon_strength=3.5)
    st = stone(1.3)
    # The belfry: a wall pierced by a great arched opening onto the night, the bell hanging in it.
    B.wall_with_arch((0, 12, -4), 30, 26, 2.0, 9, 13, "mid", st)
    B.box((0, 6, -4.3), (30, 14, 0.6), "mid", st, name="belfryfloor")
    for side in (-1, 1):
        B.box((side * 6.5, 6, 6), (1.2, 14, 20), "near", st, name="belfrywall")
    beam = B.mat_stone("beam", (0.022, 0.014, 0.01), bump=0.3)
    B.box((0, 9, 8.6), (13, 0.6, 0.6), "mid", beam, name="headstock")
    B.chain((0, 9, 8.3), 1.4, "mid")
    B.bell((0, 9, 3.2), 3.2, "mid")
    for k, x in enumerate((-2.0, 1.5)):
        B.chain((x, 4, 9), 9 + k, "near", sway=0.3)
    B.box((0, 2, 9.2), (13, 0.5, 0.5), "near", beam, name="beam")
    B.light("AREA", (0, 4, 0), (1.0, 0.25, 0.08), 400, size=2, look_at=(0, 9, 4), name="bellunder")
    B.embers((0, 14, 0), (16, 6, 10), 20, 9, "mid", size=0.05, strength=10)
    return {"sky": {"moon": [moon[0], moon[1], 70], "horizon": 330, "clouds": 0.75, "stars": 0.7, "seed": 24},
            "haze": {"amount": 0.7},
            "anchors": hs((-5.5, 4, 0), (-3.5, 12, 1), (5.4, 6, -2), (0, 9, 5.5))}


# ================================================================ HAUNTED FOREST
def forest_floor(cuts, far=300, seed=1):
    ground_layers(0, 80, cuts, far, B.mat_stone("forestfloor", (0.012, 0.011, 0.012), bump=0.7, scale=3), rough=0.35, seed=seed)


@scene("haunted-forest/forest-edge")
def forest_edge():
    B.camera((0, -8, 2.2), (0, 100, 7), lens=24)
    moon = (480, 175)
    vista(moon, far=(2300, 380), near_range=(900, 160), fog=(-30, 6, 0.012), ground_z=-30)
    rk = dark_rock(1.1)
    cuts = (14, 70)
    forest_floor(cuts, 600)
    trees_row([-60, -46, -34, -24, 24, 33, 45, 58, -15, 15], 70, 0, (18, 30), 41, "mid", rk)
    trees_row([-90, -70, 72, 92, -40, 40, 0], 140, 0, (24, 36), 51, "far", rk)
    trees_row([-11, 10], 8, 0, (13, 16), 61, "near", rk, lean=0.4)
    B.rock((-6, 4, 0), 1.6, 3, "near", rk, squash=(1.4, 1, 0.6))
    # A path into the trees, a signpost.
    B.mesh_object("path", [(-1.5, -6, 0.05), (1.5, -6, 0.05), (0.8, 70, 0.05), (-0.8, 70, 0.05)], [(0, 1, 2, 3)], "mid",
                  B.mat_stone("pathmat", (0.03, 0.028, 0.03), bump=0.4))
    wood = B.mat_stone("wood", (0.02, 0.012, 0.008), bump=0.2)
    B.box((3, 12, 1.2), (0.15, 0.15, 2.4), "mid", wood, name="post")
    B.box((3.5, 12, 2.1), (1.2, 0.08, 0.3), "mid", wood, rotation=(0, 0.2, 0.3), name="sign")
    lantern((3.0, 11.8, 1.7), "mid", strength=80)
    B.light("AREA", (0, 160, 30), CRIMSON, 3.0e5, size=40, look_at=(0, 60, 10), name="rim")
    wisps((0, 60, 2), (40, 30, 4), 14, 7, "mid")
    return {"sky": {"moon": [moon[0], moon[1], 100], "horizon": 330, "clouds": 0.6, "stars": 0.6, "seed": 31},
            "haze": {"amount": 0.75},
            "anchors": hs((-6, 4, 1.5), (0, 50, 1), (-30, 80, 3), (24, 70, 4)) + [lightpt((3.0, 11.8, 1.8), 10, "fire")]}


@scene("haunted-forest/old-path")
def forest_old_path():
    B.camera((0, -4, 1.8), (0, 60, 4.5), lens=20)
    moon = (520, 120)
    vista(moon, far=None, near_range=None, fog=None, moon_strength=2.5)
    rk = dark_rock(1.1)
    cuts = (10, 40)
    forest_floor(cuts, 200, seed=4)
    # Trees crowding the path, leaning over it.
    rnd = random.Random(5)
    for k in range(34):
        y = 3 + k * 2.2
        side = -1 if k % 2 else 1
        x = side * (3 + rnd.random() * 6 + y * 0.05)
        B.dead_tree((x, y, 0), 12 + rnd.random() * 12, 70 + k, B.layer_by_depth(y, cuts), rk, branches=4, depth=3, lean=0.5)
    B.mesh_object("path", [(-1.2, -4, 0.05), (1.2, -4, 0.05), (0.6, 80, 0.05), (-0.6, 80, 0.05)], [(0, 1, 2, 3)], "mid",
                  B.mat_stone("pathmat", (0.03, 0.028, 0.03), bump=0.4))
    for k, (x, y) in enumerate(((-1.8, 6), (2.0, 18), (-1.5, 30))):
        B.rock((x, y, 0), 0.5, 80 + k, B.layer_by_depth(y, cuts), rk, squash=(1.2, 1, 0.6))
    B.light("AREA", (0, 90, 8), CRIMSON, 2.0e4, size=10, look_at=(0, 30, 2), name="pathglow")
    wisps((0, 30, 1.5), (8, 40, 3), 22, 9, "mid", strength=14)
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.05, noise_scale=0.1, z0=-1, z1=2.0, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 40, 1.5), (40, 90, 3.5), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 60], "horizon": 360, "clouds": 0.5, "stars": 0.5, "seed": 32},
            "haze": {"amount": 0.8},
            "anchors": hs((0, 2, 0.5), (0, 20, 1), (-5, 34, 2), (4, 46, 2)) + [lightpt((0, 30, 1.5), 40, "lava")]}


@scene("haunted-forest/witchs-hut")
def forest_witchs_hut():
    B.camera((-2, -6, 2.4), (2, 40, 4), lens=22)
    moon = (260, 120)
    vista(moon, far=(2300, 360), near_range=None, fog=(-20, 2, 0.01), ground_z=-20, moon_strength=3.0)
    rk = dark_rock(1.1)
    cuts = (8, 30)
    forest_floor(cuts, 200, seed=7)
    wood = B.mat_stone("hutwood", (0.02, 0.013, 0.009), bump=0.5, scale=4)
    thatch = B.mat_stone("thatch", (0.016, 0.012, 0.01), bump=0.9, scale=6)
    # The hut on crooked stilts.
    for dx, dy in ((-2.4, -1.8), (2.4, -1.8), (-2.4, 1.8), (2.6, 2.0)):
        B.cylinder((4 + dx, 22 + dy, 0), 0.18, 1.8, "mid", wood, sides=6, name="stilt").rotation_euler = (dx * 0.02, dy * 0.03, 0)
    B.cottage((4, 22, 1.8), 5.5, 4.5, 3.0, "mid", wood, thatch, B.mat_emission((1.0, 0.12, 0.04), 6.0, "hutwin"), rotation_z=0.25, seed=3, crooked=0.06)
    B.steps((2.8, 18.5, 0), 1.4, 4, 0.45, 0.5, "mid", wood)
    B.cauldron((-2.5, 16, 0.3), 1.2, "mid", glow=5.0)
    trees_row([-14, -9, 12, 16, 22], 30, 0, (14, 22), 91, "far", rk, lean=0.4)
    trees_row([-6.5], 4, 0, (10, 12), 95, "near", rk, lean=0.5)
    B.rock((5, 3, 0), 1.0, 6, "near", rk, squash=(1.5, 1, 0.5))
    B.light("POINT", (4, 19.5, 3.3), (1.0, 0.18, 0.05), 300, size=0.3, name="hutglow")
    B.embers((-2.5, 16, 1.2), (1.5, 1.5, 4), 20, 3, "mid", size=0.04, strength=14)
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.05, noise_scale=0.1, z0=-1, z1=1.4, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 25, 1), (50, 60, 3), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 85], "horizon": 330, "clouds": 0.6, "stars": 0.6, "seed": 33},
            "haze": {"amount": 0.75},
            "anchors": hs((0, 4, 0.5), (-8, 30, 3), (3.4, 19.6, 3.4), (12, 30, 4))
            + [lightpt((-2.5, 16, 1.2), 14, "fire"), lightpt((3.4, 19.6, 3.4), 8, "window")]}


@scene("haunted-forest/ancient-grove")
def forest_ancient_grove():
    B.camera((0, -10, 2.6), (0, 30, 6), lens=22)
    moon = (690, 110)
    vista(moon, far=(2300, 360), near_range=None, fog=(-20, 3, 0.01), ground_z=-20, moon_strength=3.0)
    rk = dark_rock(1.1)
    cuts = (6, 26)
    forest_floor(cuts, 200, seed=9)
    # A ring of standing stones around the great ancient tree and its altar stone.
    menhir_mat = B.mat_stone("menhir", (0.03, 0.029, 0.032), bump=1.0, scale=2)
    for k in range(9):
        a = math.pi * 1.1 + 2 * math.pi * k / 9
        B.menhir((math.cos(a) * 9, 20 + math.sin(a) * 6, 0), 3.2 + (k % 3) * 0.8, 100 + k, B.layer_by_depth(20 + math.sin(a) * 6, cuts), menhir_mat)
    B.dead_tree((0, 25, 0), 22, 7, "mid", B.mat_stone("ancient", (0.016, 0.014, 0.013), bump=1.0), branches=6, depth=3, lean=0.05)
    B.box((0, 18, 0.45), (2.6, 1.4, 0.9), "mid", menhir_mat, name="altar")
    glow = B.box((0, 18, 0.92), (1.6, 0.6, 0.04), "mid", B.mat_emission(CRIMSON, 4.0, "altarglow"), name="altarglow")
    glow.visible_shadow = False
    B.light("POINT", (0, 17.5, 1.6), CRIMSON, 900, size=0.5, name="altarlight")
    trees_row([-26, -18, 18, 27, -34, 34], 36, 0, (16, 26), 121, "far", rk)
    wisps((0, 20, 2), (16, 10, 5), 24, 11, "mid", strength=16)
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.05, noise_scale=0.1, z0=-1, z1=1.2, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 20, 1), (60, 50, 3), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 90], "horizon": 330, "clouds": 0.6, "stars": 0.7, "seed": 34},
            "haze": {"amount": 0.75},
            "anchors": hs((0, -2, 0.5), (-9, 18, 2), (9, 20, 2), (0, 18, 1.6)) + [lightpt((0, 18, 1.0), 26, "lava")]}


# ======================================================================= CAVERNS
@scene("caverns/cave-mouth")
def caverns_cave_mouth():
    B.camera((0, -10, 4), (0, 80, 18), lens=22)
    moon = (760, 95)
    vista(moon, far=(2300, 600), near_range=(1300, 320), fog=(-150, 0, 0.01), ground_z=-200, moon_strength=3.5)
    rk = dark_rock(1.15)
    # A cliff face with a gaping mouth, a glow deep inside.
    B.crag((0, 120, -60), 120, 160, 4, "mid", rk, spikes=0.4, mesa=0.5)
    B.crag((-90, 140, -60), 80, 130, 5, "mid", rk, spikes=0.5)
    B.arch_shape((-6, 65, -2), 28, 34, "mid", B.mat_emission((1.0, 0.06, 0.03), 0.06, "mouth"), pointed=False, name="mouth")
    B.light("POINT", (-6, 70, 6), CRIMSON, 1.5e4, size=4, name="mouthglow")
    ground_patch(-2, -80, 80, -10, 64, "mid", rk, rough=2.0, seed=4)
    for k, (x, y, s) in enumerate(((-18, 10, 6), (20, 14, 8), (-28, 30, 10), (32, 34, 12), (8, 6, 3))):
        B.rock((x, y, -1), s, 200 + k, "near" if y < 16 else "mid", rk, squash=(1.2, 1, 0.9))
    B.embers((-6, 60, 0), (20, 10, 20), 30, 5, "mid", size=0.12, strength=12)
    return {"sky": {"moon": [moon[0], moon[1], 75], "horizon": 280, "clouds": 0.7, "stars": 0.6, "seed": 41},
            "haze": {"amount": 0.7},
            "anchors": hs((0, 8, 2), (-20, 64, 8), (8, 64, 4), (-6, 64, 18)) + [lightpt((-6, 64, 6), 40, "lava")]}


@scene("caverns/crystal-cavern")
def caverns_crystal_cavern():
    B.camera((0, -2, 2.2), (0, 40, 3.6), lens=20)
    cuts = (8, 26)
    rk = dungeon_stone(1.1)
    B.cave_tunnel(50, 11, cuts, rk, 51, wobble=0.35, flatten=0.4, ceiling=1.0)
    cm = B.mat_crystal((1.0, 0.1, 0.07), 2.0)
    pale = B.mat_crystal((0.9, 0.85, 0.95), 0.4)
    rnd = random.Random(3)
    for k in range(46):
        y = 3 + rnd.random() * 40
        side = -1 if k % 2 else 1
        x = side * (5 + rnd.random() * 6)
        L = B.layer_by_depth(y, cuts)
        h = 1.0 + rnd.random() * (4 if y > 12 else 2)
        B.crystal((x, y, 0), h, h * 0.16, L, cm if rnd.random() < 0.6 else pale, tilt=((rnd.random() - 0.5) * 0.8, side * -0.3 * rnd.random()))
    for k in range(12):
        y = 6 + rnd.random() * 30
        B.crystal(((rnd.random() - 0.5) * 8, y, 9 + rnd.random()), 2 + rnd.random() * 2, 0.3, B.layer_by_depth(y, cuts), cm, tilt=(math.pi, 0))
    B.crystal((0, 34, 0), 7, 1.0, "far", cm)
    for y in (10, 22, 34):
        B.light("POINT", (0, y, 2), CRIMSON, 600, size=1.0, name="crystallight")
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    interior_fog(50, 24, 14, density=0.015, top=3)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 260, 90, 0.4]], "seed": 42},
            "haze": {"amount": 0.7},
            "anchors": hs((0, 3, 0.3), (0, 34, 4), (-7, 20, 1.5), (7, 26, 1.5))
            + [lightpt((0, 34, 4), 30, "glint"), lightpt((-7, 14, 1.5), 14, "glint"), lightpt((7, 20, 1.5), 14, "glint")]}


@scene("caverns/underground-lake")
def caverns_underground_lake():
    B.camera((0, -4, 2.4), (0, 60, 5), lens=20)
    cuts = (10, 40)
    rk = dungeon_stone(1.1)
    B.cave_tunnel(90, 20, cuts, rk, 61, wobble=0.35, flatten=0.3, ceiling=0.8)
    water = B.mat_water()
    for k, (y0, y1) in enumerate(((4, cuts[0]), (cuts[0], cuts[1]), (cuts[1], 92))):
        B.mesh_object("lake", [(-40, y0, 0.5), (40, y0, 0.5), (40, y1, 0.5), (-40, y1, 0.5)], [(0, 1, 2, 3)], ("near", "mid", "far")[k], water)
    rnd = random.Random(5)
    for k in range(24):
        y = 8 + rnd.random() * 70
        x = (rnd.random() - 0.5) * 36
        B.cone((x, y, 20), 0.5 + rnd.random(), -(2 + rnd.random() * 6), B.layer_by_depth(y, cuts), rk, sides=6, name="stalactite")
    # A far glow beyond the water and an islet with a lantern.
    B.light("AREA", (0, 88, 6), CRIMSON, 2.0e4, size=20, look_at=(0, 40, 0), name="farglow")
    B.rock((6, 34, 0), 3, 9, "mid", rk, squash=(1.6, 1.2, 0.4))
    lantern((6, 34, 1.6), "mid", strength=120)
    B.rock((-8, 2, 0), 2.0, 11, "near", rk, squash=(1.4, 1, 0.5))
    B.rock((9, 6, 0), 2.6, 12, "near", rk, squash=(1.4, 1, 0.5))
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    interior_fog(90, 40, 30, density=0.012, top=3)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 270, 160, 0.5]], "seed": 43},
            "haze": {"amount": 0.75},
            "anchors": hs((-8, 2, 1.5), (-12, 40, 4), (0, 70, 2), (6, 34, 2.4))
            + [lightpt((6, 34, 1.6), 8, "fire"), lightpt((0, 60, 0.6), 50, "glint"), lightpt((3, 30, 0.6), 30, "glint")]}


@scene("caverns/dragons-hoard")
def caverns_dragons_hoard():
    B.camera((0, -6, 3.0), (0, 40, 5), lens=22)
    cuts = (8, 26)
    rk = dungeon_stone(1.1)
    B.cave_tunnel(60, 18, cuts, rk, 71, wobble=0.4, flatten=0.35, ceiling=1.1)
    B.gold_pile((0, 30, 0), 12, 3.5, "far", 11, coins=90)
    B.gold_pile((-10, 18, 0), 4, 1.4, "mid", 13, coins=40)
    B.gold_pile((9, 14, 0), 3.5, 1.2, "mid", 15, coins=40)
    B.dragon((2, 32, 2.6), 2.6, "far", rotation_z=0.25)
    B.chest((-6, 8, 0), "mid", size=1.2, rotation_z=0.5)
    B.chest((7, 22, 0), "mid", size=1.3, rotation_z=-0.6)
    for k, (x, y, s) in enumerate(((-11, 3, 2.5), (12, 4, 3))):
        B.rock((x, y, 0), s, 300 + k, "near", rk, squash=(1.3, 1, 0.8))
    B.light("POINT", (0, 22, 6), (1.0, 0.3, 0.1), 5000, size=2, name="goldglow")
    B.light("AREA", (0, 55, 16), CRIMSON, 2.0e4, size=20, look_at=(0, 30, 2), name="rim")
    B.light("SPOT", (4, 30, 30), CRIMSON, 2.0e5, size=1, look_at=(2, 30, 3), name="shaft", spot=16)
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    interior_fog(60, 36, 20, density=0.012, top=3)
    B.embers((0, 26, 2), (16, 10, 8), 30, 21, "mid", size=0.05, strength=10, colour=(1.0, 0.45, 0.15))
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 250, 120, 0.4]], "seed": 44},
            "haze": {"amount": 0.7},
            "anchors": hs((-11, 3, 2), (-10, 18, 2), (9, 14, 2), (2, 32, 6))
            + [lightpt((0, 30, 3), 60, "glint"), lightpt((-10, 18, 1.2), 26, "glint"), lightpt((-9.4, 25.4, 8.0), 5, "eyes")]}


# ======================================================================= VILLAGE
def village_mats():
    return (B.mat_stone("plaster", (0.035, 0.032, 0.03), bump=0.4, scale=3), B.mat_stone("thatch", (0.016, 0.012, 0.01), bump=0.9, scale=6),
            B.mat_emission((1.0, 0.16, 0.05), 6.0, "cottagewin"))


@scene("village/village-green")
def village_green():
    B.camera((0, -10, 3.0), (0, 60, 7), lens=22)
    moon = (300, 120)
    vista(moon, far=(2300, 420), near_range=(1000, 200), fog=(-40, 3, 0.01), ground_z=-40, moon_strength=3.0)
    wall, roof, win = village_mats()
    cuts = (12, 40)
    ground_layers(0, 90, cuts, 300, B.mat_stone("green", (0.012, 0.013, 0.012), bump=0.6), rough=0.3, seed=11)
    rnd = random.Random(4)
    for k, (x, y, r) in enumerate(((-18, 30, 0.4), (-10, 42, 0.2), (12, 40, -0.3), (20, 28, -0.5), (-24, 48, 0.6), (26, 50, -0.2), (2, 56, 0.0))):
        B.cottage((x, y, 0), 6 + rnd.random() * 2, 5 + rnd.random() * 2, 3.5, B.layer_by_depth(y, cuts), wall, roof, win, rotation_z=r, seed=k)
    st, rf = stone(1.3), roof_mat()
    B.spire((-2, 70, 0), 3.0, 18, "far", st, rf, roof_h=14, windows=windows(5.0), seed=2, pinnacles=False)
    # The well on the green, a lamp post, an old oak.
    B.cylinder((0, 18, 0), 1.2, 1.0, "mid", st, sides=16, name="well")
    beam = B.mat_stone("beam", (0.022, 0.014, 0.01), bump=0.3)
    for side in (-1, 1):
        B.box((side * 1.0, 18, 1.8), (0.15, 0.15, 1.6), "mid", beam, name="wellpost")
    B.prism_roof((0, 18, 2.6), 2.6, 1.8, 0.8, "mid", roof, rotation_z=math.pi / 2)
    lantern((6, 14, 2.6), "mid", strength=120)
    B.cylinder((6, 14, 0), 0.08, 2.5, "mid", B.mat_metal((0.05, 0.05, 0.05), 0.5, "iron"), sides=6, name="lamppost")
    B.dead_tree((-8, 12, 0), 10, 5, "mid", dark_rock(), branches=5, depth=3)
    B.light("AREA", (0, 120, 30), CRIMSON, 3.0e5, size=40, look_at=(0, 40, 6), name="rim")
    return {"sky": {"moon": [moon[0], moon[1], 80], "horizon": 330, "clouds": 0.6, "stars": 0.7, "seed": 51},
            "haze": {"amount": 0.75},
            "anchors": hs((0, 18, 1.5), (-18, 27, 2), (12, 37, 2), (20, 25, 2))
            + [lightpt((6, 14, 2.6), 10, "fire"), lightpt((-18, 27.4, 1.6), 8, "window"), lightpt((20, 25.4, 1.6), 8, "window")]}


@scene("village/tavern")
def village_tavern():
    W, D, H = 12.0, 20.0, 4.2
    B.camera((1.0, -1.0, 1.7), (-0.5, 18, 1.6), lens=20)
    cuts = (5, 12)
    wall = B.mat_stone("tavernwall", (0.03, 0.022, 0.016), bump=0.6, scale=3)
    beam = B.mat_stone("beam", (0.02, 0.012, 0.008), bump=0.3)
    floor = B.mat_stone("planks", (0.022, 0.014, 0.01), bump=0.3, scale=5)
    wooden_room(W, D, H, lambda y: B.layer_by_depth(y, cuts), wall, beam, floor, cuts)
    # The hearth at the far end, tables with candles, barrels, a bar.
    st = stone(1.4)
    B.box((0, D - 0.4, 1.6), (4.5, 1.2, 3.2), "far", st, name="hearth")
    B.arch_shape((0, D - 1.05, 0.2), 2.6, 1.8, "far", B.mat_emission((1.0, 0.2, 0.05), 3.0, "fireplace"), pointed=False, name="fireplace")
    B.light("POINT", (0, D - 2.0, 0.8), (1.0, 0.25, 0.07), 1500, size=0.5, name="hearthfire")
    for k, (x, y) in enumerate(((-3, 5), (3, 7), (-3, 11), (3, 13))):
        L = B.layer_by_depth(y, cuts)
        B.table((x, y, 0), 2.2, 1.0, L, height=0.8)
        B.candle((x, y, 0.86), L, height=0.18, strength=10)
    wood = B.mat_stone("barrel", (0.025, 0.015, 0.01), bump=0.3)
    for k, y in enumerate((3, 4.2, 15)):
        B.cylinder((-W / 2 + 0.8, y, 0), 0.45, 1.0, B.layer_by_depth(y, cuts), wood, sides=12, name="barrel")
    B.box((W / 2 - 1.2, 9, 0.55), (0.8, 7, 1.1), "mid", wood, name="bar")
    B.box((W / 2 - 0.4, 9, 2.4), (0.3, 7, 0.08), "mid", wood, name="shelf")
    B.sun((0.2, -1, 0.8), SLATE, 0.02, name="fill")
    interior_fog(D, W, H, density=0.03, top=H)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 300, 80, 0.3]], "seed": 52},
            "haze": {"amount": 0.6},
            "anchors": hs((-W / 2 + 0.8, 3, 1.4), (0, 9, 1), (-3, 11, 1.2), (0, D - 1, 1.2))
            + [lightpt((0, D - 1.1, 0.8), 26, "fire")] + [lightpt((x, y, 1.1), 5, "candle") for (x, y) in ((-3, 5), (3, 7), (-3, 11), (3, 13))]}


@scene("village/market-square")
def village_market():
    B.camera((0, -8, 2.4), (0, 40, 4.5), lens=22)
    moon = (640, 110)
    vista(moon, far=(2300, 380), near_range=None, fog=(-20, 2, 0.008), ground_z=-20, moon_strength=3.0)
    wall, roof, win = village_mats()
    cuts = (10, 28)
    ground_layers(0, 60, cuts, 200, B.mat_stone("cobbles", (0.026, 0.025, 0.027), bump=1.2, scale=8), rough=0.05, seed=12)
    for k, (x, y, r) in enumerate(((-16, 26, 0.0), (-16, 34, 0.0), (16, 24, 0.0), (16, 33, 0.0), (-7, 44, 0.0), (7, 44, 0.0), (0, 48, 0.0))):
        B.cottage((x, y, 0), 7, 7, 6, B.layer_by_depth(y, cuts), wall, roof, win, rotation_z=(math.pi / 2 if x else 0) * (1 if x < 0 else -1) if abs(x) > 10 else 0, seed=10 + k)
    reds = [(0.08, 0.006, 0.008), (0.03, 0.025, 0.03), (0.05, 0.004, 0.006)]
    for k, (x, y, r) in enumerate(((-5, 14, 0.2), (5, 16, -0.2), (-6, 24, 0.1), (6, 26, -0.1))):
        B.market_stall((x, y, 0), 3, 1.6, B.layer_by_depth(y, cuts), reds[k % 3], rotation_z=r)
        lantern((x, y - 0.9, 2.2), B.layer_by_depth(y, cuts), strength=70)
    st = stone(1.3)
    B.cylinder((0, 32, 0), 1.6, 0.8, "far", st, sides=16, name="fountain")
    B.cylinder((0, 32, 0.8), 0.3, 2.2, "far", st, sides=8, name="fountaincol")
    B.light("AREA", (0, 80, 20), CRIMSON, 1.5e5, size=30, look_at=(0, 30, 4), name="rim")
    return {"sky": {"moon": [moon[0], moon[1], 70], "horizon": 330, "clouds": 0.6, "stars": 0.6, "seed": 53},
            "haze": {"amount": 0.7},
            "anchors": hs((0, 2, 0.5), (-16, 22, 2), (0, 32, 2.5), (16, 22, 2))
            + [lightpt((x, y - 0.9, 2.2), 7, "fire") for (x, y) in ((-5, 14), (5, 16), (-6, 24), (6, 26))]}


@scene("village/blacksmith")
def village_blacksmith():
    W, D, H = 10.0, 12.0, 4.5
    B.camera((0.5, -1.0, 1.8), (-0.5, 10, 1.7), lens=20)
    cuts = (4, 8)
    wall = B.mat_stone("smithwall", (0.028, 0.025, 0.024), bump=0.8, scale=3)
    beam = B.mat_stone("beam", (0.02, 0.012, 0.008), bump=0.3)
    floor = B.mat_stone("dirt", (0.02, 0.017, 0.015), bump=0.6, scale=4)
    wooden_room(W, D, H, lambda y: B.layer_by_depth(y, cuts), wall, beam, floor, cuts)
    st = stone(1.3)
    # The forge, glowing, its hood; the anvil; tools on the wall; a quench barrel.
    B.box((-1.5, 9.5, 0.6), (2.6, 1.8, 1.2), "far", st, name="forge")
    coals = B.box((-1.5, 9.5, 1.22), (2.0, 1.3, 0.06), "far", B.mat_lava(3.0, 14.0, crack=0.2), name="coals")
    B.box((-1.5, 10.0, 3.3), (2.8, 2.0, 1.6), "far", st, name="hood")
    B.light("POINT", (-1.5, 9.2, 1.7), (1.0, 0.2, 0.05), 2500, size=0.5, name="forgefire")
    B.anvil((0.8, 6.2, 0.0), 1.2, "mid")
    B.light("POINT", (0.8, 5.8, 1.4), (1.0, 0.35, 0.1), 60, size=0.1, name="hotiron")
    iron = B.mat_metal((0.07, 0.065, 0.06), 0.4, "iron")
    for k in range(6):
        B.box((W / 2 - 0.1, 3 + k * 1.0, 2.2), (0.04, 0.12, 1.0 + (k % 3) * 0.3), "mid", iron, name="tool")
    B.cylinder((3.3, 8, 0), 0.5, 0.9, "mid", B.mat_stone("barrel", (0.025, 0.015, 0.01), bump=0.3), sides=12, name="quench")
    B.sun((0.2, -1, 0.8), SLATE, 0.02, name="fill")
    interior_fog(D, W, H, density=0.03, top=H)
    B.embers((-1.5, 9.2, 1.4), (1.6, 1.2, 2.5), 50, 31, "far", size=0.025, strength=25)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[400, 300, 80, 0.3]], "seed": 54},
            "haze": {"amount": 0.6},
            "anchors": hs((3.5, 2, 1.2), (-4, 4, 1.4), (3.3, 8, 1.2), (0.8, 6.2, 1.0))
            + [lightpt((-1.5, 9.4, 1.3), 30, "fire"), lightpt((0.8, 6.2, 0.9), 10, "fire")]}


# ========================================================================= SWAMP
def swamp_base(cuts, far=300):
    water = B.mat_water((0.01, 0.011, 0.012))
    for k, (y0, y1) in enumerate(((-10, cuts[0]), (cuts[0], cuts[1]), (cuts[1], far))):
        B.mesh_object("water", [(-120, y0, 0), (120, y0, 0), (120, y1, 0), (-120, y1, 0)], [(0, 1, 2, 3)], ("near", "mid", "far")[k], water)


@scene("swamp/mire")
def swamp_mire():
    B.camera((0, -8, 1.8), (0, 80, 4), lens=22)
    moon = (480, 150)
    vista(moon, far=(2300, 360), near_range=(900, 140), fog=None, ground_z=-5, moon_strength=3.0)
    rk = dark_rock(1.0)
    cuts = (10, 40)
    swamp_base(cuts, 800)
    rnd = random.Random(7)
    for k in range(16):
        y = 6 + rnd.random() * 60
        x = (rnd.random() - 0.5) * 70
        if abs(x) < 4 and y < 30:
            x += 10
        B.dead_tree((x, y, -0.5), 6 + rnd.random() * 10, 400 + k, B.layer_by_depth(y, cuts), rk, branches=4, depth=3, lean=0.7)
        B.rock((x, y, -0.3), 1.2, 450 + k, B.layer_by_depth(y, cuts), rk, squash=(1.6, 1.6, 0.3))
    for k, (x, y) in enumerate(((-7, 4), (8, 6))):
        B.rock((x, y, -0.2), 1.6, 470 + k, "near", rk, squash=(1.6, 1.2, 0.35))
    wood = B.mat_stone("boardwalk", (0.02, 0.013, 0.009), bump=0.4, scale=4)
    for k in range(10):
        y = 2 + k * 3
        B.box((math.sin(k * 0.6) * 1.2, y, 0.25), (1.4, 2.8, 0.12), B.layer_by_depth(y, cuts), wood, rotation=(0, 0, math.sin(k) * 0.1), name="plank")
    lantern((1.5, 18, 1.3), "mid", strength=80)
    wisps((0, 40, 1.5), (40, 40, 3), 20, 17, "mid", strength=14)
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.03, noise_scale=0.06, z0=-0.5, z1=2.5, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 70, 1.5), (160, 150, 4), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 85], "horizon": 345, "clouds": 0.6, "stars": 0.6, "seed": 61},
            "haze": {"amount": 0.8},
            "anchors": hs((0, 4, 0.6), (-12, 30, 2), (14, 40, 2)) + [lightpt((1.5, 18, 1.3), 8, "fire"), lightpt((0, 70, 0.1), 50, "glint")]}


@scene("swamp/bog")
def swamp_bog():
    B.camera((0, -4, 1.4), (0, 40, 2.5), lens=20)
    moon = (300, 130)
    vista(moon, far=None, near_range=None, fog=None, moon_strength=2.5)
    rk = dark_rock(1.0)
    cuts = (6, 22)
    swamp_base(cuts, 200)
    rnd = random.Random(9)
    root = B.mat_stone("roots", (0.016, 0.013, 0.011), bump=0.8)
    for k in range(26):
        y = 3 + rnd.random() * 36
        x = (rnd.random() - 0.5) * 30
        L = B.layer_by_depth(y, cuts)
        B.dead_tree((x, y, -0.3), 4 + rnd.random() * 8, 500 + k, L, root, branches=5, depth=3, lean=0.9)
    for k in range(10):
        y = 4 + rnd.random() * 20
        x = (rnd.random() - 0.5) * 16
        B.rock((x, y, -0.2), 0.8 + rnd.random(), 540 + k, B.layer_by_depth(y, cuts), B.mat_stone("moss", (0.014, 0.016, 0.012), bump=1.0), squash=(1.5, 1.5, 0.3))
    wisps((0, 24, 1.0), (14, 16, 2.5), 26, 19, "mid", strength=18)
    B.light("POINT", (2, 16, 1.2), (1.0, 0.3, 0.25), 250, size=0.5, name="wispglow")
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.06, noise_scale=0.1, z0=-0.5, z1=1.6, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 30, 1), (60, 70, 3), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 60], "horizon": 360, "clouds": 0.5, "stars": 0.4, "seed": 62},
            "haze": {"amount": 0.85},
            "anchors": hs((0, 2, 0.4), (-6, 14, 1), (6, 20, 1.2)) + [lightpt((2, 16, 1.2), 30, "lava")]}


@scene("swamp/sunken-ruins")
def swamp_sunken_ruins():
    B.camera((0, -8, 2.0), (0, 50, 6), lens=22)
    moon = (560, 120)
    vista(moon, far=(2300, 380), near_range=(900, 150), fog=None, ground_z=-5, moon_strength=3.5)
    rk = dark_rock(1.0)
    cuts = (10, 34)
    swamp_base(cuts, 600)
    st = B.mat_stone("ruinstone", (0.03, 0.029, 0.03), bump=1.0, scale=2)
    # A drowned chapel: broken arches and columns standing out of the black water.
    for k, (x, y, h) in enumerate(((-9, 20, 9), (-9, 28, 7), (9, 22, 10), (9, 30, 4), (-9, 36, 5), (9, 38, 8))):
        L = B.layer_by_depth(y, cuts)
        B.cylinder((x, y, -1), 0.8, h, L, st, sides=10, name="col")
    for x in (-9, 9):
        pts = [(x, 20 + 8 * u, 9 + math.sin(math.pi * u) ** 0.7 * 4) for u in [t / 10 for t in range(11)]]
        B.tube(pts, 0.5, "mid", st, name="arch")
    B.wall_with_arch((0, 44, -1), 22, 18, 1.5, 7, 13, "far", st)
    B.arch_shape((0, 44.8, -1), 7, 13, "far", B.mat_emission((1.0, 0.05, 0.03), 0.15, "ruinglow"), name="ruinglow")
    B.dead_tree((-14, 30, -0.5), 10, 9, "mid", rk, branches=4, depth=3, lean=0.6)
    for k, (x, y) in enumerate(((-6, 4), (7, 5))):
        B.rock((x, y, -0.3), 1.4, 600 + k, "near", st, squash=(1.4, 1.2, 0.6))
    B.light("AREA", (0, 60, 10), CRIMSON, 4.0e4, size=10, look_at=(0, 30, 2), name="rim")
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.03, noise_scale=0.06, z0=-0.5, z1=2.0, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 50, 1.5), (120, 110, 4), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 80], "horizon": 345, "clouds": 0.6, "stars": 0.6, "seed": 63},
            "haze": {"amount": 0.8},
            "anchors": hs((0, 4, 0.6), (-9, 24, 3), (0, 44, 6)) + [lightpt((0, 44, 5), 30, "window"), lightpt((0, 30, 0.1), 40, "glint")]}


# ===================================================================== MOUNTAINS
def snow():
    return B.mat_stone("snow", (0.18, 0.18, 0.2), rough=0.6, bump=0.6, scale=2)


@scene("mountains/foothills")
def mountains_foothills():
    B.camera((0, -10, 6), (0, 200, 60), lens=24)
    moon = (700, 100)
    vista(moon, far=(2600, 1100), near_range=(1500, 600), fog=(-120, 20, 0.008), ground_z=-200, moon_strength=3.5)
    rk = dark_rock(1.15)
    cuts = (20, 120)
    ground_layers(0, 160, cuts, 500, B.mat_stone("hills", (0.016, 0.015, 0.017), bump=0.8, scale=0.4), rough=4.0, seed=21)
    B.mesh_object("road", [(-2.5, -10, 0.2), (2.5, -10, 0.2), (1.0, 120, 3), (-1.0, 120, 3)], [(0, 1, 2, 3)], "mid",
                  B.mat_stone("pathmat", (0.03, 0.028, 0.03), bump=0.4))
    for k, (x, y, s) in enumerate(((-14, 12, 5), (18, 16, 7), (-30, 40, 10), (34, 50, 14), (-8, 70, 6))):
        B.rock((x, y, 0), s, 700 + k, B.layer_by_depth(y, cuts), rk, squash=(1.2, 1, 0.8))
    B.dead_tree((10, 14, 0), 9, 3, "mid", rk, branches=4, depth=3)
    # A watchtower ruin on a knoll.
    st = stone(1.2)
    B.crag((-60, 160, -10), 40, 40, 7, "mid", rk, spikes=0.3, mesa=0.6)
    B.cylinder((-60, 160, 28), 5, 18, "mid", st, sides=12, top_radius=4.5, name="watchtower")
    B.arch_shape((-60, 155.2, 40), 1.0, 2.2, "mid", windows(6.0), name="towerwin")
    B.light("AREA", (0, 600, 300), CRIMSON, 2.0e6, size=100, look_at=(0, 200, 50), name="rim")
    return {"sky": {"moon": [moon[0], moon[1], 80], "horizon": 280, "clouds": 0.7, "stars": 0.7, "seed": 71},
            "haze": {"amount": 0.75},
            "anchors": hs((0, 10, 1), (0, 120, 6), (60, 600, 260), (-60, 160, 42)) + [lightpt((-60, 155, 41), 8, "window")]}


@scene("mountains/mountain-pass")
def mountains_pass():
    B.camera((0, -6, 3), (0, 120, 20), lens=22)
    moon = (480, 105)
    moon_lighting(moon, strength=3.5)
    rk = dark_rock(1.1)
    B.ridge(-2400, 2600, 2300, -80, 700, 260, 5, "far", B.mat_stone("far2", (0.03, 0.028, 0.035), haze=0.05), segments=240, rows=4)
    cuts = (16, 80)
    # Two towering cliff walls, the path between them.
    for side in (-1, 1):
        for k, (y0, y1) in enumerate(((-10, cuts[0]), (cuts[0], cuts[1]), (cuts[1], 300))):
            L = ("near", "mid", "far")[k]
            from mathutils import noise as N
            verts, faces = [], []
            ny, nz = 30, 30
            for i in range(ny + 1):
                y = y0 + (y1 - y0) * i / ny
                for j in range(nz + 1):
                    z = -10 + 160 * j / nz
                    x = side * (8 + y * 0.12 + 6 * N.fractal(Vector((y * 0.05, z * 0.05, side * 3)), 0.7, 2.0, 5) + z * 0.06)
                    verts.append((x, y, z))
            for i in range(ny):
                for j in range(nz):
                    a = i * (nz + 1) + j
                    faces.append((a, a + 1, a + nz + 2, a + nz + 1) if side > 0 else (a, a + nz + 1, a + nz + 2, a + 1))
            B.mesh_object("cliff", verts, faces, L, rk, smooth=True)
    ground_layers(0, 40, cuts, 300, B.mat_stone("scree", (0.02, 0.019, 0.021), bump=1.0, scale=1.0), rough=1.0, seed=22)
    for k, (x, y, s) in enumerate(((-4, 8, 1.5), (5, 14, 2), (-6, 40, 3), (7, 60, 4))):
        B.rock((x, y, 0), s, 750 + k, B.layer_by_depth(y, cuts), rk, squash=(1.2, 1, 0.8))
    B.light("AREA", (0, 260, 80), CRIMSON, 3.0e6, size=60, look_at=(0, 60, 20), name="rim")
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.01, noise_scale=0.02, z0=-2, z1=12, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 150, 8), (100, 300, 20), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 70], "horizon": 300, "clouds": 0.6, "stars": 0.6, "seed": 72},
            "haze": {"amount": 0.75},
            "anchors": hs((0, 6, 0.8), (0, 60, 4), (-14, 80, 40), (14, 40, 6))}


@scene("mountains/frozen-peak")
def mountains_frozen_peak():
    B.camera((0, -4, 1002), (0, 200, 990), lens=24)
    moon = (520, 130)
    moon_lighting(moon, strength=4.0)
    sn, rk = snow(), dark_rock(1.2)
    B.ridge(-3000, 3000, 3000, 300, 900, 300, 9, "far", B.mat_stone("far2", (0.05, 0.05, 0.06), haze=0.06), segments=240, rows=4)
    B.ridge(-2400, 2400, 1600, 400, 650, 260, 13, "far", B.mat_stone("far1s", (0.06, 0.06, 0.07), haze=0.03), segments=240, rows=4)
    # The summit: snow and black rock in the foreground, a cairn, a frozen banner.
    for k, (x, y, s, sq) in enumerate(((-10, 14, 8, (1.4, 1, 0.7)), (12, 18, 9, (1.3, 1, 0.8)), (0, 10, 5, (2.0, 1, 0.4)), (-20, 30, 10, (1.2, 1, 1.0)), (22, 34, 12, (1.0, 1, 1.2)))):
        B.rock((x, y, 990 - s * 0.3), s, 800 + k, "near" if y < 20 else "mid", sn if k % 2 == 0 else rk, squash=sq)
    for k in range(5):
        B.rock((-3 + (k % 2) * 0.4, 16, 993 + k * 0.6), 0.5 - k * 0.06, 830 + k, "mid", rk, squash=(1.2, 1, 0.6))
    B.cylinder((3, 16, 992), 0.06, 4, "mid", B.mat_stone("pole", (0.02, 0.015, 0.01)), sides=6, name="pole")
    B.banner((3.6, 16, 996), 1.2, 1.6, "mid", rotation_z=0.3)
    f = B.mat_fog(colour=(0.62, 0.6, 0.72), density=0.006, noise_scale=0.004, z0=500, z1=900, anisotropy=0.3, emission=0.0012, emit_colour=SLATE)
    B.fog_box((0, 1500, 700), (6000, 3000, 420), "fog", f)
    B.embers((0, 60, 990), (80, 60, 30), 50, 33, "mid", size=0.08, strength=4, colour=(0.9, 0.85, 0.95))
    return {"sky": {"moon": [moon[0], moon[1], 120], "horizon": 300, "clouds": 0.8, "stars": 0.9, "seed": 73},
            "haze": {"amount": 0.7},
            "anchors": hs((0, 10, 993), (-20, 30, 1000), (22, 34, 1002), (3, 16, 995))}


@scene("mountains/dwarven-hall")
def mountains_dwarven_hall():
    L_, W, H = 80.0, 22.0, 30.0
    B.camera((0, -2, 3.0), (0, 70, 10), lens=20)
    cuts = (14, 44)
    st = B.mat_stone("dwarfstone", (0.03, 0.028, 0.03), bump=0.8, scale=1.0)
    for k, (y0, y1) in enumerate(((-6, cuts[0]), (cuts[0], cuts[1]), (cuts[1], L_))):
        Lr = ("near", "mid", "far")[k]
        B.mesh_object("floor", [(-W, y0, 0), (W, y0, 0), (W, y1, 0), (-W, y1, 0)], [(0, 1, 2, 3)], Lr, st)
        B.box((0, (y0 + y1) / 2, H + 1), (2 * W + 4, y1 - y0, 2), Lr, st, name="ceiling")
        for side in (-1, 1):
            B.box((side * (W + 1), (y0 + y1) / 2, H / 2), (2, y1 - y0, H), Lr, st, name="wall")
    # Massive square pillars carved with bands, a river of fire in a channel, the great gate.
    for k in range(7):
        y = 6 + k * 11
        Lr = B.layer_by_depth(y, cuts)
        for side in (-1, 1):
            B.box((side * 8, y, H / 2), (3.2, 3.2, H), Lr, st, name="pillar")
            for b in (2.5, H - 3):
                B.box((side * 8, y, b), (3.8, 3.8, 0.8), Lr, st, name="band")
            B.brazier((side * 5.5, y, 1.2), Lr, strength=1200, size=0.9)
    for k, (y0, y1) in enumerate(((0, cuts[0]), (cuts[0], cuts[1]), (cuts[1], L_ - 2))):
        lava_strip(-0.8, 0.8, y0, y1, 0.03, ("near", "mid", "far")[k], scale=1.5, strength=9.0)
    B.box((0, L_ - 1, 9), (14, 2, 18), "far", st, name="gateframe")
    B.box((0, L_ - 2.05, 7), (9, 0.1, 14), "far", B.mat_emission(CRIMSON, 1.6, "gateglow"), name="gatelight")
    B.light("POINT", (0, L_ - 6, 6), CRIMSON, 8000, size=2, name="gatefire")
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    interior_fog(L_, 2 * W, H, density=0.012, top=6)
    B.embers((0, 40, 1), (12, 70, 6), 60, 41, "mid", size=0.05, strength=16)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 230, 120, 0.5]], "seed": 74},
            "haze": {"amount": 0.7},
            "anchors": hs((0, 4, 0.5), (-12, 30, 4), (12, 30, 4), (0, L_ - 2, 8))
            + [lightpt((side * 5.5, 6 + k * 11, 2.0), 10, "fire") for k in range(7) for side in (-1, 1)] + [lightpt((0, 20, 0.05), 30, "lava")]}


# ==================================================================== NECROPOLIS
def necro_stone():
    return B.mat_stone("tombstone", (0.034, 0.033, 0.036), bump=0.9, scale=3)


def mausoleum(location, scale, layer, rotation_z=0.0):
    st, rf = necro_stone(), roof_mat()
    x, y, z = location
    s = scale
    B.box((x, y, z + 2.5 * s), (6 * s, 5 * s, 5 * s), layer, st, rotation=(0, 0, rotation_z), name="mausoleum")
    B.prism_roof((x, y, z + 5 * s), 7 * s, 5.6 * s, 2.4 * s, layer, rf, rotation_z=rotation_z + math.pi / 2)
    for k in range(4):
        B.cylinder((x - 2.4 * s + k * 1.6 * s, y - 2.8 * s, z), 0.3 * s, 5 * s, layer, st, sides=10, name="portico")
    B.arch_shape((x, y - 2.52 * s, z), 1.8 * s, 3.2 * s, layer, B.mat_emission(CRIMSON, 0.8, "tombglow"), name="tombdoor")
    B.cone((x, y, z + 7.4 * s), 0.5 * s, 2.0 * s, layer, rf, sides=4, name="finial")


def graves(rows, cols, x0, y0, dx, dy, seed, layer_fn):
    rnd = random.Random(seed)
    st = necro_stone()
    for r in range(rows):
        for c in range(cols):
            if rnd.random() < 0.2:
                continue
            x = x0 + c * dx + (rnd.random() - 0.5) * dx * 0.4
            y = y0 + r * dy + (rnd.random() - 0.5) * dy * 0.3
            B.tombstone((x, y, 0), 0.9 + rnd.random() * 0.5, layer_fn(y), st, seed=seed * 100 + r * cols + c, cross=rnd.random() < 0.3)


@scene("necropolis/gates")
def necropolis_gates():
    B.camera((0, -8, 2.0), (0, 60, 5.5), lens=22)
    moon = (480, 120)
    vista(moon, far=(2300, 420), near_range=(900, 160), fog=None, ground_z=-5, moon_strength=3.5)
    rk = dark_rock(1.1)
    cuts = (6, 30)
    ground_layers(0, 80, cuts, 500, B.mat_stone("graveyardground", (0.013, 0.012, 0.013), bump=0.7), rough=0.4, seed=31)
    st = necro_stone()
    # Two great pillars with urns, the iron gate between them, walls either side.
    for side in (-1, 1):
        B.box((side * 3.6, 6, 3), (1.6, 1.6, 6), "mid", st, name="gatepillar")
        B.lathe((side * 3.6, 6, 6), [(0.0, 0.0), (0.5, 0.1), (0.7, 0.5), (0.45, 0.9), (0.55, 1.1), (0.0, 1.15)], "mid", st, name="urn")
        B.box((side * 14, 6, 1.6), (18, 0.8, 3.2), "mid", st, name="cemeterywall")
    B.iron_gate((0, 6, 0), 6.2, 4.6, "mid")
    lantern((-2.6, 5.4, 4.6), "mid", strength=90)
    graves(6, 9, -20, 14, 5, 5, 3, lambda y: B.layer_by_depth(y, cuts))
    mausoleum((6, 44, 0), 1.6, "far")
    trees_row([-24, 22, -12], 30, 0, (10, 16), 141, "far", rk, lean=0.3)
    B.dead_tree((-8, 3, 0), 9, 7, "near", rk, branches=4, depth=3, lean=0.4)
    B.light("AREA", (0, 80, 20), CRIMSON, 1.5e5, size=30, look_at=(0, 30, 3), name="rim")
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.04, noise_scale=0.06, z0=-0.5, z1=1.4, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 40, 1), (100, 90, 3), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 90], "horizon": 345, "clouds": 0.6, "stars": 0.7, "seed": 81},
            "haze": {"amount": 0.8},
            "anchors": hs((0, 6, 2.2), (-10, 24, 1.5), (6, 42, 3)) + [lightpt((-2.6, 5.4, 4.6), 8, "fire")]}


@scene("necropolis/graveyard")
def necropolis_graveyard():
    B.camera((0, -10, 2.0), (0, 50, 4), lens=22)
    moon = (300, 110)
    vista(moon, far=(2300, 380), near_range=None, fog=None, ground_z=-5, moon_strength=3.5)
    rk = dark_rock(1.1)
    cuts = (8, 26)
    ground_layers(0, 60, cuts, 400, B.mat_stone("graveyardground", (0.013, 0.012, 0.013), bump=0.7), rough=0.5, seed=32)
    graves(8, 10, -18, 7, 4, 4, 9, lambda y: B.layer_by_depth(y, cuts))
    trees_row([-16, 14, -26, 25, 4], 26, 0, (10, 18), 151, "mid", rk, lean=0.4)
    mausoleum((-4, 46, 0), 1.7, "far", rotation_z=0.2)
    for k in range(4):
        B.spire((14 + k * 7, 60, 0), 1.0, 6 + k * 2, "far", necro_stone(), roof_mat(), roof_h=4, pinnacles=False)
    B.candle((2, 6, 0.0), "near", height=0.3, strength=20)
    B.candle((-5, 10, 0.0), "mid", height=0.25, strength=16)
    B.embers((0, 20, 1), (30, 30, 4), 10, 7, "mid", size=0.08, strength=8, colour=(1.0, 0.35, 0.3))
    f = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.05, noise_scale=0.08, z0=-0.5, z1=1.3, anisotropy=0.3, emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 30, 1), (90, 80, 3), "fog", f)
    return {"sky": {"moon": [moon[0], moon[1], 85], "horizon": 350, "clouds": 0.6, "stars": 0.7, "seed": 82},
            "haze": {"amount": 0.8},
            "anchors": hs((0, 46, 6), (2, 14, 1.2), (-4, 44, 3)) + [lightpt((2, 6, 0.4), 6, "candle"), lightpt((-5, 10, 0.35), 5, "candle")]}


@scene("necropolis/mausoleum")
def necropolis_mausoleum():
    L_, W, H = 18.0, 8.0, 7.0
    B.camera((0, -1, 1.7), (0, 16, 2.0), lens=20)
    cuts = (4, 10)
    st = necro_stone()
    ys = B.gothic_hall(L_, W, H, 4, cuts, st, st, None, aisle=2.0, column_r=0.45, seed=91, floor_mat=dungeon_stone(1.2))
    # The tomb: a sarcophagus with a stone effigy, candles, niches with urns, a dim red window.
    B.box((0, 11, 0.55), (1.6, 3.2, 1.1), "mid", st, name="sarcophagus")
    B.box((0, 11, 1.15), (1.75, 3.35, 0.15), "mid", stone(1.6), name="lid")
    B.box((0, 11, 1.35), (0.55, 2.2, 0.28), "mid", stone(1.2), name="effigy")
    for k in range(6):
        B.candle(((-1.2 if k % 2 else 1.2), 9.5 + k * 0.6, 0.0), "mid", height=0.5 + 0.2 * (k % 3), strength=42)
    B.arch_shape((0, L_ - 0.1, 2.2), 1.6, 3.2, "far", B.mat_emission((1.0, 0.05, 0.03), 1.6, "tombwin"), name="tombwin")
    for side in (-1, 1):
        for k in range(3):
            B.lathe((side * (W / 2 + 1.9), 4 + k * 4, 1.4), [(0.0, 0.0), (0.18, 0.05), (0.25, 0.25), (0.15, 0.45), (0.0, 0.48)], B.layer_by_depth(4 + k * 4, cuts), stone(1.2), name="urn")
    for yy, zz in ((1.0, 3.0), (6.0, 3.0), (12.0, 3.2), (18.0, 3.2), (0.0, 9.0), (0.0, 15.0)):
        B.light("POINT", (0.0, yy, zz), EMBER, 350, size=1.0, name="dimfill")
    B.sun((0.2, -1, 0.8), SLATE, 0.02, name="fill")
    interior_fog(L_, W + 4, H, density=0.05, top=1.0)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 260, 60, 0.3]], "seed": 83},
            "haze": {"amount": 0.7},
            "anchors": hs((0, 2, 0.4), (-3, 8, 1.2), (0, 11, 1.6))
            + [lightpt(((-1.2 if k % 2 else 1.2), 9.5 + k * 0.6, 0.6), 5, "candle") for k in range(6)]}


# ================================================================ WIZARD'S TOWER
def round_room(radius, height, layer_split, wall, floor_mat, window_mat=None, windows_at=(), sides=28):
    """A round tower room around the origin: wall ring (inside), floor; the camera sits
    inside near the wall at -Y."""
    for k in range(sides):
        a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
        am = (a0 + a1) / 2
        y = math.sin(am) * radius
        L = layer_split(y)
        x0, y0, x1, y1 = math.cos(a0) * radius, math.sin(a0) * radius, math.cos(a1) * radius, math.sin(a1) * radius
        B.mesh_object("roundwall", [(x0, y0, 0), (x1, y1, 0), (x1, y1, height), (x0, y0, height)], [(0, 3, 2, 1)], L, wall)
    B.cylinder((0, 0, -0.1), radius + 0.5, 0.1, "far", floor_mat, sides=sides, name="roundfloor")
    for a in windows_at:
        x, y = math.cos(a) * (radius - 0.05), math.sin(a) * (radius - 0.05)
        B.arch_shape((x, y, height * 0.35), 1.0, 2.6, layer_split(y), window_mat, rotation_z=a - math.pi / 2, name="towerwin")


@scene("wizards-tower/stairwell")
def wizard_stairwell():
    B.camera((0, -5.2, 1.7), (0, 4, 6.5), lens=18)
    st = stone(1.3)
    win = B.mat_emission((1.0, 0.05, 0.03), 2.0, "towerwin")
    split = lambda y: "near" if y < -3.5 else ("mid" if y < 2.5 else "far")
    round_room(6.0, 22, split, st, dungeon_stone(1.2), win, windows_at=(math.pi * 0.5, math.pi * 0.15, math.pi * 0.85))
    B.spiral_stair((0, 0, 0), 5.5, 20, 2.2, "mid", st, start_angle=-math.pi / 2)
    for k, (a, z) in enumerate(((0.1, 2.5), (1.6, 6.5), (3.0, 10.5), (4.6, 14.5))):
        B.candle((math.cos(a) * 5.4, math.sin(a) * 5.4, z), "mid" if math.sin(a) < 2.5 / 5.4 else "far", height=0.4, strength=180)
    B.light("POINT", (0, -1.5, 5), EMBER, 600, size=0.6, name="stairlight")
    B.light("POINT", (0, 1.5, 12), EMBER, 600, size=0.6, name="stairlight")
    for a in (math.pi * 0.5, math.pi * 0.15, math.pi * 0.85):
        B.light("SPOT", (math.cos(a) * 9, math.sin(a) * 9, 10), CRIMSON, 4000, size=0.3, look_at=(0, 0, 2), name="winshaft", spot=14)
    for yy, zz in ((1.0, 3.0), (6.0, 3.0), (12.0, 3.2), (18.0, 3.2), (0.0, 9.0), (0.0, 15.0)):
        B.light("POINT", (0.0, yy, zz), EMBER, 350, size=1.0, name="dimfill")
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    f = B.mat_fog(colour=(0.6, 0.56, 0.7), density=0.03, noise_scale=0.2, z0=-1, z1=22, anisotropy=0.35, emission=0.0004, emit_colour=SLATE)
    B.fog_box((0, 0, 11), (12, 12, 22), "fog", f)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 120, 140, 0.3]], "seed": 91},
            "haze": {"amount": 0.6},
            "anchors": hs((0, -3, 0.5), (-4.5, 2, 4.5), (4.5, 2, 9), (0, 5, 14))
            + [lightpt((math.cos(a) * 5.4, math.sin(a) * 5.4, z + 0.45), 6, "candle") for (a, z) in ((0.1, 2.5), (1.6, 6.5), (3.0, 10.5), (4.6, 14.5))]}


@scene("wizards-tower/library")
def wizard_library():
    L_, W, H = 22.0, 12.0, 10.0
    B.camera((0, -1, 1.8), (0, 20, 3.5), lens=20)
    cuts = (5, 12)
    wood = B.mat_stone("libwood", (0.022, 0.014, 0.01), bump=0.3)
    st = stone(1.3)
    wooden_room(W, L_, H, lambda y: B.layer_by_depth(y, cuts), st, wood, B.mat_stone("libfloor", (0.02, 0.014, 0.01), bump=0.3, scale=5), cuts)
    for side in (-1, 1):
        for k in range(5):
            y = 2 + k * 4
            B.bookshelf((side * (W / 2 - 0.5), y, 0), 3.6, 7.5, 0.5, B.layer_by_depth(y, cuts), 10 + k * 2 + side, rotation_z=side * math.pi / 2)
    B.arch_shape((0, L_ - 0.05, 2.5), 3.5, 6.5, "far", B.mat_emission((1.0, 0.05, 0.03), 0.9, "libwin"), name="libwin")
    B.tracery(0, L_ - 0.05, 2.5, 3.5, 6.5, "far", columns=3, rows=5)
    B.table((0, 12, 0), 2.6, 1.4, "mid", height=0.85)
    B.candle((0.3, 11.6, 0.9), "mid", height=0.25, strength=120)
    B.box((-0.3, 12.2, 0.95), (0.5, 0.35, 0.08), "mid", B.mat_stone("tome", (0.05, 0.03, 0.02), bump=0.2), rotation=(0, 0, 0.3), name="tome")
    B.lathe((-0.6, 12.6, 0.91), [(0.0, 0.0), (0.12, 0.02), (0.2, 0.2), (0.16, 0.36), (0.0, 0.4)], "mid", B.mat_metal((0.3, 0.2, 0.1), 0.4, "brass"), name="globe")
    B.light("SPOT", (0, L_ + 8, 10), CRIMSON, 4.0e4, size=0.5, look_at=(0, 12, 0), name="winshaft", spot=12)
    for x in (-3.5, 3.5):
        for y in (6.0, 14.0):
            B.light("POINT", (x, y, 2.6), EMBER, 260, size=0.4, name="shelflight")
    for yy, zz in ((1.0, 3.0), (6.0, 3.0), (12.0, 3.2), (18.0, 3.2), (0.0, 9.0), (0.0, 15.0)):
        B.light("POINT", (0.0, yy, zz), EMBER, 350, size=1.0, name="dimfill")
    B.sun((0.2, -1, 0.8), SLATE, 0.02, name="fill")
    interior_fog(L_, W, H, density=0.02, top=H)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 240, 80, 0.3]], "seed": 92},
            "haze": {"amount": 0.6},
            "anchors": hs((0, 2, 0.3), (0, 12, 1.2), (-5, 16, 2.5), (0, L_, 6)) + [lightpt((0.3, 11.6, 1.2), 10, "candle")]}


@scene("wizards-tower/alchemy-lab")
def wizard_alchemy_lab():
    L_, W, H = 16.0, 12.0, 7.0
    B.camera((0.8, -1, 1.9), (-0.5, 14, 1.8), lens=20)
    cuts = (4, 9)
    st = stone(1.3)
    wood = B.mat_stone("labwood", (0.022, 0.014, 0.01), bump=0.3)
    wooden_room(W, L_, H, lambda y: B.layer_by_depth(y, cuts), st, wood, dungeon_stone(1.2), cuts)
    B.cauldron((0, 10, 0.3), 1.4, "mid", glow=6.0)
    glass = B.mat_crystal((1.0, 0.12, 0.08), 3.0)
    pale = B.mat_crystal((0.8, 0.8, 0.9), 0.5)
    rnd = random.Random(5)
    for side in (-1, 1):
        B.table((side * 4, 8, 0), 6, 1.2, "mid", height=0.9)
        for k in range(8):
            y = 5.6 + k * 0.65
            h = 0.25 + rnd.random() * 0.4
            B.lathe((side * 4 + (rnd.random() - 0.5) * 0.6, y, 0.96), [(0.0, 0.0), (0.12 * h / 0.4, 0.02), (0.16 * h / 0.4, h * 0.45), (0.05, h * 0.75), (0.05, h), (0.0, h)],
                    "mid", glass if rnd.random() < 0.5 else pale, name="flask")
        B.bookshelf((side * (W / 2 - 0.4), 13, 0), 3, 5, 0.4, "far", 40 + side, rotation_z=side * math.pi / 2)
    for k in range(5):
        x = -4 + k * 2
        B.tube([(x, 7, H), (x, 7, H - 1.2)], 0.02, "mid", wood, name="string")
        B.rock((x, 7, H - 1.4), 0.15, 900 + k, "mid", B.mat_stone("herbs", (0.02, 0.025, 0.015), bump=0.5), squash=(0.6, 0.6, 1.6))
    B.embers((0, 10, 1.5), (1.2, 1.2, 3), 30, 37, "mid", size=0.03, strength=18, colour=(1.0, 0.3, 0.2))
    B.sun((0.2, -1, 0.8), SLATE, 0.02, name="fill")
    interior_fog(L_, W, H, density=0.04, top=H)
    return {"sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 280, 60, 0.3]], "seed": 93},
            "haze": {"amount": 0.6},
            "anchors": hs((0, 2, 0.4), (-4, 12, 2), (0, 10, 1.4), (4, 12, 2)) + [lightpt((0, 10, 1.3), 24, "fire"), lightpt((-4, 8, 1.2), 30, "glint"), lightpt((4, 8, 1.2), 30, "glint")]}


@scene("wizards-tower/observatory")
def wizard_observatory():
    B.camera((0, -7.5, 1.7), (0, 6, 8.5), lens=18)
    moon = (520, 110)
    moon_lighting(moon, strength=2.0)
    st = stone(1.3)
    split = lambda y: "near" if y < -5.5 else ("mid" if y < 3 else "far")
    # The domed room: walls, a dome with a wide slit onto the stars and the moon.
    sides, R, Hw = 28, 9.0, 6.0
    for k in range(sides):
        a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
        y = math.sin((a0 + a1) / 2) * R
        x0, y0, x1, y1 = math.cos(a0) * R, math.sin(a0) * R, math.cos(a1) * R, math.sin(a1) * R
        B.mesh_object("roundwall", [(x0, y0, 0), (x1, y1, 0), (x1, y1, Hw), (x0, y0, Hw)], [(0, 3, 2, 1)], split(y), st)
        for j in range(8):
            e0, e1 = math.pi / 2 * j / 8, math.pi / 2 * (j + 1) / 8
            xm = math.cos((a0 + a1) / 2) * R * math.cos(e0)
            if abs(xm) < 3.2 and math.sin((a0 + a1) / 2) > 0:
                continue   # the slit
            pts = [(math.cos(a) * R * math.cos(e), math.sin(a) * R * math.cos(e), Hw + R * math.sin(e)) for a, e in ((a0, e0), (a1, e0), (a1, e1), (a0, e1))]
            B.mesh_object("dome", pts, [(0, 3, 2, 1)], split(y), roof_mat())
    B.cylinder((0, 0, -0.1), R + 0.5, 0.1, "far", dungeon_stone(1.2), sides=sides, name="floor")
    B.telescope((0, 1, 2.5), 9, "mid", aim=(0.0, 0.85))
    B.tube([(-2, 1, 0), (0, 1, 2.6), (2, 1, 0)], 0.15, "mid", B.mat_metal((0.3, 0.2, 0.1), 0.4, "brass"), name="tripod")
    for k in range(3):
        r = 1.1 - k * 0.25
        B.tube([(4 + math.cos(2 * math.pi * t / 24) * r, -2, 1.4 + math.sin(2 * math.pi * t / 24) * r * (0.4 + 0.3 * k)) for t in range(25)], 0.03, "mid",
               B.mat_metal((0.35, 0.22, 0.12), 0.3, "brass"), name="armillary")
    B.table((-4.5, -1, 0), 2.0, 1.2, "mid", height=0.85)
    B.candle((-4.3, -1.2, 0.9), "mid", height=0.25, strength=40)
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    return {"sky": {"moon": [moon[0], moon[1], 60], "horizon": 600, "clouds": 0.3, "stars": 1.6, "seed": 94},
            "haze": {"amount": 0.5},
            "anchors": hs((0, -5, 0.5), (-4.5, -1, 1.4), (4, -2, 1.4), (0, 4, 8)) + [lightpt((-4.3, -1.2, 1.2), 10, "candle")]}
