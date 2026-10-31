#include "Scenes.h"
#include "Motifs.h"

// Milestone 8: an engraved illustration for every place (40), in the map's style.
// Each scene is a composition of motifs over one of three set-ups: an interior in
// one-point perspective, an exterior under an engraved sky, or a cave framed by rock.
// Lighting rule: lit stone stays in the mid tones (0.45-0.8) so the hatching reads;
// only deep shadows go below 0.2.
namespace dc::art {
using namespace motif;

namespace {
constexpr float W = virtualWidth, H = virtualHeight;
constexpr float pi = juce::MathConstants<float>::pi;

juce::Path rect(float x, float y, float w, float h) { juce::Path p; p.addRectangle(x, y, w, h); return p; }
juce::Path ellipse(float x, float y, float w, float h) { juce::Path p; p.addEllipse(x, y, w, h); return p; }

// --- Set-ups --------------------------------------------------------------------
// An interior in one-point perspective: the back wall `back`, side walls, floor and
// ceiling converge on it.
void interior(Plate& plate, juce::Rectangle<float> back, float wall, float floorTone, float ceiling, bool vault = false) {
    const P tl { 0, 0 }, tr { W, 0 }, bl { 0, H }, br { W, H };
    const P btl = back.getTopLeft(), btr = back.getTopRight(), bbl = back.getBottomLeft(), bbr = back.getBottomRight();
    plate.fillGradient(poly({ tl, btl, bbl, bl }), wall * 0.75f, { 0, H * 0.5f }, wall, { back.getX(), H * 0.5f }, 35.0f);
    plate.fillGradient(poly({ tr, btr, bbr, br }), wall * 0.6f, { W, H * 0.5f }, wall * 0.9f, { back.getRight(), H * 0.5f }, 145.0f);
    plate.fillGradient(poly({ bl, bbl, bbr, br }), floorTone, { W * 0.5f, back.getBottom() }, floorTone * 0.55f, { W * 0.5f, H }, 0.0f);
    plate.fillGradient(poly({ tl, tr, btr, btl }), ceiling * 0.6f, { W * 0.5f, 0 }, ceiling, { W * 0.5f, back.getY() }, 0.0f);
    plate.fill(rect(back.getX(), back.getY(), back.getWidth(), back.getHeight()), wall * 1.05f, 90.0f);
    for (auto [a, b] : { std::pair<P, P> { tl, btl }, { tr, btr }, { bl, bbl }, { br, bbr } }) plate.line(a.x, a.y, b.x, b.y, 1.0f);
    plate.stroke(rect(back.getX(), back.getY(), back.getWidth(), back.getHeight()), 1.0f);
    // Floor joints converging on the back wall.
    for (int k = 1; k < 9; ++k) {
        const float t = static_cast<float>(k) / 9.0f;
        plate.line(back.getX() + back.getWidth() * t, back.getBottom(), -W * 0.6f + W * 2.2f * t, H, 0.45f, 0.5f);
    }
    for (float t = 0.12f; t < 1.0f; t *= 1.55f) {
        const float y = back.getBottom() + (H - back.getBottom()) * t;
        plate.line(0, y, W, y, 0.45f, 0.45f);
    }
    if (vault) {
        // Ribs of a barrel vault over the receding ceiling.
        for (int k = 0; k < 4; ++k) {
            const float t = static_cast<float>(k) / 4.0f;
            const float l = back.getX() * t, r = W - (W - back.getRight()) * t, top = back.getY() * t;
            juce::Path rib; rib.startNewSubPath(l, top + (back.getY() - top) * 0.9f + 30.0f);
            rib.quadraticTo((l + r) * 0.5f, top - 10.0f, r, top + (back.getY() - top) * 0.9f + 30.0f);
            plate.stroke(rib, 1.2f, 0.8f);
        }
    }
}

// An exterior: an engraved sky (horizontal lines, lighter towards the horizon) and
// the ground below it.
void exterior(Plate& plate, float horizon, float skyTop, float skyLow, float ground, float groundFar) {
    plate.fillGradient(rect(0, 0, W, horizon + 2), skyTop, { 0, 0 }, skyLow, { 0, horizon }, 0.0f);
    plate.fillGradient(rect(0, horizon, W, H - horizon), groundFar, { 0, horizon }, ground, { 0, H }, 8.0f);
}

void clouds(Plate& plate, float y, std::uint32_t seed, float tone = 0.86f) {
    for (int k = 0; k < 4; ++k) {
        const float x = 60.0f + static_cast<float>(k) * 240.0f + noise2(static_cast<float>(k), 1.0f, seed) * 80.0f;
        const float yy = y + noise2(1.0f, static_cast<float>(k), seed) * 40.0f;
        juce::Path c;
        for (int b = 0; b < 4; ++b) c.addEllipse(x + static_cast<float>(b) * 34.0f, yy - (b % 2) * 12.0f, 70.0f, 30.0f);
        plate.fill(c, tone, 0.0f);
        plate.stroke(c, 0.6f, 0.5f);
    }
}

void moon(Plate& plate, float x, float y, float r) {
    plate.fill(ellipse(x - r, y - r, 2 * r, 2 * r), 0.97f, 0.0f);
    plate.stroke(ellipse(x - r, y - r, 2 * r, 2 * r), 0.8f);
    plate.fill(ellipse(x - r * 0.3f, y - r * 0.5f, r * 0.5f, r * 0.4f), 0.85f, 30.0f);
    plate.light({ x, y }, r * 3.0f, 2);
}

// A frame of rock around the picture (caves): dark, rough edge.
void rockFrame(Plate& plate, float inset, std::uint32_t seed) {
    juce::Path frame; frame.addRectangle(-10, -10, W + 20, H + 20);
    juce::Path hole;
    const int n = 36;
    for (int k = 0; k < n; ++k) {
        const float a = 2.0f * pi * static_cast<float>(k) / static_cast<float>(n);
        const float rx = (W * 0.5f - inset) * (0.92f + 0.12f * noise2(static_cast<float>(k) * 0.7f, 0.3f, seed));
        const float ry = (H * 0.55f - inset * 0.6f) * (0.9f + 0.14f * noise2(0.3f, static_cast<float>(k) * 0.7f, seed + 1));
        const P pt { W * 0.5f + std::cos(a) * rx, H * 0.52f + std::sin(a) * ry };
        if (k == 0) hole.startNewSubPath(pt); else hole.lineTo(pt);
    }
    hole.closeSubPath();
    frame.addPath(hole);
    frame.setUsingNonZeroWinding(false);
    plate.fill(frame, 0.16f, 60.0f);
    plate.stroke(hole, 1.6f);
}

void stalactites(Plate& plate, float y, int count, std::uint32_t seed, float tone = 0.35f) {
    for (int k = 0; k < count; ++k) {
        const float x = W * noise2(static_cast<float>(k) * 3.1f, 0.2f, seed);
        const float len = 20.0f + 60.0f * noise2(0.7f, static_cast<float>(k) * 2.3f, seed + 1);
        const float w = 6.0f + 10.0f * noise2(static_cast<float>(k), 5.0f, seed + 2);
        juce::Path s = poly({ { x - w, y }, { x + w, y }, { x + w * 0.1f, y + len } });
        plate.fill(s, tone, 80.0f);
        plate.stroke(s, 0.7f);
    }
}

// --- Props ----------------------------------------------------------------------
void bars(Plate& plate, juce::Rectangle<float> r, int count) {
    for (int k = 0; k <= count; ++k) {
        const float x = r.getX() + r.getWidth() * static_cast<float>(k) / static_cast<float>(count);
        juce::Path bar = rect(x - 2.2f, r.getY(), 4.4f, r.getHeight());
        plate.fill(bar, 0.2f, 90.0f);
        plate.stroke(bar, 0.6f);
    }
    for (float y : { r.getY() + 6.0f, r.getBottom() - 10.0f, r.getCentreY() }) {
        juce::Path rail = rect(r.getX() - 4, y, r.getWidth() + 8, 5.0f);
        plate.fill(rail, 0.25f, 0.0f);
        plate.stroke(rail, 0.6f);
    }
}

void chest(Plate& plate, float x, float base, float s, bool open) {
    juce::Path body = rect(x - 30 * s, base - 26 * s, 60 * s, 26 * s);
    plate.fill(body, 0.45f, 90.0f);
    plate.stroke(body, 0.9f);
    for (float bx : { x - 22 * s, x + 18 * s }) plate.fill(rect(bx, base - 26 * s, 4 * s, 26 * s), 0.25f, 90.0f);
    if (open) {
        juce::Path lid = poly({ { x - 30 * s, base - 26 * s }, { x - 26 * s, base - 50 * s }, { x + 34 * s, base - 50 * s }, { x + 30 * s, base - 26 * s } });
        plate.fill(lid, 0.3f, 70.0f);
        plate.stroke(lid, 0.9f);
        for (int k = 0; k < 7; ++k) {
            const float cx = x - 22 * s + static_cast<float>(k) * 7.5f * s;
            plate.fill(ellipse(cx - 4 * s, base - 31 * s - (k % 2) * 3 * s, 8 * s, 6 * s), 0.9f, 0.0f);
            plate.stroke(ellipse(cx - 4 * s, base - 31 * s - (k % 2) * 3 * s, 8 * s, 6 * s), 0.4f);
        }
    } else {
        juce::Path lid; lid.startNewSubPath(x - 30 * s, base - 26 * s); lid.quadraticTo(x, base - 44 * s, x + 30 * s, base - 26 * s); lid.closeSubPath();
        plate.fill(lid, 0.35f, 90.0f);
        plate.stroke(lid, 0.9f);
    }
}

void goldPile(Plate& plate, float x, float base, float width, float height, std::uint32_t seed) {
    juce::Path mound; mound.startNewSubPath(x - width * 0.5f, base); mound.quadraticTo(x, base - height * 2.0f, x + width * 0.5f, base); mound.closeSubPath();
    plate.fillGradient(mound, 0.92f, { x - width * 0.2f, base - height }, 0.55f, { x + width * 0.4f, base }, 20.0f);
    plate.stroke(mound, 0.9f);
    for (int k = 0; k < static_cast<int>(width * height / 220.0f); ++k) {
        const float u = noise2(static_cast<float>(k) * 1.3f, 0.1f, seed), v = noise2(0.1f, static_cast<float>(k) * 1.7f, seed + 1);
        const float px = x - width * 0.45f + width * 0.9f * u;
        const float top = base - height * (1.0f - std::pow(std::abs(px - x) / (width * 0.5f), 2.0f)) * 0.95f;
        const float py = top + (base - top) * v;
        plate.stroke(ellipse(px - 4, py - 2, 8, 4), 0.5f, 0.8f);
        if (k % 9 == 0) plate.light({ px, py }, 10.0f, 2);
    }
}

void banner(Plate& plate, float x, float top, float w, float h, float accent) {
    juce::Path b = poly({ { x - w * 0.5f, top }, { x + w * 0.5f, top }, { x + w * 0.5f, top + h }, { x, top + h - w * 0.4f }, { x - w * 0.5f, top + h } });
    plate.fill(b, 0.4f, 90.0f, accent);
    plate.stroke(b, 0.9f);
    plate.line(x - w * 0.6f, top, x + w * 0.6f, top, 2.0f);
    // A simple heraldic device.
    plate.fill(ellipse(x - w * 0.18f, top + h * 0.3f, w * 0.36f, w * 0.36f), 0.85f, 0.0f);
}

void window(Plate& plate, float x, float base, float w, float h, bool pointed, float light = 0.92f) {
    const auto shape = pointed ? pointedArch(x, base, w, h) : roundArch(x, base, w, h);
    plate.fill(shape, light, 0.0f);
    plate.stroke(shape, 1.2f);
    plate.line(x, base - h + (pointed ? 0.0f : w * 0.5f), x, base, 0.8f);
    plate.line(x - w * 0.5f, base - h * 0.45f, x + w * 0.5f, base - h * 0.45f, 0.8f);
}

void table(Plate& plate, float x, float y, float w, float depth) {
    juce::Path top = poly({ { x - w * 0.5f, y }, { x + w * 0.5f, y }, { x + w * 0.5f + depth, y + depth * 0.6f }, { x - w * 0.5f - depth, y + depth * 0.6f } });
    plate.fill(top, 0.62f, 0.0f);
    plate.stroke(top, 0.9f);
    juce::Path front = rect(x - w * 0.5f - depth, y + depth * 0.6f, w + 2 * depth, 8);
    plate.fill(front, 0.3f, 0.0f);
    plate.stroke(front, 0.8f);
    for (float lx : { x - w * 0.5f - depth + 6, x + w * 0.5f + depth - 10 }) {
        plate.fill(rect(lx, y + depth * 0.6f + 8, 5, 40), 0.25f, 90.0f);
        plate.stroke(rect(lx, y + depth * 0.6f + 8, 5, 40), 0.6f);
    }
}

void barrel(Plate& plate, float x, float base, float s) {
    juce::Path b; b.startNewSubPath(x - 18 * s, base); b.quadraticTo(x - 24 * s, base - 22 * s, x - 18 * s, base - 44 * s);
    b.lineTo(x + 18 * s, base - 44 * s); b.quadraticTo(x + 24 * s, base - 22 * s, x + 18 * s, base); b.closeSubPath();
    plate.fillGradient(b, 0.7f, { x - 14 * s, base }, 0.25f, { x + 22 * s, base }, 90.0f);
    plate.stroke(b, 0.9f);
    for (float f : { 0.2f, 0.8f }) plate.line(x - 21 * s, base - 44 * s * f, x + 21 * s, base - 44 * s * f, 1.6f);
}

void bookshelf(Plate& plate, juce::Rectangle<float> r, std::uint32_t seed) {
    plate.fill(rect(r.getX(), r.getY(), r.getWidth(), r.getHeight()), 0.3f, 90.0f);
    plate.stroke(rect(r.getX(), r.getY(), r.getWidth(), r.getHeight()), 1.0f);
    const int shelves = std::max(2, static_cast<int>(r.getHeight() / 48.0f));
    for (int s = 0; s < shelves; ++s) {
        const float y = r.getY() + r.getHeight() * static_cast<float>(s + 1) / static_cast<float>(shelves);
        plate.line(r.getX(), y, r.getRight(), y, 2.0f);
        float x = r.getX() + 3;
        int k = 0;
        while (x < r.getRight() - 8) {
            const float bw = 5.0f + 6.0f * noise2(x * 0.3f, static_cast<float>(s), seed);
            const float bh = (r.getHeight() / static_cast<float>(shelves)) * (0.6f + 0.3f * noise2(static_cast<float>(s), x * 0.2f, seed + 1));
            juce::Path book = rect(x, y - bh, bw, bh - 2);
            plate.fill(book, 0.45f + 0.4f * noise2(x, static_cast<float>(k), seed + 2), 90.0f);
            plate.stroke(book, 0.5f);
            x += bw + 1.0f;
            ++k;
        }
    }
}

void flask(Plate& plate, float x, float base, float s, float tone) {
    juce::Path f;
    f.addEllipse(x - 12 * s, base - 22 * s, 24 * s, 22 * s);
    f.addRectangle(x - 3.5f * s, base - 38 * s, 7 * s, 18 * s);
    plate.fill(f, tone, 30.0f);
    plate.stroke(f, 0.8f);
    plate.fill(ellipse(x - 6 * s, base - 17 * s, 5 * s, 5 * s), 0.98f, 0.0f);
}

void cauldron(Plate& plate, float x, float base, float s, bool fire) {
    juce::Path pot; pot.addEllipse(x - 40 * s, base - 50 * s, 80 * s, 52 * s);
    plate.fillGradient(pot, 0.5f, { x - 20 * s, base - 40 * s }, 0.1f, { x + 30 * s, base }, 100.0f);
    plate.stroke(pot, 1.1f);
    plate.fill(ellipse(x - 36 * s, base - 54 * s, 72 * s, 14 * s), 0.75f, 0.0f, 0.0f);
    plate.stroke(ellipse(x - 36 * s, base - 54 * s, 72 * s, 14 * s), 1.0f);
    for (int k = 0; k < 4; ++k) {
        const float bx = x - 18 * s + static_cast<float>(k) * 12 * s;
        plate.stroke(ellipse(bx, base - 64 * s - static_cast<float>(k % 2) * 10 * s, 8 * s, 8 * s), 0.7f);
        plate.light({ bx + 4 * s, base - 60 * s }, 8 * s, 2);
    }
    if (fire) {
        juce::Path flames;
        for (int k = 0; k < 5; ++k) {
            const float fx = x - 28 * s + static_cast<float>(k) * 14 * s;
            flames.addPath(poly({ { fx - 6 * s, base + 6 * s }, { fx, base - 10 * s }, { fx + 6 * s, base + 6 * s } }));
        }
        plate.fill(flames, 0.8f, 90.0f, 1.0f);
        plate.glow({ x, base }, 90 * s, 0.4f);
    }
}

void stairsUp(Plate& plate, float x, float base, float w, int steps, float rise, float shift) {
    for (int k = 0; k < steps; ++k) {
        const float y = base - static_cast<float>(k) * rise;
        const float l = x - w * 0.5f + static_cast<float>(k) * shift, r = l + w - static_cast<float>(k) * shift * 2.0f;
        juce::Path tread = poly({ { l, y }, { r, y }, { r - shift, y - rise }, { l + shift, y - rise } });
        plate.fill(tread, 0.72f - 0.03f * static_cast<float>(k), 0.0f);
        plate.stroke(tread, 0.7f);
        plate.fill(rect(l, y, r - l, rise * 0.45f), 0.35f, 90.0f);
    }
}

void skull(Plate& plate, float x, float y, float s) {
    plate.fill(ellipse(x - 8 * s, y - 9 * s, 16 * s, 14 * s), 0.9f, 0.0f);
    plate.fill(rect(x - 5 * s, y + 2 * s, 10 * s, 6 * s), 0.85f, 0.0f);
    plate.stroke(ellipse(x - 8 * s, y - 9 * s, 16 * s, 14 * s), 0.7f);
    plate.fill(ellipse(x - 5 * s, y - 4 * s, 4 * s, 4 * s), 0.05f, 0.0f);
    plate.fill(ellipse(x + 1 * s, y - 4 * s, 4 * s, 4 * s), 0.05f, 0.0f);
}

void crow(Plate& plate, float x, float y, float s) {
    juce::Path c;
    c.startNewSubPath(x - 14 * s, y - 4 * s);
    c.quadraticTo(x - 6 * s, y - 8 * s, x, y);
    c.quadraticTo(x + 6 * s, y - 8 * s, x + 14 * s, y - 4 * s);
    c.quadraticTo(x + 6 * s, y - 3 * s, x, y + 2 * s);
    c.quadraticTo(x - 6 * s, y - 3 * s, x - 14 * s, y - 4 * s);
    plate.fillInk(c);
}

void tombstone(Plate& plate, float x, float base, float s, int kind) {
    if (kind == 0) {
        plate.fill(rect(x - 2.5f * s, base - 30 * s, 5 * s, 30 * s), 0.6f, 90.0f);
        plate.fill(rect(x - 11 * s, base - 24 * s, 22 * s, 5 * s), 0.6f, 0.0f);
        plate.stroke(rect(x - 2.5f * s, base - 30 * s, 5 * s, 30 * s), 0.7f);
        plate.stroke(rect(x - 11 * s, base - 24 * s, 22 * s, 5 * s), 0.7f);
        return;
    }
    const auto t = roundArch(x, base, 24 * s, 34 * s);
    plate.fillGradient(t, 0.75f, { x - 10 * s, base - 30 * s }, 0.35f, { x + 12 * s, base }, 90.0f);
    plate.stroke(t, 0.9f);
    plate.line(x - 6 * s, base - 20 * s, x + 6 * s, base - 20 * s, 0.6f, 0.7f);
}

void sarcophagus(Plate& plate, float x, float base, float s) {
    juce::Path body = poly({ { x - 70 * s, base }, { x + 70 * s, base }, { x + 60 * s, base - 34 * s }, { x - 60 * s, base - 34 * s } });
    plate.fill(body, 0.55f, 90.0f);
    plate.stroke(body, 1.0f);
    juce::Path lid = poly({ { x - 64 * s, base - 34 * s }, { x + 64 * s, base - 34 * s }, { x + 48 * s, base - 50 * s }, { x - 48 * s, base - 50 * s } });
    plate.fill(lid, 0.8f, 0.0f);
    plate.stroke(lid, 1.0f);
    // An effigy's outline on the lid.
    plate.stroke(ellipse(x - 40 * s, base - 47 * s, 18 * s, 10 * s), 0.6f, 0.7f);
    plate.line(x - 22 * s, base - 42 * s, x + 40 * s, base - 42 * s, 0.6f, 0.6f);
}

// --- Location scenes ----------------------------------------------------------
using Draw = void (*)(Plate&);

// DUNGEON ---------------------------------------------------------------------
void dungeonCorridors(Plate& plate) {
    interior(plate, { 400, 200, 160, 150 }, 0.52f, 0.5f, 0.35f, true);
    // Receding arches along the passage.
    for (int k = 0; k < 4; ++k) {
        const float t = static_cast<float>(k) / 4.0f;
        const float w = 820.0f - 620.0f * t, base = 520.0f - 170.0f * t, h = 470.0f - 300.0f * t;
        juce::Path arch = roundArch(480, base, w, h);
        plate.stroke(arch, 2.4f - 1.4f * t);
        plate.stroke(roundArch(480, base, w - 24 * (1 - t), h - 14 * (1 - t)), 0.8f, 0.7f);
    }
    plate.fill(roundArch(480, 350, 90, 110), 0.06f, 0.0f);
    masonry(plate, { 0, 60, 230, 380 }, 22.0f);
    masonry(plate, { 730, 60, 230, 380 }, 22.0f);
    torch(plate, 160, 220, 1.2f);
    torch(plate, 800, 220, 1.2f);
    torch(plate, 395, 270, 0.6f);
    torch(plate, 565, 270, 0.6f);
    // Puddles on the floor.
    plate.fill(ellipse(300, 470, 140, 22), 0.75f, 0.0f);
    plate.fill(ellipse(560, 420, 90, 14), 0.75f, 0.0f);
    plate.light({ 370, 480 }, 30, 2);
}

void dungeonEntrance(Plate& plate) {
    interior(plate, { 250, 120, 460, 300 }, 0.6f, 0.62f, 0.4f, true);
    // The great doorway to the daylight outside.
    plate.fill(roundArch(480, 420, 170, 250), 0.97f, 0.0f);
    plate.stroke(roundArch(480, 420, 170, 250), 2.4f);
    plate.stroke(roundArch(480, 420, 200, 275), 1.0f);
    mountain(plate, 400, 420, 150, 60, 4, false, 0.9f);
    // Two columns, steps up to the door.
    column(plate, 300, 150, 470, 34);
    column(plate, 660, 150, 470, 34);
    stairsUp(plate, 480, 500, 380, 4, 20, 14);
    masonry(plate, { 250, 120, 460, 300 }, 26.0f, 0.4f);
    torch(plate, 230, 250, 1.4f);
    torch(plate, 730, 250, 1.4f);
    banner(plate, 120, 80, 60, 170, 1.0f);
    banner(plate, 840, 80, 60, 170, 1.0f);
}

void dungeonCells(Plate& plate) {
    interior(plate, { 330, 180, 300, 200 }, 0.48f, 0.55f, 0.3f);
    masonry(plate, { 330, 180, 300, 200 }, 20.0f);
    // A high barred window: a shaft of light falls in.
    plate.fill(rect(445, 205, 70, 40), 0.97f, 0.0f);
    bars(plate, { 448, 205, 64, 40 }, 5);
    plate.fillGradient(poly({ { 445, 245 }, { 515, 245 }, { 600, 520 }, { 420, 520 } }), 0.82f, { 480, 245 }, 0.55f, { 510, 520 }, 70.0f);
    // The cell bars across the right half; shackles on the left wall.
    bars(plate, { 600, 40, 360, 500 }, 9);
    for (int k = 0; k < 2; ++k) {
        const float x = 90.0f + static_cast<float>(k) * 90.0f;
        juce::Path chain; chain.startNewSubPath(x, 180); chain.quadraticTo(x + 20, 260, x + 6, 300);
        plate.stroke(chain, 2.0f);
        plate.stroke(ellipse(x - 2, 296, 16, 12), 1.6f);
    }
    // Straw and a bowl.
    for (int k = 0; k < 40; ++k) {
        const float x = 330.0f + noise2(static_cast<float>(k), 0.5f, 9) * 240.0f, y = 450.0f + noise2(0.5f, static_cast<float>(k), 9) * 60.0f;
        plate.line(x, y, x + 14.0f * (noise2(static_cast<float>(k), 2.0f, 3) - 0.5f) * 2.0f, y - 4, 0.6f, 0.8f);
    }
    plate.fill(ellipse(250, 470, 40, 14), 0.4f, 0.0f);
    torch(plate, 270, 200, 0.9f);
}

void dungeonTreasure(Plate& plate) {
    interior(plate, { 280, 150, 400, 250 }, 0.55f, 0.6f, 0.4f, true);
    masonry(plate, { 280, 150, 400, 250 }, 24.0f, 0.4f);
    window(plate, 480, 300, 80, 120, false, 0.3f);
    goldPile(plate, 480, 470, 420, 70, 31);
    goldPile(plate, 170, 520, 260, 50, 33);
    goldPile(plate, 800, 520, 240, 46, 35);
    chest(plate, 320, 470, 1.3f, true);
    chest(plate, 660, 480, 1.1f, false);
    // Goblets and a crown.
    for (float gx : { 560.0f, 420.0f }) {
        juce::Path goblet = poly({ { gx - 10, 410 }, { gx + 10, 410 }, { gx + 3, 428 }, { gx + 3, 440 }, { gx + 9, 444 }, { gx - 9, 444 }, { gx - 3, 440 }, { gx - 3, 428 } });
        plate.fill(goblet, 0.88f, 90.0f);
        plate.stroke(goblet, 0.7f);
    }
    juce::Path crown = poly({ { 455, 405 }, { 505, 405 }, { 510, 380 }, { 497, 393 }, { 480, 374 }, { 463, 393 }, { 450, 380 } });
    plate.fill(crown, 0.95f, 0.0f, 0.3f);
    plate.stroke(crown, 0.9f);
    torch(plate, 250, 230, 1.1f);
    torch(plate, 710, 230, 1.1f);
}

void dungeonLair(Plate& plate) {
    // A vast dark cavern hall; the master of the dungeon looms on a throne of bones.
    plate.fillGradient(rect(0, 0, W, H), 0.3f, { 480, 180 }, 0.08f, { 480, 560 }, 70.0f, true);
    perspectiveFloor(plate, 400, { 480, 320 }, 0.4f, 12, 0.35f);
    rockFrame(plate, 30, 41);
    // The silhouette: horns, shoulders, a cloak, red eyes.
    juce::Path beast;
    beast.startNewSubPath(330, 420);
    beast.quadraticTo(340, 250, 400, 200);
    beast.quadraticTo(420, 120, 380, 60);   // left horn
    beast.quadraticTo(440, 110, 450, 160);
    beast.quadraticTo(480, 140, 510, 160);
    beast.quadraticTo(520, 110, 580, 60);   // right horn
    beast.quadraticTo(540, 120, 560, 200);
    beast.quadraticTo(620, 250, 630, 420);
    beast.closeSubPath();
    plate.fill(beast, 0.04f, 80.0f);
    plate.stroke(beast, 1.6f);
    for (float ex : { 458.0f, 502.0f }) {
        juce::Path eye = ellipse(ex - 7, 178, 14, 6);
        plate.fillAccent(eye);
        plate.glow({ ex, 181 }, 30, 0.35f);
    }
    // The throne of bones and skulls.
    juce::Path throne = poly({ { 290, 470 }, { 300, 330 }, { 330, 330 }, { 340, 420 }, { 620, 420 }, { 630, 330 }, { 660, 330 }, { 670, 470 } });
    plate.fill(throne, 0.55f, 90.0f);
    plate.stroke(throne, 1.2f);
    for (int k = 0; k < 9; ++k) skull(plate, 320.0f + static_cast<float>(k) * 40.0f, 455.0f - static_cast<float>(k % 2) * 8.0f, 1.2f);
    // Braziers either side.
    for (float bx : { 170.0f, 790.0f }) {
        juce::Path bowl; bowl.startNewSubPath(bx - 40, 380); bowl.quadraticTo(bx, 420, bx + 40, 380); bowl.closeSubPath();
        plate.fill(bowl, 0.3f, 0.0f); plate.stroke(bowl, 1.0f);
        plate.fill(rect(bx - 4, 395, 8, 90), 0.25f, 90.0f);
        juce::Path fire = poly({ { bx - 34, 380 }, { bx - 20, 330 }, { bx - 6, 360 }, { bx + 4, 310 }, { bx + 16, 356 }, { bx + 30, 335 }, { bx + 36, 380 } });
        plate.fill(fire, 0.85f, 90.0f, 1.0f);
        plate.glow({ bx, 350 }, 130, 0.45f);
    }
}

// CASTLE ------------------------------------------------------------------------
void castleCourtyard(Plate& plate) {
    exterior(plate, 300, 0.62f, 0.93f, 0.6f, 0.78f);
    clouds(plate, 60, 51);
    // Walls all around, the keep at the back with its gate.
    plate.fill(rect(0, 150, W, 170), 0.6f, 90.0f);
    masonry(plate, { 0, 150, W, 170 }, 22.0f, 0.45f);
    for (int k = 0; k < 24; ++k) plate.fill(rect(static_cast<float>(k) * 40.0f, 134, 22, 18), 0.6f, 90.0f);
    plate.line(0, 150, W, 150, 1.4f);
    tower(plate, 110, 330, 90, 260, true, 0.66f);
    tower(plate, 850, 330, 90, 260, true, 0.66f);
    tower(plate, 480, 300, 130, 230, false, 0.62f);
    plate.fill(roundArch(480, 320, 80, 110), 0.08f, 0.0f);
    bars(plate, { 446, 230, 68, 60 }, 5);
    flag(plate, 110, 30, 30, 1.0f);
    flag(plate, 850, 30, 30, 1.0f);
    // The well in the middle.
    juce::Path well = rect(420, 420, 120, 50);
    plate.fill(well, 0.65f, 90.0f); plate.stroke(well, 1.0f);
    masonry(plate, { 420, 420, 120, 50 }, 12.0f);
    plate.fill(ellipse(420, 410, 120, 20), 0.15f, 0.0f); plate.stroke(ellipse(420, 410, 120, 20), 1.0f);
    plate.line(430, 420, 430, 350, 2.0f); plate.line(530, 420, 530, 350, 2.0f);
    plate.fill(poly({ { 410, 355 }, { 480, 320 }, { 550, 355 } }), 0.35f, 60.0f);
    plate.stroke(poly({ { 410, 355 }, { 480, 320 }, { 550, 355 } }), 1.0f);
}

void castleGreatHall(Plate& plate) {
    interior(plate, { 300, 140, 360, 240 }, 0.6f, 0.58f, 0.45f);
    // Timber roof beams.
    for (int k = 0; k < 6; ++k) {
        const float t = static_cast<float>(k) / 6.0f;
        const float y = 140.0f * t, l = 300.0f * t, r = W - 300.0f * t;
        plate.fill(rect(l, y, r - l, 10.0f - 6.0f * t), 0.2f, 0.0f);
    }
    // The hearth at the back.
    juce::Path hearth = rect(410, 250, 140, 130);
    plate.fill(hearth, 0.62f, 90.0f); plate.stroke(hearth, 1.2f);
    plate.fill(roundArch(480, 380, 100, 90), 0.08f, 0.0f);
    juce::Path fire = poly({ { 440, 380 }, { 455, 340 }, { 468, 365 }, { 480, 325 }, { 494, 362 }, { 508, 340 }, { 520, 380 } });
    plate.fill(fire, 0.85f, 90.0f, 1.0f);
    plate.glow({ 480, 355 }, 150, 0.45f);
    // The long table with benches.
    table(plate, 480, 420, 380, 40);
    for (int k = 0; k < 5; ++k) candle(plate, 360.0f + static_cast<float>(k) * 60.0f, 418, 14);
    banner(plate, 150, 60, 70, 220, 1.0f);
    banner(plate, 810, 60, 70, 220, 1.0f);
    banner(plate, 340, 150, 34, 100, 0.0f);
    banner(plate, 620, 150, 34, 100, 0.0f);
}

void castleThrone(Plate& plate) {
    interior(plate, { 280, 100, 400, 300 }, 0.66f, 0.62f, 0.5f);
    // Tall windows, light pouring in.
    window(plate, 350, 300, 60, 170, false);
    window(plate, 610, 300, 60, 170, false);
    // The dais, the throne, the red carpet up to it.
    stairsUp(plate, 480, 420, 260, 3, 16, 12);
    juce::Path throne = poly({ { 430, 370 }, { 432, 220 }, { 455, 190 }, { 480, 175 }, { 505, 190 }, { 528, 220 }, { 530, 370 } });
    plate.fill(throne, 0.4f, 90.0f, 0.0f);
    plate.stroke(throne, 1.4f);
    plate.fill(rect(440, 300, 80, 24), 0.3f, 0.0f, 1.0f);
    juce::Path carpet = poly({ { 440, 420 }, { 520, 420 }, { 640, H }, { 320, H } });
    plate.fill(carpet, 0.5f, 0.0f, 0.65f);
    plate.stroke(carpet, 1.0f);
    for (float x : { 180.0f, 300.0f, 660.0f, 780.0f }) column(plate, x, 80.0f + std::abs(x - 480.0f) * 0.05f, 500.0f, 44.0f);
    banner(plate, 480, 110, 60, 70, 1.0f);
    torch(plate, 240, 230, 1.0f);
    torch(plate, 720, 230, 1.0f);
}

void castleChapel(Plate& plate) {
    interior(plate, { 330, 110, 300, 290 }, 0.62f, 0.6f, 0.45f, true);
    // A tall pointed window of stained glass over the altar.
    juce::Path glass = pointedArch(480, 330, 120, 210);
    plate.fill(glass, 0.85f, 45.0f);
    plate.stroke(glass, 1.6f);
    for (int k = 1; k < 4; ++k) plate.line(450.0f + static_cast<float>(k) * 15.0f - 15.0f, 330, 450.0f + static_cast<float>(k) * 15.0f - 15.0f, 150, 0.8f);
    plate.stroke(ellipse(455, 150, 50, 50), 1.0f);
    plate.fill(ellipse(470, 165, 20, 20), 0.5f, 0.0f, 1.0f);
    // The altar with candles, pews.
    juce::Path altar = rect(400, 350, 160, 50);
    plate.fill(altar, 0.78f, 90.0f); plate.stroke(altar, 1.0f);
    plate.fill(rect(400, 350, 160, 8), 0.4f, 0.0f, 1.0f);
    for (int k = 0; k < 5; ++k) candle(plate, 415.0f + static_cast<float>(k) * 32.0f, 350, 18.0f + static_cast<float>(k % 2) * 6.0f);
    for (int row = 0; row < 3; ++row) {
        const float y = 430.0f + static_cast<float>(row) * 40.0f, inset = 120.0f - static_cast<float>(row) * 40.0f;
        for (int side = 0; side < 2; ++side) {
            const float x = side == 0 ? inset : W - inset - 260.0f;
            juce::Path pew = rect(x, y, 260, 16);
            plate.fill(pew, 0.4f, 0.0f); plate.stroke(pew, 0.9f);
        }
    }
    candle(plate, 300, 330, 40);
    candle(plate, 660, 330, 40);
}

void castleTowerTop(Plate& plate) {
    exterior(plate, 330, 0.55f, 0.95f, 0.7f, 0.85f);
    clouds(plate, 50, 61, 0.9f);
    clouds(plate, 140, 63, 0.93f);
    // The land far below.
    mountain(plate, 40, 330, 260, 90, 71, true, 0.86f);
    mountain(plate, 600, 330, 300, 120, 73, true, 0.86f);
    for (int k = 0; k < 30; ++k) conifer(plate, 300.0f + static_cast<float>(k) * 10.0f, 340.0f + static_cast<float>(k % 3) * 4.0f, 10.0f, 0.4f);
    // The battlements in front.
    plate.fill(rect(0, 400, W, H - 400), 0.55f, 90.0f);
    masonry(plate, { 0, 400, W, H - 400 }, 24.0f);
    for (int k = 0; k < 6; ++k) {
        juce::Path merlon = rect(static_cast<float>(k) * 180.0f - 20.0f, 330, 110, 72);
        plate.fill(merlon, 0.6f, 90.0f);
        plate.stroke(merlon, 1.3f);
        masonry(plate, { static_cast<float>(k) * 180.0f - 20.0f, 330, 110, 72 }, 24.0f, 0.4f);
    }
    plate.line(0, 400, W, 400, 1.4f);
    // A flag in the wind.
    plate.line(820, 330, 820, 120, 2.5f);
    juce::Path f; f.startNewSubPath(820, 120); f.quadraticTo(880, 110, 930, 140); f.quadraticTo(880, 150, 820, 170); f.closeSubPath();
    plate.fill(f, 0.4f, 20.0f, 1.0f); plate.stroke(f, 1.0f);
    plate.light({ 480, 300 }, 300, 3);
}
}   // namespace (part 1)
}

