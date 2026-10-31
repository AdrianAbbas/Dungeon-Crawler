# The painted art pipeline

DUNGEON CRAWLER's map and scenes are built in **Blender** from Python (no hand
modelling), rendered as **depth layers**, and then **painted** by a post-process. The
plug-in moves the layers at different speeds while zooming (parallax) and animates the
fog, embers and the moon's glow on top.

```
scenes.py   one function per scene: geometry, lights, fog, anchors (hotspots, landmarks, lights)
blib.py     the builders: jagged ridges, crags, gothic spires and castles, cathedral, arches,
            walls with arched openings, vaulted halls, cave tunnels, rocks, dead trees,
            torches, braziers, candles, bars, chains, gold, chests, banners, the throne,
            the horned lord; materials (stone, lava cracks, emission, metal, volumetric fog)
render.py   renders one scene's layers with Cycles (CPU) to multi-layer EXR + anchors.json
paint.py    denoise (OIDN), haze by layer, bloom, tone map, colour grade, generalised
            Kuwahara (oil paint), brush strokes along the structure, paint dabs, grain and
            speckle; paints the sky and moon layer; writes the finished layers
build.py    all of the above for a list of scenes (default: the prototype), then writes
            Source/UI/PaintedManifest.inc
```

## Running it

Blender runs as the `bpy` Python module (the same Blender, without its window):

```bash
python3 -m venv /opt/bvenv
/opt/bvenv/bin/pip install bpy==5.2.3 numpy scipy pillow OpenEXR
/opt/bvenv/bin/python tools/painted/build.py                      # the prototype scenes
/opt/bvenv/bin/python tools/painted/build.py castle/chapel        # one scene
/opt/bvenv/bin/python tools/painted/build.py --manifest-only      # only the manifest
```

A quick look while working on a scene (quarter size, a few seconds):

```bash
/opt/bvenv/bin/python tools/painted/render.py castle/chapel /tmp/r --scale 0.25 --samples 12
/opt/bvenv/bin/python tools/painted/paint.py /tmp/r /tmp/a --preview /tmp/chapel.jpg
```

Full size (1920 x 1120, 48 samples) takes 2 to 10 minutes per scene on 4 CPU cores;
interiors full of fog and torches are the slow ones. Renders go to `build-art/` (ignored
by git); the finished layers to `assets/painted/<location>/<place>/` and are committed.

## The layers

| Layer | What | Format |
|---|---|---|
| `sky` | painted in paint.py: zenith to horizon mist, the moon (maria, halo), torn clouds, stars, or a glow in the depth of an interior | `sky.jpg` (opaque) |
| `far` | distant ranges (paler with distance), the outside beyond a gate | `far.jpg` + `far.png` (alpha) |
| `mid` | the subject: castle, halls, the lord, the map's land and landmarks | `mid.jpg` + `mid.png` |
| `fog` | the volumetric fog alone (half resolution) | `fog.jpg` + `fog.png` |
| `near` | foreground rocks, pillars, chains, the parapet, cloud wisps | `near.jpg` + `near.png` |

Each layer is rendered with only its own objects visible to the camera; every other
object still casts shadows and blocks light, so the lighting stays whole. Colour and
alpha are separate files (JPEG colour bleeds outwards under transparent pixels, so no
fringes). The plug-in joins them back into premultiplied images.

## Look

- Palette: about 80 % near-black and charcoal; slate, purple and grey mists for depth
  (each farther range a little paler, haze per layer); crimson only for the moon, lava,
  embers, windows and the rim lights. The grade desaturates every hue except red and
  keeps red crimson rather than salmon.
- Painting: generalised Kuwahara (8 sectors), larger in the distance; brush streaks by
  line-integral convolution along the image's structure (sweeping, near horizontal in
  sky and fog); low-frequency paint dabs; grain and speckle; sharper near layers.

## Drop-in art

See [docs/ART_SOURCES.md](../../docs/ART_SOURCES.md): a flat PNG or a folder of layers
replaces any scene without rebuilding.
