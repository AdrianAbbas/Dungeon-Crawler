#include "Painted.h"
#include <cmath>

namespace dc::art {
namespace {
#include "PaintedManifest.inc"

juce::Image loadFile(const juce::File& f) {
    return f.existsAsFile() ? juce::ImageFileFormat::loadFrom(f) : juce::Image();
}

// A layer of the built-in art: <name>.jpg (colour) + <name>.png (alpha mask), joined
// into a premultiplied ARGB image at the files' resolution.
juce::Image loadLayer(const juce::File& folder, const juce::String& name) {
    auto colour = loadFile(folder.getChildFile(name + ".jpg"));
    if (colour.isNull()) return {};
    auto alpha = loadFile(folder.getChildFile(name + ".png"));
    if (alpha.isNull()) return colour.convertedToFormat(juce::Image::RGB);
    const int w = colour.getWidth(), h = colour.getHeight();
    if (alpha.getWidth() != w || alpha.getHeight() != h) alpha = alpha.rescaled(w, h);
    juce::Image out(juce::Image::ARGB, w, h, false, juce::SoftwareImageType());
    const juce::Image::BitmapData c(colour, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData a(alpha, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData o(out, juce::Image::BitmapData::writeOnly);
    // A greyscale PNG loads as RGB (grey in every channel) or, on some platforms, as a
    // single channel (the value in alpha).
    const bool single = alpha.getFormat() == juce::Image::SingleChannel;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const auto ap = a.getPixelColour(x, y);
            o.setPixelColour(x, y, c.getPixelColour(x, y).withAlpha(single ? ap.getAlpha() : ap.getRed()));   // premultiplies
        }
    return out;
}

// Drop-in layers: <name>.png (with alpha) or <name>.jpg (opaque).
juce::Image loadDropIn(const juce::File& folder, const juce::String& name) {
    auto image = loadFile(folder.getChildFile(name + ".png"));
    if (image.isNull()) image = loadFile(folder.getChildFile(name + ".jpg"));
    return image.isValid() ? image.convertedToFormat(juce::Image::ARGB) : image;
}

juce::Image scaledTo(const juce::Image& src, int w, int h, juce::Image::PixelFormat format) {
    juce::Image out(format, juce::jmax(1, w), juce::jmax(1, h), true, juce::SoftwareImageType());
    if (src.isNull()) return out;
    juce::Graphics g(out);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(src, out.getBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    return out;
}

// The bounds of an image's non-transparent pixels (a few pixels of margin).
juce::Rectangle<int> opaqueBounds(const juce::Image& image) {
    const juce::Image::BitmapData d(image, juce::Image::BitmapData::readOnly);
    int x0 = image.getWidth(), y0 = image.getHeight(), x1 = -1, y1 = -1;
    for (int y = 0; y < image.getHeight(); y += 2)
        for (int x = 0; x < image.getWidth(); x += 2)
            if (d.getPixelColour(x, y).getAlpha() > 2) {
                x0 = juce::jmin(x0, x); y0 = juce::jmin(y0, y);
                x1 = juce::jmax(x1, x); y1 = juce::jmax(y1, y);
            }
    if (x1 < 0) return {};
    return juce::Rectangle<int>::leftTopRightBottom(x0, y0, x1 + 2, y1 + 2).expanded(4).getIntersection(image.getBounds());
}

bool readAbort(const std::atomic<bool>* abort) { return abort != nullptr && abort->load(); }
}

std::size_t Planes::bytes() const noexcept {
    std::size_t total = 0;
    for (const auto* image : { &back, &mid, &fog, &near, &flat })
        if (image->isValid()) total += static_cast<std::size_t>(image->getWidth()) * static_cast<std::size_t>(image->getHeight())
                                       * (image->getFormat() == juce::Image::RGB ? 3u : 4u);
    return total;
}

const PaintedScene* paintedScene(SceneId id) {
    const auto slug = sceneSlug(id);
    for (const auto& s : paintedScenes)
        if (slug == s.slug) return &s;
    return nullptr;
}

juce::File paintedFolder() {
    static const juce::File folder = [] {
        juce::Array<juce::File> candidates;
        const auto binary = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        // Bundles (AU, VST3, the app): Contents/<MacOS | arch-os>/binary -> Contents/Resources.
        candidates.add(binary.getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("Painted"));
        candidates.add(binary.getParentDirectory().getChildFile("Painted"));
       #ifdef DC_PAINTED_DIR
        candidates.add(juce::File(DC_PAINTED_DIR));
       #endif
        for (const auto& f : candidates)
            if (f.getChildFile("map").getChildFile("sky.jpg").existsAsFile()) return f;
        return juce::File();
    }();
    return folder;
}

bool isPainted(SceneId id) {
    const auto dropIn = overrideFile(id);
    if (dropIn.existsAsFile() || dropIn.getSiblingFile(dropIn.getFileNameWithoutExtension()).isDirectory()) return true;
    const auto* s = paintedScene(id);
    return s != nullptr && paintedFolder() != juce::File() && paintedFolder().getChildFile(s->slug).getChildFile("sky.jpg").existsAsFile();
}

Planes loadPainted(SceneId id, int w, int h, const std::atomic<bool>* abort) {
    Planes p;
    const auto* manifest = paintedScene(id);
    if (manifest != nullptr) { p.moon = { manifest->moon[0], manifest->moon[1] }; p.moonRadius = manifest->moon[2]; }
    juce::Image sky, far, mid, fog, near;
    const auto dropIn = overrideFile(id);
    const auto dropInFolder = dropIn.getSiblingFile(dropIn.getFileNameWithoutExtension());
    if (dropIn.existsAsFile()) {
        sky = loadFile(dropIn);
        p.moonRadius = 0;   // own art: no moon glow over it
    } else if (dropInFolder.isDirectory()) {
        sky = loadDropIn(dropInFolder, "back");
        if (sky.isNull()) sky = loadDropIn(dropInFolder, "sky");
        far = loadDropIn(dropInFolder, "far");
        mid = loadDropIn(dropInFolder, "mid");
        fog = loadDropIn(dropInFolder, "fog");
        near = loadDropIn(dropInFolder, "near");
        p.moonRadius = 0;
    } else if (manifest != nullptr) {
        const auto folder = paintedFolder().getChildFile(manifest->slug);
        sky = loadFile(folder.getChildFile("sky.jpg"));
        const auto names = juce::StringArray::fromTokens(manifest->layers, "|", "");
        if (names.contains("far")) far = loadLayer(folder, "far");
        if (readAbort(abort)) return {};
        if (names.contains("mid")) mid = loadLayer(folder, "mid");
        if (readAbort(abort)) return {};
        if (names.contains("fog")) fog = loadLayer(folder, "fog");
        if (names.contains("near")) near = loadLayer(folder, "near");
    }
    if (sky.isNull() || readAbort(abort)) return {};
    // back = sky + far, opaque.
    p.back = juce::Image(juce::Image::RGB, w, h, true, juce::SoftwareImageType());
    {
        juce::Graphics g(p.back);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(sky, p.back.getBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        if (far.isValid()) g.drawImage(far, p.back.getBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }
    if (readAbort(abort)) return {};
    if (mid.isValid()) p.mid = scaledTo(mid, w, h, juce::Image::ARGB);
    if (fog.isValid()) {
        p.fog = scaledTo(fog, (w + 1) / 2, (h + 1) / 2, juce::Image::ARGB);
        const auto b = opaqueBounds(p.fog);
        p.fogArea = { b.getX() * 2, b.getY() * 2, b.getWidth() * 2, b.getHeight() * 2 };
    }
    if (near.isValid()) {
        const auto full = scaledTo(near, w, h, juce::Image::ARGB);
        p.nearArea = opaqueBounds(full);
        if (!p.nearArea.isEmpty()) p.near = full.getClippedImage(p.nearArea).createCopy();
    }
    if (readAbort(abort)) return {};
    p.flat = composite(p, w, h);
    return p;
}

juce::Image composite(const Planes& p, int w, int h) {
    juce::Image out(juce::Image::RGB, w, h, true, juce::SoftwareImageType());
    juce::Graphics g(out);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    if (p.back.isValid()) g.drawImageAt(p.back, 0, 0);
    if (p.mid.isValid()) g.drawImageAt(p.mid, 0, 0);
    if (p.fog.isValid()) g.drawImage(p.fog, out.getBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    if (p.near.isValid()) g.drawImageAt(p.near, p.nearArea.getX(), p.nearArea.getY());
    return out;
}

std::vector<LightSource> paintedLights(SceneId id) {
    std::vector<LightSource> lights;
    const auto* s = paintedScene(id);
    if (s == nullptr || overrideFile(id).existsAsFile()) return lights;
    for (int i = 0; i < s->anchorCount; ++i) {
        const auto& a = s->anchors[i];
        int kind = -1;
        switch (a.kind) {
            case anchorFire: kind = 0; break;
            case anchorCandle: kind = 1; break;
            case anchorGlint: kind = 2; break;
            case anchorWindow: kind = 4; break;
            case anchorLava: kind = 5; break;
            case anchorEyes: kind = 6; break;
            default: break;
        }
        if (kind >= 0) lights.push_back({ { a.x, a.y }, a.value, kind });
    }
    return lights;
}

const juce::Image& mistTexture() {
    // 384 x 128, tileable horizontally and vertically: periodic value noise, a few octaves,
    // shaped into soft wisps, tinted slate; the editor fades it (opacity).
    static const juce::Image texture = [] {
        constexpr int W = 384, H = 128;
        juce::Image image(juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
        juce::Image::BitmapData d(image, juce::Image::BitmapData::writeOnly);
        const auto lattice = [](int x, int y, int px, int py, std::uint32_t seed) {
            x = ((x % px) + px) % px; y = ((y % py) + py) % py;
            std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343u ^ static_cast<std::uint32_t>(y) * 0xd8163841u ^ seed * 0xcb1ab31fu;
            h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
            return static_cast<float>(h & 0xffffu) / 65535.0f;
        };
        const auto periodic = [&](float x, float y, int px, int py, std::uint32_t seed) {
            const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
            float tx = x - static_cast<float>(ix), ty = y - static_cast<float>(iy);
            tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
            const float a = lattice(ix, iy, px, py, seed), b = lattice(ix + 1, iy, px, py, seed);
            const float c = lattice(ix, iy + 1, px, py, seed), e = lattice(ix + 1, iy + 1, px, py, seed);
            return a + (b - a) * tx + (c - a) * ty + (a - b - c + e) * tx * ty;
        };
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                float v = 0, amp = 1, total = 0;
                for (int o = 0; o < 4; ++o) {
                    const int px = 6 << o, py = 2 << o;
                    v += amp * periodic(static_cast<float>(x) * static_cast<float>(px) / W, static_cast<float>(y) * static_cast<float>(py) / H, px, py, 7u + static_cast<std::uint32_t>(o));
                    total += amp; amp *= 0.5f;
                }
                v /= total;
                const float a = juce::jlimit(0.0f, 1.0f, (v - 0.45f) * 2.6f);
                // Vertical fade (the sheet has no hard top or bottom edge), slate tint.
                const float fade = std::pow(std::sin(juce::MathConstants<float>::pi * (static_cast<float>(y) + 0.5f) / H), 1.5f);
                d.setPixelColour(x, y, juce::Colour(0xff8c88a0).withAlpha(a * a * fade * 0.16f));
            }
        return image;
    }();
    return texture;
}
}