namespace dc::art {
using namespace motif;
namespace {
// CATHEDRAL -----------------------------------------------------------------------
void cathedralCloister(Plate& plate) {
    exterior(plate, 330, 0.66f, 0.95f, 0.66f, 0.8f);
    clouds(plate, 40, 81);
    // The garth: grass, a well and a tree.
    oak(plate, 480, 340, 150, 83, 0.5f);
    // The arcade in front: round arches on slim columns, a roof above.
    plate.fill(rect(0, 0, W, 70), 0.38f, 0.0f);
    plate.line(0, 70, W, 70, 1.4f);
    for (int k = 0; k < 6; ++k) {
        const float x = 80.0f + static_cast<float>(k) * 160.0f;
        column(plate, x, 120, 520, 26);
        if (k < 5) {
            juce::Path arch;
            arch.startNewSubPath(x + 13, 130);
            arch.quadraticTo(x + 80, 50, x + 147, 130);
            plate.stroke(arch, 2.0f);
            plate.fill(poly({ { x + 13, 70 }, { x + 147, 70 }, { x + 147, 130 } }), 0.5f, 90.0f);
            plate.fill(poly({ { x + 13, 70 }, { x + 13, 130 }, { x + 80, 92 }, { x + 147, 130 }, { x + 147, 70 } }), 0.52f, 90.0f);
        }
    }
    plate.fill(rect(0, 520, W, 40), 0.45f, 0.0f);
    plate.line(0, 520, W, 520, 1.2f);
    plate.light({ 480, 380 }, 260, 3);
}

void cathedralNave(Plate& plate) {
    interior(plate, { 380, 60, 200, 330 }, 0.66f, 0.62f, 0.45f);
    // Pointed arcades receding on both sides, a ribbed vault, the rose window.
    for (int k = 0; k < 5; ++k) {
        const float t = static_cast<float>(k) / 5.0f;
        const float lx = 380.0f * t, rx = W - 380.0f * t, base = 560.0f - 170.0f * t, h = 520.0f - 260.0f * t;
        plate.stroke(pointedArch(lx + 60.0f * (1 - t), base, 120.0f * (1 - t) + 20, h), 1.6f - t);
        plate.stroke(pointedArch(rx - 60.0f * (1 - t), base, 120.0f * (1 - t) + 20, h), 1.6f - t);
        juce::Path rib; rib.startNewSubPath(lx + 10, 300 * t + 40); rib.quadraticTo(480, -40.0f + 90.0f * t, rx - 10, 300 * t + 40);
        plate.stroke(rib, 1.2f, 0.8f);
    }
    for (float x : { 120.0f, 250.0f, 710.0f, 840.0f }) column(plate, x, 60.0f, 540.0f - std::abs(x - 480.0f) * 0.0f, 30.0f + std::abs(x - 480.0f) * 0.04f);
    juce::Path rose = ellipse(420, 90, 120, 120);
    plate.fill(rose, 0.88f, 0.0f);
    plate.stroke(rose, 1.6f);
    for (int k = 0; k < 12; ++k) {
        const float a = static_cast<float>(k) * pi / 6.0f;
        plate.line(480, 150, 480 + std::cos(a) * 60, 150 + std::sin(a) * 60, 0.8f);
    }
    plate.fill(ellipse(470, 140, 20, 20), 0.5f, 0.0f, 1.0f);
    window(plate, 480, 380, 70, 150, true, 0.86f);
    // Light beams falling across the nave.
    plate.fillGradient(poly({ { 420, 200 }, { 540, 200 }, { 700, 560 }, { 470, 560 } }), 0.85f, { 480, 200 }, 0.6f, { 560, 560 }, 70.0f);
    for (int k = 0; k < 4; ++k) candle(plate, 440.0f + static_cast<float>(k) * 27.0f, 390, 16);
}

void cathedralCrypt(Plate& plate) {
    interior(plate, { 330, 230, 300, 150 }, 0.45f, 0.5f, 0.32f);
    // Low groin vaults on squat columns.
    for (int k = 0; k < 3; ++k) {
        const float t = static_cast<float>(k) / 3.0f;
        const float w = 900.0f - 560.0f * t, base = 470.0f - 90.0f * t, h = 380.0f - 200.0f * t;
        plate.stroke(roundArch(480, base, w, h), 2.0f - t);
    }
    for (float x : { 150.0f, 810.0f, 330.0f, 630.0f }) column(plate, x, std::abs(x - 480.0f) > 200 ? 200.0f : 250.0f, std::abs(x - 480.0f) > 200 ? 520.0f : 420.0f, std::abs(x - 480.0f) > 200 ? 54.0f : 32.0f);
    sarcophagus(plate, 480, 470, 1.4f);
    sarcophagus(plate, 170, 545, 0.9f);
    sarcophagus(plate, 790, 545, 0.9f);
    for (float x : { 380.0f, 580.0f }) { candle(plate, x, 470, 22); candle(plate, x + 14, 470, 14); }
    plate.light({ 480, 500 }, 200, 3);
}

void cathedralBellTower(Plate& plate) {
    interior(plate, { 300, 120, 360, 300 }, 0.6f, 0.55f, 0.45f);
    // Louvred openings to the sky.
    for (float x : { 360.0f, 600.0f }) {
        plate.fill(pointedArch(x, 330, 80, 180), 0.92f, 0.0f);
        plate.stroke(pointedArch(x, 330, 80, 180), 1.4f);
        for (int k = 0; k < 7; ++k) {
            const float y = 190.0f + static_cast<float>(k) * 20.0f;
            plate.fill(poly({ { x - 40, y }, { x + 40, y }, { x + 40, y + 8 }, { x - 40, y + 14 } }), 0.4f, 0.0f);
        }
    }
    // The timber frame and the great bell.
    plate.fill(rect(150, 70, 660, 22), 0.35f, 0.0f);
    plate.stroke(rect(150, 70, 660, 22), 1.0f);
    for (float x : { 170.0f, 770.0f }) { plate.fill(rect(x, 70, 22, 470), 0.35f, 90.0f); plate.stroke(rect(x, 70, 22, 470), 1.0f); }
    juce::Path bell;
    bell.startNewSubPath(400, 360);
    bell.quadraticTo(410, 300, 420, 200);
    bell.quadraticTo(480, 120, 540, 200);
    bell.quadraticTo(550, 300, 560, 360);
    bell.quadraticTo(480, 380, 400, 360);
    plate.fillGradient(bell, 0.85f, { 440, 220 }, 0.2f, { 560, 340 }, 90.0f);
    plate.stroke(bell, 1.6f);
    plate.line(480, 92, 480, 140, 4.0f);
    plate.fill(ellipse(470, 350, 20, 26), 0.25f, 0.0f);
    juce::Path rope; rope.startNewSubPath(480, 376); rope.quadraticTo(470, 460, 490, 560);
    plate.stroke(rope, 2.4f);
}

// HAUNTED FOREST ------------------------------------------------------------------
void forestEdge(Plate& plate) {
    exterior(plate, 320, 0.45f, 0.82f, 0.55f, 0.62f);
    moon(plate, 760, 100, 36);
    clouds(plate, 140, 91, 0.7f);
    // The wall of dark trees, a path entering it.
    for (int k = 0; k < 40; ++k) {
        const float x = static_cast<float>(k) * 25.0f + noise2(static_cast<float>(k), 0.3f, 93) * 20.0f;
        conifer(plate, x, 330.0f + noise2(0.4f, static_cast<float>(k), 95) * 20.0f, 120.0f + 80.0f * noise2(static_cast<float>(k), 2.0f, 97), 0.18f);
    }
    juce::Path path; path.startNewSubPath(380, H); path.quadraticTo(440, 420, 470, 330); path.lineTo(500, 330); path.quadraticTo(520, 420, 620, H); path.closeSubPath();
    plate.fill(path, 0.85f, 10.0f);
    plate.stroke(path, 0.8f);
    for (float x : { 80.0f, 860.0f }) deadTree(plate, x, 540, 300, static_cast<std::uint32_t>(x));
    for (int k = 0; k < 30; ++k) {
        const float x = noise2(static_cast<float>(k) * 2.0f, 1.0f, 99) * W, y = 380.0f + noise2(1.0f, static_cast<float>(k), 99) * 160.0f;
        plate.line(x, y, x - 3, y - 10, 0.6f, 0.7f);
        plate.line(x, y, x + 4, y - 9, 0.6f, 0.7f);
    }
    fogBand(plate, 340, 40, 0.75f);
    plate.light({ 480, 360 }, 400, 3);
}

void forestOldPath(Plate& plate) {
    exterior(plate, 260, 0.3f, 0.55f, 0.45f, 0.4f);
    // A tunnel of twisted dead trees over a winding path; fog; eyes in the dark.
    juce::Path path; path.startNewSubPath(300, H); path.cubicTo(380, 460, 600, 400, 470, 270); path.lineTo(500, 270); path.cubicTo(660, 400, 520, 470, 680, H); path.closeSubPath();
    plate.fill(path, 0.72f, 10.0f);
    plate.stroke(path, 0.8f);
    for (int k = 0; k < 7; ++k) {
        const float t = static_cast<float>(k) / 7.0f;
        deadTree(plate, 40.0f + 300.0f * t, 560.0f - 260.0f * t, 420.0f - 300.0f * t, static_cast<std::uint32_t>(k * 13 + 1));
        deadTree(plate, 920.0f - 300.0f * t, 560.0f - 260.0f * t, 420.0f - 300.0f * t, static_cast<std::uint32_t>(k * 17 + 2));
    }
    for (auto e : { P { 230, 300 }, P { 720, 250 }, P { 640, 330 } }) {
        plate.fillAccent(ellipse(e.x - 6, e.y, 4, 3)); plate.fillAccent(ellipse(e.x + 2, e.y, 4, 3));
    }
    fogBand(plate, 300, 50, 0.7f);
    fogBand(plate, 420, 40, 0.75f);
    plate.light({ 480, 330 }, 420, 3);
    plate.light({ 480, 440 }, 420, 3);
}

void forestHut(Plate& plate) {
    exterior(plate, 330, 0.35f, 0.65f, 0.5f, 0.55f);
    moon(plate, 160, 90, 28);
    for (int k = 0; k < 12; ++k) conifer(plate, static_cast<float>(k) * 85.0f + 20.0f, 340, 150, 0.2f);
    // The crooked hut on its leaning frame, a glowing window, a smoking chimney.
    juce::Path walls = poly({ { 360, 430 }, { 372, 270 }, { 610, 262 }, { 600, 430 } });
    plate.fill(walls, 0.58f, 92.0f); plate.stroke(walls, 1.4f);
    for (int k = 1; k < 8; ++k) plate.line(360.0f + static_cast<float>(k) * 30.0f, 432, 372.0f + static_cast<float>(k) * 30.0f, 266, 0.6f, 0.6f);
    juce::Path roof = poly({ { 330, 280 }, { 470, 140 }, { 520, 150 }, { 645, 270 } });
    plate.fill(roof, 0.3f, 50.0f); plate.stroke(roof, 1.4f);
    juce::Path chimney = poly({ { 560, 200 }, { 586, 196 }, { 590, 120 }, { 562, 126 } });
    plate.fill(chimney, 0.4f, 90.0f); plate.stroke(chimney, 1.0f);
    for (int k = 0; k < 4; ++k) plate.stroke(ellipse(570.0f + static_cast<float>(k) * 10.0f, 100.0f - static_cast<float>(k) * 24.0f, 22, 16), 0.8f, 0.7f);
    juce::Path win = rect(410, 320, 50, 44);
    plate.fill(win, 0.9f, 0.0f, 0.4f); plate.stroke(win, 1.0f);
    plate.line(435, 320, 435, 364, 1.0f); plate.line(410, 342, 460, 342, 1.0f);
    plate.glow({ 435, 342 }, 70, 0.35f);
    plate.fill(roundArch(540, 430, 44, 90), 0.1f, 0.0f);
    cauldron(plate, 250, 480, 1.0f, true);
    for (float x : { 120.0f, 820.0f }) deadTree(plate, x, 520, 240, static_cast<std::uint32_t>(x) + 5);
    for (int k = 0; k < 3; ++k) skull(plate, 680.0f + static_cast<float>(k) * 30.0f, 470, 1.0f);
}

void forestGrove(Plate& plate) {
    exterior(plate, 360, 0.55f, 0.88f, 0.6f, 0.7f);
    // Light shafts through the canopy onto an ancient oak and a circle of stones.
    for (int k = 0; k < 4; ++k) {
        const float x = 200.0f + static_cast<float>(k) * 160.0f;
        plate.fillGradient(poly({ { x, 0 }, { x + 60, 0 }, { x + 160, 480 }, { x + 60, 480 } }), 0.95f, { x, 0 }, 0.8f, { x + 100, 480 }, 75.0f);
    }
    oak(plate, 480, 440, 420, 101, 0.42f);
    for (int k = 0; k < 7; ++k) {
        const float a = pi * (0.1f + 0.8f * static_cast<float>(k) / 6.0f);
        const float x = 480.0f + std::cos(a) * 360.0f, y = 470.0f + std::sin(a) * 50.0f;
        juce::Path stone = poly({ { x - 18, y }, { x - 14, y - 70 }, { x + 4, y - 80 }, { x + 18, y - 62 }, { x + 20, y } });
        plate.fillGradient(stone, 0.8f, { x - 10, y - 70 }, 0.35f, { x + 20, y }, 90.0f);
        plate.stroke(stone, 1.0f);
    }
    for (int k = 0; k < 14; ++k) {
        const float x = 300.0f + noise2(static_cast<float>(k), 3.0f, 103) * 360.0f, y = 200.0f + noise2(3.0f, static_cast<float>(k), 103) * 220.0f;
        plate.light({ x, y }, 10, 2);
        plate.fill(ellipse(x - 2, y - 2, 4, 4), 1.0f, 0.0f);
    }
    plate.light({ 480, 380 }, 400, 3);
}

// CAVERNS -------------------------------------------------------------------------
void caveMouth(Plate& plate) {
    // From inside the cave, looking out at a bright valley.
    exterior(plate, 330, 0.8f, 0.97f, 0.7f, 0.85f);
    mountain(plate, 200, 330, 300, 120, 111, true, 0.9f);
    mountain(plate, 480, 330, 280, 90, 113, false, 0.9f);
    rockFrame(plate, 80, 115);
    stalactites(plate, 40, 14, 117, 0.3f);
    // Boulders and a dripping pool near the floor.
    for (int k = 0; k < 5; ++k) {
        const float x = 100.0f + static_cast<float>(k) * 190.0f, y = 520.0f;
        juce::Path b = ellipse(x - 50, y - 40, 100, 60);
        plate.fillGradient(b, 0.6f, { x - 30, y - 30 }, 0.2f, { x + 40, y }, 120.0f);
        plate.stroke(b, 1.0f);
    }
    plate.fill(ellipse(380, 470, 200, 26), 0.75f, 0.0f);
    plate.light({ 480, 482 }, 40, 2);
    plate.light({ 480, 300 }, 380, 3);
}

void caveCrystal(Plate& plate) {
    plate.fillGradient(rect(0, 0, W, H), 0.4f, { 480, 260 }, 0.12f, { 480, 560 }, 60.0f, true);
    rockFrame(plate, 20, 121);
    stalactites(plate, 30, 10, 123, 0.4f);
    const auto crystal = [&](float x, float base, float h, float lean, float tone) {
        const float w = h * 0.22f;
        juce::Path c = poly({ { x - w, base }, { x - w * 0.8f + lean, base - h * 0.8f }, { x + lean, base - h }, { x + w * 0.8f + lean, base - h * 0.8f }, { x + w, base } });
        plate.fill(poly({ { x - w, base }, { x - w * 0.8f + lean, base - h * 0.8f }, { x + lean, base - h }, { x + lean * 0.4f, base } }), tone, 80.0f);
        plate.fill(poly({ { x + lean * 0.4f, base }, { x + lean, base - h }, { x + w * 0.8f + lean, base - h * 0.8f }, { x + w, base } }), tone * 0.45f, 100.0f);
        plate.stroke(c, 1.0f);
        plate.light({ x + lean, base - h }, 14, 2);
    };
    for (int k = 0; k < 14; ++k) {
        const float x = 80.0f + static_cast<float>(k) * 62.0f, h = 80.0f + 220.0f * noise2(static_cast<float>(k) * 1.3f, 0.5f, 125);
        crystal(x, 540.0f - noise2(0.5f, static_cast<float>(k), 127) * 40.0f, h, (noise2(static_cast<float>(k), 9.0f, 129) - 0.5f) * 60.0f, 0.95f);
    }
    for (int k = 0; k < 6; ++k) crystal(250.0f + static_cast<float>(k) * 90.0f, 400.0f, 60.0f + static_cast<float>(k % 3) * 30.0f, 0.0f, 0.9f);
}

void caveLake(Plate& plate) {
    plate.fillGradient(rect(0, 0, W, 300), 0.3f, { 480, 0 }, 0.62f, { 480, 300 }, 60.0f);
    stalactites(plate, 0, 18, 131, 0.45f);
    // The far shore and the still water with its reflections.
    juce::Path shore = poly({ { 0, 300 }, { 200, 270 }, { 420, 290 }, { 640, 262 }, { W, 290 }, { W, 310 }, { 0, 310 } });
    plate.fill(shore, 0.42f, 120.0f); plate.stroke(shore, 1.0f);
    plate.fillGradient(rect(0, 310, W, H - 310), 0.72f, { 480, 310 }, 0.42f, { 480, H }, 0.0f);
    for (int k = 0; k < 18; ++k) {
        const float y = 320.0f + static_cast<float>(k * k) * 0.9f;
        const float x = 100.0f + noise2(static_cast<float>(k), 2.0f, 133) * 700.0f;
        plate.line(x, y, x + 60.0f + static_cast<float>(k) * 6.0f, y, 0.7f, 0.6f);
        if (k % 3 == 0) plate.light({ x + 30, y }, 18, 2);
    }
    // A small boat with a lantern.
    juce::Path boat; boat.startNewSubPath(560, 400); boat.quadraticTo(640, 430, 720, 400); boat.lineTo(700, 390); boat.lineTo(580, 390); boat.closeSubPath();
    plate.fill(boat, 0.25f, 0.0f); plate.stroke(boat, 1.4f);
    plate.line(600, 390, 600, 340, 1.6f);
    plate.fill(rect(594, 330, 12, 14), 0.9f, 0.0f, 0.5f);
    plate.glow({ 600, 337 }, 80, 0.4f);
    rockFrame(plate, -20, 135);
}

void caveHoard(Plate& plate) {
    plate.fillGradient(rect(0, 0, W, H), 0.66f, { 480, 260 }, 0.22f, { 480, 600 }, 60.0f, true);
    stalactites(plate, 0, 12, 141, 0.4f);
    goldPile(plate, 480, 520, 860, 130, 143);
    // The sleeping dragon coiled on the gold: a long body, folded wing, head on paws.
    juce::Path body;
    body.startNewSubPath(140, 420);
    body.cubicTo(220, 300, 420, 300, 500, 360);
    body.cubicTo(580, 420, 720, 420, 790, 360);
    body.cubicTo(830, 330, 860, 360, 820, 400);
    body.cubicTo(740, 470, 580, 470, 480, 420);
    body.cubicTo(380, 370, 240, 380, 190, 450);
    body.closeSubPath();
    plate.fillGradient(body, 0.5f, { 400, 320 }, 0.18f, { 600, 460 }, 30.0f);
    plate.stroke(body, 2.2f);
    for (int k = 0; k < 16; ++k) {
        const float t = static_cast<float>(k) / 16.0f;
        const float x = 200.0f + t * 600.0f, y = 360.0f + std::sin(t * pi * 2.0f) * 30.0f;
        plate.stroke(ellipse(x - 8, y - 4, 16, 10), 0.5f, 0.7f);
    }
    juce::Path wing = poly({ { 360, 340 }, { 420, 180 }, { 470, 230 }, { 520, 160 }, { 560, 250 }, { 600, 210 }, { 600, 380 } });
    plate.fill(wing, 0.36f, 60.0f); plate.stroke(wing, 2.0f);
    for (int k = 0; k < 3; ++k) plate.line(380.0f + static_cast<float>(k) * 60.0f, 330, 420.0f + static_cast<float>(k) * 50.0f, 190.0f + static_cast<float>(k % 2) * 30.0f, 1.0f);
    juce::Path head = poly({ { 800, 380 }, { 870, 360 }, { 920, 380 }, { 880, 400 }, { 810, 405 } });
    plate.fill(head, 0.32f, 20.0f); plate.stroke(head, 1.8f);
    plate.fillAccent(ellipse(842, 372, 10, 3));
    for (int k = 0; k < 3; ++k) skull(plate, 120.0f + static_cast<float>(k) * 40.0f, 515, 1.1f);
    plate.light({ 860, 380 }, 30, 1);
}

// VILLAGE -------------------------------------------------------------------------
void cottageBig(Plate& plate, float x, float base, float s, bool flip, std::uint32_t seed) {
    const float w = 150 * s, h = 90 * s;
    juce::Path wall = rect(x - w * 0.5f, base - h, w, h);
    plate.fill(wall, 0.82f, 90.0f); plate.stroke(wall, 1.0f);
    for (int k = 1; k < 4; ++k) plate.line(x - w * 0.5f + w * static_cast<float>(k) / 4.0f, base - h, x - w * 0.5f + w * static_cast<float>(k) / 4.0f, base, 2.0f * s, 0.8f);
    plate.line(x - w * 0.5f, base - h * 0.5f, x + w * 0.5f, base - h * 0.5f, 2.0f * s, 0.8f);
    juce::Path roof = poly({ { x - w * 0.6f, base - h }, { x + (flip ? 0.1f : -0.1f) * w, base - h - 80 * s }, { x + w * 0.6f, base - h } });
    plate.fill(roof, 0.35f, flip ? 120.0f : 60.0f); plate.stroke(roof, 1.2f);
    for (int k = 0; k < 6; ++k) plate.line(x - w * 0.55f + static_cast<float>(k) * w * 0.22f, base - h - 4, x + (flip ? 0.1f : -0.1f) * w, base - h - 76 * s, 0.4f, 0.4f);
    plate.fill(rect(x - 12 * s, base - 46 * s, 24 * s, 46 * s), 0.15f, 0.0f);
    juce::Path win = rect(x + (flip ? -50.0f : 26.0f) * s, base - 70 * s, 26 * s, 22 * s);
    plate.fill(win, 0.9f, 0.0f, 0.3f); plate.stroke(win, 0.8f);
    plate.glow({ x + (flip ? -37.0f : 39.0f) * s, base - 59 * s }, 30 * s, 0.25f);
    (void) seed;
}

void villageGreen(Plate& plate) {
    exterior(plate, 300, 0.68f, 0.96f, 0.72f, 0.86f);
    clouds(plate, 50, 151);
    cottageBig(plate, 170, 330, 0.9f, false, 1);
    cottageBig(plate, 790, 330, 0.9f, true, 2);
    cottageBig(plate, 380, 300, 0.55f, true, 3);
    cottageBig(plate, 600, 300, 0.55f, false, 4);
    oak(plate, 480, 430, 230, 153, 0.5f);
    // A maypole with ribbons and a bench.
    plate.line(700, 520, 700, 340, 2.4f);
    for (int k = 0; k < 5; ++k) {
        juce::Path ribbon; ribbon.startNewSubPath(700, 345); ribbon.quadraticTo(700.0f + static_cast<float>(k - 2) * 30.0f, 420, 700.0f + static_cast<float>(k - 2) * 50.0f, 500);
        if (k == 2) plate.strokeAccent(ribbon, 1.2f); else plate.stroke(ribbon, 0.8f);
    }
    juce::Path bench = rect(200, 470, 120, 10);
    plate.fill(bench, 0.4f, 0.0f); plate.stroke(bench, 0.8f);
    for (int k = 0; k < 40; ++k) {
        const float x = noise2(static_cast<float>(k) * 1.7f, 0.0f, 155) * W, y = 360.0f + noise2(0.0f, static_cast<float>(k), 155) * 190.0f;
        plate.line(x, y, x + 2, y - 7, 0.6f, 0.6f);
    }
}

void villageTavern(Plate& plate) {
    interior(plate, { 260, 150, 440, 240 }, 0.62f, 0.55f, 0.4f);
    for (int k = 0; k < 5; ++k) {
        const float t = static_cast<float>(k) / 5.0f;
        plate.fill(rect(260.0f * t, 150.0f * t, W - 520.0f * t, 12.0f - 8.0f * t), 0.22f, 0.0f);
    }
    // The bar with barrels behind, the hearth, a table with tankards, lanterns.
    juce::Path bar = rect(280, 300, 260, 90);
    plate.fill(bar, 0.4f, 90.0f); plate.stroke(bar, 1.2f);
    plate.fill(rect(270, 292, 280, 12), 0.6f, 0.0f); plate.stroke(rect(270, 292, 280, 12), 1.0f);
    for (int k = 0; k < 4; ++k) barrel(plate, 300.0f + static_cast<float>(k) * 60.0f, 290, 0.9f);
    juce::Path hearth = rect(570, 250, 110, 140);
    plate.fill(hearth, 0.6f, 90.0f); plate.stroke(hearth, 1.0f);
    plate.fill(roundArch(625, 390, 80, 80), 0.08f, 0.0f);
    plate.fill(poly({ { 595, 390 }, { 610, 355 }, { 625, 372 }, { 640, 345 }, { 655, 390 } }), 0.85f, 90.0f, 1.0f);
    plate.glow({ 625, 370 }, 120, 0.42f);
    table(plate, 480, 450, 220, 30);
    for (float tx : { 420.0f, 470.0f, 540.0f }) {
        juce::Path mug = rect(tx, 432, 18, 22);
        plate.fill(mug, 0.65f, 90.0f); plate.stroke(mug, 0.8f);
        plate.stroke(ellipse(tx + 16, 436, 10, 12), 0.8f);
    }
    for (float lx : { 180.0f, 780.0f }) {
        plate.line(lx, 0, lx, 120, 1.0f);
        juce::Path lantern = rect(lx - 12, 120, 24, 30);
        plate.fill(lantern, 0.9f, 0.0f, 0.5f); plate.stroke(lantern, 1.0f);
        plate.glow({ lx, 135 }, 90, 0.4f);
    }
    barrel(plate, 120, 520, 1.6f);
    barrel(plate, 850, 530, 1.5f);
}

void villageMarket(Plate& plate) {
    exterior(plate, 280, 0.7f, 0.96f, 0.75f, 0.88f);
    clouds(plate, 40, 161);
    // Houses around the square, stalls with striped awnings, a fountain.
    cottageBig(plate, 100, 300, 0.8f, false, 5);
    cottageBig(plate, 300, 285, 0.6f, true, 6);
    cottageBig(plate, 650, 285, 0.6f, false, 7);
    cottageBig(plate, 860, 300, 0.8f, true, 8);
    const auto stall = [&](float x, float base, float s, bool red) {
        juce::Path awning = poly({ { x - 70 * s, base - 90 * s }, { x + 70 * s, base - 90 * s }, { x + 80 * s, base - 60 * s }, { x - 80 * s, base - 60 * s } });
        plate.fill(awning, 0.85f, 0.0f, red ? 0.6f : 0.0f);
        plate.stroke(awning, 1.0f);
        for (int k = 0; k < 6; ++k) plate.fill(rect(x - 80 * s + static_cast<float>(k) * 27 * s, base - 75 * s, 13 * s, 15 * s), 0.35f, 90.0f);
        plate.line(x - 75 * s, base - 60 * s, x - 75 * s, base, 1.6f);
        plate.line(x + 75 * s, base - 60 * s, x + 75 * s, base, 1.6f);
        juce::Path counter = rect(x - 70 * s, base - 30 * s, 140 * s, 30 * s);
        plate.fill(counter, 0.5f, 90.0f); plate.stroke(counter, 1.0f);
        for (int k = 0; k < 6; ++k) plate.fill(ellipse(x - 60 * s + static_cast<float>(k) * 20 * s, base - 40 * s, 14 * s, 12 * s), 0.8f, 30.0f);
    };
    stall(200, 470, 1.0f, true);
    stall(760, 470, 1.0f, false);
    juce::Path basin = ellipse(380, 400, 200, 50);
    plate.fill(basin, 0.6f, 0.0f); plate.stroke(basin, 1.2f);
    plate.fill(ellipse(395, 405, 170, 30), 0.82f, 0.0f);
    column(plate, 480, 320, 420, 20);
    juce::Path spout; spout.startNewSubPath(480, 320); spout.quadraticTo(440, 300, 420, 410); plate.stroke(spout, 0.8f, 0.7f);
    juce::Path spout2; spout2.startNewSubPath(480, 320); spout2.quadraticTo(520, 300, 540, 410); plate.stroke(spout2, 0.8f, 0.7f);
    plate.light({ 480, 420 }, 30, 2);
    for (int k = 0; k < 6; ++k) plate.line(0, 480.0f + static_cast<float>(k) * 14.0f, W, 480.0f + static_cast<float>(k) * 14.0f, 0.4f, 0.4f);
}

void villageSmithy(Plate& plate) {
    interior(plate, { 280, 160, 400, 230 }, 0.5f, 0.5f, 0.35f);
    // The forge with its fire, bellows, the anvil in front, tools on the wall.
    juce::Path forge = rect(330, 290, 200, 100);
    plate.fill(forge, 0.5f, 90.0f); plate.stroke(forge, 1.2f);
    masonry(plate, { 330, 290, 200, 100 }, 16.0f);
    juce::Path hood = poly({ { 310, 290 }, { 550, 290 }, { 480, 170 }, { 380, 170 } });
    plate.fill(hood, 0.35f, 90.0f); plate.stroke(hood, 1.2f);
    plate.fill(poly({ { 360, 292 }, { 380, 250 }, { 400, 272 }, { 425, 230 }, { 450, 270 }, { 475, 245 }, { 500, 292 } }), 0.85f, 90.0f, 1.0f);
    plate.glow({ 430, 270 }, 190, 0.5f);
    juce::Path bellows = poly({ { 560, 330 }, { 640, 300 }, { 650, 340 }, { 570, 360 } });
    plate.fill(bellows, 0.35f, 30.0f); plate.stroke(bellows, 1.0f);
    juce::Path anvil = poly({ { 330, 420 }, { 560, 420 }, { 590, 400 }, { 600, 412 }, { 560, 440 }, { 520, 440 }, { 510, 480 }, { 540, 520 }, { 360, 520 }, { 390, 480 }, { 380, 440 }, { 330, 432 } });
    plate.fillGradient(anvil, 0.55f, { 400, 420 }, 0.1f, { 560, 520 }, 90.0f);
    plate.stroke(anvil, 1.4f);
    // A hammer resting on the anvil, a glowing bar.
    plate.fill(rect(420, 404, 70, 8), 0.75f, 0.0f, 0.9f);
    plate.glow({ 455, 408 }, 40, 0.3f);
    juce::Path hammer = rect(500, 380, 14, 30);
    plate.fill(hammer, 0.3f, 90.0f); plate.stroke(hammer, 0.8f);
    plate.line(507, 395, 580, 380, 3.0f);
    for (int k = 0; k < 5; ++k) {
        const float x = 690.0f + static_cast<float>(k) * 40.0f;
        plate.line(x, 160, x, 260, 2.0f);
        plate.fill(rect(x - 8, 250, 16, 20), 0.3f, 90.0f);
    }
    barrel(plate, 150, 520, 1.5f);
}
}   // namespace (part 2)
}

