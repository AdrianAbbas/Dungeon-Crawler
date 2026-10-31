#include "Engraving.h"
#include <array>
#include <cmath>

namespace dc::art {
namespace {
inline std::uint32_t hash(std::int32_t x, std::int32_t y, std::uint32_t seed) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343u ^ static_cast<std::uint32_t>(y) * 0xd8163841u ^ seed * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
inline float smooth(float t) noexcept { return t * t * (3.0f - 2.0f * t); }
juce::Colour fieldColour(float tone, float angle, float accent) {
    angle = std::fmod(angle, 180.0f);
    if (angle < 0) angle += 180.0f;
    return juce::Colour::fromRGB(static_cast<juce::uint8>(juce::jlimit(0.0f, 1.0f, tone) * 255.0f + 0.5f),
                                 static_cast<juce::uint8>(angle / 180.0f * 255.0f + 0.5f),
                                 static_cast<juce::uint8>(juce::jlimit(0.0f, 1.0f, accent) * 255.0f + 0.5f));
}
}

float noise2(float x, float y, std::uint32_t seed) noexcept {
    const float fx = std::floor(x), fy = std::floor(y);
    const auto ix = static_cast<std::int32_t>(fx), iy = static_cast<std::int32_t>(fy);
    const float tx = smooth(x - fx), ty = smooth(y - fy);
    const auto v = [seed](std::int32_t a, std::int32_t b) { return static_cast<float>(hash(a, b, seed) & 0xffffu) / 65535.0f; };
    const float a = v(ix, iy), b = v(ix + 1, iy), c = v(ix, iy + 1), d = v(ix + 1, iy + 1);
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
}

Plate::Plate(int widthPixels, int heightPixels)
    : w(juce::jmax(1, widthPixels)), h(juce::jmax(1, heightPixels)),
      toPixels(juce::AffineTransform::scale(static_cast<float>(w) / virtualWidth, static_cast<float>(h) / virtualHeight)),
      scaleOnly(toPixels),
      fields(juce::Image::ARGB, w, h, false, juce::SoftwareImageType()),
      inkMask(juce::Image::SingleChannel, w, h, true, juce::SoftwareImageType()),
      accentMask(juce::Image::SingleChannel, w, h, true, juce::SoftwareImageType()) {
    fields.clear(fields.getBounds(), fieldColour(1.0f, 45.0f, 0.0f));
}

void Plate::setOffset(float dx, float dy) noexcept {
    offset = { dx, dy };
    toPixels = juce::AffineTransform::translation(dx, dy).followedBy(scaleOnly);
}

void Plate::fill(const juce::Path& path, float tone, float angle, float accent) {
    juce::Graphics g(fields);
    g.addTransform(toPixels);
    g.setColour(fieldColour(tone, angle, accent));
    g.fillPath(path);
}

void Plate::fillRect(juce::Rectangle<float> r, float tone, float angle, float accent) {
    juce::Path p; p.addRectangle(r); fill(p, tone, angle, accent);
}

void Plate::fillEllipse(juce::Rectangle<float> r, float tone, float angle, float accent) {
    juce::Path p; p.addEllipse(r); fill(p, tone, angle, accent);
}

void Plate::fillGradient(const juce::Path& path, float toneA, juce::Point<float> a, float toneB, juce::Point<float> b, float angle, bool radial) {
    juce::Graphics g(fields);
    g.addTransform(toPixels);
    g.setGradientFill(juce::ColourGradient(fieldColour(toneA, angle, 0), a, fieldColour(toneB, angle, 0), b, radial));
    g.fillPath(path);
}

void Plate::stroke(const juce::Path& path, float thickness, float alpha) {
    juce::Graphics g(inkMask);
    g.addTransform(toPixels);
    g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 1.0f, alpha)));
    g.strokePath(path, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Plate::line(float x1, float y1, float x2, float y2, float thickness, float alpha) {
    juce::Path p; p.startNewSubPath(x1, y1); p.lineTo(x2, y2); stroke(p, thickness, alpha);
}

void Plate::fillInk(const juce::Path& path, float alpha) {
    juce::Graphics g(inkMask);
    g.addTransform(toPixels);
    g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 1.0f, alpha)));
    g.fillPath(path);
}

