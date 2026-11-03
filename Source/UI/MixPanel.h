#pragma once
// Session 11: the MIX section, a panel that opens from the Mix button (the FX's toolbar,
// the instrument's toolbar and its panel) over the left of the scene, like the Journey
// panel, and closes the same way (its ✕, the button again, Esc, a click outside).
//
// Five sliders, each 0-150 % (100 % = the place as designed; double-click returns there):
// Reverb / Space, Echoes, Modulation, Character, Atmosphere; then the reverbs' helpers:
// Low Cut, Pre-delay, Duck. Every one is a host parameter (automatable, in presets and
// sessions). The Effects roller stays the master over all of them.
//
// Drawing: opaque, two gradients and a few lines; the sliders repaint themselves.
#include <juce_audio_processors/juce_audio_processors.h>

namespace dc::ui {

class MixPanel final : public juce::Component {
public:
    explicit MixPanel(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::Button& closeButton() { return close; }
    std::function<void()> onClose;
    static constexpr int rowCount = 8;
    juce::Slider& sliderForTest(int k) { return rows[std::size_t(k)].slider; }
    juce::String labelForTest(int k) const { return rows[std::size_t(k)].label.getText(); }
    static constexpr int panelHeight = 330;

private:
    struct Row {
        juce::Label label;
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    juce::TextButton close;
    juce::Label title, hint, helpers;
    std::array<Row, rowCount> rows;
};
}
