"""The painted scenes: one function per scene, building it with blib and returning its
metadata (sky painting parameters, haze, anchors). Order of work: the map, the Dungeon
and the Castle (prototype); the other locations follow after approval.

Palette (linear): near-black stone, slate/purple mists, crimson light.
"""
import math
import random

from mathutils import Vector

import blib as B

CRIMSON = (1.0, 0.09, 0.04)
EMBER = (1.0, 0.13, 0.04)
SLATE = (0.32, 0.36, 0.5)
MOONLIGHT = (1.0, 0.07, 0.045)
WINDOW = (1.0, 0.1, 0.035)

REGISTRY = {}


def scene(name):
    def wrap(fn):
        REGISTRY[name] = fn
        return fn
    return wrap


def stone(v=1.0):
    return B.mat_stone("stone", (0.03 * v, 0.029 * v, 0.034 * v), bump=0.5, scale=1.2)


def dark_rock(v=1.0):
    return B.mat_stone("rock", (0.014 * v, 0.013 * v, 0.017 * v), bump=1.0, scale=1.4)


def roof_mat():
    return B.mat_stone("roof", (0.014, 0.013, 0.017), rough=0.6, bump=0.2, scale=4.0)


def windows(strength=9.0):
    return B.mat_emission(WINDOW, strength, "window")


def moon_lighting(moon_xy, strength=4.0, fill=0.35, fill_dir=(-0.4, -1.0, 0.6)):
    """The red moon as a backlight (rim light on every silhouette) and a cool fill."""
    origin, d = B.screen_ray(*moon_xy)
    B.sun(d, MOONLIGHT, strength, angle=4.0, name="moon")
    B.sun(Vector(fill_dir), SLATE, fill, angle=10.0, name="fill")
    return d


def hotspot(sub, world, label=None):
    return {"kind": "hotspot", "sub": sub, "world": tuple(world)}


def lightpt(world, radius, kind):
    """kind: window (flicker), fire (flicker + embers), lava (embers), candle, glint."""
    return {"kind": kind, "world": tuple(world), "radius": radius}


# =============================================================== CASTLE: Courtyard
@scene("castle/courtyard")
def castle_courtyard():
    B.camera((0, -10, 30), (12, 300, 92), lens=28)
    moon = (282, 150)
    moon_lighting(moon, strength=4.0)
    st, rk, rf, win = stone(), dark_rock(), roof_mat(), windows()

    # Far: two ranges of jagged mountains under the moon.
    B.ridge(-1800, 2000, 2300, -80, 640, 250, 3, "far", B.mat_stone("far2", (0.03, 0.028, 0.035), haze=0.05), segments=240, rows=4)
    B.ridge(-1500, 1700, 1400, -80, 300, 200, 7, "far", B.mat_stone("far1", (0.02, 0.019, 0.024), haze=0.022), segments=220, rows=4)

    # Mid: the castle on its crag rising out of the valley of mist.
    cx, cy = 34, 300
    B.crag((cx, cy, -140), 95, 196, 11, "mid", rk, spikes=0.3, mesa=0.75)
    base = 58
    B.castle((cx + 4, cy + 26, base - 8), 4.6, 5, "mid", st, rf, win, towers=14, spread=1.25, with_crag=False)
    # The keep (Tower Top), the great hall, the throne tower, the chapel.
    B.spire((cx, cy, base), 7.5, 105, "mid", st, rf, roof_h=58, windows=win, seed=21)
    hall_x = cx - 40
    B.box((hall_x, cy - 4, base - 20), (36, 14, 88), "mid", st, name="hall")
    B.crag((hall_x, cy, -120), 40, 150, 12, "mid", rk, spikes=0.35, mesa=0.5)
    roof_v = [(-18, -7, 0), (18, -7, 0), (0, -7, 20), (-18, 7, 0), (18, 7, 0), (0, 7, 20)]
    ob = B.mesh_object("hallroof", roof_v, [(0, 1, 2), (3, 5, 4), (0, 3, 4, 1), (1, 4, 5, 2), (0, 2, 5, 3)], "mid", rf)
    ob.location = (hall_x, cy - 4, base + 24)
    for k in range(3):
        B.arch_shape((hall_x - 10 + k * 10, cy - 11.1, base + 10), 1.3, 7, "mid", win, name="hallwin")
    B.spire((cx + 34, cy - 6, base - 6), 5.5, 74, "mid", st, rf, roof_h=34, windows=None, seed=33)
    B.arch_shape((cx + 34, cy - 11.6, base + 40), 1.4, 4.0, "mid", win, name="thronewin")
    B.cathedral((cx + 70, cy + 10, base - 12), 1.8, 41, "mid", st, rf, B.mat_emission(WINDOW, 4.0, "dimwin"), rotation_z=0.2)
    B.crag((cx + 72, cy + 18, -130), 45, 125, 14, "mid", rk, spikes=0.35, mesa=0.6)
    B.crag((cx + 34, cy - 4, -130), 30, 128, 15, "mid", rk, spikes=0.3, mesa=0.6)
    # The gate, glowing, at the end of the causeway.
    B.wall_with_arch((cx - 6, cy - 40, base - 34), 30, 30, 6, 9, 16, "mid", st)
    B.arch_shape((cx - 6, cy - 37.5, base - 34), 4, 12, "mid", B.mat_emission(CRIMSON, 2.5, "gateglow"), name="gatelight")
    # Red rim light from behind the castle (the moon's glow on the mist).
    B.light("AREA", (cx - 40, cy + 180, 120), CRIMSON, 5.0e6, size=60, look_at=(cx, cy, 90), name="rim")
    B.light("AREA", (cx + 120, cy + 120, 60), CRIMSON, 2.0e6, size=40, look_at=(cx, cy, 60), name="rim2")
    B.light("POINT", (cx - 6, cy - 46, base - 26), EMBER, 1.2e4, size=4, name="gatefire")
    B.embers((cx, cy - 70, base - 30), (70, 40, 70), 60, 7, "mid", size=0.35, strength=12)

    # Near: the cliff edge the viewer stands on, rocks framing the corners, a dead tree.
    B.ground(-240, 8000, "far", B.mat_stone("valley", (0.01, 0.01, 0.012)))
    for k, (x, y, z, s, sq) in enumerate([(-36, 40, 2, 12, (1.4, 1, 0.9)), (-54, 62, 0, 19, (1.1, 1, 1.5)),
                                          (40, 36, 2, 10, (1.6, 1, 0.8)), (60, 72, -4, 22, (1.0, 1, 1.7)),
                                          (-14, 26, 7, 6, (2.4, 1, 0.45)), (18, 28, 7, 6, (1.8, 1, 0.45))]):
        B.rock((x, y, z), s, 100 + k, "near", rk, squash=sq)
    B.dead_tree((-40, 54, 18), 34, 4, "near", rk, branches=4, depth=3)
    B.light("AREA", (0, 140, 30), CRIMSON, 2.5e3, size=20, look_at=(0, 20, 14), name="nearrim")

    # Fog: the sea of mist in the valley, lit red by the moon and the gate.
    fog = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.012, noise_scale=0.007, z0=-150, z1=10, anisotropy=0.15,
                    emission=0.0010, emit_colour=SLATE)
    B.fog_box((0, 950, -80), (3000, 1800, 200), "fog", fog)
    # Mist banks between the ranges (haze between the layers).
    bank = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.004, noise_scale=0.005, z0=-80, z1=160, anisotropy=0.2,
                     emission=0.0012, emit_colour=SLATE)
    B.fog_box((0, 1100, 40), (3600, 500, 240), "fog", bank)

    return {
        "sky": {"moon": [moon[0], moon[1], 118], "horizon": 300, "clouds": 0.75, "stars": 0.5, "seed": 1},
        "haze": {"near": 60, "far": 2600, "amount": 0.75},
        "anchors": [
            hotspot(0, (cx - 6, cy - 40, base - 24)),
            hotspot(1, (hall_x, cy - 11, base + 16)),
            hotspot(2, (cx + 34, cy - 11, base + 43)),
            hotspot(3, (cx + 70, cy + 10, base + 14)),
            hotspot(4, (cx, cy, base + 118)),
            lightpt((cx - 6, cy - 40, base - 26), 30, "fire"),
            lightpt((hall_x, cy - 11, base + 13), 22, "window"),
            lightpt((cx + 34, cy - 11, base + 43), 10, "window"),
        ],
    }


