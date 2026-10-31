#include "Scenes.h"
#include "Motifs.h"
#include "Painted.h"

namespace dc::art {
using namespace motif;

namespace {
// Map landmarks, virtual coordinates. Order = Location order.
const std::array<Landmark, LocationCount> landmarks {{
    { Dungeon, { 560, 292 }, 42 },
    { Castle, { 452, 178 }, 48 },
    { Cathedral, { 648, 196 }, 42 },
    { HauntedForest, { 248, 268 }, 52 },
    { Caverns, { 770, 372 }, 44 },
    { Village, { 410, 372 }, 44 },
    { Swamp, { 178, 396 }, 46 },
    { Mountains, { 812, 150 }, 54 },
    { Necropolis, { 872, 282 }, 40 },
    { WizardsTower, { 132, 196 }, 42 },
}};

juce::Path textPath(const juce::String& text, juce::Font font, float x, float y, float width, juce::Justification just = juce::Justification::centred) {
    juce::GlyphArrangement glyphs;
    glyphs.addFittedText(font, text, x, y, width, font.getHeight() * 1.4f, just, 1);
    juce::Path p;
    glyphs.createPath(p);
    return p;
}
juce::Font serif(float height, bool bold = false) {
    return juce::Font(juce::FontOptions("Georgia", height, bold ? juce::Font::bold : juce::Font::plain));
}

// The coastline of the western sea (the map's left and bottom-left edge).
juce::Path coastline() {
    juce::Path c;
    c.startNewSubPath(0, 330);
    const P pts[] = { { 22, 336 }, { 40, 352 }, { 34, 372 }, { 58, 392 }, { 70, 418 }, { 62, 440 }, { 88, 470 },
                      { 120, 498 }, { 160, 512 }, { 200, 530 }, { 252, 538 }, { 300, 560 } };
    for (auto p : pts) c.lineTo(p);
    return c;
}
juce::Path sea() {
    juce::Path s = coastline();
    s.lineTo(0, 560);
    s.closeSubPath();
    return s;
}

void cottage(Plate& plate, float x, float base, float s, bool flip) {
    const float w = 22 * s, h = 13 * s;
    juce::Path wall; wall.addRectangle(x - w * 0.5f, base - h, w, h);
    plate.fill(wall, 0.8f, 90.0f);
    plate.stroke(wall, 0.7f);
    juce::Path roof = poly({ { x - w * 0.62f, base - h }, { x + (flip ? 0.15f : -0.15f) * w, base - h - 11 * s }, { x + w * 0.62f, base - h } });
    plate.fill(roof, 0.28f, flip ? 125.0f : 55.0f);
    plate.stroke(roof, 0.8f);
    juce::Path door; door.addRectangle(x - 2.5f * s, base - 7 * s, 5 * s, 7 * s);
    plate.fill(door, 0.08f, 0.0f);
}

void tombstone(Plate& plate, float x, float base, float s, bool cross) {
    if (cross) {
        plate.line(x, base, x, base - 14 * s, 2.0f * s);
        plate.line(x - 5 * s, base - 10 * s, x + 5 * s, base - 10 * s, 2.0f * s);
        return;
    }
    juce::Path t = roundArch(x, base, 9 * s, 13 * s);
    plate.fill(t, 0.55f, 90.0f);
    plate.stroke(t, 0.7f);
}

void reeds(Plate& plate, float x, float base, float s, std::uint32_t seed) {
    for (int k = 0; k < 5; ++k) {
        const float dx = (static_cast<float>(k) - 2.0f) * 2.2f * s;
        const float lean = (noise2(static_cast<float>(k), x, seed) - 0.5f) * 6.0f * s;
        plate.line(x + dx, base, x + dx + lean, base - (8 + 6 * noise2(x, static_cast<float>(k), seed + 1)) * s, 0.6f);
    }
}

void compassRose(Plate& plate, P c, float r) {
    juce::Path ring; ring.addEllipse(c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);
    plate.stroke(ring, 0.8f);
    juce::Path ring2; ring2.addEllipse(c.x - r * 0.55f, c.y - r * 0.55f, r * 1.1f, r * 1.1f);
    plate.stroke(ring2, 0.5f);
    for (int k = 0; k < 8; ++k) {
        const float a = static_cast<float>(k) * juce::MathConstants<float>::pi / 4.0f - juce::MathConstants<float>::halfPi;
        const float len = (k % 2 == 0) ? r : r * 0.55f, side = r * 0.13f;
        const P tip { c.x + std::cos(a) * len, c.y + std::sin(a) * len };
        const P l { c.x + std::cos(a - 1.5708f) * side, c.y + std::sin(a - 1.5708f) * side };
        const P rr { c.x + std::cos(a + 1.5708f) * side, c.y + std::sin(a + 1.5708f) * side };
        plate.fill(poly({ c, l, tip }), 0.92f, 0.0f);
        plate.fill(poly({ c, tip, rr }), 0.12f, 45.0f);
        plate.stroke(poly({ l, tip, rr }), 0.6f);
    }
    plate.fillInk(textPath("N", serif(15.0f, true), c.x - 20, c.y - r - 24, 40));
}

void cartouche(Plate& plate, juce::Rectangle<float> r) {
    // A scroll: the banner with curled ends, a double rule, the title with a red initial.
    juce::Path banner;
    banner.addRoundedRectangle(r, 6.0f);
    plate.fill(banner, 0.9f, 0.0f);
    plate.stroke(banner, 1.4f);
    juce::Path inner; inner.addRoundedRectangle(r.reduced(5.0f), 4.0f);
    plate.stroke(inner, 0.6f);
    for (int side = 0; side < 2; ++side) {
        const float x = side == 0 ? r.getX() : r.getRight();
        const float dir = side == 0 ? -1.0f : 1.0f;
        juce::Path curl;
        curl.addEllipse(x + dir * 10.0f - 9.0f, r.getCentreY() - 16.0f, 18.0f, 32.0f);
        plate.fill(curl, 0.55f, 90.0f);
        plate.stroke(curl, 1.0f);
        juce::Path inner2; inner2.addEllipse(x + dir * 10.0f - 4.0f, r.getCentreY() - 8.0f, 8.0f, 16.0f);
        plate.stroke(inner2, 0.6f);
    }
    const auto titleFont = serif(25.0f, true).withExtraKerningFactor(0.05f);
    const auto initial = serif(34.0f, true);
    juce::GlyphArrangement rest;
    rest.addLineOfText(titleFont, "UNGEON CRAWLER", 0, 0);
    juce::GlyphArrangement first;
    first.addLineOfText(initial, "D", 0, 0);
    const float total = first.getBoundingBox(0, -1, true).getWidth() + rest.getBoundingBox(0, -1, true).getWidth() + 2.0f;
    const float x0 = r.getCentreX() - total * 0.5f, baseline = r.getY() + 44.0f;
    juce::Path dPath; first.createPath(dPath);
    dPath.applyTransform(juce::AffineTransform::translation(x0, baseline));
    plate.fillAccent(dPath);
    juce::Path restPath; rest.createPath(restPath);
    restPath.applyTransform(juce::AffineTransform::translation(x0 + first.getBoundingBox(0, -1, true).getWidth() + 2.0f, baseline));
    plate.fillInk(restPath);
    plate.fillInk(textPath("a map of the dark realm", serif(12.5f).italicised(), r.getX(), baseline + 6.0f, r.getWidth()), 0.85f);
}

void drawMap(Plate& plate) {
    // Paper: lightly hatched land, darker at the edges.
    backdrop(plate, 0.93f, 0.86f, 30.0f);
    // The western sea: horizontal ripples, darker away from the shore, with the
    // engraver's coastline hatching (offset copies of the shore).
    plate.fillGradient(sea(), 0.66f, { 60, 380 }, 0.45f, { 0, 560 }, 0.0f);
    for (int k = 1; k <= 6; ++k) {
        juce::Path off = coastline();
        off.applyTransform(juce::AffineTransform::translation(-5.0f * static_cast<float>(k), 4.0f * static_cast<float>(k)));
        plate.stroke(off, 0.5f, 0.75f - 0.1f * static_cast<float>(k));
    }
    plate.stroke(coastline(), 1.6f);
    // A sea serpent's coils and a ship's wake would be next; keep the sea quiet.
    for (int k = 0; k < 6; ++k) {
        const float x = 20.0f + static_cast<float>(k) * 30.0f, y = 470.0f + static_cast<float>(k % 3) * 22.0f;
        juce::Path wave; wave.startNewSubPath(x, y); wave.quadraticTo(x + 6, y - 4, x + 12, y); wave.quadraticTo(x + 18, y - 4, x + 24, y);
        if (x < 40 + static_cast<float>(k) * 25) plate.stroke(wave, 0.6f, 0.8f);
    }

    // A river from the mountains to the sea, past the village.
    {
        juce::Path river;
        river.startNewSubPath(700, 214);
        river.cubicTo(640, 260, 610, 360, 520, 380);
        river.cubicTo(450, 396, 470, 470, 380, 488);
        river.cubicTo(320, 500, 300, 520, 258, 540);
        juce::Path banks;
        juce::PathStrokeType(9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(banks, river);
        plate.fill(banks, 0.72f, 0.0f);
        plate.stroke(banks, 0.7f);
        juce::Path bridge; bridge.addRectangle(470, 440, 14, 5);
        plate.fill(bridge, 0.6f, 90.0f); plate.stroke(bridge, 0.6f);
    }
    // Scattered woods across the land.
    for (int k = 0; k < 70; ++k) {
        const float x = 300.0f + noise2(static_cast<float>(k) * 2.3f, 9.0f, 81) * 600.0f;
        const float y = 230.0f + noise2(9.0f, static_cast<float>(k) * 1.9f, 83) * 300.0f;
        const auto near = [&](const Landmark& l) { return l.position.getDistanceFrom({ x, y }) < l.radius * 1.35f; };
        if (std::any_of(landmarks.begin(), landmarks.end(), near) || (x > 860 && y > 430)) continue;
        if (k % 3 == 0) oak(plate, x, y, 12.0f, static_cast<std::uint32_t>(k), 0.5f);
        else conifer(plate, x, y, 11.0f, 0.35f);
    }

    // Mountains along the north-east, the realm's spine.
    mountain(plate, 600, 150, 90, 70, 3);
    mountain(plate, 660, 120, 110, 92, 5, true);
    mountain(plate, 735, 160, 120, 118, 7, true);
    mountain(plate, 830, 140, 115, 108, 11, true);
    mountain(plate, 885, 195, 90, 80, 13);
    mountain(plate, 760, 205, 80, 55, 17);
    mountain(plate, 240, 150, 70, 44, 19);
    mountain(plate, 290, 140, 60, 36, 23);
    // Rolling hills (small arcs).
    for (int k = 0; k < 18; ++k) {
        const float x = 300.0f + noise2(static_cast<float>(k), 2.0f, 41) * 520.0f, y = 230.0f + noise2(3.0f, static_cast<float>(k), 43) * 260.0f;
        juce::Path hill; hill.startNewSubPath(x - 12, y); hill.quadraticTo(x, y - 9, x + 12, y);
        const auto near = [&](const Landmark& l) { return l.position.getDistanceFrom({ x, y }) < l.radius * 1.1f; };
        if (std::none_of(landmarks.begin(), landmarks.end(), near)) plate.stroke(hill, 0.6f, 0.7f);
    }

    // Roads: dotted lines between the places.
    const auto road = [&](P a, P b, std::uint32_t seed) {
        const float len = a.getDistanceFrom(b);
        const int dots = static_cast<int>(len / 7.0f);
        const P n { -(b.y - a.y) / len, (b.x - a.x) / len };
        for (int k = 1; k < dots; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(dots);
            const float bend = std::sin(t * juce::MathConstants<float>::pi) * (noise2(static_cast<float>(seed), 0.3f, seed) - 0.5f) * 40.0f;
            const P p = a + (b - a) * t + n * bend;
            juce::Path d; d.addEllipse(p.x - 0.9f, p.y - 0.9f, 1.8f, 1.8f);
            plate.fillInk(d, 0.8f);
        }
    };
    const auto at = [](int l) { return landmarks[static_cast<std::size_t>(l)].position; };
    road(at(Village), at(Castle), 1); road(at(Village), at(Dungeon), 2); road(at(Castle), at(Cathedral), 3);
    road(at(Dungeon), at(Caverns), 4); road(at(Village), at(Swamp), 5); road(at(HauntedForest), at(Village), 6);
    road(at(HauntedForest), at(WizardsTower), 7); road(at(Cathedral), at(Necropolis), 8); road(at(Cathedral), at(Mountains), 9);
    road(at(Dungeon), at(Necropolis), 10); road(at(Castle), at(HauntedForest), 11);

    // --- Landmarks
    // Wizard's Tower: a tall tower on a crag.
    rock(plate, poly({ { 92, 236 }, { 108, 214 }, { 126, 210 }, { 150, 220 }, { 172, 238 } }), 0.5f, 120.0f, 1);
    tower(plate, 132, 214, 20, 74, true, 0.7f);
    plate.fill(poly({ { 127, 168 }, { 137, 168 }, { 137, 176 }, { 127, 176 } }), 0.05f, 0.0f);
    plate.setOffset(0, -24);
    // Haunted Forest: a dense wood of dark conifers and dead trees.
    for (int k = 0; k < 26; ++k) {
        const float x = 196.0f + noise2(static_cast<float>(k) * 1.7f, 0.2f, 51) * 110.0f;
        const float y = 262.0f + noise2(0.4f, static_cast<float>(k) * 1.3f, 53) * 70.0f;
        if (k % 7 == 3) deadTree(plate, x, y, 26.0f, static_cast<std::uint32_t>(k));
        else conifer(plate, x, y, 20.0f + 8.0f * noise2(static_cast<float>(k), 0.9f, 55), 0.22f);
    }
    plate.setOffset(0, 0);
    // Castle on its hill.
    rock(plate, poly({ { 396, 214 }, { 420, 194 }, { 452, 188 }, { 486, 194 }, { 512, 214 } }), 0.72f, 20.0f, 2);
    tower(plate, 422, 196, 16, 42, false);
    tower(plate, 482, 196, 16, 42, false);
    tower(plate, 452, 192, 22, 62, true, 0.66f);
    juce::Path wall; wall.addRectangle(430, 174, 44, 22);
    plate.fill(wall, 0.6f, 90.0f); plate.stroke(wall, 0.8f);
    flag(plate, 452, 128, 14, 1.0f);
    // Cathedral: a nave and two spires, a rose window.
    juce::Path nave; nave.addRectangle(622, 186, 56, 26);
    plate.fill(nave, 0.7f, 90.0f); plate.stroke(nave, 0.8f);
    tower(plate, 628, 212, 14, 52, true, 0.66f);
    tower(plate, 672, 212, 14, 52, true, 0.66f);
    juce::Path rose; rose.addEllipse(644, 190, 12, 12);
    plate.fill(rose, 0.1f, 0.0f); plate.stroke(rose, 0.6f);
    plate.fill(pointedArch(650, 212, 9, 14), 0.05f, 0.0f);
    plate.setOffset(0, -26);
    // Dungeon: a black arch into a rocky outcrop, steps down.
    rock(plate, poly({ { 520, 342 }, { 528, 312 }, { 548, 296 }, { 576, 294 }, { 598, 312 }, { 604, 342 } }), 0.52f, 135.0f, 3);
    plate.fill(roundArch(562, 342, 24, 30), 0.02f, 0.0f);
    plate.stroke(roundArch(562, 342, 24, 30), 1.0f);
    for (int k = 0; k < 4; ++k) plate.line(552.0f + static_cast<float>(k) * 1.5f, 336.0f - static_cast<float>(k) * 5.0f, 572.0f - static_cast<float>(k) * 1.5f, 336.0f - static_cast<float>(k) * 5.0f, 0.6f, 0.6f);
    plate.setOffset(0, 0);
    plate.setOffset(0, -46);
    // Caverns: cliffs with two cave mouths.
    rock(plate, poly({ { 720, 446 }, { 728, 408 }, { 752, 386 }, { 790, 382 }, { 818, 398 }, { 826, 446 } }), 0.6f, 110.0f, 4);
    plate.fill(roundArch(758, 446, 22, 26), 0.03f, 0.0f);
    plate.fill(roundArch(796, 440, 14, 18), 0.03f, 0.0f);
    plate.stroke(roundArch(758, 446, 22, 26), 0.9f);
    plate.setOffset(0, 0);
    plate.setOffset(0, -46);
    // Village: cottages around a green.
    cottage(plate, 386, 418, 1.0f, false); cottage(plate, 414, 410, 1.1f, true); cottage(plate, 440, 424, 0.9f, false);
    cottage(plate, 398, 438, 0.85f, true); cottage(plate, 430, 444, 0.95f, false);
    oak(plate, 364, 440, 22, 61); oak(plate, 458, 404, 18, 63);
    plate.setOffset(0, 0);
    plate.setOffset(0, -56);
    // Swamp: pools and reeds near the coast.
    for (int k = 0; k < 5; ++k) {
        const float x = 140.0f + static_cast<float>(k) * 18.0f, y = 440.0f + static_cast<float>(k % 2) * 16.0f;
        juce::Path pool; pool.addEllipse(x - 14, y - 4, 28, 8);
        plate.fill(pool, 0.55f, 0.0f);
        plate.stroke(pool, 0.6f, 0.8f);
        reeds(plate, x + 12, y - 2, 1.0f, static_cast<std::uint32_t>(k));
    }
    deadTree(plate, 214, 452, 22, 71);
    plate.setOffset(0, 0);
    // Mountains landmark: a great peak with a dwarven gate.
    plate.fill(roundArch(814, 160, 12, 14), 0.03f, 0.0f);
    plate.setOffset(0, -18);
    // Necropolis: a walled field of tombstones and a mausoleum.
    for (int k = 0; k < 10; ++k) {
        const float x = 846.0f + static_cast<float>(k % 5) * 12.0f, y = 302.0f + static_cast<float>(k / 5) * 16.0f;
        tombstone(plate, x, y, 0.8f, (k * 7) % 3 == 0);
    }
    juce::Path mausoleum = poly({ { 880, 296 }, { 880, 280 }, { 892, 270 }, { 904, 280 }, { 904, 296 } });
    plate.fill(mausoleum, 0.62f, 90.0f); plate.stroke(mausoleum, 0.8f);
    deadTree(plate, 914, 318, 24, 73);

    plate.setOffset(0, 0);
    // Labels: small engraved capitals under each landmark, on a cleared ribbon.
    for (const auto& l : landmarks) {
        const auto name = juce::String(locationNames[static_cast<std::size_t>(l.location)]).toUpperCase();
        const auto font = serif(10.5f).withExtraKerningFactor(0.12f);
        const auto text = textPath(name, font, l.position.x - 70, l.position.y + l.radius * 0.62f, 140);
        const auto box = text.getBounds().expanded(5.0f, 3.0f);
        juce::Path ribbon; ribbon.addRoundedRectangle(box, 2.0f);
        plate.fill(ribbon, 0.97f, 0.0f);
        plate.stroke(ribbon, 0.5f, 0.6f);
        plate.fillInk(text, 0.95f);
    }
    plate.fillInk(textPath("The Western Sea", serif(13.0f).italicised(), 0, 500, 120), 0.8f);

    compassRose(plate, { 900, 396 }, 30);
    cartouche(plate, { 330, 16, 300, 68 });
    // The plate's border: a double rule with corner ornaments.
    juce::Path border; border.addRectangle(8, 8, 944, 544);
    plate.stroke(border, 1.6f);
    juce::Path border2; border2.addRectangle(13, 13, 934, 534);
    plate.stroke(border2, 0.6f);
}

// Placeholder scene for places without their own illustration yet (Milestone 8 draws
// them all): a stone archway framing the place's name.
void drawPlaceholder(Plate& plate, SceneId id) {
    backdrop(plate, 0.35f, 0.15f, 90.0f);
    perspectiveFloor(plate, 380, { 480, 300 }, 0.5f);
    plate.fill(pointedArch(480, 380, 300, 300), 0.08f, 0.0f);
    plate.stroke(pointedArch(480, 380, 300, 300), 2.0f);
    torch(plate, 290, 230);
    torch(plate, 670, 230);
    plate.fillInk(textPath(subLocationName(id.location, id.sub), serif(28.0f, true), 280, 440, 400), 0.9f);
}
}

const std::array<Landmark, LocationCount>& mapLandmarks() {
    // The painted map's landmarks where it is installed (its manifest), else the
    // engraved map's.
    static const std::array<Landmark, LocationCount> painted = [] {
        auto marks = landmarks;
        if (const auto* s = paintedScene({ -1, 0 }))
            for (int i = 0; i < s->anchorCount; ++i)
                if (s->anchors[i].kind == anchorLandmark) {
                    const int index = juce::roundToInt(s->anchors[i].value);
                    if (index >= 0 && index < LocationCount)
                        marks[static_cast<std::size_t>(index)].position = { s->anchors[i].x, s->anchors[i].y };
                }
        return marks;
    }();
    return isPainted({ -1, 0 }) ? painted : landmarks;
}

void drawPlace(Plate&, SceneId, bool& drawn);   // ScenePlaces.cpp

juce::Image renderScene(SceneId id, int widthPixels, int heightPixels, const std::atomic<bool>* abort, std::vector<LightSource>* lights) {
    Plate plate(widthPixels, heightPixels);
    Plate::Style style;
    if (id.isMap()) {
        drawMap(plate);
        style.paper = juce::Colour(0xffa39478);
        style.vignette = 0.6f;
        style.seed = 7;
    } else {
        bool drawn = false;
        drawPlace(plate, id, drawn);
        if (!drawn) drawPlaceholder(plate, id);
        style.seed = static_cast<std::uint32_t>(id.location * 8 + id.sub + 11);
    }
    if (lights != nullptr)
        for (const auto& l : plate.recordedLights()) lights->push_back({ l.centre, l.radius, l.kind });
    return plate.render(style, abort);
}

juce::String sceneSlug(SceneId id) {
    const auto slug = [](juce::String s) { return s.toLowerCase().removeCharacters("'").replaceCharacter(' ', '-'); };
    if (id.isMap()) return "map";
    return slug(locationNames[static_cast<std::size_t>(juce::jlimit(0, LocationCount - 1, id.location))]) + "/"
         + slug(subLocationName(id.location, id.sub));
}

juce::File overrideFile(SceneId id) {
    // macOS: ~/Library/Application Support/DungeonCrawler/scenes (JUCE's user data
    // folder there is ~/Library itself).
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile("Application Support");
   #endif
    return base.getChildFile("DungeonCrawler").getChildFile("scenes").getChildFile(sceneSlug(id) + ".png");
}
}
