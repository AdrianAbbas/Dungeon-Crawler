#pragma once
// Hiss fix (build 3): the warm chain's own controls, beside the GRIT control in both plug-ins:
// WARMTH (the "warmth" parameter: the warm character chain), AIR (the "air" parameter: its faint
// tape hiss; 0 % none, 15 % the default, barely there; 100 % obvious) and a small MONO switch
// ("stereoMono"). The two rollers are the Effects roller's slider, drawn the same way (vertical
// drag, the mouse wheel, double-click back to the default, host automation through their
// attachments); the switch follows its parameter. Titles run across the top, as on GRIT.
#include "EffectsRoller.h"

namespace dc::ui {

class WarmthControl final : public juce::Component {
public:
    explicit WarmthControl(juce::AudioProcessorValueTreeState& state) {
        for (auto* r : { &warmth, &air }) {
            r->showLabel = false;
            r->showNotch = false;
            r->readoutHeight = 15.0f;
            addAndMakeVisible(*r);
        }
        if (auto* p = state.getParameter(dc::ids::warmth)) {
            warmth.attach(*p);
            warmth.setDoubleClickReturnValue(true, double(p->convertFrom0to1(p->getDefaultValue())));
        }
        if (auto* p = state.getParameter(dc::ids::air)) {
            air.attach(*p);
            air.setDoubleClickReturnValue(true, double(p->convertFrom0to1(p->getDefaultValue())));
        }
        warmth.setTooltip("Warmth: the warm character (tape, glue, ensemble, the warm hall). 0 %: off");
        air.setTooltip("Air: the faint tape hiss under the notes (15 %: barely there, heard soloed on headphones; 0 %: none at all). "
                       "It stops within 400 ms of the last note, and dips under long held notes");
        mono.setClickingTogglesState(false);
        mono.setColour(juce::TextButton::buttonOnColourId, colours::blood.withAlpha(0.85f));
        mono.setTooltip("Mono: fold the whole sound to mono");
        addAndMakeVisible(mono);
        if (auto* p = state.getParameter(dc::ids::stereoMono)) {
            monoAttachment = std::make_unique<juce::ParameterAttachment>(*p, [this](float value) { showMono(value >= 0.5f); }, nullptr);
            monoAttachment->sendInitialUpdate();
            mono.onClick = [this] { monoAttachment->setValueAsCompleteGesture(mono.getToggleState() ? 0.0f : 1.0f); };
        }
    }
    EffectsRoller& warmthRoller() { return warmth; }
    EffectsRoller& airRoller() { return air; }
    juce::TextButton& monoButton() { return mono; }
    float titleHeight = 14.0f;

    void paint(juce::Graphics& g) override {
        g.setColour(colours::ivory.withAlpha(0.92f));
        g.setFont(titleFont(9.5f));
        const int h = juce::roundToInt(titleHeight);
        for (auto [c, text] : { std::pair<juce::Component*, const char*> { &warmth, "WARMTH" }, { &air, "AIR" }, { &mono, "MONO" } }) {
            const auto column = juce::Rectangle<int>(c->getX() - 2, 0, c->getWidth() + 4, h);
            g.drawFittedText(text, column, juce::Justification::centred, 1, 0.7f);
        }
    }
    void resized() override {
        auto area = getLocalBounds();
        area.removeFromTop(juce::roundToInt(titleHeight));
        const int rollerWidth = juce::jlimit(26, 40, (area.getWidth() - 4) * 35 / 100);
        warmth.setBounds(area.removeFromLeft(rollerWidth));
        area.removeFromLeft(2);
        air.setBounds(area.removeFromLeft(rollerWidth));
        area.removeFromLeft(2);
        // The switch, centred on the rollers' wheels (as GRIT's Era buttons).
        const int buttonHeight = juce::jmin(22, area.getHeight() / 2);
        mono.setBounds(area.withSizeKeepingCentre(area.getWidth(), buttonHeight).withY(area.getY() + juce::jmax(0, (area.getHeight() - 15 - buttonHeight) / 2)));
    }

private:
    void showMono(bool on) {
        mono.setToggleState(on, juce::dontSendNotification);
        mono.setButtonText(on ? "On" : "Off");
    }
    EffectsRoller warmth, air;
    juce::TextButton mono { "Off" };
    std::unique_ptr<juce::ParameterAttachment> monoAttachment;
};

} // namespace dc::ui