void Plate::fillAccent(const juce::Path& path, float alpha) {
    juce::Graphics g(accentMask);
    g.addTransform(toPixels);
    g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 1.0f, alpha)));
    g.fillPath(path);
}

void Plate::strokeAccent(const juce::Path& path, float thickness, float alpha) {
    juce::Graphics g(accentMask);
    g.addTransform(toPixels);
    g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 1.0f, alpha)));
    g.strokePath(path, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Plate::glow(juce::Point<float> centre, float radius, float strength) {
    glows.push_back({ centre + offset, radius, strength });
    lights.push_back({ centre + offset, radius, radius < 32.0f ? 1 : 0 });
}

juce::Image Plate::render(const Style& style, const std::atomic<bool>* abort) const {
    juce::Image out(juce::Image::RGB, w, h, false, juce::SoftwareImageType());
    const juce::Image::BitmapData f(fields, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData ink(inkMask, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData red(accentMask, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData o(out, juce::Image::BitmapData::writeOnly);

    const float scale = static_cast<float>(w) / virtualWidth;
    const float spacing = style.spacing * scale;       // hatch period in pixels
    const float aa = 1.0f / std::max(0.5f, spacing);   // one pixel in periods
    std::array<float, 256> cosA {}, sinA {};
    for (int i = 0; i < 256; ++i) {
        const float a = static_cast<float>(i) / 255.0f * juce::MathConstants<float>::pi;
        cosA[static_cast<std::size_t>(i)] = std::cos(a);
        sinA[static_cast<std::size_t>(i)] = std::sin(a);
    }
    // Cross-hatch directions relative to the main one.
    const float c2 = std::cos(1.22f), s2 = std::sin(1.22f), c3 = std::cos(-0.87f), s3 = std::sin(-0.87f);
    const float paperR = style.paper.getFloatRed(), paperG = style.paper.getFloatGreen(), paperB = style.paper.getFloatBlue();
    const float inkR = style.ink.getFloatRed(), inkG = style.ink.getFloatGreen(), inkB = style.ink.getFloatBlue();
    const float redR = style.accent.getFloatRed(), redG = style.accent.getFloatGreen(), redB = style.accent.getFloatBlue();
    const float glowR = style.glow.getFloatRed(), glowG = style.glow.getFloatGreen(), glowB = style.glow.getFloatBlue();
    const std::uint32_t seed = style.seed;
    const float cx = static_cast<float>(w) * 0.5f, cy = static_cast<float>(h) * 0.48f;
    const float invR2 = 1.0f / (cx * cx + cy * cy);
    // Glows in pixels.
    struct PixelGlow { float x, y, r2, s; };
    std::vector<PixelGlow> pixelGlows;
    for (const auto& gl : glows) pixelGlows.push_back({ gl.centre.x * scale, gl.centre.y * scale, gl.radius * gl.radius * scale * scale, gl.strength });

    const auto lineInk = [aa](float u, float halfWidth) {
        const float frac = u - std::floor(u);
        const float dist = std::min(frac, 1.0f - frac);
        return juce::jlimit(0.0f, 1.0f, (halfWidth - dist) / aa + 0.5f);
    };

    for (int y = 0; y < h; ++y) {
        if (abort != nullptr && abort->load(std::memory_order_relaxed)) return {};
        const auto* fr = f.getLinePointer(y);
        const auto* ir = ink.getLinePointer(y);
        const auto* rr = red.getLinePointer(y);
        auto* orow = o.getLinePointer(y);
        const float fy = static_cast<float>(y);
        for (int x = 0; x < w; ++x) {
            const auto* px = fr + x * f.pixelStride;
            // ARGB images are stored as BGRA in memory.
            const float tone = px[2] / 255.0f;
            const auto angleIndex = static_cast<std::size_t>(px[1]);
            const float accent = px[0] / 255.0f;
            const float fx = static_cast<float>(x);
            const float sx = fx / scale, sy = fy / scale;   // virtual coordinates for textures
            const float dark = 1.0f - tone;
            float coverage = 0;
            if (dark > 0.02f) {
                const float wobble = (noise2(sx * 0.02f, sy * 0.02f, seed) - 0.5f) * 0.6f + (noise2(sx * 0.15f, sy * 0.15f, seed + 7) - 0.5f) * 0.06f;
                const float swell = 0.9f + 0.2f * noise2(sx * 0.05f, sy * 0.05f, seed + 3);
                const float ca = cosA[angleIndex], sa = sinA[angleIndex];
                // Angle 0 = horizontal lines: the coordinate across the lines.
                const float u1 = (fy * ca - fx * sa) / spacing + wobble;
                const float w1 = juce::jlimit(0.0f, 1.0f, dark / 0.6f) * 0.4f * swell;
                float keep = 1.0f - lineInk(u1, w1);
                if (dark > 0.45f) {
                    const float ca2 = ca * c2 - sa * s2, sa2 = sa * c2 + ca * s2;
                    const float u2 = (fy * ca2 - fx * sa2) / (spacing * 1.25f) + wobble * 0.5f;
                    keep *= 1.0f - lineInk(u2, juce::jlimit(0.0f, 1.0f, (dark - 0.45f) / 0.4f) * 0.3f * swell);
                }
                if (dark > 0.78f) {
                    const float ca3 = ca * c3 - sa * s3, sa3 = sa * c3 + ca * s3;
                    const float u3 = (fy * ca3 - fx * sa3) / (spacing * 1.5f) + wobble * 0.4f;
                    keep *= 1.0f - lineInk(u3, juce::jlimit(0.0f, 1.0f, (dark - 0.8f) / 0.2f) * 0.28f);
                }
                coverage = 1.0f - keep;
            }
            const float strokeInk = ir[x * ink.pixelStride] / 255.0f;
            const float strokeRed = rr[x * red.pixelStride] / 255.0f;
            coverage = 1.0f - (1.0f - coverage) * (1.0f - strokeInk);

            // Dark parchment: fibres, foxing, vignette.
            const float fibre = noise2(sx * 0.6f, sy * 0.06f, seed + 11) * 0.6f + noise2(sx * 0.05f, sy * 0.6f, seed + 13) * 0.4f;
            const float foxing = noise2(sx * 0.006f, sy * 0.006f, seed + 17);
            const float dx = fx - cx, dy = fy - cy;
            const float vig = 1.0f - style.vignette * std::pow((dx * dx + dy * dy) * invR2, 1.3f);
            const float paperShade = (0.86f + 0.1f * fibre + 0.12f * (foxing - 0.5f)) * vig;
            float r = paperR * paperShade, g = paperG * paperShade, b = paperB * paperShade;
            // Ink: charcoal, or blood-red where the accent asks.
            const float lr = inkR + (redR - inkR) * accent, lg = inkG + (redG - inkG) * accent, lb = inkB + (redB - inkB) * accent;
            r += (lr - r) * coverage; g += (lg - g) * coverage; b += (lb - b) * coverage;
            if (strokeRed > 0.0f) { r += (redR - r) * strokeRed; g += (redG - g) * strokeRed; b += (redB - b) * strokeRed; }
            for (const auto& gl : pixelGlows) {
                const float ddx = fx - gl.x, ddy = fy - gl.y, d2 = ddx * ddx + ddy * ddy;
                if (d2 >= gl.r2) continue;
                const float k = (1.0f - d2 / gl.r2);
                const float s = gl.s * k * k;
                r = 1.0f - (1.0f - r) * (1.0f - glowR * s); g = 1.0f - (1.0f - g) * (1.0f - glowG * s); b = 1.0f - (1.0f - b) * (1.0f - glowB * s);
            }
            auto* op = orow + x * o.pixelStride;
            op[2] = static_cast<juce::uint8>(juce::jlimit(0.0f, 1.0f, r) * 255.0f);
            op[1] = static_cast<juce::uint8>(juce::jlimit(0.0f, 1.0f, g) * 255.0f);
            op[0] = static_cast<juce::uint8>(juce::jlimit(0.0f, 1.0f, b) * 255.0f);
        }
    }
    return out;
}
}
