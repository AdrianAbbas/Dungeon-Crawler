#pragma once
// The engraving renderer (Milestone 7). A scene is drawn into a Plate as fields, not
// pixels: a tone (0 = black, 1 = bare paper), a hatch direction and a blood-red accent,
// plus ink strokes (outlines). render() turns the fields into an engraving: parallel
// hatching whose line width follows the tone, cross-hatching in the shadows, slightly
// wavering lines, a dark parchment with fibres and a vignette. Everything is
// deterministic and pure computation, so scenes are rendered off the message thread.
//
// Drawing coordinates are virtual: 960 x 560 for any output size (the editor's aspect).
#include <juce_graphics/juce_graphics.h>
#include <atomic>

namespace dc::art {
inline constexpr float virtualWidth = 960.0f, virtualHeight = 560.0f;

class Plate {
public:
    Plate(int widthPixels, int heightPixels);
    int width() const noexcept { return w; }
    int height() const noexcept { return h; }
    // Shifts everything drawn afterwards (virtual units), e.g. to place a motif.
    void setOffset(float dx, float dy) noexcept;

    // Fields. `tone` 0..1 (0 black), `angle` in degrees (0 = horizontal lines),
    // `accent` 0..1 (lines turn blood-red).
    void fill(const juce::Path&, float tone, float angle, float accent = 0.0f);
    void fillRect(juce::Rectangle<float>, float tone, float angle, float accent = 0.0f);
    void fillEllipse(juce::Rectangle<float>, float tone, float angle, float accent = 0.0f);
    // A tone gradient across a shape (same hatch direction).
    void fillGradient(const juce::Path&, float toneA, juce::Point<float> a, float toneB, juce::Point<float> b, float angle, bool radial = false);
    // Ink strokes on top of the hatching (outlines, details). `alpha` < 1 for faint lines.
    void stroke(const juce::Path&, float thickness, float alpha = 1.0f);
    void line(float x1, float y1, float x2, float y2, float thickness, float alpha = 1.0f);
    // Solid ink and solid red areas (lettering, the title's initial).
    void fillInk(const juce::Path&, float alpha = 1.0f);
    void fillAccent(const juce::Path&, float alpha = 1.0f);
    // Red ink strokes (the active place's marker, a path line).
    void strokeAccent(const juce::Path&, float thickness, float alpha = 1.0f);
    // A soft warm glow (torchlight, candles) added over the finished engraving. It is
    // also recorded as a light (kind 0 flame) for the editor's animated life.
    void glow(juce::Point<float> centre, float radius, float strength);
    // Records a light for the animation only: kind 1 candle, 2 water glint, 3 fog band.
    void light(juce::Point<float> centre, float radius, int kind) { lights.push_back({ centre + offset, radius, kind }); }
    struct Light { juce::Point<float> centre; float radius; int kind; };
    const std::vector<Light>& recordedLights() const noexcept { return lights; }

    struct Style {
        juce::Colour paper { 0xff9a8e76 }, ink { 0xff15110e }, accent { 0xffa8262a }, glow { 0xffd06a2a };
        float spacing = 3.1f;      // hatch period in virtual units
        float vignette = 0.55f;
        std::uint32_t seed = 1;
    };
    // `abort` (optional) is checked every row: a closing editor never waits long.
    juce::Image render(const Style&, const std::atomic<bool>* abort = nullptr) const;
    juce::Image render() const { return render(Style {}); }

private:
    struct Glow { juce::Point<float> centre; float radius, strength; };
    int w, h;
    juce::AffineTransform toPixels, scaleOnly;
    juce::Point<float> offset;
    juce::Image fields;   // R tone, G angle (0..180 deg), B accent
    juce::Image inkMask, accentMask;
    std::vector<Glow> glows;
    std::vector<Light> lights;
};

// Small deterministic value noise (0..1), for wobbles and textures.
float noise2(float x, float y, std::uint32_t seed) noexcept;
}
