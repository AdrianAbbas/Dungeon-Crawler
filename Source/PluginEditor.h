#pragma once
// Milestone 1 editor (simple, before the map of Milestone 3): the ten locations as a
// list, the current location's sub-locations, its themed macros, the main faders,
// Travel Time and Sync, Bypass, and the Advanced drawer (every module parameter as an
// offset or override, hidden by default). The background is drawn once per size into
// a cached image; there is no timer, so nothing repaints unless a control changes.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/EngravedLookAndFeel.h"

class DungeonCrawlerEditor final : public juce::AudioProcessorEditor {
public:
    static constexpr int defaultWidth = 960, defaultHeight = 560;
    explicit DungeonCrawlerEditor(DungeonCrawlerProcessor&);
    ~DungeonCrawlerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    int shownLocation() const noexcept { return shown; }
    int shownSubLocation() const noexcept { return shownSub; }
    juce::Button& locationButton(int index) { return *places[static_cast<std::size_t>(index)]; }
    juce::Button& subLocationButton(int index) { return *subs[static_cast<std::size_t>(index)]; }
    juce::Button& advancedButton() { return advancedToggle; }
    juce::String macroLabel(int index) const { return macros[static_cast<std::size_t>(index)].label.getText(); }
    bool macroVisible(int index) const { return macros[static_cast<std::size_t>(index)].slider.isVisible(); }
    bool advancedOpen() const { return drawer.isVisible(); }

private:
    struct Fader {
        juce::Slider slider { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    // The Advanced drawer: one row per module parameter, grouped by module.
    class Drawer final : public juce::Component {
    public:
        explicit Drawer(juce::AudioProcessorValueTreeState&);
        void paint(juce::Graphics&) override;
        void resized() override;
    private:
        struct Row {
            juce::Label label;
            std::unique_ptr<juce::Slider> slider;
            std::unique_ptr<juce::ComboBox> box;
            std::unique_ptr<juce::SliderParameterAttachment> sliderAttachment;
            std::unique_ptr<juce::ComboBoxParameterAttachment> boxAttachment;
            int group = 0;
        };
        juce::Viewport viewport;
        juce::Component content;
        std::vector<std::unique_ptr<Row>> rows;
        std::vector<std::unique_ptr<juce::Label>> headings;
        juce::Label hint;
    };
    void setUpFader(Fader&, const juce::String& id, const juce::String& name);
    void showLocation(int index);
    void showSubLocation(int index);
    void renderBackground();

    DungeonCrawlerProcessor& processor;
    dc::ui::EngravedLookAndFeel look;
    juce::Image background;
    std::array<std::unique_ptr<juce::TextButton>, dc::LocationCount> places;
    std::array<std::unique_ptr<juce::TextButton>, dc::placesPerLocation> subs;
    std::unique_ptr<juce::ParameterAttachment> locationAttachment, subLocationAttachment;
    std::array<Fader, dc::macroCount> macros;
    Fader input, intensity, mix, output;
    juce::Slider travelTime { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label travelLabel;
    juce::ComboBox travelSync;
    std::unique_ptr<juce::SliderParameterAttachment> travelTimeAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> travelSyncAttachment;
    juce::TextButton bypass { "Bypass" }, advancedToggle { "Advanced" };
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttachment;
    juce::Label placeName, macroHeading;
    Drawer drawer;
    int shown = -1, shownSub = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DungeonCrawlerEditor)
};
