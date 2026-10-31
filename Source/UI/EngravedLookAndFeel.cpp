#include "EngravedLookAndFeel.h"

namespace dc::ui {
// System fonts for now: a serif for the title, a sans for labels. An embedded OFL
// blackletter face arrives with the art pipeline (Milestone 3).
juce::Font titleFont(float height) {
    return juce::Font(juce::FontOptions("Georgia", height, juce::Font::bold)).withExtraKerningFactor(0.08f);
}
juce::Font labelFont(float height) { return juce::Font(juce::FontOptions(height)); }

EngravedLookAndFeel::EngravedLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, colours::ivory);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, colours::ivory);
    setColour(juce::TooltipWindow::backgroundColourId, colours::charcoal);
    setColour(juce::TooltipWindow::textColourId, colours::ivory);
}

void EngravedLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                           float, juce::Slider::SliderStyle, juce::Slider& slider) {
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float trackWidth = 6.0f, capWidth = juce::jmin(46.0f, area.getWidth() - 4.0f), capHeight = 16.0f;
    const float cx = area.getCentreX();
    const auto track = juce::Rectangle<float>(cx - trackWidth * 0.5f, area.getY() + capHeight * 0.5f, trackWidth,
                                              area.getHeight() - capHeight);
    g.setColour(colours::ink.brighter(0.05f));
    g.fillRoundedRectangle(track, 2.0f);
    // The value track runs from the bottom (minimum) up to the cap.
    const float top = juce::jlimit(track.getY(), track.getBottom(), sliderPos);
    g.setColour(slider.isEnabled() ? colours::blood : colours::dust);
    g.fillRoundedRectangle(track.withTop(top), 2.0f);
    const auto cap = juce::Rectangle<float>(capWidth, capHeight).withCentre({ cx, top });
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(cap.translated(0.0f, 2.0f), capHeight * 0.45f);
    g.setColour(slider.isMouseOverOrDragging() ? colours::ivory.brighter(0.2f) : colours::ivory);
    g.fillRoundedRectangle(cap, capHeight * 0.45f);
    g.setColour(colours::ash);
    g.drawHorizontalLine(juce::roundToInt(cap.getCentreY()), cap.getX() + 6.0f, cap.getRight() - 6.0f);
}

void EngravedLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    if (button.getToggleState()) {
        g.setColour(colours::blood.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(colours::blood);
        g.fillRect(bounds.withHeight(2.0f).withY(bounds.getBottom() - 2.0f));
    } else if (highlighted) {
        g.setColour(colours::ivory.withAlpha(0.06f));
        g.fillRoundedRectangle(bounds, 3.0f);
    }
}

void EngravedLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool highlighted, bool) {
    g.setFont(labelFont(juce::jmin(17.0f, static_cast<float>(button.getHeight()) * 0.48f)));
    g.setColour(button.getToggleState() ? colours::ivory : (highlighted ? colours::ivory.withAlpha(0.85f) : colours::ash));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 2), juce::Justification::centred, 1);
}
}
