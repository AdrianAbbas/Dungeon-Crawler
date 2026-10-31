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
    setColour(juce::ComboBox::backgroundColourId, colours::charcoal);
    setColour(juce::ComboBox::textColourId, colours::ivory);
    setColour(juce::ComboBox::outlineColourId, colours::dust.withAlpha(0.6f));
    setColour(juce::ComboBox::arrowColourId, colours::ash);
    setColour(juce::PopupMenu::backgroundColourId, colours::charcoal);
    setColour(juce::PopupMenu::textColourId, colours::ivory);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::blood.withAlpha(0.5f));
    setColour(juce::ScrollBar::thumbColourId, colours::dust);
}

void EngravedLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                           float, juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style == juce::Slider::LinearHorizontal) {
        // Thin horizontal slider (travel time, the Advanced drawer). A bipolar range
        // (an Advanced offset) draws its red track from zero.
        const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
        const float cy = area.getCentreY();
        const auto track = juce::Rectangle<float>(area.getX(), cy - 1.5f, area.getWidth(), 3.0f);
        g.setColour(colours::ink.brighter(0.08f));
        g.fillRoundedRectangle(track, 1.5f);
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float origin = bipolar ? static_cast<float>(slider.getPositionOfValue(0.0)) : area.getX();
        g.setColour(slider.isEnabled() ? colours::blood : colours::dust);
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(juce::jmin(origin, sliderPos), cy - 1.5f, juce::jmax(origin, sliderPos), cy + 1.5f));
        const auto cap = juce::Rectangle<float>(8.0f, juce::jmin(16.0f, area.getHeight() - 2.0f)).withCentre({ sliderPos, cy });
        g.setColour(slider.isMouseOverOrDragging() ? colours::ivory.brighter(0.2f) : colours::ivory);
        g.fillRoundedRectangle(cap, 3.0f);
        return;
    }
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float trackWidth = 4.0f, capWidth = juce::jmin(30.0f, area.getWidth() - 4.0f), capHeight = 13.0f;
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
