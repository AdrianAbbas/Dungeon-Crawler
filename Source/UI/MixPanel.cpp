#include "MixPanel.h"
#include "EngravedLookAndFeel.h"
#include "../Parameters.h"

namespace dc::ui {

MixPanel::MixPanel(juce::AudioProcessorValueTreeState& state) {
    setOpaque(true);
    setWantsKeyboardFocus(true);
    title.setText("MIX", juce::dontSendNotification);
    title.setFont(titleFont(16.0f));
    title.setColour(juce::Label::textColourId, colours::ivory);
    hint.setText("100 % = as designed", juce::dontSendNotification);
    hint.setFont(labelFont(13.0f));
    hint.setColour(juce::Label::textColourId, colours::ash);
    hint.setJustificationType(juce::Justification::centredRight);
    helpers.setText("the reverbs", juce::dontSendNotification);
    helpers.setFont(titleFont(11.5f));
    helpers.setColour(juce::Label::textColourId, colours::blood.brighter(0.35f));
    for (auto* l : { &title, &hint, &helpers }) {
        l->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*l);
    }
    close.setButtonText(juce::String::fromUTF8("\xe2\x9c\x95"));
    close.setTooltip("Close (Esc)");
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(close);

    struct Spec { const char* id; const char* name; const char* tip; double reset; };
    const std::array<Spec, rowCount> specs { {
        { ids::mixGroups[0], "Reverb / Space", "All the place's reverbs and spaces (0 %: none, 150 %: more and longer)", 1.0 },
        { ids::mixGroups[1], "Echoes", "Delays, tape echoes, slap-backs", 1.0 },
        { ids::mixGroups[2], "Modulation", "Chorus, shimmer, flutter, choirs and pitch movement", 1.0 },
        { ids::mixGroups[3], "Character", "Drive, filters, resonators, bells, the place's own signature effect", 1.0 },
        { ids::mixGroups[4], "Atmosphere", "The Atmosphere layer's level (its sounds of the place)", 1.0 },
        { ids::mixLowCut, "Low Cut", "Removes reverb mud below this (Off, 80-600 Hz)", double(mixLowCutOff) },
        { ids::mixPreDelay, "Pre-delay", "Moves the reverbs later (the dry hit first) or earlier", 0.0 },
        { ids::mixDuck, "Duck", "The reverb tails dip while you play and bloom in the gaps (0 %: off)", 0.0 },
    } };
    for (std::size_t k = 0; k < rows.size(); ++k) {
        auto& row = rows[k];
        row.label.setText(specs[k].name, juce::dontSendNotification);
        row.label.setFont(labelFont(15.0f));
        row.label.setColour(juce::Label::textColourId, colours::ivory);
        row.label.setTooltip(specs[k].tip);
        row.slider.setTooltip(specs[k].tip);
        row.slider.setTextBoxStyle(juce::Slider::TextBoxRight, true, 62, 18);
        row.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        row.slider.setDoubleClickReturnValue(true, specs[k].reset);
        if (auto* p = state.getParameter(specs[k].id)) row.attachment = std::make_unique<juce::SliderParameterAttachment>(*p, row.slider);
        addAndMakeVisible(row.label);
        addAndMakeVisible(row.slider);
    }
}

void MixPanel::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1b181e), 0, 0, colours::ink, 0, b.getHeight(), false));
    g.fillRect(b);
    g.setColour(juce::Colour(0xff3e3a44));
    g.drawRect(b, 1.5f);
    g.setColour(colours::blood);
    g.fillRect(b.getX() + 1.5f, b.getY() + 1.5f, b.getWidth() - 3.0f, 2.0f);
    // A rule above the reverbs' helpers.
    const auto h = helpers.getBounds().toFloat();
    g.setColour(juce::Colour(0xff3e3a44));
    const float textW = juce::GlyphArrangement::getStringWidth(titleFont(11.5f), helpers.getText().toUpperCase()) + 14.0f;
    g.fillRect(h.getX() + textW, h.getCentreY(), b.getWidth() - 24.0f - textW, 1.0f);
}

void MixPanel::resized() {
    auto area = getLocalBounds().reduced(12, 8);
    auto top = area.removeFromTop(30);
    close.setBounds(top.removeFromRight(28).withSizeKeepingCentre(28, 26));
    title.setBounds(top.removeFromLeft(80));
    hint.setBounds(top.reduced(4, 0));
    area.removeFromTop(4);
    const int rowHeight = 31;
    for (std::size_t k = 0; k < rows.size(); ++k) {
        if (k == 5) {
            area.removeFromTop(4);
            helpers.setText(helpers.getText().toUpperCase(), juce::dontSendNotification);
            helpers.setBounds(area.removeFromTop(20));
        }
        auto r = area.removeFromTop(rowHeight);
        rows[k].label.setBounds(r.removeFromLeft(112));
        rows[k].slider.setBounds(r.reduced(0, 4));
    }
}
}
