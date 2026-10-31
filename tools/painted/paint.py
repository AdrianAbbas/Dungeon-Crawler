"""Painterly post-process for the rendered layers (numpy + scipy; OIDN via ctypes).

    /opt/bvenv/bin/python tools/painted/paint.py <render dir> <asset dir> [--preview file.jpg]

Per layer: denoise (Intel Open Image Denoise, bundled with Blender), atmospheric haze
by depth layer, bloom on the emissive lights, tone map and colour grade (near-black,
slate and purple mists, crimson kept intense), a generalised Kuwahara (oil-paint)
filter, brush streaks along the image's structure (line-integral convolution), grain
and speckle. The sky and moon layer is painted here from the scene's parameters.

Writes <asset dir>/sky.jpg, <layer>.jpg + <layer>.png (colour + alpha mask) for the
other layers, and scene.json (anchors, layer list, moon) for the manifest.
"""
import argparse
import ctypes
import glob
import json
import os

import numpy as np
import OpenEXR
from PIL import Image
from scipy import ndimage, signal

VW, VH = 960.0, 560.0

# ------------------------------------------------------------------------- OIDN
_lib = None


def _oidn():
    global _lib
    if _lib is None:
        path = glob.glob(os.path.join(os.path.dirname(__import__("bpy").__file__), "lib", "libOpenImageDenoise.so.2"))
        if not path:
            path = glob.glob("/opt/bvenv/lib/python3*/site-packages/bpy/lib/libOpenImageDenoise.so.2")
        lib = ctypes.CDLL(path[0])
        lib.oidnNewDevice.restype = ctypes.c_void_p
        lib.oidnNewDevice.argtypes = [ctypes.c_int]
        lib.oidnCommitDevice.argtypes = [ctypes.c_void_p]
        lib.oidnNewFilter.restype = ctypes.c_void_p
        lib.oidnNewFilter.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        lib.oidnSetSharedFilterImage.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int] + [ctypes.c_size_t] * 5
        lib.oidnSetFilterBool.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_bool]
        lib.oidnCommitFilter.argtypes = [ctypes.c_void_p]
        lib.oidnExecuteFilter.argtypes = [ctypes.c_void_p]
        lib.oidnReleaseFilter.argtypes = [ctypes.c_void_p]
        lib.oidnReleaseDevice.argtypes = [ctypes.c_void_p]
        _lib = lib
    return _lib


def denoise(rgb, hdr=True):
    lib = _oidn()
    h, w, _ = rgb.shape
    c = np.ascontiguousarray(rgb[..., :3], dtype=np.float32)
    out = np.zeros_like(c)
    dev = lib.oidnNewDevice(0)
    lib.oidnCommitDevice(dev)
    f = lib.oidnNewFilter(dev, b"RT")
    lib.oidnSetSharedFilterImage(f, b"color", c.ctypes.data, 3, w, h, 0, 0, 0)
    lib.oidnSetSharedFilterImage(f, b"output", out.ctypes.data, 3, w, h, 0, 0, 0)
    lib.oidnSetFilterBool(f, b"hdr", hdr)
    lib.oidnCommitFilter(f)
    lib.oidnExecuteFilter(f)
    lib.oidnReleaseFilter(f)
    lib.oidnReleaseDevice(dev)
    return out


# ------------------------------------------------------------------------ helpers
def load_exr(path):
    with OpenEXR.File(path, separate_channels=True) as f:
        ch = f.channels()
        get = lambda c: ch["ViewLayer.Combined." + c].pixels.astype(np.float32)
        return np.dstack([get("R"), get("G"), get("B"), get("A")])


