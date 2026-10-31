#pragma once
// Engraved motifs shared by the map and the scenes: mountains, trees, towers, arches,
// torches... Each draws fields (tone + hatch direction) and ink outlines into a Plate.
#include "Engraving.h"
#include <cmath>
#include <initializer_list>

namespace dc::art::motif {
using P = juce::Point<float>;

inline juce::Path poly(std::initializer_list<P> points, bool closed = true) {
    juce::Path p;
    bool first = true;
    for (auto pt : points) { if (first) p.startNewSubPath(pt); else p.lineTo(pt); first = false; }
    if (closed) p.closeSubPath();
    return p;
}

// A hand-drawn line: a polyline with small deterministic wobbles.
inline juce::Path wobbly(P a, P b, float amount, std::uint32_t seed, int segments = 12) {
    juce::Path p;
    p.startNewSubPath(a);
    const P d = b - a;
    const P n { -d.y, d.x };
    const float len = std::max(1.0f, a.getDistanceFrom(b));
    for (int i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float off = i == segments ? 0.0f : (noise2(t * 3.0f, static_cast<float>(seed) * 0.37f, seed) - 0.5f) * amount;
        p.lineTo(a + d * t + n * (off / len));
    }
    return p;
}

// Background: a full-plate tone gradient (sky or wall) with one hatch direction.
inline void backdrop(Plate& plate, float top, float bottom, float angle) {
    juce::Path all; all.addRectangle(0, 0, virtualWidth, virtualHeight);
    plate.fillGradient(all, top, { 480, 0 }, bottom, { 480, virtualHeight }, angle);
}

// A rocky mountain: lit left flank, shadowed right flank, a jagged outline.
inline void mountain(Plate& plate, float x, float base, float width, float height, std::uint32_t seed, bool snow = false, float light = 0.82f) {
    const P peak { x + width * (0.42f + 0.16f * noise2(static_cast<float>(seed), 1.0f, seed)), base - height };
    const P left { x, base }, right { x + width, base };
    const P mid { peak.x + width * 0.05f, base };
    plate.fill(poly({ left, peak, mid }), light, 62.0f);
    plate.fill(poly({ mid, peak, right }), 0.32f, 118.0f);
    // Ridges down the shadow side.
    for (int k = 1; k <= 3; ++k) {
        const float t = static_cast<float>(k) / 4.0f;
        plate.stroke(wobbly(peak, { mid.x + (right.x - mid.x) * t * 0.7f, base - height * 0.15f * t }, 6.0f, seed + static_cast<std::uint32_t>(k), 6), 0.6f, 0.7f);
    }
    if (snow) {
        const float sh = height * 0.28f;
        plate.fill(poly({ peak, { peak.x - width * 0.12f, peak.y + sh }, { peak.x - width * 0.04f, peak.y + sh * 0.8f },
                          { peak.x + width * 0.03f, peak.y + sh * 1.05f }, { peak.x + width * 0.1f, peak.y + sh * 0.7f } }), 0.97f, 62.0f);
    }
    juce::Path outline = wobbly(left, peak, 5.0f, seed, 10);
    outline.addPath(wobbly(peak, right, 5.0f, seed + 9, 10));
    plate.stroke(outline, 1.1f);
}

// A conifer: stacked triangles, dark, with a lit edge.
inline void conifer(Plate& plate, float x, float base, float height, float tone = 0.3f) {
    const float w = height * 0.42f;
    juce::Path tree;
    for (int k = 0; k < 3; ++k) {
        const float top = base - height + static_cast<float>(k) * height * 0.22f;
        const float bottom = top + height * 0.45f;
        const float half = w * (0.45f + 0.25f * static_cast<float>(k));
        tree.addPath(poly({ { x, top }, { x + half, bottom }, { x - half, bottom } }));
    }
    plate.fill(tree, tone, 75.0f);
    plate.stroke(tree, 0.6f, 0.85f);
    plate.line(x, base - height * 0.1f, x, base, 1.2f);
}

// A round, gnarled deciduous tree.
inline void oak(Plate& plate, float x, float base, float height, std::uint32_t seed, float tone = 0.45f) {
    juce::Path crown;
    for (int k = 0; k < 5; ++k) {
        const float a = static_cast<float>(k) * 1.257f + noise2(static_cast<float>(seed), static_cast<float>(k), seed) * 0.6f;
        const float r = height * (0.2f + 0.08f * noise2(static_cast<float>(k), static_cast<float>(seed), seed + 1));
        crown.addEllipse(x + std::cos(a) * height * 0.16f - r, base - height * 0.62f + std::sin(a) * height * 0.12f - r, 2 * r, 2 * r);
    }
    plate.fill(crown, tone, 30.0f);
    plate.stroke(crown, 0.7f, 0.8f);
    juce::Path trunk = poly({ { x - height * 0.05f, base }, { x - height * 0.025f, base - height * 0.45f }, { x + height * 0.025f, base - height * 0.45f }, { x + height * 0.06f, base } });
    plate.fill(trunk, 0.2f, 90.0f);
    plate.stroke(trunk, 0.7f);
}

// A dead, twisted tree (haunted places): bare branches as ink.
inline void deadTree(Plate& plate, float x, float base, float height, std::uint32_t seed) {
    const auto branch = [&](auto&& self, P from, float angle, float length, float thickness, int depth) -> void {
        if (depth == 0 || length < 3.0f) return;
        const P to { from.x + std::cos(angle) * length, from.y - std::sin(angle) * length };
        plate.stroke(wobbly(from, to, length * 0.12f, seed + static_cast<std::uint32_t>(depth * 31 + static_cast<int>(length)), 5), thickness);
        const float spread = 0.45f + 0.25f * noise2(from.x * 0.1f, from.y * 0.1f, seed);
        self(self, to, angle + spread, length * 0.68f, thickness * 0.68f, depth - 1);
        self(self, to, angle - spread * 0.9f, length * 0.62f, thickness * 0.66f, depth - 1);
    };
    branch(branch, { x, base }, 1.5708f + (noise2(x, base, seed) - 0.5f) * 0.3f, height * 0.42f, height * 0.06f, 5);
}

// A stone tower with a conical or crenellated top.
inline void tower(Plate& plate, float x, float base, float width, float height, bool conical, float tone = 0.62f) {
    const float l = x - width * 0.5f, r = x + width * 0.5f, top = base - height;
    juce::Path body = poly({ { l, base }, { l + width * 0.04f, top }, { r - width * 0.04f, top }, { r, base } });
    juce::Path light = poly({ { l, base }, { l + width * 0.04f, top }, { x, top }, { x, base } });
    juce::Path shade = poly({ { x, top }, { r - width * 0.04f, top }, { r, base }, { x, base } });
    plate.fill(light, tone, 90.0f);
    plate.fill(shade, tone * 0.45f, 90.0f);
    plate.stroke(body, 0.9f);
    if (conical) {
        juce::Path roof = poly({ { l - width * 0.12f, top }, { x, top - width * 1.3f }, { r + width * 0.12f, top } });
        plate.fill(roof, 0.25f, 100.0f);
        plate.fill(poly({ { l - width * 0.12f, top }, { x, top - width * 1.3f }, { x, top } }), 0.5f, 70.0f);
        plate.stroke(roof, 0.9f);
    } else {
        const int merlons = 3;
        for (int k = 0; k < merlons; ++k) {
            const float mw = (width * 1.1f) / (2 * merlons - 1);
            const float mx = l - width * 0.05f + static_cast<float>(2 * k) * mw;
            juce::Path m; m.addRectangle(mx, top - mw * 0.9f, mw, mw * 0.9f);
            plate.fill(m, tone * 0.8f, 90.0f);
            plate.stroke(m, 0.7f);
        }
    }
    // Arrow slits.
    for (int k = 1; k <= 2; ++k) {
        juce::Path slit; slit.addRectangle(x - width * 0.04f, top + height * 0.25f * static_cast<float>(k), width * 0.08f, height * 0.12f);
        plate.fill(slit, 0.05f, 0.0f);
    }
}

// A pennant on a pole.
inline void flag(Plate& plate, float x, float y, float size, float accent = 0.0f) {
    plate.line(x, y, x, y - size, 0.8f);
    juce::Path f = poly({ { x, y - size }, { x + size * 0.8f, y - size * 0.82f }, { x, y - size * 0.62f } });
    plate.fill(f, 0.35f, 20.0f, accent);
    plate.stroke(f, 0.6f);
}

// A gothic pointed arch opening (door, window, vault).
inline juce::Path pointedArch(float x, float base, float width, float height) {
    juce::Path a;
    const float l = x - width * 0.5f, r = x + width * 0.5f, spring = base - height + width * 0.6f;
    a.startNewSubPath(l, base);
    a.lineTo(l, spring);
    a.quadraticTo(l, base - height + width * 0.1f, x, base - height);
    a.quadraticTo(r, base - height + width * 0.1f, r, spring);
    a.lineTo(r, base);
    a.closeSubPath();
    return a;
}
inline juce::Path roundArch(float x, float base, float width, float height) {
    juce::Path a;
    const float l = x - width * 0.5f, r = x + width * 0.5f, spring = base - height + width * 0.5f;
    a.startNewSubPath(l, base);
    a.lineTo(l, spring);
    a.addCentredArc(x, spring, width * 0.5f, width * 0.5f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
    a.lineTo(r, base);
    a.closeSubPath();
    return a;
}

// A wall-mounted torch: bracket, flame (red accent) and a glow.
inline void torch(Plate& plate, float x, float y, float size = 1.0f) {
    plate.line(x, y + 14 * size, x, y, 2.2f * size);
    plate.line(x - 5 * size, y + 4 * size, x + 5 * size, y + 4 * size, 1.4f * size);
    juce::Path flame;
    flame.startNewSubPath(x, y - 16 * size);
    flame.quadraticTo(x + 7 * size, y - 6 * size, x + 3 * size, y);
    flame.lineTo(x - 3 * size, y);
    flame.quadraticTo(x - 7 * size, y - 6 * size, x, y - 16 * size);
    plate.fill(flame, 0.75f, 90.0f, 1.0f);
    plate.strokeAccent(flame, 0.9f * size);
    plate.glow({ x, y - 6 * size }, 70.0f * size, 0.35f);
}

// A candle: a short pillar with a small flame.
inline void candle(Plate& plate, float x, float base, float height = 14.0f) {
    juce::Path c; c.addRectangle(x - 2.2f, base - height, 4.4f, height);
    plate.fill(c, 0.85f, 90.0f);
    plate.stroke(c, 0.5f);
    juce::Path flame; flame.addEllipse(x - 1.6f, base - height - 6.5f, 3.2f, 6.0f);
    plate.fillAccent(flame);
    plate.glow({ x, base - height - 3.0f }, 26.0f, 0.3f);
}

// Stone block courses on a wall area: horizontal joints with staggered verticals.
inline void masonry(Plate& plate, juce::Rectangle<float> area, float course, float alpha = 0.55f) {
    int row = 0;
    for (float y = area.getY() + course; y < area.getBottom(); y += course, ++row) {
        plate.line(area.getX(), y, area.getRight(), y, 0.5f, alpha);
        const float offset = (row % 2) * course;
        for (float x = area.getX() + offset; x < area.getRight(); x += course * 2.0f)
            plate.line(x, y - course, x, y, 0.45f, alpha * 0.85f);
    }
}

// Floor with perspective: lines converging to a vanishing point.
inline void perspectiveFloor(Plate& plate, float horizon, P vanish, float tone, int rays = 14, float alpha = 0.5f) {
    juce::Path floor; floor.addRectangle(0, horizon, virtualWidth, virtualHeight - horizon);
    plate.fillGradient(floor, tone, { 480, horizon }, tone * 0.55f, { 480, virtualHeight }, 0.0f);
    for (int k = 0; k <= rays; ++k) {
        const float x = -400.0f + static_cast<float>(k) * (virtualWidth + 800.0f) / static_cast<float>(rays);
        plate.line(vanish.x, vanish.y, x, virtualHeight, 0.5f, alpha);
    }
    for (float t = 0.08f; t < 1.0f; t *= 1.45f) {
        const float y = horizon + (virtualHeight - horizon) * t;
        plate.line(0, y, virtualWidth, y, 0.5f, alpha * 0.8f);
    }
}

// A stone column (cylinder shading: lit left, dark right).
inline void column(Plate& plate, float x, float top, float base, float width) {
    const float l = x - width * 0.5f, r = x + width * 0.5f;
    plate.fill(poly({ { l, top }, { x - width * 0.1f, top }, { x - width * 0.1f, base }, { l, base } }), 0.78f, 90.0f);
    plate.fill(poly({ { x - width * 0.1f, top }, { r, top }, { r, base }, { x - width * 0.1f, base } }), 0.3f, 90.0f);
    juce::Path c; c.addRectangle(l, top, width, base - top);
    plate.stroke(c, 0.8f);
    juce::Path cap; cap.addRectangle(l - width * 0.18f, top - width * 0.3f, width * 1.36f, width * 0.3f);
    juce::Path foot; foot.addRectangle(l - width * 0.2f, base - width * 0.3f, width * 1.4f, width * 0.3f);
    plate.fill(cap, 0.55f, 0.0f); plate.stroke(cap, 0.7f);
    plate.fill(foot, 0.5f, 0.0f); plate.stroke(foot, 0.7f);
}

// Rough rock mass from a list of points (lit top, darker body).
inline void rock(Plate& plate, const juce::Path& shape, float tone, float angle, std::uint32_t) {
    plate.fill(shape, tone, angle);
    plate.stroke(shape, 1.0f);
}

// Ground fog: soft light bands (lifts the tone).
inline void fogBand(Plate& plate, float y, float height, float tone) {
    juce::Path band; band.addRectangle(0, y, virtualWidth, height);
    plate.fillGradient(band, tone, { 0, y }, tone * 0.98f, { 0, y + height }, 0.0f);
}

// Stars: tiny crosses in ink on a dark sky (tone handles the sky).
inline void stars(Plate& plate, juce::Rectangle<float> area, int count, std::uint32_t seed) {
    for (int k = 0; k < count; ++k) {
        const float x = area.getX() + area.getWidth() * noise2(static_cast<float>(k) * 7.3f, 0.5f, seed);
        const float y = area.getY() + area.getHeight() * noise2(0.5f, static_cast<float>(k) * 5.1f, seed + 1);
        const float s = 1.0f + 2.5f * noise2(static_cast<float>(k), static_cast<float>(k), seed + 2);
        juce::Path star; star.addEllipse(x - s * 0.5f, y - s * 0.5f, s, s);
        plate.fill(star, 1.0f, 0.0f);
        if (s > 2.6f) plate.light({ x, y }, s * 4.0f, 2);
    }
}
}