# ------------------------------------------------------------------ interiors
def interior_fog(length, width, height, density=0.02, top=None, colour=(0.6, 0.56, 0.7), emission=0.0006):
    """Low floor mist and a faint haze in the whole room (catches the light shafts)."""
    fog = B.mat_fog(colour=colour, density=density, noise_scale=0.08, z0=-1, z1=top if top else height * 0.35,
                    anisotropy=0.35, emission=emission, emit_colour=SLATE)
    B.fog_box((0, length / 2, height / 2), (width + 20, length + 40, height + 2), "fog", fog)


def shafts(points, target_z=0.0, energy=6.0e4, colour=CRIMSON, spot=18):
    """Red moonlight falling through windows (spot lights outside, beams in the haze)."""
    for (x, y, z), (tx, ty) in points:
        B.light("SPOT", (x, y, z), colour, energy, size=0.5, look_at=(tx, ty, target_z), name="shaft", spot=spot)


def door(location, width, height, layer, rotation_z=0.0, glow=1.2):
    """A doorway: a dark arched opening, faintly red from a fire somewhere beyond, and a
    patch of that light spilling onto the floor in front of it."""
    B.arch_shape(location, width, height, layer, B.mat_emission((1.0, 0.06, 0.03), 0.12 * glow, "doorglow"), rotation_z=rotation_z, name="door")
    d = Vector((math.sin(rotation_z), -math.cos(rotation_z), 0)) * 0.6   # in front of the opening
    x, y, z = location
    B.light("POINT", (x + d.x, y + d.y, z + height * 0.35), CRIMSON, 60 * glow, size=0.6, name="doorspill")


# =============================================================== CASTLE: Great Hall
@scene("castle/great-hall")
def castle_great_hall():
    L, W, H = 64.0, 13.0, 24.0
    B.camera((0, -1.5, 3.4), (0, 60, 8.5), lens=22)
    cuts = (12, 36)
    st, rib = stone(1.4), stone(1.1)
    win = B.mat_emission((1.0, 0.05, 0.03), 1.4, "lancet")
    def end_window(length):
        B.arch_shape((0, length - 0.05, 8), 7, 13, "far", B.mat_emission((1.0, 0.05, 0.03), 2.0, "endwin"), name="endwin")
        B.tracery(0, length - 0.05, 8, 7, 13, "far", columns=4, rows=5)
    ys = B.gothic_hall(L, W, H, 8, cuts, st, rib, win, aisle=4.5, column_r=1.0, end_window=end_window, seed=3)
    # The long table with candles, a great hearth on the left, banners between windows.
    B.table((0, 30, 0), 34, 2.6, "mid")
    for k in range(9):
        y = 15 + k * 4
        B.candle((0.4 * (1 if k % 2 else -1), y, 1.06), B.layer_by_depth(y, cuts), height=0.5, strength=25)
    for i, y in enumerate(ys):
        lay = B.layer_by_depth(y, cuts)
        for side in (-1, 1):
            if i % 2 == 1:
                B.banner((side * (W / 2 - 1.3), y, 12), 1.8, 6.5, lay, rotation_z=0.0)
            B.torch((side * (W / 2 - 1.2), y + 2, 6.0), lay, strength=500)
    door((-W / 2 - 4.4, ys[2] + 3.5, 0), 3.0, 6.0, "mid", rotation_z=math.pi / 2)            # to the Chapel
    door((W / 2 + 4.4, ys[3] + 3.5, 0), 3.0, 6.0, "mid", rotation_z=-math.pi / 2)            # to the Tower
    door((0, L - 0.1, 0), 4.0, 7.0, "far", glow=2.0)                                        # to the Throne Room
    B.brazier((-3.5, L - 4, 1.1), "far", strength=2500)
    B.brazier((3.5, L - 4, 1.1), "far", strength=2500)
    shafts([((-W / 2 - 14, y, 22), (2, y + 2)) for y in ys[1:6:2]] + [((W / 2 + 14, y, 22), (-2, y + 2)) for y in ys[2:7:2]], energy=4.0e5, spot=12)
    B.light("AREA", (0, L - 2, 18), CRIMSON, 3.0e3, size=8, look_at=(0, 20, 2), name="endglow")
    B.sun((0.2, -1, 0.8), SLATE, 0.05, name="fill")
    interior_fog(L, W + 10, H, density=0.03)
    B.embers((0, L - 6, 2), (8, 4, 6), 30, 3, "far", size=0.04, strength=15)
    return {
        "sky": {"moon": [0, -999, 0], "horizon": 300, "clouds": 0, "stars": 0, "glows": [[480, 230, 160, 0.4]], "seed": 2},
        "haze": {"amount": 0.7},
        "anchors": [
            hotspot(0, (0, 4, 0.5)),
            hotspot(1, (0, 24, 2.0)),
            hotspot(2, (0, L, 4.0)),
            hotspot(3, (-W / 2 - 4.4, ys[2] + 3.5, 3.5)),
            hotspot(4, (W / 2 + 4.4, ys[3] + 3.5, 3.5)),
        ] + [lightpt((side * (W / 2 - 1.2), y + 2, 6.4), 8, "fire") for y in ys for side in (-1, 1)]
          + [lightpt((0, L - 4, 2.0), 14, "fire")],
    }


# =============================================================== CASTLE: Throne Room
@scene("castle/throne-room")
def castle_throne_room():
    L, W, H = 40.0, 16.0, 30.0
    B.camera((0, -2, 3.0), (0, 38, 10.0), lens=22)
    cuts = (9, 24)
    st, rib = stone(1.3), stone(1.1)
    win = B.mat_emission((1.0, 0.05, 0.03), 1.4, "lancet")
    def rose(length):
        y = length - 0.1
        B.arch_shape((0, y, 13), 13, 13, "far", B.mat_emission((1.0, 0.05, 0.03), 3.0, "rose"), pointed=False, name="rose")
        tr = stone(0.6)
        for k in range(12):
            a = 2 * math.pi * k / 12
            B.tube([(0, y - 0.2, 19.5), (math.cos(a) * 6.4, y - 0.2, 19.5 + math.sin(a) * 6.4)], 0.22, "far", tr, name="tracery")
        B.tube([(math.cos(2 * math.pi * k / 48) * 6.5, y - 0.2, 19.5 + math.sin(2 * math.pi * k / 48) * 6.5) for k in range(49)], 0.3, "far", tr, name="rosering")
        B.tube([(math.cos(2 * math.pi * k / 32) * 2.2, y - 0.2, 19.5 + math.sin(2 * math.pi * k / 32) * 2.2) for k in range(33)], 0.25, "far", tr, name="rosering")
    ys = B.gothic_hall(L, W, H, 6, cuts, st, rib, win, aisle=4.0, column_r=1.3, end_window=rose, seed=5, window_every=2)
    # The dais and the throne, braziers either side, a runner of red cloth.
    B.steps((0, 26, 0), 10, 5, 0.35, 0.9, "far", stone(1.2))
    B.throne((0, 31.5, 1.75), "far", scale=1.25)
    for side in (-1, 1):
        B.brazier((side * 4.2, 27.5, 2.0), "far", strength=3500)
        B.banner((side * 6.5, L - 1.0, 17), 2.4, 10, "far")
    B.mesh_object("runner", [(-1.3, 1, 0.02), (1.3, 1, 0.02), (1.3, 26, 0.02), (-1.3, 26, 0.02)], [(0, 1, 2, 3)], "mid", B.mat_cloth((0.1, 0.006, 0.008)))
    door((-W / 2 - 3.9, ys[1] + 3.3, 0), 3.0, 6.0, "mid", rotation_z=math.pi / 2)
    door((W / 2 + 3.9, ys[1] + 3.3, 0), 3.0, 6.0, "mid", rotation_z=-math.pi / 2)
    B.light("SPOT", (0, L + 18, 22), CRIMSON, 1.2e6, size=2, look_at=(0, 24, 0), name="roseshaft", spot=22)
    B.light("AREA", (0, 34, 9), CRIMSON, 2.0e3, size=6, look_at=(0, 31, 3), name="throneback")
    B.sun((0.2, -1, 0.8), SLATE, 0.05, name="fill")
    interior_fog(L, W + 8, H, density=0.03)
    B.embers((0, 27.5, 3), (11, 3, 6), 40, 9, "far", size=0.04, strength=15)
    return {
        "sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 150, 160, 0.4]], "seed": 3},
        "haze": {"amount": 0.7},
        "anchors": [
            hotspot(0, (0, 5, 0.3)),
            hotspot(1, (-W / 2 - 3.9, ys[1] + 3.3, 3.5)),
            hotspot(2, (0, 31.5, 4.0)),
            hotspot(3, (W / 2 + 3.9, ys[1] + 3.3, 3.5)),
            hotspot(4, (0, L, 19.5)),
            lightpt((-4.2, 27.5, 3.0), 14, "fire"), lightpt((4.2, 27.5, 3.0), 14, "fire"),
        ],
    }


