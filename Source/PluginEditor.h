#pragma once
// Milestone 0 editor: the title, the ten locations as buttons, four faders and Bypass.
// The background (hatching, frame, title) is drawn once per size into a cached image;
// there is no timer, so nothing repaints unless a control changes. The map, the
// location illustrations and the transitions replace this in Milestone 3.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/EngravedLookAndFeel.h"

class DungeonCrawlerEditor final : public juce::AudioProcessorEditor {
public:
    static constexpr int defaultWidth = 960, defaultHeight = 540;
    explicit DungeonCrawlerEditor(DungeonCrawlerProcessor&);
    ~DungeonCrawlerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    int shownLocation() const noexcept { return shown; }
    juce::Button& locationButton(int index) { return *places[static_cast<std::size_t>(index)]; }

private:
    struct Fader {
        juce::Slider slider { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    void setUpFader(Fader&, const char* id, const juce::String& name);
    void showLocation(int index);
    void renderBackground();

    DungeonCrawlerProcessor& processor;
    dc::ui::EngravedLookAndFeel look;
    juce::Image background;
    std::array<std::unique_ptr<juce::TextButton>, dc::LocationCount> places;
    std::unique_ptr<juce::ParameterAttachment> locationAttachment;
    Fader input, intensity, mix, output;
    juce::TextButton bypass { "Bypass" };
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttachment;
    juce::Label placeName;
    int shown = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DungeonCrawlerEditor)
};
