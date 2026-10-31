#include "EngravedLookAndFeel.h"
#include "BinaryData.h"

namespace dc::ui {
namespace {
juce::Typeface::Ptr typeface(const void* data, int size) {
    return juce::Typeface::createSystemTypefaceFor(data, static_cast<std::size_t>(size));
}
const juce::Typeface::Ptr& cinzel() {
    static const auto t = typeface(DungeonCrawlerFonts::CinzelSemiBold_ttf, DungeonCrawlerFonts::CinzelSemiBold_ttfSize);
    return t;
}
const juce::Typeface::Ptr& cormorant(bool bold) {
    static const auto semi = typeface(DungeonCrawlerFonts::CormorantGaramondSemiBold_ttf, DungeonCrawlerFonts::CormorantGaramondSemiBold_ttfSize);
    static const auto heavy = typeface(DungeonCrawlerFonts::CormorantGaramondBold_ttf, DungeonCrawlerFonts::CormorantGaramondBold_ttfSize);
    return bold ? heavy : semi;
}
// Text that needs a symbol the serif faces lack (arrows, triangles, the gear).
bool needsSymbols(const juce::String& text) {
    for (auto c : text)
        if (c >= 0x2190 && c != 0x2014 && c != 0x2039 && c != 0x203a) return true;
    return false;
}
}

juce::Font titleFont(float height) {
    return juce::Font(juce::FontOptions(cinzel()).withHeight(height * 0.92f)).withExtraKerningFactor(0.06f);
}
juce::Font labelFont(float height) {
    // Cormorant's small x-height: set a little larger, bold below 15 px so it stays readable.
    return juce::Font(juce::FontOptions(cormorant(height < 15.0f)).withHeight(height * 1.14f));
}
juce::Font symbolFont(float height) { return juce::Font(juce::FontOptions(height)); }

EngravedLookAndFeel::EngravedLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, colours::ash);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, colours::ivory);
    setColour(juce::TooltipWindow::backgroundColourId, colours::ink);
    setColour(juce::TooltipWindow::textColourId, colours::ivory);
    setColour(juce::TooltipWindow::outlineColourId, colours::dust);
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, colours::ivory);
    setColour(juce::ComboBox::outlineColourId, colours::dust.withAlpha(0.7f));
    setColour(juce::ComboBox::arrowColourId, colours::ash);
    setColour(juce::PopupMenu::backgroundColourId, colours::ink);
    setColour(juce::PopupMenu::textColourId, colours::ivory);
    setColour(juce::PopupMenu::headerTextColourId, colours::ash);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::blood.withAlpha(0.42f));
    setColour(juce::PopupMenu::highlightedTextColourId, colours::ivory);
    setColour(juce::ScrollBar::thumbColourId, colours::dust);
    setColour(juce::TextEditor::backgroundColourId, colours::charcoal);
    setColour(juce::TextEditor::textColourId, colours::ivory);
    setColour(juce::TextEditor::outlineColourId, colours::dust);
    setColour(juce::AlertWindow::backgroundColourId, colours::ink);
    setColour(juce::AlertWindow::textColourId, colours::ivory);
    setColour(juce::ToggleButton::textColourId, colours::ivory);
}

void EngravedLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                           float, juce::Slider::SliderStyle style, juce::Slider& slider) {
    const bool hot = slider.isMouseOverOrDragging();
    const auto trackColour = colours::ivory.withAlpha(0.13f);
    const auto valueColour = slider.isEnabled() ? colours::blood : colours::dust;
    if (style == juce::Slider::LinearHorizontal) {
        // A hairline track, the crimson value, a slim ivory cap. A bipolar range (an
        // Advanced offset) runs its value from zero.
        const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
        const float cy = area.getCentreY();
        g.setColour(trackColour);
        g.fillRoundedRectangle(area.getX(), cy - 1.0f, area.getWidth(), 2.0f, 1.0f);
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float origin = bipolar ? static_cast<float>(slider.getPositionOfValue(0.0)) : area.getX();
        g.setColour(valueColour);
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(juce::jmin(origin, sliderPos), cy - 1.0f, juce::jmax(origin, sliderPos), cy + 1.0f));
        const auto cap = juce::Rectangle<float>(5.0f, juce::jmin(14.0f, area.getHeight() - 2.0f)).withCentre({ sliderPos, cy });
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(cap.translated(0.0f, 1.0f).expanded(0.5f), 2.0f);
        g.setColour(hot ? colours::ivory.brighter(0.2f) : colours::ivory);
        g.fillRoundedRectangle(cap, 2.0f);
        return;
    }
    // Vertical: a slim fader. A 2 px track, the crimson value rising from the bottom
    // with a faint glow, an ivory cap with a dark centre line.
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float capWidth = juce::jmin(24.0f, area.getWidth() - 6.0f), capHeight = 9.0f;
    const float cx = area.getCentreX();
    const auto track = juce::Rectangle<float>(cx - 1.0f, area.getY() + capHeight * 0.5f, 2.0f, area.getHeight() - capHeight);
    g.setColour(trackColour);
    g.fillRoundedRectangle(track, 1.0f);
    const float top = juce::jlimit(track.getY(), track.getBottom(), sliderPos);
    if (slider.isEnabled()) {
        g.setColour(colours::blood.withAlpha(0.18f));
        g.fillRoundedRectangle(track.withTop(top).expanded(2.0f, 0.0f), 2.0f);
    }
    g.setColour(valueColour);
    g.fillRoundedRectangle(track.withTop(top), 1.0f);
    const auto cap = juce::Rectangle<float>(capWidth, capHeight).withCentre({ cx, top });
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(cap.translated(0.0f, 1.5f).expanded(1.0f), 3.0f);
    g.setColour(hot ? colours::ivory.brighter(0.25f) : colours::ivory);
    g.fillRoundedRectangle(cap, 3.0f);
    g.setColour(colours::ink.withAlpha(0.65f));
    g.fillRect(juce::Rectangle<float>(capWidth - 10.0f, 1.0f).withCentre(cap.getCentre()));
}