# =============================================================== CASTLE: Chapel
@scene("castle/chapel")
def castle_chapel():
    L, W, H = 46.0, 11.0, 26.0
    B.camera((1.2, -1.5, 2.6), (0, 44, 9.0), lens=22)
    cuts = (10, 28)
    st, rib = stone(1.3), stone(1.1)
    win = B.mat_emission((1.0, 0.05, 0.03), 1.4, "lancet")
    def end_window(length):
        for dx, hh in ((-3.2, 12), (0, 16), (3.2, 12)):
            B.arch_shape((dx, length - 0.05, 7), 2.2, hh, "far", B.mat_emission((1.0, 0.05, 0.03), 2.4, "endwin"), name="endwin")
            B.tracery(dx, length - 0.05, 7, 2.2, hh, "far", columns=2, rows=5)
    ys = B.gothic_hall(L, W, H, 7, cuts, st, rib, win, aisle=3.5, column_r=0.85, end_window=end_window, seed=7)
    B.pew_rows(-3.0, 6, 9, 2.6, 4.2, lambda y: B.layer_by_depth(y, cuts))
    B.pew_rows(3.0, 6, 9, 2.6, 4.2, lambda y: B.layer_by_depth(y, cuts))
    # The altar with candles, a tall cross.
    B.steps((0, 36, 0), 8, 3, 0.3, 0.8, "far", stone(1.2))
    B.box((0, 39.5, 1.5), (4.0, 1.4, 1.2), "far", stone(1.6), name="altar")
    B.box((0, 42, 5.5), (0.35, 0.35, 7.0), "far", stone(0.7), name="cross")
    B.box((0, 42, 7.2), (3.0, 0.35, 0.35), "far", stone(0.7), name="crossbar")
    for k in range(7):
        B.candle((-1.6 + k * 0.53, 39.3, 2.1), "far", height=0.35 + 0.25 * (k % 3), strength=30)
    for side in (-1, 1):
        for k in range(5):
            B.candle((side * (3.0 + k * 0.4), 38.0, 0.9), "far", height=0.8 + 0.4 * (k % 2), strength=20)
    door((-W / 2 - 3.4, ys[1] + 3.3, 0), 2.6, 5.2, "mid", rotation_z=math.pi / 2)
    door((W / 2 + 3.4, ys[2] + 3.3, 0), 2.6, 5.2, "mid", rotation_z=-math.pi / 2)
    shafts([((-W / 2 - 12, y, 24), (2, y + 1)) for y in ys[1:6:2]] + [((0, L + 12, 22), (0, 30))], energy=4.0e5, spot=11)
    B.sun((0.2, -1, 0.8), SLATE, 0.05, name="fill")
    interior_fog(L, W + 8, H, density=0.035)
    return {
        "sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 200, 140, 0.4]], "seed": 4},
        "haze": {"amount": 0.7},
        "anchors": [
            hotspot(0, (0, 3, 0.2)),
            hotspot(1, (-W / 2 - 3.4, ys[1] + 3.3, 3.0)),
            hotspot(2, (W / 2 + 3.4, ys[2] + 3.3, 3.0)),
            hotspot(3, (0, 39.5, 3.2)),
            hotspot(4, (0, L, 16.0)),
            lightpt((0, 39.3, 2.6), 22, "candle"),
            lightpt((-3.8, 38.0, 1.8), 10, "candle"), lightpt((3.8, 38.0, 1.8), 10, "candle"),
        ],
    }


# =============================================================== CASTLE: Tower Top
@scene("castle/tower-top")
def castle_tower_top():
    B.camera((0, -2, 152), (0, 200, 140), lens=26)
    moon = (520, 150)
    moon_lighting(moon, strength=4.0)
    st, rk, rf, win = stone(1.2), dark_rock(), roof_mat(), windows()
    B.ridge(-2400, 2600, 2600, -80, 470, 300, 13, "far", B.mat_stone("far2", (0.03, 0.028, 0.035), haze=0.05), segments=260, rows=4)
    B.ridge(-2000, 2200, 1500, -80, 240, 200, 17, "far", B.mat_stone("far1", (0.02, 0.019, 0.024), haze=0.022), segments=220, rows=4)
    # The other towers of the castle, backlit by the moon, rising out of the mist.
    B.castle((40, 230, 40), 5.0, 31, "mid", st, rf, win, towers=14, spread=2.0, with_crag=False)
    B.spire((34, 190, 40), 7, 120, "mid", st, rf, roof_h=62, windows=win, seed=5)            # the keep, against the moon
    B.spire((-60, 150, 30), 4.5, 105, "mid", st, rf, roof_h=40, windows=win, seed=7)
    B.cathedral((150, 260, 40), 2.6, 9, "mid", st, rf, B.mat_emission(WINDOW, 3.0, "dimwin"), rotation_z=-0.3)
    B.box((-30, 210, 70), (54, 16, 30), "mid", st, name="hall")
    for k in range(4):
        B.arch_shape((-46 + k * 11, 201.9, 70), 1.4, 8, "mid", win, name="hallwin")
    B.light("AREA", (40, 420, 200), CRIMSON, 5.0e6, size=80, look_at=(30, 200, 120), name="rim")
    # Near: weathered merlons of our tower's parapet along the lower edge.
    stone_near = B.mat_stone("parapet", (0.03, 0.028, 0.033), bump=1.2, scale=2.5)
    for k in range(7):
        x = -9.0 + k * 3.0
        B.rock((x, 8, 147.6), 1.25, 300 + k, "near", stone_near, squash=(0.8, 0.5, 1.15), detail=1.6)
    B.box((0, 8, 145.2), (28, 1.4, 2.6), "near", stone_near, name="parapet")
    B.light("AREA", (0, 30, 156), CRIMSON, 1.2e3, size=10, look_at=(0, 8, 148), name="nearrim")
    B.embers((0, 60, 140), (60, 40, 30), 18, 13, "mid", size=0.08, strength=10)
    fog = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.008, noise_scale=0.009, z0=-100, z1=35, anisotropy=0.2,
                    emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 1200, -40), (5000, 2300, 200), "fog", fog)
    return {
        "sky": {"moon": [moon[0], moon[1], 125], "horizon": 290, "clouds": 0.8, "stars": 0.6, "seed": 9},
        "haze": {"amount": 0.75},
        "anchors": [
            hotspot(0, (40, 230, 60)),
            hotspot(1, (-30, 201, 76)),
            hotspot(2, (34, 190, 150)),
            hotspot(3, (150, 260, 70)),
            hotspot(4, (0, 8, 149.5)),
            lightpt((-30, 201, 74), 18, "window"),
        ],
    }


