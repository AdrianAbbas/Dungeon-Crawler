#pragma once
// The painted art (visual overhaul): scenes rendered in Blender as depth layers (sky,
// far, mid, fog, near) and painted (tools/painted). The layers ship inside the plug-in
// bundle (Contents/Resources/Painted/<scene>/); the manifest of their anchors
// (hotspots, landmarks, lights) is compiled in (PaintedManifest.inc).
//
// At run time a scene becomes four planes at the window's pixel size: `back` (sky and
// far, opaque), `mid`, `fog` (half resolution, it is soft) and `near` (cropped to what
// it covers), plus `flat`, all of them composited at rest, for blits and transitions.
// The editor moves the planes at different speeds while zooming (parallax) and drifts
// the fog. Places without painted art yet keep the engraved illustration (Scenes.h).
#include "Scenes.h"

namespace dc::art {
struct PaintedAnchor { int kind; float x, y, value; };
struct PaintedScene {
    const char* slug;        // "map", "dungeon/corridors"...
    const char* layers;      // "sky|far|mid|fog|near"
    float moon[3];           // virtual x, y, radius (radius 0: no moon)
    const PaintedAnchor* anchors;
    int anchorCount;
};
enum AnchorKind { anchorHotspot = 0, anchorLandmark, anchorFire, anchorWindow, anchorLava, anchorCandle, anchorGlint, anchorEyes };

// The compiled-in manifest entry of a scene, or null when it has no painted art.
const PaintedScene* paintedScene(SceneId);
// Where the painted layers are: the bundle's Resources/Painted, else the source tree's
// assets/painted (development and tests). Invalid when neither exists.
juce::File paintedFolder();
// Whether the scene will be shown painted (manifest entry and its files present).
bool isPainted(SceneId);

struct Planes {
    juce::Image back, mid, fog, near, flat;
    juce::Rectangle<int> nearArea;   // where `near` sits in the frame (pixels)
    juce::Rectangle<int> fogArea;    // the fog's bounds at full resolution (pixels)
    juce::Point<float> moon; float moonRadius = 0;   // virtual coordinates
    bool layered() const noexcept { return mid.isValid() || fog.isValid() || near.isValid(); }
    std::size_t bytes() const noexcept;
};

// Loads and composites a painted scene at a pixel size (pure computation: called on the
// cache's thread). Drop-in art (overrideFile) wins: a flat PNG becomes `back`; a folder
// <scenes>/<slug>/ with any of back/sky, far, mid, fog, near .png gives layers.
Planes loadPainted(SceneId, int widthPixels, int heightPixels, const std::atomic<bool>* abort = nullptr);
// Composites planes into one image (fog offset in pixels, near offset for parallax).
juce::Image composite(const Planes&, int width, int height);

// The painted scene's light sources (fires, windows, lava, candles, glints, eyes) as
// LightSource kinds for the editor's life: 0 flame, 1 candle, 2 glint, 4 window, 5 lava
// (ember source), 6 eyes.
std::vector<LightSource> paintedLights(SceneId);
// A tileable mist texture (procedural, generated once): the wisps that drift across the
// painted scenes' low ground.
const juce::Image& mistTexture();
}
