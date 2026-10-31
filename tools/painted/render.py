"""Renders one painted scene's depth layers with Blender (Cycles, CPU).

    /opt/bvenv/bin/python tools/painted/render.py <scene> <out dir> [--scale 1.0] [--samples 48]

Writes <out>/<layer>.exr (Combined RGBA, Depth, Denoising Albedo/Normal) for every
layer the scene uses and <out>/anchors.json: the scene's sky parameters and its
anchors (hotspots, landmarks, lights, ember sources) projected to the virtual 960 x 560
frame. paint.py turns these into the painted layers.
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blib  # noqa: E402
import scenes  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scene")
    ap.add_argument("out")
    ap.add_argument("--scale", type=float, default=1.0, help="1.0 = 1920 x 1120")
    ap.add_argument("--samples", type=int, default=48)
    ap.add_argument("--layers", default="")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    blib.reset()
    build = scenes.REGISTRY[args.scene]
    meta = build()
    width, height = int(round(1920 * args.scale)), int(round(1120 * args.scale))
    blib.configure(width, height, args.samples)

    anchors = []
    for a in meta.get("anchors", []):
        a = dict(a)
        if "world" in a:
            x, y, z = blib.project(a.pop("world"))
            a["x"], a["y"] = round(x, 1), round(y, 1)
        anchors.append(a)
    meta["anchors"] = anchors
    meta["scene"] = args.scene
    layers = [l for l in blib.LAYERS if l != "sky" and any(o.get("layer") == l for o in bpy_objects())]
    meta["rendered"] = layers
    with open(os.path.join(args.out, "anchors.json"), "w") as f:
        json.dump(meta, f, indent=1)

    wanted = [l for l in layers if not args.layers or l in args.layers.split(",")]
    for layer in wanted:
        t = time.time()
        sc = blib.bpy.context.scene
        # The fog layer is soft: render it at half resolution.
        scale = 0.5 if layer == "fog" else 1.0
        sc.render.resolution_x, sc.render.resolution_y = int(width * scale), int(height * scale)
        sc.cycles.samples = args.samples if layer != "fog" else max(16, args.samples // 2)
        blib.render_layer(layer, os.path.join(os.path.abspath(args.out), layer + ".exr"))
        print(f"[{args.scene}] {layer}: {time.time() - t:.1f} s", flush=True)


def bpy_objects():
    return blib.bpy.context.scene.objects


if __name__ == "__main__":
    main()