# ====================================================================== DUNGEON
def dungeon_stone(v=1.0):
    return B.mat_stone("dstone", (0.026 * v, 0.024 * v, 0.027 * v), bump=1.1, scale=1.6)


def lava_strip(x0, x1, y0, y1, z, layer, scale=1.2, strength=10.0):
    return B.mesh_object("lava", [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], [(0, 1, 2, 3)], layer, B.mat_lava(scale, strength))


@scene("dungeon/corridors")
def dungeon_corridors():
    L, W, H = 58.0, 7.0, 11.0
    B.camera((0.6, -1.0, 2.3), (0, 50, 3.4), lens=20)
    cuts = (10, 30)
    st = dungeon_stone(1.3)
    ys = B.gothic_hall(L, W, H, 8, cuts, st, dungeon_stone(1.1), None, aisle=3.2, column_r=0.75, seed=11,
                       floor_mat=dungeon_stone(1.2))
    # A crack of lava down the middle of the floor.
    for k, (y0, y1) in enumerate(((0, cuts[0]), (cuts[0], cuts[1]), (cuts[1], L))):
        lava_strip(-0.35, 0.35, y0, y1, 0.02, ("near", "mid", "far")[k], scale=2.5, strength=8.0)
    B.light("AREA", (0, 18, 0.3), EMBER, 600, size=1.5, look_at=(0, 18, 4), name="lavaglow")
    B.light("AREA", (0, 40, 0.3), EMBER, 600, size=1.5, look_at=(0, 40, 4), name="lavaglow")
    for i, y in enumerate(ys):
        if i % 2 == 0:
            for side in (-1, 1):
                B.torch((side * (W / 2 - 0.7), y, 3.4), B.layer_by_depth(y, cuts), strength=260, wall_dir=(-side * 0.3, 0))
    # Doors: the Entrance Hall (left, near), the Cells (right), the Treasure Room (left, far), the Lair (the end).
    door((-W / 2 - 3.15, ys[1] + 3.2, 0), 2.4, 4.4, "mid", rotation_z=math.pi / 2, glow=0.5)
    door((W / 2 + 3.15, ys[3] + 3.2, 0), 2.4, 4.4, "mid", rotation_z=-math.pi / 2, glow=0.6)
    door((-W / 2 - 3.15, ys[5] + 3.2, 0), 2.4, 4.4, "far", rotation_z=math.pi / 2, glow=0.8)
    door((0, L - 0.1, 0), 4.2, 7.5, "far", glow=3.0)
    B.light("POINT", (0, L - 3, 2), CRIMSON, 3000, size=1.0, name="lairglow")
    B.chain((1.5, 6, H * 0.8), 4.5, "near", sway=0.2)
    B.chain((-1.8, 14, H * 0.8), 3.0, "mid")
    B.rock((-2.6, 4, 0), 0.6, 501, "near", dungeon_stone(), squash=(1.4, 1, 0.6))
    B.rock((2.8, 7, 0), 0.45, 502, "near", dungeon_stone(), squash=(1.6, 1, 0.5))
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    interior_fog(L, W + 8, H, density=0.04, top=2.5)
    B.embers((0, 30, 0.5), (1.5, 50, 2.5), 50, 21, "mid", size=0.03, strength=18)
    return {
        "sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 260, 120, 0.5]], "seed": 5},
        "haze": {"amount": 0.7},
        "anchors": [
            hotspot(0, (0, 5, 0.2)),
            hotspot(1, (-W / 2 - 3.15, ys[1] + 3.2, 2.2)),
            hotspot(2, (W / 2 + 3.15, ys[3] + 3.2, 2.2)),
            hotspot(3, (-W / 2 - 3.15, ys[5] + 3.2, 2.2)),
            hotspot(4, (0, L, 3.0)),
            lightpt((0, 20, 0.1), 40, "lava"),
        ] + [lightpt((side * (W / 2 - 0.7), y, 3.7), 9, "fire") for i, y in enumerate(ys) if i % 2 == 0 for side in (-1, 1)],
    }


@scene("dungeon/entrance-hall")
def dungeon_entrance_hall():
    B.camera((2, -4, 3.0), (0, 60, 9.5), lens=22)
    moon = (470, 205)
    moon_lighting(moon, strength=5.0, fill=0.08)
    st, rk = dungeon_stone(1.3), dark_rock()
    # The great gate: a wall with a vast pointed opening onto the night.
    B.wall_with_arch((0, 40, 0), 90, 60, 5, 17, 31, "mid", st)
    B.steps((0, 26, 0), 16, 10, 0.45, 1.4, "mid", st)
    B.box((0, 36, 4.5 / 2), (24, 8, 4.5), "mid", st, name="landing")
    B.box((0, 20, 38), (90, 44, 2), "mid", st, name="ceiling")
    for side in (-1, 1):
        B.box((side * 22, 18, 20), (2, 44, 40), "mid", st, name="sidewall")
        B.cylinder((side * 11.5, 28, 0), 1.6, 34, "mid", st, sides=12, name="pillar")
        B.cylinder((side * 8.0, 8, 0), 1.4, 34, "near", st, sides=12, name="pillar")
        B.brazier((side * 6.5, 22, 1.2), "mid", strength=1800, size=1.1)
        B.chain((side * 4.5, 6, 30), 22, "near", sway=0.4 * side)
    door((-21, 14, 0), 3.0, 6.0, "mid", rotation_z=math.pi / 2, glow=0.6)     # back to the corridors
    door((21, 14, 0), 3.0, 6.0, "mid", rotation_z=-math.pi / 2, glow=0.6)     # down to the cells
    # Outside: a ravine of jagged mountains under the moon.
    B.ridge(-1800, 2000, 2200, -60, 520, 250, 3, "far", B.mat_stone("far2", (0.03, 0.028, 0.035), haze=0.05), segments=240, rows=4)
    B.ridge(-1400, 1600, 1100, -60, 260, 200, 9, "far", B.mat_stone("far1", (0.02, 0.019, 0.024), haze=0.02), segments=220, rows=4)
    B.ridge(-300, 300, 260, -40, 70, 60, 4, "far", rk, segments=120, rows=4)
    B.ground(-80, 6000, "far", B.mat_stone("valley", (0.01, 0.01, 0.012)))
    fog = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.01, noise_scale=0.008, z0=-80, z1=20, anisotropy=0.3,
                    emission=0.0008, emit_colour=SLATE)
    B.fog_box((0, 1000, -40), (3000, 1800, 120), "fog", fog)
    inner = B.mat_fog(colour=(0.6, 0.56, 0.7), density=0.03, noise_scale=0.1, z0=-1, z1=1.4, anisotropy=0.3, emission=0.0003, emit_colour=SLATE)
    B.fog_box((0, 20, 2), (40, 44, 4), "fog", inner)
    B.sun((0.0, 1.0, 0.25), CRIMSON, 1.5, name="moonspill")
    B.embers((0, 22, 2), (14, 4, 6), 30, 4, "mid", size=0.05, strength=14)
    return {
        "sky": {"moon": [moon[0], moon[1], 92], "horizon": 330, "clouds": 0.6, "stars": 0.5, "seed": 6},
        "haze": {"amount": 0.7},
        "anchors": [
            hotspot(0, (-21, 14, 3.0)),
            hotspot(1, (0, 40, 9.0)),
            hotspot(2, (21, 14, 3.0)),
            hotspot(3, (-6.5, 22, 3.2)),
            hotspot(4, (6.5, 22, 3.2)),
            lightpt((-6.5, 22, 2.0), 16, "fire"), lightpt((6.5, 22, 2.0), 16, "fire"),
        ],
    }