namespace dc::art {
using namespace motif;
namespace {
// SWAMP ---------------------------------------------------------------------------
void pools(Plate& plate, float y0, int count, std::uint32_t seed) {
    for (int k = 0; k < count; ++k) {
        const float x = noise2(static_cast<float>(k) * 2.1f, 0.4f, seed) * W, y = y0 + noise2(0.4f, static_cast<float>(k) * 1.9f, seed) * (H - y0 - 20.0f);
        const float w = 80.0f + 160.0f * noise2(static_cast<float>(k), 3.0f, seed + 1);
        plate.fill(ellipse(x - w * 0.5f, y - w * 0.07f, w, w * 0.14f), 0.7f, 0.0f);
        plate.stroke(ellipse(x - w * 0.5f, y - w * 0.07f, w, w * 0.14f), 0.7f, 0.8f);
        if (k % 2 == 0) plate.light({ x, y }, 20, 2);
    }
}
void reedClump(Plate& plate, float x, float base, float h, std::uint32_t seed) {
    for (int k = 0; k < 9; ++k) {
        const float dx = (static_cast<float>(k) - 4.0f) * 5.0f, lean = (noise2(static_cast<float>(k), x, seed) - 0.5f) * 30.0f;
        juce::Path r; r.startNewSubPath(x + dx, base); r.quadraticTo(x + dx + lean * 0.3f, base - h * 0.6f, x + dx + lean, base - h * (0.7f + 0.3f * noise2(x, static_cast<float>(k), seed)));
        plate.stroke(r, 1.0f);
        if (k % 3 == 0) plate.fill(ellipse(x + dx + lean - 2.5f, base - h * 0.85f, 5, 16), 0.2f, 90.0f);
    }
}

void swampMire(Plate& plate) {
    exterior(plate, 300, 0.5f, 0.78f, 0.55f, 0.62f);
    clouds(plate, 80, 171, 0.65f);
    pools(plate, 320, 9, 173);
    for (int k = 0; k < 8; ++k) reedClump(plate, 60.0f + static_cast<float>(k) * 120.0f, 380.0f + static_cast<float>(k % 3) * 60.0f, 60.0f + static_cast<float>(k % 2) * 30.0f, static_cast<std::uint32_t>(k));
    for (float x : { 200.0f, 700.0f, 860.0f }) deadTree(plate, x, 330, 180, static_cast<std::uint32_t>(x) + 7);
    fogBand(plate, 290, 40, 0.78f);
    fogBand(plate, 400, 30, 0.8f);
    plate.light({ 480, 310 }, 450, 3);
}

void swampBog(Plate& plate) {
    exterior(plate, 220, 0.35f, 0.55f, 0.32f, 0.4f);
    // Thick black water, gnarled roots, bubbles rising.
    plate.fillGradient(rect(0, 260, W, H - 260), 0.35f, { 480, 260 }, 0.15f, { 480, H }, 0.0f);
    for (int k = 0; k < 14; ++k) {
        const float y = 280.0f + static_cast<float>(k * k) * 1.2f;
        plate.line(0, y, W, y + (noise2(static_cast<float>(k), 1.0f, 181) - 0.5f) * 10.0f, 0.5f, 0.5f);
    }
    for (int k = 0; k < 4; ++k) {
        const float x = 120.0f + static_cast<float>(k) * 240.0f;
        juce::Path root; root.startNewSubPath(x - 60, 300); root.cubicTo(x - 30, 200, x + 40, 360, x + 80, 260);
        plate.stroke(root, 6.0f);
        plate.stroke(root, 2.0f, 0.5f);
        deadTree(plate, x, 270, 220, static_cast<std::uint32_t>(x));
    }
    for (int k = 0; k < 16; ++k) {
        const float x = 100.0f + noise2(static_cast<float>(k) * 1.4f, 2.0f, 183) * 760.0f, y = 340.0f + noise2(2.0f, static_cast<float>(k), 183) * 180.0f;
        const float r = 4.0f + 10.0f * noise2(static_cast<float>(k), static_cast<float>(k), 185);
        plate.fill(ellipse(x - r, y - r * 0.7f, 2 * r, 1.4f * r), 0.8f, 30.0f);
        plate.stroke(ellipse(x - r, y - r * 0.7f, 2 * r, 1.4f * r), 0.6f);
        plate.light({ x, y }, r * 2.0f, 2);
    }
    fogBand(plate, 230, 60, 0.6f);
    plate.light({ 480, 260 }, 450, 3);
}

void swampRuins(Plate& plate) {
    exterior(plate, 330, 0.48f, 0.8f, 0.5f, 0.6f);
    clouds(plate, 70, 191, 0.7f);
    // Half-sunken arches and columns, broken, their reflections in the water.
    plate.fillGradient(rect(0, 330, W, H - 330), 0.6f, { 480, 330 }, 0.35f, { 480, H }, 0.0f);
    const auto ruin = [&](float x, float h, float w, bool arch) {
        if (arch) {
            juce::Path a = roundArch(x, 330, w, h);
            juce::Path frame; frame.addRectangle(x - w * 0.5f - 18, 330 - h - 18, w + 36, h + 18);
            frame.addPath(a);
            frame.setUsingNonZeroWinding(false);
            plate.fill(frame, 0.62f, 90.0f);
            plate.stroke(frame, 1.2f);
            masonry(plate, { x - w * 0.5f - 18, 330 - h - 18, w + 36, h + 18 }, 18.0f, 0.35f);
        } else {
            column(plate, x, 330 - h, 330, w);
        }
        // Reflection: a darker, wavering copy below the waterline.
        for (int k = 0; k < 10; ++k) {
            const float y = 340.0f + static_cast<float>(k) * 9.0f;
            const float off = (noise2(static_cast<float>(k), x, 193) - 0.5f) * 10.0f;
            plate.line(x - w * 0.5f + off, y, x + w * 0.5f + off, y, 1.6f, 0.5f - static_cast<float>(k) * 0.04f);
        }
    };
    ruin(260, 200, 150, true);
    ruin(560, 150, 26, false);
    ruin(640, 90, 26, false);
    ruin(800, 230, 120, true);
    for (int k = 0; k < 4; ++k) reedClump(plate, 100.0f + static_cast<float>(k) * 250.0f, 520.0f, 70.0f, static_cast<std::uint32_t>(k + 20));
    for (int k = 0; k < 8; ++k) plate.light({ 100.0f + static_cast<float>(k) * 110.0f, 400.0f + static_cast<float>(k % 3) * 40.0f }, 16, 2);
    fogBand(plate, 310, 40, 0.75f);
}

// MOUNTAINS -----------------------------------------------------------------------
void mountainsFoothills(Plate& plate) {
    exterior(plate, 300, 0.66f, 0.96f, 0.72f, 0.86f);
    clouds(plate, 40, 201, 0.92f);
    mountain(plate, 300, 300, 420, 230, 203, true, 0.88f);
    mountain(plate, 580, 300, 380, 180, 205, true, 0.88f);
    mountain(plate, 0, 300, 360, 150, 207, false, 0.86f);
    // Rolling hills in front, a path, a few pines.
    for (int k = 0; k < 3; ++k) {
        juce::Path hill; const float y = 330.0f + static_cast<float>(k) * 70.0f;
        hill.startNewSubPath(-50, y + 60); hill.quadraticTo(240.0f + static_cast<float>(k) * 120.0f, y - 40, 600, y + 20); hill.quadraticTo(800, y - 30, W + 50, y + 40); hill.lineTo(W + 50, H); hill.lineTo(-50, H); hill.closeSubPath();
        plate.fill(hill, 0.8f - static_cast<float>(k) * 0.08f, 15.0f + static_cast<float>(k) * 10.0f);
        plate.stroke(hill, 1.0f);
    }
    juce::Path path; path.startNewSubPath(520, H); path.cubicTo(500, 470, 620, 420, 560, 340); path.lineTo(575, 340); path.cubicTo(650, 420, 540, 470, 600, H); path.closeSubPath();
    plate.fill(path, 0.92f, 0.0f); plate.stroke(path, 0.7f);
    for (int k = 0; k < 9; ++k) conifer(plate, 80.0f + static_cast<float>(k) * 100.0f, 470.0f + static_cast<float>(k % 3) * 20.0f, 60.0f, 0.3f);
}

void mountainsPass(Plate& plate) {
    exterior(plate, 360, 0.7f, 0.96f, 0.7f, 0.85f);
    mountain(plate, 380, 360, 220, 200, 211, true, 0.9f);
    // Sheer canyon walls on both sides, the path between them.
    juce::Path left = poly({ { 0, 0 }, { 330, 0 }, { 300, 120 }, { 360, 260 }, { 330, 380 }, { 420, H }, { 0, H } });
    juce::Path right = poly({ { W, 0 }, { 640, 0 }, { 680, 140 }, { 610, 260 }, { 650, 380 }, { 560, H }, { W, H } });
    plate.fillGradient(left, 0.62f, { 300, 200 }, 0.2f, { 0, 300 }, 75.0f);
    plate.fillGradient(right, 0.5f, { 650, 200 }, 0.15f, { W, 300 }, 105.0f);
    plate.stroke(left, 1.6f);
    plate.stroke(right, 1.6f);
    for (int k = 0; k < 10; ++k) {
        const float y = 40.0f + static_cast<float>(k) * 50.0f;
        plate.line(30, y, 300.0f + static_cast<float>(k % 3) * 20.0f, y + 20, 0.7f, 0.6f);
        plate.line(W - 30, y + 10, 660.0f - static_cast<float>(k % 3) * 20.0f, y + 25, 0.7f, 0.6f);
    }
    juce::Path path = poly({ { 420, H }, { 470, 380 }, { 500, 380 }, { 560, H } });
    plate.fill(path, 0.85f, 0.0f); plate.stroke(path, 0.7f);
    for (int k = 0; k < 6; ++k) {
        const float x = 440.0f + static_cast<float>(k) * 20.0f, y = 540.0f - static_cast<float>(k) * 28.0f;
        plate.fill(ellipse(x - 8, y - 5, 16, 10), 0.5f, 30.0f);
    }
    crow(plate, 470, 120, 1.2f);
    crow(plate, 520, 90, 0.9f);
}

void mountainsPeak(Plate& plate) {
    exterior(plate, 380, 0.3f, 0.7f, 0.95f, 0.95f);
    stars(plate, { 0, 0, W, 220 }, 60, 221);
    // The summit: snow and ice, wind-blown, thin air.
    juce::Path summit = poly({ { 0, H }, { 0, 420 }, { 220, 300 }, { 400, 160 }, { 470, 140 }, { 560, 200 }, { 760, 320 }, { W, 380 }, { W, H } });
    plate.fill(summit, 0.95f, 60.0f);
    plate.fill(poly({ { 470, 140 }, { 560, 200 }, { 760, 320 }, { W, 380 }, { W, H }, { 520, H } }), 0.6f, 120.0f);
    plate.stroke(summit, 1.6f);
    for (int k = 0; k < 8; ++k) {
        const float x = 400.0f + static_cast<float>(k) * 50.0f;
        plate.stroke(wobbly({ 470, 140 }, { x, 560 }, 20.0f, static_cast<std::uint32_t>(k + 223), 8), 0.7f, 0.6f);
    }
    // Icicles hanging from an overhang, wind streaks.
    for (int k = 0; k < 10; ++k) {
        const float x = 150.0f + static_cast<float>(k) * 22.0f, y = 330.0f - static_cast<float>(k) * 9.0f;
        plate.fill(poly({ { x - 5, y }, { x + 5, y }, { x, y + 20.0f + static_cast<float>(k % 3) * 12.0f } }), 0.98f, 90.0f);
        plate.stroke(poly({ { x - 5, y }, { x + 5, y }, { x, y + 20.0f + static_cast<float>(k % 3) * 12.0f } }), 0.5f);
        plate.light({ x, y + 10 }, 10, 2);
    }
    for (int k = 0; k < 12; ++k) {
        const float y = 120.0f + static_cast<float>(k) * 30.0f;
        juce::Path streak; streak.startNewSubPath(W - 300.0f + static_cast<float>(k % 4) * 40.0f, y); streak.quadraticTo(W - 150, y - 8, W - 20, y + 4);
        plate.stroke(streak, 0.6f, 0.5f);
    }
    // A cairn and a flag at the top.
    plate.line(470, 140, 470, 70, 2.0f);
    juce::Path f = poly({ { 470, 70 }, { 520, 80 }, { 470, 96 } });
    plate.fill(f, 0.5f, 20.0f, 1.0f); plate.stroke(f, 0.8f);
}

void mountainsDwarven(Plate& plate) {
    interior(plate, { 300, 170, 360, 220 }, 0.55f, 0.55f, 0.35f);
    // Massive square pillars carved with runes, a great geometric door, forge glow.
    for (float x : { 120.0f, 840.0f, 260.0f, 700.0f }) {
        const bool near = x < 200 || x > 760;
        const float w = near ? 110.0f : 60.0f, top = near ? 0.0f : 120.0f, base = near ? H : 430.0f;
        juce::Path pillar = rect(x - w * 0.5f, top, w, base - top);
        plate.fill(rect(x - w * 0.5f, top, w * 0.55f, base - top), 0.68f, 90.0f);
        plate.fill(rect(x + w * 0.05f, top, w * 0.45f, base - top), 0.3f, 90.0f);
        plate.stroke(pillar, 1.4f);
        for (int k = 0; k < 6; ++k) {
            const float y = top + 40.0f + static_cast<float>(k) * (base - top - 80.0f) / 6.0f;
            juce::Path rune = poly({ { x - w * 0.2f, y }, { x, y - 12 }, { x + w * 0.2f, y }, { x, y + 12 } }, true);
            plate.stroke(rune, 0.9f, 0.8f);
        }
    }
    juce::Path door = poly({ { 400, 390 }, { 400, 240 }, { 440, 200 }, { 520, 200 }, { 560, 240 }, { 560, 390 } });
    plate.fill(door, 0.4f, 90.0f); plate.stroke(door, 1.6f);
    plate.line(480, 200, 480, 390, 1.4f);
    for (int k = 0; k < 4; ++k) plate.stroke(poly({ { 480, 230.0f + static_cast<float>(k) * 36.0f }, { 520, 250.0f + static_cast<float>(k) * 36.0f }, { 480, 270.0f + static_cast<float>(k) * 36.0f }, { 440, 250.0f + static_cast<float>(k) * 36.0f } }), 0.8f);
    plate.fillGradient(rect(0, 440, W, 120), 0.6f, { 480, 440 }, 0.35f, { 480, H }, 0.0f);
    for (float x : { 340.0f, 620.0f }) {
        juce::Path brazier = poly({ { x - 30, 400 }, { x + 30, 400 }, { x + 18, 430 }, { x - 18, 430 } });
        plate.fill(brazier, 0.3f, 0.0f); plate.stroke(brazier, 1.0f);
        plate.fill(poly({ { x - 24, 400 }, { x - 10, 370 }, { x, 388 }, { x + 10, 360 }, { x + 24, 400 } }), 0.85f, 90.0f, 1.0f);
        plate.glow({ x, 385 }, 110, 0.42f);
    }
}

// NECROPOLIS ----------------------------------------------------------------------
void necropolisGates(Plate& plate) {
    exterior(plate, 360, 0.35f, 0.72f, 0.5f, 0.6f);
    moon(plate, 480, 110, 50);
    clouds(plate, 160, 231, 0.6f);
    // The stone wall, two pillars with skulls, the iron gate ajar.
    plate.fill(rect(0, 250, W, 140), 0.55f, 90.0f);
    masonry(plate, { 0, 250, W, 140 }, 22.0f, 0.45f);
    plate.line(0, 250, W, 250, 1.4f);
    for (float x : { 330.0f, 630.0f }) {
        juce::Path pillar = rect(x - 30, 150, 60, 300);
        plate.fill(rect(x - 30, 150, 34, 300), 0.66f, 90.0f);
        plate.fill(rect(x + 4, 150, 26, 300), 0.3f, 90.0f);
        plate.stroke(pillar, 1.4f);
        plate.fill(rect(x - 38, 140, 76, 14), 0.5f, 0.0f); plate.stroke(rect(x - 38, 140, 76, 14), 1.0f);
        skull(plate, x, 120, 2.0f);
    }
    // The gate: tall iron bars with spear tips, one leaf swung open.
    for (int k = 0; k < 7; ++k) {
        const float x = 365.0f + static_cast<float>(k) * 18.0f;
        plate.fill(rect(x - 2, 200, 4, 250), 0.15f, 90.0f);
        plate.fill(poly({ { x - 5, 200 }, { x, 186 }, { x + 5, 200 } }), 0.15f, 90.0f);
    }
    for (int k = 0; k < 5; ++k) {
        const float x = 520.0f + static_cast<float>(k) * 22.0f, top = 210.0f + static_cast<float>(k) * 6.0f;
        plate.fill(poly({ { x - 2, top }, { x + 2, top }, { x + 2, 450.0f - static_cast<float>(k) * 4.0f }, { x - 2, 450.0f - static_cast<float>(k) * 4.0f } }), 0.15f, 90.0f);
    }
    for (float y : { 230.0f, 330.0f, 430.0f }) plate.line(365, y, 473, y, 3.0f);
    juce::Path path = poly({ { 400, H }, { 455, 450 }, { 505, 450 }, { 600, H } });
    plate.fill(path, 0.75f, 0.0f);
    for (auto c : { P { 150, 200 }, P { 210, 170 }, P { 760, 220 }, P { 330, 100 } }) crow(plate, c.x, c.y, 1.4f);
    deadTree(plate, 120, 540, 300, 233);
    deadTree(plate, 850, 540, 260, 235);
    fogBand(plate, 430, 50, 0.7f);
    plate.light({ 480, 450 }, 420, 3);
}

void necropolisGraveyard(Plate& plate) {
    exterior(plate, 300, 0.4f, 0.78f, 0.55f, 0.66f);
    moon(plate, 760, 90, 40);
    stars(plate, { 0, 0, 600, 200 }, 30, 241);
    // Rows of tombstones and crosses receding, a dead tree, a lone crow.
    for (int row = 0; row < 5; ++row) {
        const float y = 330.0f + static_cast<float>(row * row) * 9.0f + static_cast<float>(row) * 20.0f, s = 0.5f + static_cast<float>(row) * 0.35f;
        for (int k = 0; k < 9 - row; ++k) {
            const float x = 60.0f + (static_cast<float>(k) + 0.5f * static_cast<float>(row % 2)) * (W - 120.0f) / static_cast<float>(9 - row);
            if (std::abs(x - 480.0f) < 50.0f && row > 1) continue;
            tombstone(plate, x, y, s, (k + row) % 3 == 0 ? 0 : 1);
        }
    }
    deadTree(plate, 200, 330, 260, 243);
    crow(plate, 230, 160, 1.4f);
    juce::Path path = poly({ { 420, H }, { 470, 310 }, { 490, 310 }, { 540, H } });
    plate.fill(path, 0.8f, 0.0f);
    fogBand(plate, 360, 60, 0.72f);
    fogBand(plate, 470, 40, 0.75f);
    plate.light({ 480, 390 }, 450, 3);
}

void necropolisMausoleum(Plate& plate) {
    interior(plate, { 320, 180, 320, 210 }, 0.48f, 0.5f, 0.3f, true);
    masonry(plate, { 320, 180, 320, 210 }, 20.0f, 0.5f);
    // Burial niches in the walls, a sarcophagus, candles, a carved angel's silhouette.
    for (int side = 0; side < 2; ++side)
        for (int row = 0; row < 3; ++row)
            for (int k = 0; k < 2; ++k) {
                const float x = side == 0 ? 40.0f + static_cast<float>(k) * 120.0f : W - 140.0f - static_cast<float>(k) * 120.0f;
                const float y = 120.0f + static_cast<float>(row) * 110.0f;
                juce::Path niche = roundArch(x + 40, y + 60, 70, 60);
                plate.fill(niche, 0.1f, 0.0f); plate.stroke(niche, 1.0f);
                if ((row + k + side) % 2 == 0) skull(plate, x + 40, y + 45, 1.0f);
            }
    sarcophagus(plate, 480, 470, 1.5f);
    juce::Path angel;
    angel.startNewSubPath(480, 200); angel.quadraticTo(440, 220, 450, 260); angel.quadraticTo(400, 230, 380, 300);
    angel.quadraticTo(430, 290, 450, 300); angel.lineTo(450, 380); angel.lineTo(510, 380); angel.lineTo(510, 300);
    angel.quadraticTo(530, 290, 580, 300); angel.quadraticTo(560, 230, 510, 260); angel.quadraticTo(520, 220, 480, 200);
    plate.fill(angel, 0.75f, 90.0f); plate.stroke(angel, 1.2f);
    plate.fill(ellipse(468, 182, 24, 24), 0.75f, 0.0f); plate.stroke(ellipse(468, 182, 24, 24), 1.0f);
    for (float x : { 360.0f, 380.0f, 580.0f, 600.0f }) candle(plate, x, 410, 18.0f + (x > 500 ? 6.0f : 0.0f));
}

// WIZARD'S TOWER --------------------------------------------------------------------
void wizardStairwell(Plate& plate) {
    plate.fillGradient(rect(0, 0, W, H), 0.6f, { 480, 280 }, 0.25f, { 0, 0 }, 35.0f, true);
    // The curved wall of the tower, the central column, steps spiralling up around it.
    masonry(plate, { 0, 0, W, H }, 26.0f, 0.35f);
    column(plate, 480, 0, H, 90);
    for (int k = 0; k < 14; ++k) {
        const float a = static_cast<float>(k) * 0.45f;
        const float y = 540.0f - static_cast<float>(k) * 40.0f;
        const float x0 = 480.0f + std::cos(a) * 45.0f, x1 = 480.0f + std::cos(a) * 380.0f;
        const float x2 = 480.0f + std::cos(a + 0.45f) * 380.0f, x3 = 480.0f + std::cos(a + 0.45f) * 45.0f;
        if (std::sin(a) < -0.2f) continue;   // steps behind the column are hidden
        juce::Path step = poly({ { x0, y }, { x1, y + 10 }, { x2, y - 24 }, { x3, y - 30 } });
        plate.fill(step, 0.75f - 0.25f * std::abs(std::cos(a)), 0.0f);
        plate.stroke(step, 1.0f);
        plate.fill(poly({ { x0, y }, { x1, y + 10 }, { x1, y + 22 }, { x0, y + 12 } }), 0.3f, 90.0f);
    }
    // A narrow window and an oil lamp in a niche.
    window(plate, 160, 260, 40, 120, false, 0.95f);
    plate.fill(roundArch(800, 300, 50, 70), 0.15f, 0.0f);
    juce::Path lamp = ellipse(785, 280, 30, 16);
    plate.fill(lamp, 0.7f, 0.0f); plate.stroke(lamp, 0.8f);
    plate.fillAccent(ellipse(797, 268, 6, 12));
    plate.glow({ 800, 272 }, 90, 0.4f);
}

void wizardLibrary(Plate& plate) {
    interior(plate, { 250, 110, 460, 290 }, 0.5f, 0.55f, 0.35f);
    bookshelf(plate, { 250, 110, 460, 290 }, 251);
    bookshelf(plate, { 0, 30, 200, 520 }, 253);
    bookshelf(plate, { 760, 30, 200, 520 }, 255);
    // A ladder against the shelves, a reading desk with an open book and a candle.
    for (float x : { 620.0f, 660.0f }) plate.line(x, 120, x - 30, 420, 2.4f);
    for (int k = 0; k < 9; ++k) { const float y = 140.0f + static_cast<float>(k) * 32.0f; plate.line(618.0f - static_cast<float>(k) * 3.0f, y, 658.0f - static_cast<float>(k) * 3.0f, y, 2.0f); }
    table(plate, 430, 430, 200, 26);
    juce::Path book = poly({ { 380, 425 }, { 430, 418 }, { 480, 425 }, { 480, 440 }, { 430, 432 }, { 380, 440 } });
    plate.fill(book, 0.95f, 0.0f); plate.stroke(book, 0.8f);
    for (int k = 0; k < 4; ++k) { plate.line(388, 426.0f + static_cast<float>(k) * 3.0f, 424, 421.0f + static_cast<float>(k) * 3.0f, 0.4f, 0.6f); plate.line(436, 421.0f + static_cast<float>(k) * 3.0f, 472, 426.0f + static_cast<float>(k) * 3.0f, 0.4f, 0.6f); }
    candle(plate, 520, 428, 22);
    for (int k = 0; k < 3; ++k) {
        juce::Path stack = rect(300.0f + static_cast<float>(k) * 4.0f, 404.0f - static_cast<float>(k) * 8.0f, 50, 8);
        plate.fill(stack, 0.5f + static_cast<float>(k) * 0.15f, 0.0f); plate.stroke(stack, 0.6f);
    }
    plate.light({ 480, 300 }, 380, 3);
}

void wizardAlchemy(Plate& plate) {
    interior(plate, { 280, 140, 400, 250 }, 0.52f, 0.55f, 0.35f);
    // Shelves of jars, a workbench of flasks and retorts, a bubbling cauldron.
    for (int s = 0; s < 3; ++s) {
        const float y = 200.0f + static_cast<float>(s) * 60.0f;
        plate.fill(rect(290, y, 380, 6), 0.3f, 0.0f);
        for (int k = 0; k < 9; ++k) flask(plate, 310.0f + static_cast<float>(k) * 42.0f, y, 0.7f + 0.2f * static_cast<float>((k + s) % 2), 0.45f + 0.4f * static_cast<float>((k * 3 + s) % 3) / 2.0f);
    }
    table(plate, 300, 420, 300, 30);
    flask(plate, 220, 418, 1.4f, 0.7f);
    flask(plate, 300, 418, 1.0f, 0.4f);
    // A retort with its long neck.
    plate.fill(ellipse(340, 380, 44, 40), 0.75f, 30.0f); plate.stroke(ellipse(340, 380, 44, 40), 0.9f);
    juce::Path neck; neck.startNewSubPath(380, 392); neck.quadraticTo(420, 360, 450, 400);
    plate.stroke(neck, 3.0f);
    cauldron(plate, 680, 500, 1.4f, true);
    // A skull and a candle, hanging herbs.
    skull(plate, 410, 410, 1.2f);
    candle(plate, 250, 420, 20);
    for (int k = 0; k < 6; ++k) {
        const float x = 120.0f + static_cast<float>(k) * 140.0f;
        plate.line(x, 0, x, 50, 0.8f);
        for (int l = 0; l < 4; ++l) plate.line(x, 50, x - 12.0f + static_cast<float>(l) * 8.0f, 80, 0.7f);
    }
}

void wizardObservatory(Plate& plate) {
    // A domed room open to the night sky through its slit, a great telescope.
    plate.fillGradient(rect(0, 0, W, H), 0.55f, { 480, 400 }, 0.2f, { 480, 0 }, 30.0f, true);
    juce::Path dome; dome.startNewSubPath(0, 300); dome.quadraticTo(480, -150, W, 300); dome.lineTo(W, 0); dome.lineTo(0, 0); dome.closeSubPath();
    juce::Path sky = poly({ { 380, 0 }, { 580, 0 }, { 560, 260 }, { 400, 260 } });
    plate.fill(sky, 0.12f, 0.0f);
    stars(plate, { 380, 0, 200, 260 }, 40, 261);
    plate.stroke(sky, 1.4f);
    for (int k = 0; k < 9; ++k) {
        juce::Path rib; const float x = static_cast<float>(k) * 120.0f;
        rib.startNewSubPath(x, 340); rib.quadraticTo(480.0f + (x - 480.0f) * 0.2f, -40, 480.0f + (x - 480.0f) * 0.08f, 0);
        if (std::abs(x - 480) > 80) plate.stroke(rib, 1.0f, 0.7f);
    }
    perspectiveFloor(plate, 400, { 480, 300 }, 0.6f, 14, 0.4f);
    // The telescope on its mount, pointing up through the slit.
    juce::Path tube = poly({ { 340, 380 }, { 360, 400 }, { 560, 140 }, { 530, 120 } });
    plate.fillGradient(tube, 0.85f, { 350, 380 }, 0.3f, { 560, 140 }, 45.0f);
    plate.stroke(tube, 1.4f);
    plate.fill(ellipse(522, 110, 46, 30), 0.5f, 0.0f); plate.stroke(ellipse(522, 110, 46, 30), 1.0f);
    juce::Path mount = poly({ { 380, 520 }, { 400, 390 }, { 420, 390 }, { 460, 520 } });
    plate.fill(mount, 0.35f, 90.0f); plate.stroke(mount, 1.2f);
    // An armillary sphere and star charts.
    for (int k = 0; k < 3; ++k) plate.stroke(ellipse(720.0f - static_cast<float>(k) * 10.0f, 380.0f + static_cast<float>(k) * 12.0f, 90.0f + static_cast<float>(k) * 20.0f, 90.0f - static_cast<float>(k) * 30.0f), 1.0f);
    plate.line(765, 470, 765, 520, 2.0f);
    juce::Path chart = poly({ { 120, 360 }, { 260, 350 }, { 270, 440 }, { 130, 450 } });
    plate.fill(chart, 0.9f, 0.0f); plate.stroke(chart, 0.9f);
    plate.stroke(ellipse(160, 370, 70, 60), 0.6f, 0.7f);
    candle(plate, 290, 440, 20);
}

// --- Dispatch ---------------------------------------------------------------------
const std::array<std::array<Draw, placesPerLocation>, LocationCount> scenes {{
    {{ dungeonCorridors, dungeonEntrance, dungeonCells, dungeonTreasure, dungeonLair }},
    {{ castleCourtyard, castleGreatHall, castleThrone, castleChapel, castleTowerTop }},
    {{ cathedralCloister, cathedralNave, cathedralCrypt, cathedralBellTower, nullptr }},
    {{ forestEdge, forestOldPath, forestHut, forestGrove, nullptr }},
    {{ caveMouth, caveCrystal, caveLake, caveHoard, nullptr }},
    {{ villageGreen, villageTavern, villageMarket, villageSmithy, nullptr }},
    {{ swampMire, swampBog, swampRuins, nullptr, nullptr }},
    {{ mountainsFoothills, mountainsPass, mountainsPeak, mountainsDwarven, nullptr }},
    {{ necropolisGates, necropolisGraveyard, necropolisMausoleum, nullptr, nullptr }},
    {{ wizardStairwell, wizardLibrary, wizardAlchemy, wizardObservatory, nullptr }},
}};

// Where each place of a location sits in that location's scenes (its door, stair or
// feature). Index = sub-location (0 = the location's own scene).
const std::array<std::array<P, placesPerLocation>, LocationCount> hotspotPlaces {{
    {{ P { 110, 330 }, P { 300, 260 }, P { 700, 300 }, P { 480, 230 }, P { 860, 300 } }},   // Dungeon
    {{ P { 480, 400 }, P { 200, 240 }, P { 760, 240 }, P { 320, 320 }, P { 640, 120 } }},   // Castle
    {{ P { 140, 300 }, P { 480, 220 }, P { 300, 420 }, P { 800, 220 }, P {} }},              // Cathedral
    {{ P { 140, 420 }, P { 470, 300 }, P { 780, 340 }, P { 300, 260 }, P {} }},              // Haunted Forest
    {{ P { 480, 320 }, P { 200, 300 }, P { 760, 330 }, P { 600, 420 }, P {} }},              // Caverns
    {{ P { 480, 420 }, P { 200, 300 }, P { 760, 300 }, P { 350, 360 }, P {} }},              // Village
    {{ P { 200, 400 }, P { 520, 380 }, P { 800, 320 }, P {}, P {} }},                        // Swamp
    {{ P { 160, 430 }, P { 480, 330 }, P { 700, 160 }, P { 300, 280 }, P {} }},             // Mountains
    {{ P { 480, 430 }, P { 250, 330 }, P { 720, 330 }, P {}, P {} }},                        // Necropolis
    {{ P { 480, 470 }, P { 200, 300 }, P { 760, 300 }, P { 480, 160 }, P {} }},              // Wizard's Tower
}};
}   // namespace

bool hasIllustration(SceneId id) {
    if (id.isMap()) return true;
    if (id.location >= LocationCount) return false;
    return scenes[static_cast<std::size_t>(id.location)][static_cast<std::size_t>(clampSubLocation(id.location, id.sub))] != nullptr;
}

void drawPlace(Plate& plate, SceneId id, bool& drawn) {
    drawn = false;
    if (id.location < 0 || id.location >= LocationCount) return;
    const int sub = clampSubLocation(id.location, id.sub);
    if (auto draw = scenes[static_cast<std::size_t>(id.location)][static_cast<std::size_t>(sub)]) {
        draw(plate);
        // The plate's border, as on the map.
        juce::Path border; border.addRectangle(6, 6, W - 12, H - 12);
        plate.stroke(border, 1.6f);
        drawn = true;
    }
}

std::vector<Hotspot> sceneHotspots(int location, int) {
    std::vector<Hotspot> spots;
    if (location < 0 || location >= LocationCount) return spots;
    for (int k = 0; k <= subLocationCount(location); ++k)
        spots.push_back({ k, hotspotPlaces[static_cast<std::size_t>(location)][static_cast<std::size_t>(k)], 26.0f });
    return spots;
}

std::vector<LightSource> sceneLights(SceneId id) {
    std::vector<LightSource> lights;
    if (id.isMap()) return lights;
    Plate probe(4, 3);   // drawing into a tiny plate is cheap; only the lights are kept
    bool drawn = false;
    drawPlace(probe, id, drawn);
    for (const auto& l : probe.recordedLights()) lights.push_back({ l.centre, l.radius, l.kind });
    return lights;
}
}
