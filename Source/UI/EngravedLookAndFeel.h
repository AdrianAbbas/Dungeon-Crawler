#pragma once
// Milestone 0 look: charcoal, dusty greys, ivory and one blood-red accent, after the
// antique-engraving mood of the brief. Tall slim faders with ivory caps and a red
// value track; location buttons as engraved labels with a red marker when active.
#include <juce_gui_basics/juce_gui_basics.h>

namespace dc::ui {
namespace colours {
inline const juce::Colour ink { 0xff121110 };
inline const juce::Colour charcoal { 0xff1d1b19 };
inline const juce::Colour dust { 0xff6e6a64 };
inline const juce::Colour ash { 0xffa29d94 };
inline const juce::Colour ivory { 0xffece6d6 };
inline const juce::Colour blood { 0xffb03a3e };
}

juce::Font titleFont(float height);
juce::Font labelFont(float height);

class EngravedLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    EngravedLookAndFeel();
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
};
}