@scene("dungeon/prison-cells")
def dungeon_prison_cells():
    L, W, H = 40.0, 4.4, 6.5
    B.camera((-0.6, -1.0, 1.9), (0.4, 40, 2.4), lens=20)
    cuts = (7, 20)
    st = dungeon_stone(1.25)
    ys = B.gothic_hall(L, W, H, 8, cuts, st, dungeon_stone(1.1), None, aisle=3.6, column_r=0.45, seed=13,
                       floor_mat=dungeon_stone(1.1))
    bay = L / 8
    for i, y in enumerate(ys[:-1]):
        lay = B.layer_by_depth(y + bay / 2, cuts)
        # A row of bars across the arcade opening into each cell (right side).
        iron = B.mat_metal((0.09, 0.08, 0.08), 0.5, "iron")
        for k in range(7):
            yy = y + 0.6 + k * (bay - 1.2) / 6
            B.cylinder((W / 2, yy, 0), 0.05, 3.6, lay, iron, sides=6, name="bar")
        B.box((W / 2, y + bay / 2, 3.6), (0.1, bay - 1.0, 0.12), lay, iron, name="crossbar")
        B.chain((W / 2 + 2.2, y + bay / 2, H * 0.75), 2.8, lay, sway=0.1)
        if i % 2 == 1:
            B.torch((-W / 2 - 3.3, y + bay / 2, 2.6), lay, strength=500, wall_dir=(0.3, 0))
    B.brazier((-1.2, 15, 0.9), "mid", strength=1600, size=0.8)
    door((0, L - 0.1, 0), 2.2, 3.8, "far", glow=1.5)
    for y in ys[:-1:2]:
        B.light("POINT", (W / 2 + 3.0, y + bay / 2, 1.2), CRIMSON, 700, size=0.5, name="cellglow")
    for k, (x, y) in enumerate([(W / 2 + 2.8, 8.6), (W / 2 + 2.6, 13.4)]):
        B.rock((x, y, 0.2), 0.25, 600 + k, B.layer_by_depth(y, cuts), B.mat_stone("bone", (0.25, 0.22, 0.2), bump=0.2), squash=(1, 1.2, 0.9))
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    interior_fog(L, W + 8, H, density=0.05, top=1.6)
    return {
        "sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[500, 280, 80, 0.4]], "seed": 7},
        "haze": {"amount": 0.7},
        "anchors": [
            hotspot(0, (0, 3, 0.1)),
            hotspot(1, (0, L, 2.0)),
            hotspot(2, (W / 2, ys[1] + 2, 1.8)),
            hotspot(3, (-W / 2 - 3.3, ys[3] + 2.5, 1.8)),
            hotspot(4, (W / 2, ys[5] + 2, 1.8)),
            lightpt((-1.2, 15, 1.3), 18, "fire"),
        ] + [lightpt((-W / 2 - 3.3, y + bay / 2, 2.9), 8, "fire") for i, y in enumerate(ys[:-1]) if i % 2 == 1],
    }


@scene("dungeon/treasure-room")
def dungeon_treasure_room():
    B.camera((0, -2, 2.6), (0, 40, 2.8), lens=22)
    cuts = (8, 24)
    rk = dungeon_stone(1.2)
    B.cave_tunnel(48, 9.5, cuts, rk, 17, wobble=0.3, flatten=0.45, ceiling=0.9)
    B.ground(0.0, 200, "far", dungeon_stone(1.1), location=(0, 60))
    # Hoards of gold, chests, a shaft of red light through a crack in the vault.
    B.gold_pile((0, 30, 0), 6.0, 3.2, "far", 3, coins=70)
    B.gold_pile((-6.5, 18, 0), 3.0, 1.4, "mid", 5, coins=40)
    B.gold_pile((6.0, 14, 0), 2.6, 1.1, "mid", 7, coins=40)
    B.gold_pile((2.2, 6.5, 0), 1.2, 0.5, "near", 9, coins=20)
    B.chest((-3.5, 10, 0), "mid", size=1.1, rotation_z=0.4)
    B.chest((4.2, 24, 0), "far", size=1.2, rotation_z=-0.5)
    B.chest((-5.0, 4.0, 0), "near", size=0.9, rotation_z=0.9)
    for k, (x, y, s) in enumerate([(-9, 26, 2.5), (9, 30, 2.8), (-7.5, 7, 1.4), (8, 5, 1.6)]):
        B.rock((x, y, 0), s, 700 + k, B.layer_by_depth(y, cuts), rk, squash=(1.3, 1, 0.8))
    B.light("SPOT", (0, 30, 26), CRIMSON, 2.5e5, size=0.8, look_at=(0, 30, 0), name="shaft", spot=12)
    B.light("POINT", (0, 24, 5), (1.0, 0.3, 0.1), 4000, size=1.0, name="goldglow")
    B.light("POINT", (-4, 14, 3), (1.0, 0.3, 0.1), 1200, size=0.6, name="goldglow")
    B.light("POINT", (5, 10, 3), (1.0, 0.3, 0.1), 900, size=0.6, name="goldglow")
    B.light("AREA", (0, 44, 8), CRIMSON, 6000, size=10, look_at=(0, 0, 4), name="caverim")
    door((0, 47.5, 0), 3.0, 5.0, "far", glow=1.0)
    B.light("POINT", (-6, 4, 2.5), (1.0, 0.22, 0.06), 120, size=0.3, name="torchlike")
    B.torch((-7.6, 12, 2.6), "mid", strength=300)
    B.torch((7.8, 16, 2.6), "mid", strength=300)
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    fog = B.mat_fog(colour=(0.5, 0.45, 0.55), density=0.012, noise_scale=0.12, z0=-1, z1=14, anisotropy=0.5, emission=0.0003, emit_colour=SLATE)
    B.fog_box((0, 24, 6), (30, 56, 16), "fog", fog)
    B.embers((0, 30, 2), (6, 6, 14), 40, 31, "far", size=0.035, strength=10, colour=(1.0, 0.45, 0.15))
    return {
        "sky": {"moon": [0, -999, 0], "clouds": 0, "stars": 0, "glows": [[480, 240, 120, 0.4]], "seed": 8},
        "haze": {"amount": 0.7},
        "anchors": [
            hotspot(0, (-5.0, 4.0, 1.2)),
            hotspot(1, (-7.6, 12, 2.0)),
            hotspot(2, (7.8, 16, 2.0)),
            hotspot(3, (0, 30, 3.4)),
            hotspot(4, (0, 47.5, 2.4)),
            lightpt((0, 30, 2.5), 50, "glint"), lightpt((-6.5, 18, 1.0), 26, "glint"), lightpt((6.0, 14, 0.8), 24, "glint"),
            lightpt((-7.6, 12, 2.9), 8, "fire"), lightpt((7.8, 16, 2.9), 8, "fire"),
        ],
    }