juce::Label* EngravedLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(labelFont(12.5f));
    label->setColour(juce::Label::textColourId, colours::ash);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::TextEditor::backgroundColourId, colours::charcoal);
    label->setColour(juce::TextEditor::textColourId, colours::ivory);
    label->setJustificationType(juce::Justification::centred);
    return label;
}

void EngravedLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down) {
    // No boxes: a soft glow when hovered, a crimson underline when on.
    const auto bounds = button.getLocalBounds().toFloat().reduced(3.0f, 3.0f);
    if (highlighted || down) {
        g.setGradientFill(juce::ColourGradient(colours::ivory.withAlpha(down ? 0.12f : 0.07f), bounds.getCentre(),
                                               colours::ivory.withAlpha(0.0f), bounds.getCentre().translated(bounds.getWidth() * 0.6f, 0.0f), true));
        g.fillRect(bounds);
    }
    if (button.getToggleState()) {
        const float w = juce::jmin(bounds.getWidth() - 8.0f, 46.0f);
        auto line = juce::Rectangle<float>(w, 1.5f).withCentre({ bounds.getCentreX(), bounds.getBottom() - 1.0f });
        juce::ColourGradient grad(colours::blood.withAlpha(0.0f), line.getX(), 0, colours::blood.withAlpha(0.0f), line.getRight(), 0, false);
        grad.addColour(0.5, colours::blood);
        g.setGradientFill(grad);
        g.fillRect(line);
    }
}

void EngravedLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool highlighted, bool) {
    const auto text = button.getButtonText();
    const float size = juce::jmin(16.0f, static_cast<float>(button.getHeight()) * 0.56f);
    g.setFont(needsSymbols(text) ? symbolFont(size * 0.9f) : labelFont(size));
    const bool on = button.getToggleState();
    auto colour = on ? colours::ivory : (highlighted ? colours::ivory.withAlpha(0.92f) : colours::ash);
    if (!button.isEnabled()) colour = colours::dust;
    g.setColour(colour);
    g.drawFittedText(text, button.getLocalBounds().reduced(4, 1), juce::Justification::centred, 1);
}

void EngravedLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) {
    // A small diamond: crimson when on, an ivory outline when off.
    const auto bounds = button.getLocalBounds().toFloat();
    const auto box = juce::Rectangle<float>(10.0f, 10.0f).withCentre({ bounds.getX() + 9.0f, bounds.getCentreY() });
    juce::Path diamond;
    diamond.startNewSubPath(box.getCentreX(), box.getY());
    diamond.lineTo(box.getRight(), box.getCentreY());
    diamond.lineTo(box.getCentreX(), box.getBottom());
    diamond.lineTo(box.getX(), box.getCentreY());
    diamond.closeSubPath();
    if (button.getToggleState()) {
        g.setColour(colours::blood);
        g.fillPath(diamond);
    }
    g.setColour(button.getToggleState() ? colours::blood.brighter(0.3f) : colours::ash.withAlpha(highlighted ? 1.0f : 0.75f));
    g.strokePath(diamond, juce::PathStrokeType(1.1f));
    g.setFont(labelFont(15.0f));
    g.setColour(button.isEnabled() ? colours::ivory.withAlpha(highlighted ? 1.0f : 0.9f) : colours::dust);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(22), juce::Justification::centredLeft, 1);
}

void EngravedLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) {
    // An underline and a small chevron.
    const bool hot = box.isMouseOver(true);
    g.setColour(colours::ivory.withAlpha(hot ? 0.35f : 0.18f));
    g.fillRect(0.0f, static_cast<float>(height) - 1.0f, static_cast<float>(width), 1.0f);
    const float cx = static_cast<float>(width) - 9.0f, cy = static_cast<float>(height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath(cx - 3.5f, cy - 1.5f);
    chevron.lineTo(cx, cy + 2.0f);
    chevron.lineTo(cx + 3.5f, cy - 1.5f);
    g.setColour(hot ? colours::ivory : colours::ash);
    g.strokePath(chevron, juce::PathStrokeType(1.2f));
}

juce::Font EngravedLookAndFeel::getComboBoxFont(juce::ComboBox& box) {
    return labelFont(juce::jmin(15.0f, static_cast<float>(box.getHeight()) * 0.62f));
}

void EngravedLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(0, 0, box.getWidth() - 18, box.getHeight());
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

juce::Font EngravedLookAndFeel::getPopupMenuFont() { return labelFont(16.0f); }

void EngravedLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    g.fillAll(colours::ink);
    g.setColour(colours::dust.withAlpha(0.6f));
    g.drawRect(0, 0, width, height, 1);
}

juce::Font EngravedLookAndFeel::getLabelFont(juce::Label& label) {
    // A slider's value box: the label face, small.
    if (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr) return labelFont(13.0f);
    return label.getFont();
}

int EngravedLookAndFeel::getSliderThumbRadius(juce::Slider& slider) {
    // The caps are slim: only half a cap of margin at the ends of the travel.
    return slider.isHorizontal() ? 3 : 5;
}

void EngravedLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    g.fillAll(colours::ink);
    g.setColour(colours::dust);
    g.drawRect(0, 0, width, height, 1);
    g.setColour(colours::ivory);
    g.setFont(labelFont(14.0f));
    g.drawFittedText(text, juce::Rectangle<int>(width, height).reduced(6, 2), juce::Justification::centredLeft, 3);
}
}