def fbm(h, w, cells_y, cells_x, octaves, seed, persistence=0.5):
    """Smooth fractal value noise in [0, 1] (cubic upsampled random grids)."""
    rng = np.random.default_rng(seed)
    out = np.zeros((h, w), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        gy, gx = max(2, int(cells_y * 2 ** o)), max(2, int(cells_x * 2 ** o))
        grid = rng.random((gy + 3, gx + 3)).astype(np.float32)
        up = ndimage.zoom(grid, ((h + 3 * h / gy) / (gy + 3), (w + 3 * w / gx) / (gx + 3)), order=3)
        out += amp * up[:h, :w]
        total += amp
        amp *= persistence
    out /= total
    return (out - out.min()) / max(1e-6, out.max() - out.min())


def lum(rgb):
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def unpremultiply(img):
    a = img[..., 3:4]
    rgb = np.where(a > 1e-4, img[..., :3] / np.maximum(a, 1e-4), 0.0)
    return rgb, img[..., 3]


def screen_xy(h, w):
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    return x * (VW / w), y * (VH / h)


# ------------------------------------------------------------------ tone + grade
def tonemap(rgb, exposure=1.0, white=4.0):
    x = rgb * exposure
    l = lum(x)[..., None]
    lt = l * (1 + l / (white * white)) / (1 + l)
    out = x * (lt / np.maximum(l, 1e-6))
    out = np.clip(out, 0, None) ** (1 / 2.2)
    # Deep blacks: a black point and a contrast curve (most of the frame near-black).
    out = np.clip((out - 0.045) / 0.955, 0, 1) ** 1.18
    return out


def grade(rgb):
    """Near-black and charcoal, cool slate/purple shadows, crimson kept intense; every
    other hue desaturated."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    L = lum(rgb)
    red = np.clip((r - np.maximum(g, b)) * 3.0, 0, 1)
    # Desaturate non-red colours towards a cool grey.
    cool = np.dstack([L * 0.92, L * 0.95, L * 1.1])
    out = rgb * red[..., None] + (rgb * 0.25 + cool * 0.75) * (1 - red[..., None])
    # Split toning: shadows towards slate-purple, a lifted (but dark) floor.
    shadow = np.clip(1 - L / 0.35, 0, 1)[..., None]
    tint = np.array([0.055, 0.05, 0.075], np.float32)
    out = out * (1 - 0.35 * shadow) + tint * shadow * 0.35
    # Reds: crimson, not salmon (gamma lifts the small green and blue of a bright red).
    out[..., 1] = out[..., 1] * (1 - 0.75 * red)
    out[..., 2] = out[..., 2] * (1 - 0.6 * red)
    out[..., 0] = out[..., 0] * (1 - 0.08 * red)
    return np.clip(out, 0, 1)


# ------------------------------------------------------------------ the painting
def kuwahara(img, radius, sectors=8, q=8.0):
    """Generalised Kuwahara (Papari et al.): eight overlapping sectors, weighted by the
    inverse of their luminance variance. Works on premultiplied RGBA."""
    if radius < 1:
        return img
    size = 2 * radius + 1
    yy, xx = np.mgrid[-radius:radius + 1, -radius:radius + 1].astype(np.float32)
    ang = np.arctan2(yy, xx)
    dist = np.sqrt(xx * xx + yy * yy)
    gauss = np.exp(-(dist ** 2) / (2 * (radius * 0.6) ** 2)) * (dist <= radius + 0.5)
    L = lum(img[..., :3])
    num = np.zeros_like(img)
    den = np.zeros(img.shape[:2], np.float32)
    for k in range(sectors):
        a0 = -np.pi + 2 * np.pi * k / sectors
        d = np.angle(np.exp(1j * (ang - (a0 + np.pi / sectors))))
        w = gauss * np.exp(-(d ** 2) / (2 * (np.pi / sectors) ** 2))
        w[radius, radius] = gauss[radius, radius]
        w /= w.sum()
        conv = lambda ch: signal.oaconvolve(ch, w, mode="same")
        mean = np.dstack([conv(img[..., c]) for c in range(img.shape[2])])
        m1 = conv(L)
        m2 = conv(L * L)
        var = np.maximum(m2 - m1 * m1, 0)
        wk = 1.0 / (1.0 + (var * 255.0) ** (q / 2) * 1e-2)
        num += mean * wk[..., None]
        den += wk
    return num / np.maximum(den, 1e-8)[..., None]


def structure_angle(L, sigma=3.0):
    gx = ndimage.sobel(L, 1)
    gy = ndimage.sobel(L, 0)
    jxx = ndimage.gaussian_filter(gx * gx, sigma)
    jyy = ndimage.gaussian_filter(gy * gy, sigma)
    jxy = ndimage.gaussian_filter(gx * gy, sigma)
    # Tangent = direction of least change.
    theta = 0.5 * np.arctan2(2 * jxy, jxx - jyy) + np.pi / 2
    coherence = np.sqrt((jxx - jyy) ** 2 + 4 * jxy ** 2) / (jxx + jyy + 1e-6)
    return theta, coherence


def brush_texture(L, length, seed, scale, horizontal=False):
    """Streaks of noise smeared along the structure (line-integral convolution): the
    traces of brush strokes. Where the image has no structure the strokes follow a
    slowly varying default direction."""
    h, w = L.shape
    rng = np.random.default_rng(seed)
    base = rng.random((h, w)).astype(np.float32)
    base = ndimage.gaussian_filter(base, 0.7 * scale)
    theta, coh = structure_angle(ndimage.gaussian_filter(L, 1.5 * scale), 4.0 * scale)
    wander = (fbm(h, w, 2, 3, 2, seed + 1) - 0.5) * 0.7 + 0.12
    theta = np.where(coh > 0.45, theta, wander)
    if horizontal:
        # Long sweeping strokes, mostly horizontal, turning with the clouds and mist.
        theta = (fbm(h, w, 3, 4, 2, seed + 4) - 0.5) * 1.0 + 0.5 * np.where(coh > 0.5, np.sin(2 * theta) * 0.3, 0)
    dx, dy = np.cos(theta), np.sin(theta)
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    acc = np.zeros_like(base)
    steps = max(3, int(length))
    for s in range(-steps, steps + 1):
        acc += ndimage.map_coordinates(base, [y + dy * s, x + dx * s], order=1, mode="reflect")
    acc /= 2 * steps + 1
    acc = (acc - acc.mean()) / (acc.std() + 1e-6)
    return acc


def dabs(img, seed, scale):
    """Soft, slightly uneven paint dabs: low-frequency value and warmth variation."""
    h, w = img.shape[:2]
    v = fbm(h, w, 18, 30, 3, seed + 21) - 0.5
    warm = fbm(h, w, 10, 16, 2, seed + 22) - 0.5
    out = img.copy()
    out[..., :3] *= (1 + 0.16 * v)[..., None]
    out[..., 0] *= 1 + 0.06 * warm
    out[..., 2] *= 1 - 0.06 * warm
    return out


def painterly(img, radius, stroke, seed, scale, streak=0.08, sharpen=0.0, horizontal=False):
    """img: premultiplied RGBA in display space."""
    out = kuwahara(dabs(img, seed, scale), radius)
    if stroke > 0:
        rgb, a = unpremultiply(out)
        tex = brush_texture(lum(rgb), stroke, seed, scale, horizontal)
        rgb = rgb * (1 + (streak * 0.6 if horizontal else streak) * tex[..., None])
        out = np.dstack([rgb * a[..., None], a])
    if sharpen > 0:
        blur = ndimage.gaussian_filter(out, (1.2 * scale, 1.2 * scale, 0))
        out = out + sharpen * (out - blur)
    return np.clip(out, 0, 1)


def grain(img, seed, amount=0.018, speckle=0.0012, scale=1.0):
    h, w = img.shape[:2]
    rng = np.random.default_rng(seed + 99)
    n = rng.normal(0, amount, (h, w)).astype(np.float32)
    n = ndimage.gaussian_filter(n, 0.5 * scale) * 1.6
    a = img[..., 3]
    out = img.copy()
    for c in range(3):
        out[..., c] += n * a
    if speckle > 0:
        dots = rng.random((h, w)) < speckle
        size = ndimage.binary_dilation(dots, iterations=max(1, int(round(scale)))) if scale > 1.2 else dots
        val = rng.random((h, w)).astype(np.float32)
        light = size & (val > 0.55)
        dark = size & (val < 0.35)
        for c in range(3):
            out[..., c] = np.where(light, out[..., c] + 0.07 * a, out[..., c])
            out[..., c] = np.where(dark, out[..., c] * 0.55, out[..., c])
    return np.clip(out, 0, 1)


# --------------------------------------------------------------------------- sky
def paint_sky(h, w, p, seed):
    """The sky and moon (linear light): a near-black zenith, slate-purple horizon mist,
    a huge crimson moon with maria and a halo, torn clouds lit red near it, stars."""
    sx, sy = screen_xy(h, w)
    horizon = p.get("horizon", 300)
    t = np.clip(sy / horizon, 0, 1.4)
    zenith = np.array([0.006, 0.005, 0.01], np.float32)
    hor = np.array([0.035, 0.03, 0.048], np.float32) * p.get("glow", 1.0)
    sky = zenith + (hor - zenith) * (t[..., None] ** 1.6)
    out = sky.copy()
    mx, my, mr = p.get("moon", [0, -999, 0])
    # Halo: wide crimson glow.
    d = np.sqrt((sx - mx) ** 2 + (sy - my) ** 2)
    if mr > 0:
        halo = np.exp(-np.maximum(d - mr, 0) / (mr * 1.1)) * 0.5 + np.exp(-np.maximum(d - mr, 0) / (mr * 0.18)) * 0.6
        out += np.array([0.42, 0.025, 0.02], np.float32) * halo[..., None] * p.get("halo", 1.0)
    # Clouds: horizontal torn bands.
    cl = p.get("clouds", 0.6)
    if cl > 0:
        n1 = fbm(h, w, 6, 2, 5, seed)
        n2 = fbm(h, w, 14, 4, 4, seed + 7)
        bands = np.clip((n1 * 0.7 + n2 * 0.3 - (0.62 - 0.25 * cl)) * 3.0, 0, 1)
        bands *= np.clip(1.0 - np.abs(sy - horizon * 0.55) / (horizon * 0.7), 0, 1) ** 0.7
        lit = np.exp(-np.maximum(d - mr * 0.6, 0) / (max(mr, 1) * 1.8)) if mr > 0 else 0 * d
        cloud_col = np.array([0.02, 0.018, 0.028], np.float32)[None, None] + np.array([0.45, 0.03, 0.025], np.float32)[None, None] * lit[..., None]
        out = out * (1 - 0.85 * bands[..., None]) + cloud_col * bands[..., None]
    # The moon: a disc with maria, limb darkening, a bright rim.
    if mr > 0:
        inside = np.clip((mr - d) / 1.5 + 0.5, 0, 1)
        u = np.clip(d / mr, 0, 1)
        maria = fbm(h, w, 5, 9, 4, seed + 3)
        crater = fbm(h, w, 20, 34, 2, seed + 5)
        tone = 0.75 + 0.35 * (maria - 0.5) + 0.12 * (crater - 0.5)
        limb = (1 - u ** 2) ** 0.25
        moon = np.array([1.35, 0.075, 0.05], np.float32) * (tone * (0.55 + 0.45 * limb))[..., None]
        # Clouds cross the lower moon.
        if cl > 0:
            cover = bands[..., None] * 0.8
            moon = moon * (1 - cover) + cloud_col * cover
        out = out * (1 - inside[..., None]) + moon * inside[..., None]
    # Stars: sparse, only in the dark upper sky.
    st = p.get("stars", 0.4)
    if st > 0:
        rng = np.random.default_rng(seed + 11)
        stars = (rng.random((h, w)) < 0.0009 * st).astype(np.float32) * rng.random((h, w)).astype(np.float32)
        stars = ndimage.gaussian_filter(stars, 0.6) * 4.0
        fade = np.clip(1 - sy / (horizon * 0.8), 0, 1) * (np.clip((d - mr * 1.6) / (mr + 1), 0, 1) if mr > 0 else 1)
        out += (stars * fade)[..., None] * np.array([0.8, 0.75, 0.85], np.float32)
    # Interior backdrops: a crimson glow in the depth instead of the moon.
    for gx, gy, gr, gs in p.get("glows", []):
        dd = np.sqrt((sx - gx) ** 2 + (sy - gy) ** 2)
        out += np.array([0.7, 0.07, 0.04], np.float32) * (np.exp(-dd / gr) * gs)[..., None]
    return np.dstack([out, np.ones((h, w), np.float32)])


# -------------------------------------------------------------------- per layer
LAYER_STYLE = {
    #        haze  kuwahara stroke sharpen grain speckle
    "sky":  (0.00, 11, 14, 0.0, 0.020, 0.0016),
    "far":  (0.30, 10, 12, 0.0, 0.018, 0.0010),
    "mid":  (0.22, 7, 10, 0.25, 0.016, 0.0006),
    "near": (0.03, 5, 8, 0.5, 0.016, 0.0004),
    "fog":  (0.00, 9, 14, 0.0, 0.010, 0.0),
}


def haze_colour(h, w, meta):
    sx, sy = screen_xy(h, w)
    base = np.array([0.05, 0.047, 0.066], np.float32)
    col = np.broadcast_to(base, (h, w, 3)).copy()
    moon = meta.get("sky", {}).get("moon")
    if moon and moon[2] > 0:
        d = np.sqrt((sx - moon[0]) ** 2 + (sy - moon[1]) ** 2)
        col += np.array([0.35, 0.03, 0.02], np.float32) * np.exp(-d / (moon[2] * 2.2))[..., None]
    for gx, gy, gr, gs in meta.get("sky", {}).get("glows", []):
        d = np.sqrt((sx - gx) ** 2 + (sy - gy) ** 2)
        col += np.array([0.3, 0.03, 0.02], np.float32) * (np.exp(-d / (gr * 1.5)) * gs * 0.5)[..., None]
    return col


def bloom(img, scale):
    """Glow around emissive light (linear, premultiplied)."""
    bright = np.maximum(img[..., :3] - 1.5, 0)
    glow = np.zeros_like(bright)
    for sigma, wgt in ((3, 0.25), (10, 0.15), (28, 0.1)):
        glow += ndimage.gaussian_filter(bright, (sigma * scale, sigma * scale, 0)) * wgt
    out = img.copy()
    out[..., :3] += glow
    out[..., 3] = np.clip(np.maximum(out[..., 3], lum(glow) * 1.5), 0, 1)
    return out


def process_layer(name, img, meta, seed, scale):
    haze, rad, stroke, sharp, gr, sp = LAYER_STYLE[name]
    h, w = img.shape[:2]
    if name != "sky":
        rgb = denoise(img[..., :3], hdr=True)
        a = denoise(np.repeat(img[..., 3:4], 3, axis=2), hdr=False)[..., 0]
        a = np.clip(a, 0, 1)
        img = np.dstack([np.clip(rgb, 0, None), a])
    amount = meta.get("haze", {}).get("amount", 0.7)
    if haze > 0:
        rgb, a = unpremultiply(img)
        hc = haze_colour(h, w, meta)
        k = haze * amount
        rgb = rgb * (1 - k) + hc * k
        img = np.dstack([rgb * a[..., None], a])
    img = bloom(img, scale)
    rgb, a = unpremultiply(img)
    exposure = meta.get("exposure", 1.0)
    rgb = grade(tonemap(rgb, exposure))
    if name == "fog":
        # Fog: pale slate where it is lit by nothing, crimson where the lights reach it.
        a = np.clip(a * meta.get("fog_opacity", 0.8), 0, 1)
    img = np.dstack([rgb * a[..., None], a])
    img = painterly(img, max(1, int(round(rad * scale))), stroke * scale, seed, scale, sharpen=sharp, horizontal=name in ("fog", "sky"))
    if name in ("sky", "far"):
        img = ndimage.gaussian_filter(img, (0.8 * scale, 0.8 * scale, 0))
    img = grain(img, seed, gr, sp, scale)
    return img


def save_layer(img, path_base, opaque=False, quality=88):
    rgb, a = unpremultiply(img) if not opaque else (img[..., :3], img[..., 3])
    if not opaque:
        # Bleed the colour outwards under transparent pixels so JPEG blocks and
        # resampling never bring in black fringes.
        known = a > 0.02
        filled = rgb.copy()
        if known.any():
            idx = ndimage.distance_transform_edt(~known, return_distances=False, return_indices=True)
            filled = rgb[idx[0], idx[1]]
        rgb = np.where(known[..., None], rgb, filled)
        Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), "L").save(path_base + ".png", optimize=True)
    Image.fromarray((np.clip(rgb, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").save(path_base + ".jpg", quality=quality, optimize=True, progressive=False, subsampling=0)


def over(dst, src):
    return src + dst * (1 - src[..., 3:4])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("render")
    ap.add_argument("assets")
    ap.add_argument("--preview", default="")
    ap.add_argument("--width", type=int, default=0, help="output width (default: the render's)")
    args = ap.parse_args()
    meta = json.load(open(os.path.join(args.render, "anchors.json")))
    os.makedirs(args.assets, exist_ok=True)
    seed = abs(hash(meta["scene"])) % 10000
    layers = {}
    size = None
    for name in meta["rendered"]:
        img = load_exr(os.path.join(args.render, name + ".exr"))
        layers[name] = img
        if name != "fog":
            size = img.shape[:2]
    if size is None:
        size = (1120, 1920)
    h, w = size
    scale = w / 1920.0
    sky_params = meta.get("sky", {})
    out = {}
    out["sky"] = process_layer("sky", paint_sky(h, w, sky_params, sky_params.get("seed", 1) + seed), meta, seed, scale)
    for name, img in layers.items():
        s = scale * (0.5 if name == "fog" else 1.0)
        out[name] = process_layer(name, img, meta, seed + len(name), s)
    save_layer(out["sky"], os.path.join(args.assets, "sky"), opaque=True)
    for name in layers:
        save_layer(out[name], os.path.join(args.assets, name), quality=86 if name != "fog" else 80)
    info = {k: meta[k] for k in ("scene", "anchors") if k in meta}
    info["layers"] = ["sky"] + [l for l in ("far", "mid", "fog", "near") if l in layers]
    info["moon"] = sky_params.get("moon", [0, 0, 0])
    info["glows"] = sky_params.get("glows", [])
    json.dump(info, open(os.path.join(args.assets, "scene.json"), "w"), indent=1)
    if args.preview:
        comp = out["sky"]
        for name in ("far", "mid", "fog", "near"):
            if name in out:
                layer = out[name]
                if layer.shape[:2] != comp.shape[:2]:
                    layer = np.dstack([ndimage.zoom(layer[..., c], (comp.shape[0] / layer.shape[0], comp.shape[1] / layer.shape[1]), order=1) for c in range(4)])
                comp = over(comp, np.clip(layer, 0, 1))
        Image.fromarray((np.clip(comp[..., :3], 0, 1) * 255).astype(np.uint8)).save(args.preview, quality=90)


if __name__ == "__main__":
    main()