@scene("dungeon/final-boss-lair")
def dungeon_final_boss_lair():
    B.camera((0, -6, 7), (0, 120, 26), lens=22)
    cuts = (22, 80)
    rk = dungeon_stone(1.1)
    B.cave_tunnel(160, 36, cuts, rk, 23, wobble=0.35, flatten=0.25, ceiling=1.25, start=-20)
    # The lava lake, the rock bridge, the island and the lord rising from the fire.
    for k, (y0, y1) in enumerate(((-20, cuts[0]), (cuts[0], cuts[1]), (cuts[1], 200))):
        B.mesh_object("lava", [(-80, y0, 0.25), (80, y0, 0.25), (80, y1, 0.25), (-80, y1, 0.25)], [(0, 1, 2, 3)],
                      ("near", "mid", "far")[k], B.mat_lava(0.1, 7.0, crack=0.04, base=(0.006, 0.005, 0.005)))
    for k in range(16):
        y = 2 + k * 5.6
        x = math.sin(k * 0.7) * 1.6
        B.rock((x, y, 0.5), 2.4 + 0.4 * math.sin(k * 1.7), 800 + k, B.layer_by_depth(y, cuts), rk, squash=(1.4, 1.2, 0.35))
    B.rock((0, 102, -2), 12, 830, "far", rk, squash=(1.6, 1.1, 0.4))
    B.horned_lord((0, 128, -10), 6.5, "far", eye_strength=80)
    for k, (x, y, s) in enumerate([(-14, 8, 4), (16, 12, 5), (-24, 40, 7), (26, 52, 8), (-12, 3, 2.5)]):
        B.rock((x, y, -1), s, 850 + k, B.layer_by_depth(y, cuts), rk, squash=(1.1, 1, 1.4))
    # Behind the lord: a glowing chasm of fire he stands against (a backlit silhouette).
    B.light("AREA", (0, 150, 70), CRIMSON, 6.0e5, size=30, look_at=(0, 128, 60), name="lordrim")
    B.light("AREA", (0, 40, 0.5), EMBER, 1600, size=30, look_at=(0, 40, 30), name="lavaup")
    B.light("AREA", (0, 100, 0.5), EMBER, 900, size=30, look_at=(0, 100, 30), name="lavaup")
    B.sun((0.2, -1, 0.8), SLATE, 0.03, name="fill")
    fog = B.mat_fog(colour=(0.45, 0.4, 0.5), density=0.0025, noise_scale=0.03, z0=-2, z1=30, anisotropy=0.2, emission=0.0006, emit_colour=SLATE)
    B.fog_box((0, 90, 30), (70, 200, 60), "fog", fog)
    B.embers((0, 60, 2), (50, 100, 40), 120, 41, "mid", size=0.1, strength=14)
    return {
        "sky": {"moon": [0, -999, 0], "clouds": 0.4, "stars": 0, "glows": [[480, 230, 70, 1.1], [480, 260, 220, 0.35]], "seed": 9},
        "haze": {"amount": 0.65},
        "anchors": [
            hotspot(0, (0, 4, 0.5)),
            hotspot(1, (-24, 40, 8)),
            hotspot(2, (26, 52, 10)),
            hotspot(3, (0, 50, 1.0)),
            hotspot(4, (0, 128, 60)),
            lightpt((0, 40, -0.5), 120, "lava"),
            lightpt((-3.6, 116, 76), 6, "eyes"), lightpt((3.6, 116, 76), 6, "eyes"),
        ],
    }


# ========================================================================== MAP
# Landmark screen positions (virtual 960 x 560), in Location order.
MAP_LANDMARKS = [
    ("Dungeon", 560, 300), ("Castle", 452, 196), ("Cathedral", 652, 206), ("Haunted Forest", 250, 268),
    ("Caverns", 772, 350), ("Village", 404, 352), ("Swamp", 168, 346), ("Mountains", 812, 166),
    ("Necropolis", 880, 276), ("Wizard's Tower", 128, 206),
]
# How high each landmark's visible mass rises above its ground point (metres, before the
# landmark's own scale): the click target is centred there.
MAP_LANDMARK_LIFT = {"Dungeon": 18, "Castle": 70, "Cathedral": 45, "Haunted Forest": 20, "Caverns": 30, "Village": 14,
                     "Swamp": 10, "Mountains": 40, "Necropolis": 22, "Wizard's Tower": 110}


def _ground_point(x, y, height_fn, step=40.0, limit=60000.0):
    """Where the camera ray through screen point (x, y) first meets the terrain (ray
    marching, then bisection)."""
    origin, d = B.screen_ray(x, y)
    t0, t = 0.0, step
    while t < limit:
        p = origin + d * t
        if p.z <= height_fn(p.x, p.y):
            for _ in range(30):
                tm = (t0 + t) / 2
                q = origin + d * tm
                if q.z <= height_fn(q.x, q.y):
                    t = tm
                else:
                    t0 = tm
            p = origin + d * t
            return Vector((p.x, p.y, height_fn(p.x, p.y)))
        t0, t = t, t + step * (1 + t / 2000.0)
    p = origin + d * limit
    return Vector((p.x, p.y, height_fn(p.x, p.y)))


