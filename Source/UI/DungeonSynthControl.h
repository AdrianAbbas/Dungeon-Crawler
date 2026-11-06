#pragma once
// Session 15: the DUNGEON SYNTH control (both plug-ins): a roller drawn like the Effects
// roller (0 %: the sound exactly as it is; 25 % subtle, 50 % clearly dungeon synth, 100 % the
// full genre sound) and the Era switch beside it (Classic: the cassette era; Modern: cleaner
// and warmer; Session 16: warm, wide and polished). The title runs across the top. On the instrument panel it sits to the right of
// the keyboard; in the FX's controls between Travel and the switches.
//
// The roller is the Effects roller's slider (vertical drag, the mouse wheel, double-click back
// to 0 %, host automation through its attachment); the Era buttons follow the parameter (a
// host's automation moves them) and set it as one gesture. Nothing animates by itself.
#include "EffectsRoller.h"

namespace dc::ui {

class DungeonSynthControl final : public juce::Component {
public:
    explicit DungeonSynthControl(juce::AudioProcessorValueTreeState& state) {
        roller.showLabel = false;
        roller.showNotch = false;
        roller.readoutHeight = 15.0f;
        if (auto* amount = state.getParameter(dc::ids::dungeonSynth)) {
            roller.attach(*amount);
            roller.setDoubleClickReturnValue(true, 0.0);
        }
        roller.setTooltip("Dungeon Synth: how much it sounds like dungeon synth (0 %: as it is; 25 % subtle, 50 % clearly, 100 % the full genre sound)");
        addAndMakeVisible(roller);
        classic.setTooltip("Classic: the 90s cassette demo (a cheap 13 kHz, 8-bit sampler with its aliasing, tape wow, flutter and hiss, a dark top, nearly mono, a big grainy old-digital hall)");
        modern.setTooltip("Modern: warm, wide and polished (heavy tape saturation, a deep vintage ensemble chorus, a softer top, a slight wow, a lush, very long dark hall)");
        for (auto* b : { &classic, &modern }) {
            b->setClickingTogglesState(false);
            b->setColour(juce::TextButton::buttonOnColourId, colours::blood.withAlpha(0.85f));
            addAndMakeVisible(*b);
        }
        if (auto* era = state.getParameter(dc::ids::dungeonSynthEra)) {
            eraAttachment = std::make_unique<juce::ParameterAttachment>(*era, [this](float value) { showEra(value >= 0.5f ? 1 : 0); }, nullptr);
            eraAttachment->sendInitialUpdate();
            classic.onClick = [this] { eraAttachment->setValueAsCompleteGesture(0.0f); };
            modern.onClick = [this] { eraAttachment->setValueAsCompleteGesture(1.0f); };
        }
    }
    EffectsRoller& rollerForTest() { return roller; }
    juce::TextButton& classicButton() { return classic; }
    juce::TextButton& modernButton() { return modern; }
    int shownEra() const noexcept { return era; }
    float titleHeight = 14.0f;

    void paint(juce::Graphics& g) override {
        g.setColour(colours::ivory.withAlpha(0.92f));
        g.setFont(titleFont(10.5f));
        g.drawFittedText("DUNGEON SYNTH", getLocalBounds().removeFromTop(juce::roundToInt(titleHeight)), juce::Justification::centred, 1, 0.75f);
    }
    void resized() override {
        auto area = getLocalBounds();
        area.removeFromTop(juce::roundToInt(titleHeight));
        const int rollerWidth = juce::jlimit(30, 54, area.getWidth() * 9 / 20);
        roller.setBounds(area.removeFromLeft(rollerWidth));
        area.removeFromLeft(4);
        // The two Era buttons, stacked, centred on the roller's wheel.
        const int buttonHeight = juce::jmin(22, (area.getHeight() - 4) / 2);
        auto buttons = area.withSizeKeepingCentre(area.getWidth(), buttonHeight * 2 + 4).withY(area.getY() + juce::jmax(0, (area.getHeight() - 15 - buttonHeight * 2 - 4) / 2));
        classic.setBounds(buttons.removeFromTop(buttonHeight));
        buttons.removeFromTop(4);
        modern.setBounds(buttons.removeFromTop(buttonHeight));
    }

private:
    void showEra(int which) {
        era = which;
        classic.setToggleState(which == 0, juce::dontSendNotification);
        modern.setToggleState(which == 1, juce::dontSendNotification);
    }
    EffectsRoller roller;
    juce::TextButton classic { "Classic" }, modern { "Modern" };
    std::unique_ptr<juce::ParameterAttachment> eraAttachment;
    int era = 0;
};

} // namespace dc::ui
