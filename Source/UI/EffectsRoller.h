#pragma once
// The Effects roller (Session 9): how much of the place's effects, from nearly dry (0 %)
// through the place as designed (100 %) to stronger (150 %). Drawn like a mod wheel: a
// ridged iron wheel in a recessed stone slot, turning as it moves (the ridges roll past),
// a crimson notch at 100 %, a crimson glow on the lit part of the slot, the value under it.
//
// A juce::Slider underneath (vertical drag, the mouse wheel, double-click back to 100 %,
// host automation through its attachment). Cheap: a few gradients and a dozen lines,
// repainted only when the value changes; nothing animates by itself.
#include "EngravedLookAndFeel.h"
#include "../Parameters.h"

namespace dc::ui {

class EffectsRoller final : public juce::Slider {
public:
    EffectsRoller() : juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox) {
        setSliderSnapsToMousePosition(false);
        setVelocityBasedMode(false);
        setMouseDragSensitivity(220);
        setScrollWheelEnabled(true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        setPaintingIsUnclipped(false);
    }
    // Attaches it to the Effects parameter (0..1.5, default 1).
    void attach(juce::RangedAudioParameter& parameter) {
        attachment = std::make_unique<juce::SliderParameterAttachment>(parameter, *this);
        setDoubleClickReturnValue(true, 1.0);
    }
    // Where the label sits (above) and how the readout is written (below). The instrument
    // panel's bend and mod wheels are drawn the same way, without them: the bend lit from
    // its centre (the notch at 0), the mod from the bottom without a notch.
    juce::String title { "EFFECTS" };
    bool showLabel = true, showReadout = true, showNotch = true;
    double notchValue = 1.0, fillFrom = 0.0;
    float labelHeight = 15.0f, readoutHeight = 17.0f;
    juce::String readout() const { return juce::String(juce::roundToInt(getValue() * 100.0)) + " %"; }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        const float span = static_cast<float>(getMaximum() - getMinimum());
        const float t = span > 0.0f ? static_cast<float>((getValue() - getMinimum()) / span) : 0.0f;   // 0..1
        const float designed = span > 0.0f ? static_cast<float>((notchValue - getMinimum()) / span) : 0.0f;   // the 100 % notch
        const float base = span > 0.0f ? juce::jlimit(0.0f, 1.0f, static_cast<float>((fillFrom - getMinimum()) / span)) : 0.0f;
        if (showLabel) {
            g.setColour(colours::ivory.withAlpha(0.92f));
            g.setFont(titleFont(11.5f));
            g.drawText(title, area.removeFromTop(labelHeight), juce::Justification::centred, false);
        }
        const auto readArea = showReadout ? area.removeFromBottom(readoutHeight) : juce::Rectangle<float>();
        // The slot: dark stone, recessed (an inner shadow at the top, a faint lit lip below).
        const float slotWidth = juce::jmin(area.getWidth() * 0.72f, 40.0f);
        const auto slot = area.withSizeKeepingCentre(slotWidth, area.getHeight() - 4.0f);
        g.setColour(colours::ink);
        g.fillRoundedRectangle(slot, 5.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.8f), slot.getX(), slot.getY(),
                                               juce::Colours::black.withAlpha(0.0f), slot.getX(), slot.getY() + 10.0f, false));
        g.fillRoundedRectangle(slot, 5.0f);
        g.setColour(colours::dust.withAlpha(0.55f));
        g.drawRoundedRectangle(slot.reduced(0.5f), 5.0f, 1.0f);
        g.setColour(colours::ash.withAlpha(0.18f));
        g.drawLine(slot.getX() + 4.0f, slot.getBottom() + 0.5f, slot.getRight() - 4.0f, slot.getBottom() + 0.5f, 1.0f);
        // The wheel: an iron cylinder seen edge-on (dark at both ends, lit in the middle).
        const auto wheel = slot.reduced(4.0f, 4.0f);
        juce::ColourGradient iron(juce::Colour(0xff0e0d10), 0.0f, wheel.getY(), juce::Colour(0xff0e0d10), 0.0f, wheel.getBottom(), false);
        iron.addColour(0.30, juce::Colour(0xff3a3640));
        iron.addColour(0.50, juce::Colour(0xff4c4752));
        iron.addColour(0.70, juce::Colour(0xff2e2b33));
        g.setGradientFill(iron);
        g.fillRoundedRectangle(wheel, 3.0f);
        // The ridges roll past as the value moves: equally spaced on the cylinder, projected
        // (closer together towards the ends), brighter in the lit middle.
        const float cy = wheel.getCentreY(), radius = wheel.getHeight() * 0.5f;
        const float turn = t * 5.5f;   // a little over five ridge spacings over the whole range
        constexpr int ridges = 14;
        for (int k = -ridges; k <= ridges; ++k) {
            const float angle = (static_cast<float>(k) + std::fmod(turn, 1.0f)) * (juce::MathConstants<float>::pi / static_cast<float>(ridges));
            if (std::abs(angle) >= juce::MathConstants<float>::halfPi * 0.97f) continue;
            const float y = cy - radius * std::sin(angle);
            const float light = std::cos(angle);
            g.setColour(juce::Colours::black.withAlpha(0.55f * light));
            g.fillRect(wheel.getX() + 2.0f, y, wheel.getWidth() - 4.0f, 1.2f);
            g.setColour(colours::ash.withAlpha(0.32f * light * light));
            g.fillRect(wheel.getX() + 2.0f, y + 1.2f, wheel.getWidth() - 4.0f, 0.8f);
        }
        // The crimson notch: where the value sits on the wheel's face (always at its centre
        // line, the wheel turning under it), with its lit length along the slot's edge.
        const float valueY = slot.getBottom() - 3.0f - t * (slot.getHeight() - 6.0f);
        const float designedY = slot.getBottom() - 3.0f - designed * (slot.getHeight() - 6.0f);
        const float baseY = slot.getBottom() - 3.0f - base * (slot.getHeight() - 6.0f);
        g.setColour(colours::blood.withAlpha(0.25f));
        g.fillRect(slot.getRight() + 3.0f, slot.getY() + 3.0f, 3.0f, slot.getHeight() - 6.0f);
        g.setColour(colours::blood.withAlpha(0.9f));
        g.fillRect(slot.getRight() + 3.0f, std::min(valueY, baseY), 3.0f, std::max(1.0f, std::abs(baseY - valueY)));
        // 100 % (the designed value): a small ivory tick on the left edge.
        if (showNotch) {
            g.setColour(colours::ivory.withAlpha(0.7f));
            g.fillRect(slot.getX() - 6.0f, designedY - 0.5f, 4.0f, 1.0f);
        }
        // The grip under the finger: a crimson bar across the wheel at the value.
        const auto grip = juce::Rectangle<float>(wheel.getWidth() + 2.0f, 4.0f).withCentre({ wheel.getCentreX(), juce::jlimit(wheel.getY() + 2.0f, wheel.getBottom() - 2.0f, valueY) });
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(grip.translated(0.0f, 1.0f), 1.5f);
        g.setColour(isMouseOverOrDragging() ? colours::ember : colours::blood);
        g.fillRoundedRectangle(grip, 1.5f);
        // The value.
        if (showReadout) {
            g.setColour(colours::ivory);
            g.setFont(labelFont(14.0f));
            g.drawText(readout(), readArea, juce::Justification::centred, false);
        }
    }

private:
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};

} // namespace dc::ui