@scene("map")
def world_map():
    from mathutils import noise as N
    cam = B.camera((0, -2600, 2400), (0, 1500, 400), lens=22)
    # Lens shift so the horizon sits near the top of the frame (a high, oblique view).
    lo, hi = -0.5, 0.5
    for _ in range(30):
        mid = (lo + hi) / 2
        cam.data.shift_y = mid
        y = B.project((0, 16500, 4300))[1]   # the foot of the horizon range
        if y > 150:
            hi = mid
        else:
            lo = mid
    moon = (262, 70)
    moon_lighting(moon, strength=3.0, fill=2.6, fill_dir=(-0.75, -0.35, 0.55))

    marks = {}
    def tilt(y):
        # The land rises towards the horizon ranges (an illustrator's tilted table): rows
        # of the frame map to far more even distances than on a flat world.
        return 0.3 * y if y > 0 else 0.15 * y
    def base_height(x, y):
        h = tilt(y) + 30 * N.fractal(Vector((x / 900, y / 900, 0.3)), 0.6, 2.0, 5)
        h += 140 * N.fractal(Vector((x / 1700, y / 1700, 5.1)), 0.55, 2.0, 4)
        h += 14 * N.noise(Vector((x / 260, y / 260, 2.0)))
        return h
    for name, sx, sy in MAP_LANDMARKS:
        marks[name] = _ground_point(sx, sy, base_height)

    rivers_screen = [
        [(812, 160), (760, 210), (700, 250), (610, 290), (590, 330), (530, 380), (500, 440), (470, 520), (440, 600)],
        [(700, 250), (720, 300), (770, 340), (830, 420), (900, 520), (960, 600)],
        [(250, 300), (300, 330), (380, 340), (450, 330), (530, 380)],
    ]
    rivers = [[_ground_point(x, y, base_height) for (x, y) in r] for r in rivers_screen]

    def seg_dist(px, py, a, b):
        ax, ay, bx, by = a.x, a.y, b.x, b.y
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy + 1e-9)))
        return math.hypot(px - (ax + t * dx), py - (ay + t * dy))

    mtn = marks["Mountains"]
    lake_c = marks["Swamp"] + Vector((-420, -120, 0))

    def height(x, y):
        h = base_height(x, y)
        # The mountain spine in the north-east and the horizon highlands.
        dm = math.hypot((x - mtn.x) / 1.4, y - mtn.y)
        spine = max(0.0, 1.0 - dm / 2200.0) + max(0.0, min(1.0, (y - 10500) / 2500))
        ridged = N.ridged_multi_fractal(Vector((x / 520, y / 520, 1.7)), 0.9, 2.1, 6, 1.0, 2.0)
        h += spine ** 1.3 * 700 * ridged
        # Crags under the castle, the wizard's tower, the dungeon and the caverns.
        for name, r, hh in (("Castle", 160, 120), ("Wizard's Tower", 120, 140), ("Dungeon", 150, 70), ("Caverns", 220, 120), ("Cathedral", 120, 30)):
            c = marks[name]
            d = math.hypot(x - c.x, y - c.y)
            h += hh * max(0.0, 1.0 - (d / r) ** 2)
        # The black lake in the west.
        dl = math.hypot((x - lake_c.x) / 1.6, y - lake_c.y)
        h -= 60 * max(0.0, 1.0 - dl / 700.0)
        # River beds.
        for r in rivers:
            d = min(seg_dist(x, y, r[i], r[i + 1]) for i in range(len(r) - 1))
            if d < 70 + 0.02 * max(y, 0):
                h -= 40 * (1 - d / 70) ** 2
        return h

    # Re-solve the landmarks against the full terrain (crags, the spine, river beds).
    for name, sx, sy in MAP_LANDMARKS:
        marks[name] = _ground_point(sx, sy, height)

    terrain_mat = B.mat_stone("terrain", (0.04, 0.038, 0.045), bump=0.8, scale=0.02)
    far_mat = B.mat_stone("farland", (0.02, 0.019, 0.023), bump=0.5, scale=0.01, haze=0.012)
    B.heightfield(height, -9000, 9000, -1500, 10800, 340, 300, "mid", terrain_mat)
    B.heightfield(height, -16000, 16000, 10700, 14500, 240, 50, "far", far_mat, name="farland")
    B.ridge(-24000, 24000, 16500, 4300, 2300, 1500, 31, "far", B.mat_stone("horizon", (0.03, 0.028, 0.035), haze=0.04), segments=600, rows=4, sharp=1.7, freq=1 / 2600.0)
    B.ridge(-20000, 20000, 13600, 3700, 900, 1200, 37, "far", B.mat_stone("horizon2", (0.025, 0.024, 0.03), haze=0.018), segments=600, rows=4, sharp=1.6, freq=1 / 2200.0)
    B.world_colour((0.011, 0.012, 0.017), 1.0)
    # The black lake, a dark mirror.
    water = B.mat_metal((0.02, 0.02, 0.025), 0.08, "water")
    B.ground(-18, 2600, "mid", water, location=(lake_c.x, lake_c.y))
    # Rivers of lava.
    lava = B.mat_lava(0.04, 9.0, crack=0.3)
    for r in rivers:
        pts = []
        for i in range(len(r) - 1):
            for k in range(8):
                t = k / 8
                a, b = r[i], r[i + 1]
                x, y = a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t
                lift = 6 + (Vector((x, y, 0)) - cam.location).length / 300.0
                pts.append((x, y, height(x, y) + lift))
        pts.append((r[-1].x, r[-1].y, height(r[-1].x, r[-1].y) + 6))
        verts, faces = [], []
        for i, p in enumerate(pts):
            q = pts[min(i + 1, len(pts) - 1)] if i + 1 < len(pts) else p
            o = pts[max(i - 1, 0)]
            dx, dy = q[0] - o[0], q[1] - o[1]
            n = math.hypot(dx, dy) + 1e-6
            dist = (Vector(p) - cam.location).length
            w = (8 + 5 * math.sin(i * 0.7)) * dist / 3000.0
            verts.append((p[0] - dy / n * w, p[1] + dx / n * w, p[2]))
            verts.append((p[0] + dy / n * w, p[1] - dx / n * w, p[2]))
        for i in range(len(pts) - 1):
            faces.append((2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2))
        B.mesh_object("river", verts, faces, "mid", B.mat_lava(0.02, 5.0, crack=0.45))
        for i in range(0, len(pts), 6):
            B.light("POINT", (pts[i][0], pts[i][1], pts[i][2] + 30), EMBER, 4.0e5, size=20, name="riverglow")

    st, rf, win = stone(1.4), roof_mat(), windows(14.0)
    dim = B.mat_emission(WINDOW, 6.0, "dimwin")
    rk = dark_rock(1.3)
    camera_at = cam.location.copy()
    def scale_of(p):
        # Stylised: landmarks grow with distance so the far ones read as well as the near.
        return 2.2 * (camera_at - p).length / 1150.0

    def glow(p, k, energy=1.5e5, dz=40):
        B.light("POINT", (p.x, p.y - 30 * k, p.z + dz * k), EMBER, 1.5 * energy * k * k, size=10 * k, name="landglow")
        # A crimson rim light behind the landmark: its silhouette edges glow.
        B.light("SPOT", (p.x + 60 * k, p.y + 420 * k, p.z + 160 * k), CRIMSON, 6.0e5 * k * k, size=20 * k, look_at=(p.x, p.y, p.z + 40 * k), name="landrim", spot=9)

    # Castle: spires on its crag.
    c = marks["Castle"]; k = scale_of(c) / 2.2
    B.crag((c.x, c.y, c.z - 60 * k), 110 * k, 90 * k, 5, "mid", rk, spikes=0.3, mesa=0.6)
    B.castle((c.x, c.y, c.z + 26 * k), 3.6 * k, 5, "mid", st, rf, win, towers=12, spread=1.4, with_crag=False)
    B.spire((c.x, c.y, c.z + 26 * k), 9 * k, 130 * k, "mid", st, rf, roof_h=60 * k, windows=win, seed=1)
    glow(c, k, dz=70)
    # Cathedral: twin towers and nave.
    c = marks["Cathedral"]; k = scale_of(c) / 2.2
    B.cathedral((c.x, c.y, c.z - 4 * k), 6.5 * k, 2, "mid", st, rf, win, rotation_z=0.25)
    glow(c, k)
    # Dungeon: a black arch into a rocky outcrop, red from within, flanked by spikes.
    c = marks["Dungeon"]; k = scale_of(c) / 2.2
    B.rock((c.x, c.y + 40 * k, c.z + 10 * k), 60 * k, 3, "mid", rk, squash=(1.3, 1.0, 0.8))
    B.wall_with_arch((c.x, c.y - 26 * k, c.z - 6 * k), 70 * k, 60 * k, 12 * k, 30 * k, 44 * k, "mid", B.mat_stone("dgate", (0.025, 0.023, 0.026), bump=0.6))
    B.arch_shape((c.x, c.y - 25 * k, c.z - 6 * k), 28 * k, 42 * k, "mid", B.mat_emission(CRIMSON, 5.0, "dmouth"), name="dmouth")
    for side in (-1, 1):
        B.cone((c.x + side * 44 * k, c.y - 30 * k, c.z - 8 * k), 9 * k, 70 * k, "mid", rk, sides=5, name="spike")
    glow(c, k, 2.5e5, 20)
    # Haunted Forest: a wood of dead trees in the mist.
    c = marks["Haunted Forest"]; k = scale_of(c) / 2.2
    rnd = random.Random(4)
    for t in range(70):
        a_, d = rnd.random() * 2 * math.pi, 220 * k * math.sqrt(rnd.random())
        x, y = c.x + math.cos(a_) * d * 1.4, c.y + math.sin(a_) * d
        B.dead_tree((x, y, height(x, y)), (40 + rnd.random() * 40) * k, 100 + t, "mid", rk, branches=3, depth=2)
    glow(c, k, 1.5e5, 10)
    B.light("POINT", (c.x, c.y + 260 * k, c.z + 30 * k), CRIMSON, 3.0e5 * k * k, size=20 * k, name="forestback")
    # Caverns: a cliff wall with cave mouths.
    c = marks["Caverns"]; k = scale_of(c) / 2.2
    B.crag((c.x, c.y + 30 * k, c.z - 60 * k), 160 * k, 170 * k, 8, "mid", rk, spikes=0.5, mesa=0.4)
    for dx, sz in ((-50, 30), (40, 22)):
        B.arch_shape((c.x + dx * k, c.y - 120 * k, c.z - 10 * k), sz * k, sz * 1.1 * k, "mid", B.mat_emission(CRIMSON, 2.0, "cavemouth"), pointed=False, name="cave")
    glow(c, k, 1.2e5)
    # Village: cottages with lit windows and a steeple.
    c = marks["Village"]; k = scale_of(c) / 2.2
    rnd = random.Random(9)
    for t in range(22):
        a_, d = rnd.random() * 2 * math.pi, 150 * k * math.sqrt(rnd.random())
        x, y = c.x + math.cos(a_) * d * 1.3, c.y + math.sin(a_) * d
        z = height(x, y)
        w, l = (18 + rnd.random() * 10) * k, (26 + rnd.random() * 10) * k
        rot = rnd.random() * 3.14
        B.box((x, y, z + 8 * k), (w, l, 16 * k), "mid", st, rotation=(0, 0, rot), name="cottage")
        B.cone((x, y, z + 16 * k), w * 0.85, 18 * k, "mid", rf, sides=4, name="cottageroof")
        if rnd.random() < 0.7:
            B.box((x, y - l / 2 - 0.5, z + 7 * k), (4 * k, 0.5, 4 * k), "mid", dim, name="vwin")
    B.spire((c.x + 10 * k, c.y + 20 * k, c.z), 7 * k, 70 * k, "mid", st, rf, roof_h=40 * k, windows=win, seed=3, pinnacles=False)
    glow(c, k, 1.4e5)
    # Swamp: pools, dead trees.
    c = marks["Swamp"]; k = scale_of(c) / 2.2
    rnd = random.Random(12)
    for t in range(9):
        x, y = c.x + (rnd.random() - 0.5) * 360 * k, c.y + (rnd.random() - 0.5) * 240 * k
        z = height(x, y) + 1
        B.mesh_object("pool", [(x - 50 * k, y - 30 * k, z), (x + 50 * k, y - 30 * k, z), (x + 60 * k, y + 30 * k, z), (x - 40 * k, y + 35 * k, z)],
                      [(0, 1, 2, 3)], "mid", water)
    for t in range(18):
        x, y = c.x + (rnd.random() - 0.5) * 420 * k, c.y + (rnd.random() - 0.5) * 300 * k
        B.dead_tree((x, y, height(x, y)), (30 + rnd.random() * 25) * k, 300 + t, "mid", rk, branches=3, depth=2, lean=0.6)
    glow(c, k, 1.2e5, 15)
    B.light("POINT", (c.x, c.y + 200 * k, c.z + 20 * k), CRIMSON, 2.0e5 * k * k, size=20 * k, name="swampback")
    # Mountains: a dwarven gate glowing in the flank of the highest peak.
    c = marks["Mountains"]; k = scale_of(c) / 2.2
    B.box((c.x, c.y - 40 * k, c.z + 30 * k), (60 * k, 10 * k, 70 * k), "mid", B.mat_stone("dwarfgate", (0.03, 0.028, 0.03)), name="gateframe")
    B.box((c.x, c.y - 46 * k, c.z + 22 * k), (34 * k, 2 * k, 50 * k), "mid", B.mat_emission(CRIMSON, 4.0, "dwarfglow"), name="gatelight")
    glow(c, k, 2.0e5, 80)
    # Necropolis: tombstones, a mausoleum, spires.
    c = marks["Necropolis"]; k = scale_of(c) / 2.2
    rnd = random.Random(21)
    for t in range(60):
        x, y = c.x + (rnd.random() - 0.5) * 260 * k, c.y + (rnd.random() - 0.5) * 200 * k
        B.box((x, y, height(x, y) + 5 * k), (5 * k, 3 * k, (10 + rnd.random() * 6) * k), "mid", st, rotation=(0, rnd.random() * 0.3 - 0.15, rnd.random() * 0.4), name="tomb")
    B.box((c.x, c.y, c.z + 18 * k), (40 * k, 30 * k, 36 * k), "mid", st, name="mausoleum")
    B.cone((c.x, c.y, c.z + 36 * k), 30 * k, 30 * k, "mid", rf, sides=4, name="mroof")
    B.arch_shape((c.x, c.y - 15.2 * k, c.z), 10 * k, 18 * k, "mid", dim, name="mdoor")
    for t in range(4):
        x = c.x + (t - 1.5) * 50 * k
        B.spire((x, c.y + 70 * k, height(x, c.y + 70 * k)), 5 * k, (40 + t * 8) * k, "mid", st, rf, roof_h=24 * k, pinnacles=False)
    glow(c, k, 1.0e5)
    # Wizard's Tower: one tall thin tower on its crag, the observatory lit at the top.
    c = marks["Wizard's Tower"]; k = scale_of(c) / 2.2
    B.crag((c.x, c.y, c.z - 50 * k), 70 * k, 90 * k, 8, "mid", rk, spikes=0.4)
    top = c.z + 30 * k
    B.spire((c.x, c.y, top), 9 * k, 150 * k, "mid", st, rf, roof_h=55 * k, windows=win, seed=8, pinnacles=False, taper=0.75)
    B.cylinder((c.x, c.y, top + 140 * k), 13 * k, 10 * k, "mid", st, sides=12, name="balcony")
    B.cylinder((c.x, c.y, top + 150.5 * k), 9.5 * k, 6 * k, "mid", B.mat_emission(CRIMSON, 6.0, "observatory"), sides=12, name="obsglow")
    glow(c, k, 1.5e5, 190)
    wizard_top = Vector((c.x, c.y, top + 153 * k))

    # Near: dark cloud wisps at the frame's lower corners (they sweep past in the zoom).
    near_fog = B.mat_fog(colour=(0.35, 0.33, 0.42), density=0.004, noise_scale=0.004, z0=600, z1=1500, anisotropy=0.2, emission=0.0004, emit_colour=SLATE)
    for (sx, sy) in ((40, 530), (930, 530)):
        p = _ground_point(sx, sy, lambda x, y: 1100)
        B.fog_box((p.x, p.y, 1100), (900, 700, 600), "near", B.mat_fog(colour=(0.35, 0.33, 0.42), density=0.004, noise_scale=0.004, z0=600, z1=1500,
                  anisotropy=0.2, emission=0.0004, emit_colour=SLATE, centre=(p.x, p.y), radius=(450, 350)))
    # Fog: banks over the low ground (the lake, the forest, the swamp, the river valley).
    fog = B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.007, noise_scale=0.003, z0=-60, z1=90, anisotropy=0.25,
                    emission=0.0007, emit_colour=SLATE)
    for name, w, d in (("Swamp", 1600, 900), ("Haunted Forest", 1400, 900), ("Village", 1200, 700), ("Dungeon", 1400, 900), ("Caverns", 1200, 800)):
        c = marks[name]
        k = scale_of(c) / 2.2
        w, d = w * k / 3.2, d * k / 3.2
        B.fog_box((c.x, c.y, c.z + 10), (w, d, 300 * k), "fog", B.mat_fog(colour=(0.6, 0.55, 0.7), density=0.006 / k, noise_scale=0.003 / k,
                  z0=c.z - 60 * k, z1=c.z + 90 * k, anisotropy=0.25, emission=0.0006 / k, emit_colour=SLATE, centre=(c.x, c.y), radius=(w / 2, d / 2)))
    B.embers((marks["Dungeon"].x, marks["Dungeon"].y, marks["Dungeon"].z), (900, 700, 200), 120, 77, "mid", size=2.0, strength=12)

    anchors = []
    for i, (name, sx, sy) in enumerate(MAP_LANDMARKS):
        c = marks[name]
        k = scale_of(c) / 2.2
        anchors.append({"kind": "landmark", "index": i, "name": name, "world": (c.x, c.y, c.z + MAP_LANDMARK_LIFT[name] * k)})
    for r in rivers:
        for p in r[1:-1:2]:
            anchors.append({"kind": "lava", "world": (p.x, p.y, p.z), "radius": 30})
    anchors.append({"kind": "window", "world": tuple(wizard_top), "radius": 10})
    anchors.append({"kind": "fire", "world": tuple(marks["Dungeon"] + Vector((0, -26, 14))), "radius": 16})
    return {
        "sky": {"moon": [moon[0], moon[1], 54], "horizon": 130, "clouds": 0.7, "stars": 0.8, "seed": 12},
        "haze": {"amount": 0.5},
        "exposure": 1.5,
        "anchors": anchors,
    }


# The other eight locations.
import scenes2  # noqa: E402,F401
