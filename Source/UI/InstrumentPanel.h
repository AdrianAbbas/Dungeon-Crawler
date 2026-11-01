#pragma once
// DUNGEON CRAWLER's instrument panel (Direction Update 3): a drawer that slides up over
// the painted place, in the same dark style. The instrument's name and its place, Lock
// instrument, a chooser for any of the forty, its five macros (their names follow the
// instrument), Intensity (how strongly the place's effects apply) and Space (the place's
// mix), the pitch bend range, the wheels and the pedal as they arrive, and an on-screen
// keyboard that lights up with incoming notes and can be played.
//
// Drawing: nothing animates on its own. A 15 Hz check (only while the panel is shown)
// compares what the audio thread last saw with what is drawn and repaints only the
// keyboard or the wheel strip when it changed; the rest is static.
#include "../PluginProcessor.h"
#include "EngravedLookAndFeel.h"

namespace dc::ui {

class InstrumentPanel final : public juce::Component, private juce::Timer {
public:
    explicit InstrumentPanel(DungeonCrawlerProcessor&);
    ~InstrumentPanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    juce::Button& closeButton() { return close; }
    std::function<void()> onClose;
    // Re-reads the instrument (name, place, macro labels, lock).
    void refresh();
    // Tests.
    juce::String titleText() const { return shownTitle; }
    juce::String placeText() const { return shownPlace; }
    juce::String macroLabel(int k) const { return macros[std::size_t(k)].label.getText(); }
    juce::ToggleButton& lockButton() { return lock; }
    juce::ComboBox& chooser() { return choose; }
    juce::Component& keyboardForTest() { return keyboard; }
    void pollForTest() { timerCallback(); }
    int keyRepaints() const noexcept { return keyboard.repaints; }
    // How often the 15 Hz check asked for a repaint (of the keyboard or the wheels).
    int repaintRequestCount() const noexcept { return repaintRequests; }

    // The on-screen keyboard: C2..C7.
    class Keyboard final : public juce::Component {
    public:
        explicit Keyboard(DungeonCrawlerProcessor& p) : processor(p) {}
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
    struct Fader {
        juce::Slider slider { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    // The wheels and the pedal.
    class Wheels final : public juce::Component {
    public:
        void paint(juce::Graphics&) override;
        float bend = 0, mod = 0;
        bool pedal = false;
    };
    void setUpFader(Fader&, const juce::String& id, const juce::String& name, const juce::String& tip);
    void timerCallback() override;

    DungeonCrawlerProcessor& processor;
    juce::TextButton close;
    juce::ToggleButton lock { "Lock instrument" };
    juce::ComboBox choose, bendRange;
    std::unique_ptr<juce::ComboBoxParameterAttachment> bendAttachment;
    juce::Label bendLabel;
    std::array<Fader, dc::inst::macroCount> macros;
    Fader intensity, space;
    Wheels wheels;
    Keyboard keyboard;
    juce::String shownTitle, shownPlace, shownWheel;
    int shownInstrument = -1, shownPlaceCode = -1, repaintRequests = 0;
    bool shownLock = false;
};
}
