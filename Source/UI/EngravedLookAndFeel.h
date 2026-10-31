#pragma once
// The look of the controls (visual overhaul): near-black and charcoal, slate greys,
// ivory, and one crimson accent, to sit on the painted dark-fantasy art. Slim faders
// with ivory caps and a crimson value track; text buttons without boxes (an underline
// marks what is on); Cinzel for titles and Cormorant Garamond for labels (both SIL OFL,
// embedded).
#include <juce_gui_basics/juce_gui_basics.h>

namespace dc::ui {
namespace colours {
inline const juce::Colour ink { 0xff0b0a0d };
inline const juce::Colour charcoal { 0xff17151b };
inline const juce::Colour dust { 0xff5a5662 };
inline const juce::Colour ash { 0xffaaa3a8 };
inline const juce::Colour ivory { 0xffece4d4 };
inline const juce::Colour blood { 0xffc0242c };
inline const juce::Colour ember { 0xffe0482e };
}

// Cinzel (titles, place names, group headings).
juce::Font titleFont(float height);
// Cormorant Garamond (labels, buttons, values).
juce::Font labelFont(float height);
// A system font for symbols the serif faces lack (arrows, the gear).
juce::Font symbolFont(float height);

class EngravedLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    EngravedLookAndFeel();
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    juce::Font getLabelFont(juce::Label&) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
};
}
