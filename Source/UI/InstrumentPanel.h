#pragma once
// DUNGEON CRAWLER's instrument panel (Session 9: LIGHT SEEKER's layout, in this plug-in's
// dark painted style). Always shown under the world, as in LIGHT SEEKER (the window is
// 960 x 756 at 100 %): the instrument of the place (where it belongs, its name, step to
// the previous or next, choose any of the forty, Lock instrument, the pitch-bend range),
// its five macros (their names follow the instrument), Vowel and Ensemble (on the
// instruments that sing with the recorded voice; dimmed on the others), Level, Intensity,
// the Effects roller, the pitch-bend and mod wheels, the sustain pedal and an on-screen
// keyboard lit by the notes the host plays. Dark stone and iron, one crimson accent.
//
// Drawing: nothing animates on its own. A 15 Hz check compares what the audio thread last
// saw with what is drawn and repaints only the keyboard, a wheel or the pedal when it
// changed; the background is two gradients and a few lines.
#include "../PluginProcessor.h"
#include "EngravedLookAndFeel.h"
#include "EffectsRoller.h"
#include "Hover.h"

namespace dc::ui {

class InstrumentPanel final : public juce::Component, private juce::Timer {
public:
    static constexpr int panelHeight = 196;
    explicit InstrumentPanel(DungeonCrawlerProcessor&);
    ~InstrumentPanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    // Stale hovers cleared on every mouse move, enter and exit (Session 9).
    void mouseMove(const juce::MouseEvent& e) override { sweepHover(*this, e.getScreenPosition().toFloat()); }
    void mouseEnter(const juce::MouseEvent& e) override { sweepHover(*this, e.getScreenPosition().toFloat()); }
    void mouseExit(const juce::MouseEvent& e) override { sweepHover(*this, e.getScreenPosition().toFloat()); }
    // Re-reads the instrument (name, place, macro labels, lock, the voice controls).
    void refresh();
    // Tests.
    juce::String titleText() const { return shownTitle; }
    juce::String placeText() const { return shownPlace; }
    juce::String macroLabel(int k) const { return macroLabels[std::size_t(k)].getText(); }
    bool voiceControlsEnabled() const { return sliders[vowelSlot].isEnabled() && sliders[ensembleSlot].isEnabled(); }
    juce::Slider& sliderForTest(int k) { return sliders[std::size_t(k)]; }
    juce::ToggleButton& lockButton() { return lock; }
    juce::ComboBox& chooser() { return choose; }
    juce::Button& nextButton() { return next; }
    juce::Button& previousButton() { return previous; }
    juce::Button& sustainButton() { return sustain; }
    EffectsRoller& effectsRoller() { return effects; }
    EffectsRoller& bendWheel() { return bend; }
    EffectsRoller& modWheel() { return mod; }
    juce::Component& keyboardForTest() { return keyboard; }
    void pollForTest() { timerCallback(); }
    int keyRepaints() const noexcept { return keyboard.repaints; }
    // How often the 15 Hz check asked for a repaint (of the keyboard, the wheels, the pedal).
    int repaintRequestCount() const noexcept { return repaintRequests; }
    // The sliders, left to right: the five macros, Vowel, Ensemble, Level, Intensity.
    static constexpr std::size_t vowelSlot = 5, ensembleSlot = 6, levelSlot = 7, intensitySlot = 8, sliderCount = 9;

    // The on-screen keyboard: C2..C7.
    class Keyboard final : public juce::Component {
    public:
        explicit Keyboard(DungeonCrawlerProcessor& p) : processor(p) { setOpaque(true); }
        static constexpr int lowest = 36, highest = 96;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        int keyAt(juce::Point<float>) const;
        juce::Rectangle<float> keyBounds(int key) const;
        // Which keys are lit (128 bits).
        std::array<uint64_t, 2> lit {};
        int repaints = 0;
    private:
        void press(int key, float velocity);
        DungeonCrawlerProcessor& processor;
        int held = -1;
    };

private:
    void timerCallback() override;
    void step(int direction);

    DungeonCrawlerProcessor& processor;
    EngravedLookAndFeel look;   // the panel sits under the view, outside its look
    juce::Label place, name, status;
    juce::TextButton previous, next;
    juce::ComboBox choose, bendRange;
    juce::ToggleButton lock { "Lock instrument" };
    std::unique_ptr<juce::ComboBoxParameterAttachment> bendAttachment;
    juce::Label bendRangeLabel;
    std::array<juce::Slider, sliderCount> sliders;
    std::array<juce::Label, sliderCount> macroLabels;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>, sliderCount> attachments;
    EffectsRoller effects, bend, mod;
    juce::Label bendLabel, modLabel;
    juce::TextButton sustain { "Sustain" };
    Keyboard keyboard;
    juce::String shownTitle, shownPlace;
    int shownInstrument = -1, shownPlaceCode = -1, repaintRequests = 0;
    bool shownLock = false, shownPedal = false;
    float shownBend = 0, shownMod = 0;
};
}
