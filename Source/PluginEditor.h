#pragma once
// The editor (Milestones 7 and 8; painted art since the visual overhaul): a full-window
// map as the home screen and a full-window illustration for every place. Clicking a landmark travels there (the
// path is drawn in red) and zooms into the place; inside, the sub-locations are doors
// and points of interest (hotspots) and a compact list; a breadcrumb leads back.
// The controls float over the art in a strip that can be hidden; the Advanced drawer
// covers the art when open.
//
// Drawing rules (brief, Direction Update 1): every scene is rendered off the message
// thread (art::SceneCache) and only blitted here; transitions start when the target
// image is ready (no stale frames) and run on a 30 fps timer that stops when nothing
// moves; the subtle life repaints only small regions: flickering windows and fires,
// drifting embers, mist wisps drifting across the low ground, the moon's glow pulsing.
// Painted scenes are layered (Painted.h): during a zoom the layers move at different
// speeds (parallax).
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/EngravedLookAndFeel.h"
#include "UI/SceneCache.h"

class DungeonCrawlerView final : public juce::Component, private juce::Timer, private juce::AudioProcessorParameter::Listener {
public:
    static constexpr int defaultWidth = 960, defaultHeight = 560;
    explicit DungeonCrawlerView(DungeonCrawlerProcessor&);
    ~DungeonCrawlerView() override;
    // Asks the editor to change its size (scale of 960 x 560).
    std::function<void(float)> onScaleChosen;
    void paint(juce::Graphics&) override;
    void resized() override;
    int shownLocation() const noexcept { return shown; }
    int shownSubLocation() const noexcept { return shownSub; }
    juce::Button& locationButton(int index) { return *landmarks[static_cast<std::size_t>(index)]; }
    juce::Button& subLocationButton(int index) { return *subs[static_cast<std::size_t>(index)]; }
    juce::Button& hotspotButton(int index) { return *hotspots[static_cast<std::size_t>(index)]; }
    juce::Button& advancedButton() { return advancedToggle; }
    juce::Button& mapButton() { return mapCrumb; }
    juce::Button& journeyButton() { return journeyToggle; }
    juce::Button& undoButtonForTest() { return undoButton; }
    juce::Button& slotButton(int k) { return k == 0 ? slotA : slotB; }
    juce::String presetLabel() const { return presetButton.getButtonText(); }
    void nextPresetForTest() { stepPreset(1); }
    bool journeyOpen() const { return journeyPanel.isVisible(); }
    juce::String macroLabel(int index) const { return macros[static_cast<std::size_t>(index)].label.getText(); }
    bool macroVisible(int index) const { return macros[static_cast<std::size_t>(index)].slider.isVisible(); }
    bool advancedOpen() const { return drawer.isVisible(); }
    // The view: the map, or a place's scene.
    bool showingMap() const noexcept { return view == View::Map && !transition.active; }
    dc::art::SceneId shownScene() const noexcept { return scene; }
    bool animating() const noexcept { return isTimerRunning(); }
    int lightCount() const noexcept { return static_cast<int>(lights.size()); }
    // The regions one life frame repaints (tests: editor CPU).
    std::vector<juce::Rectangle<int>> lifeRegions() const { std::vector<juce::Rectangle<int>> r; for (const auto& l : lights) r.push_back(lifeArea(l)); return r; }
    // Every region the life repaints with its frame rate (tests: editor CPU).
    std::vector<std::pair<juce::Rectangle<int>, int>> lifeRegionsWithRates() const {
        std::vector<std::pair<juce::Rectangle<int>, int>> r;
        for (const auto& l : lights) if (l.kind != 5) r.push_back({ lifeArea(l), 15 });
        for (const auto& e : embers) r.push_back({ juce::Rectangle<float>(e.size * 8.0f, e.size * 8.0f).withCentre(e.p).getSmallestIntegerContainer(), 30 });
        if (!mistArea().isEmpty()) r.push_back({ mistArea(), 10 });
        if (!moonArea().isEmpty()) r.push_back({ moonArea(), 5 });
        return r;
    }
    void setAnimation(bool on) { animateLife = on; wakeTimer(); }
    int emberCount() const noexcept { return static_cast<int>(embers.size()); }
    bool sceneLayered() const noexcept { return planes.layered(); }
    // Tests and snapshots: render the pending scenes on this thread and finish any
    // transition at once.
    void settleForTest();
    // Tests and snapshots: start the pending zoom now (scenes prepared on this thread) and
    // hold it at progress t (0..1) until settleForTest().
    void holdTransitionForTest(float t);
    // Tests and snapshots: advance the life by a number of frames (embers, mist).
    void advanceLifeForTest(int frames);

private:
    enum class View { Map, Place };
    struct Fader {
        juce::Slider slider { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    // A landmark on the map: an invisible button over the engraved landmark that rings
    // when hovered.
    class LandmarkButton final : public juce::Button {
    public:
        LandmarkButton(const juce::String& name) : juce::Button(name) {}
        void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    };
    // A point of interest inside a scene: a small engraved ring, red for the place you
    // are in, its name on hover.
    class HotspotButton final : public juce::Button {
    public:
        HotspotButton() : juce::Button({}) {}
        void paintButton(juce::Graphics&, bool highlighted, bool down) override;
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
    // Journeys (Milestone 9): the route, roaming and MIDI travel.
    class JourneyPanel final : public juce::Component {
    public:
        JourneyPanel(DungeonCrawlerProcessor&, std::function<dc::Stop()> currentPlace);
        void paint(juce::Graphics&) override;
        void resized() override;
        void refresh();
        juce::Button& addButton() { return add; }
        juce::Button& clearButton() { return clear; }
    private:
        DungeonCrawlerProcessor& processor;
        std::function<dc::Stop()> currentPlace;
        juce::ToggleButton journey { "Journey" }, roam { "Roam" }, midi { "MIDI travel" };
        juce::ComboBox mode, roamRate, octave;
        juce::Slider atmosDensity { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight },
                     atmosSeed { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Label atmosDensityLabel, atmosSeedLabel;
        std::unique_ptr<juce::SliderParameterAttachment> atmosDensityAttachment, atmosSeedAttachment;
        juce::Slider bars { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight },
                     randomness { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight },
                     seed { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::TextButton add { "Add this place" }, removeLast { "Remove last" }, clear { "Clear" };
        juce::Label routeText, barsLabel, randomLabel, seedLabel, midiHint;
        std::unique_ptr<juce::ButtonParameterAttachment> journeyAttachment, roamAttachment, midiAttachment;
        std::unique_ptr<juce::ComboBoxParameterAttachment> modeAttachment, rateAttachment, octaveAttachment;
        std::unique_ptr<juce::SliderParameterAttachment> barsAttachment, randomAttachment, seedAttachment;
    };
    // Follows the place a journey, roaming or MIDI chooses (10 Hz, only while steering).
    class Follower final : public juce::Timer {
    public:
        explicit Follower(DungeonCrawlerView& e) : editor(e) {}
        void timerCallback() override { editor.followSteering(); }
    private:
        DungeonCrawlerView& editor;
    };
    void followSteering();
    // Parameter gestures (edits from this window) open an undo step.
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override;
    // The toolbar: presets, A/B, undo/redo, size and quality.
    void showPresetMenu();
    void showSettingsMenu();
    void stepPreset(int direction);
    void refreshToolbar();
    struct Transition {
        bool active = false;
        int kind = 0;                  // 0 crossfade, 1 zoom into a landmark, 2 zoom out to the map
        juce::Image from, to;
        dc::art::Planes fromPlanes, toPlanes;   // painted scenes: their layers (parallax)
        float held = -1.0f;            // tests: progress held here
        double start = 0, length = 0.45;
        juce::Point<float> focus;      // virtual coordinates of the landmark
    };

    void setUpFader(Fader&, const juce::String& id, const juce::String& name);
    void showLocation(int index);
    void showSubLocation(int index);
    void goTo(View, dc::art::SceneId, int kind);
    void layoutOverlays();
    void updateCrumbs();
    void timerCallback() override;
    void sceneReady();
    void wakeTimer();
    juce::Point<int> pixelSize() const;
    juce::Point<float> toLocal(juce::Point<float> virtualPoint) const;
    juce::Rectangle<float> controlsArea() const;
    void paintMapOverlay(juce::Graphics&);
    void paintTransition(juce::Graphics&, double now);
    // Subtle life: flickering torches and candles, twinkling glints, drifting fog.
    void paintLife(juce::Graphics&, double now);
    // Painted life: the moon's glow, mist wisps, embers.
    void paintMoonGlow(juce::Graphics&, double now);
    void paintMist(juce::Graphics&, double now);
    void paintEmbers(juce::Graphics&);
    void stepEmbers(float dt);
    juce::Rectangle<int> moonArea() const;
    juce::Rectangle<int> mistArea() const;
    void drawPlanes(juce::Graphics&, const dc::art::Planes&, const juce::Image& flat, std::array<float, 4> scales, juce::Point<float> about, float alpha);
    juce::Rectangle<int> lifeArea(const dc::art::LightSource&) const;
    void refreshLights();

    DungeonCrawlerProcessor& processor;
    dc::ui::EngravedLookAndFeel look;
    dc::art::SceneCache cache;
    View view = View::Map;
    dc::art::SceneId scene { -1, 0 };
    // A requested change of view that waits for its image.
    bool pending = false;
    View pendingView = View::Map;
    dc::art::SceneId pendingScene;
    int pendingKind = 0;
    double zoomAt = 0;                 // after clicking a landmark: when to zoom in
    Transition transition;
    // The travel path on the map.
    int pathFrom = -1, pathTo = -1;
    double pathEnded = 0;
    int hovered = -1;
    std::vector<dc::art::LightSource> lights;
    bool animateLife = true;
    int lifeTick = 0;
    // Painted scenes: the shown scene's planes, its ember sources and the embers aloft.
    dc::art::Planes planes;
    struct Ember { juce::Point<float> p, v; float age, life, size, phase; };
    std::vector<Ember> embers;
    std::vector<dc::art::LightSource> emberSources;
    juce::Random random { 7 };

    std::array<std::unique_ptr<LandmarkButton>, dc::LocationCount> landmarks;
    std::array<std::unique_ptr<HotspotButton>, dc::placesPerLocation> hotspots;
    std::array<std::unique_ptr<juce::TextButton>, dc::placesPerLocation> subs;
    juce::TextButton mapCrumb { juce::String::fromUTF8("\xe2\x80\xb9  Map") }, locationCrumb;
    juce::Label placeCrumb;
    std::unique_ptr<juce::ParameterAttachment> locationAttachment, subLocationAttachment;
    std::array<Fader, dc::macroCount> macros;
    Fader input, intensity, mix, output;
    juce::Slider travelTime { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider atmosphere { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label atmosphereLabel;
    std::unique_ptr<juce::SliderParameterAttachment> atmosphereAttachment;
    juce::Label travelLabel, macroHeading;
    juce::ComboBox travelSync;
    std::unique_ptr<juce::SliderParameterAttachment> travelTimeAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> travelSyncAttachment;
    juce::TextButton bypass { "Bypass" }, advancedToggle { "Advanced" }, controlsToggle, journeyToggle { "Journey" };
    juce::TextButton presetButton, prevPreset, nextPreset, slotA { "A" }, slotB { "B" }, copySlot, undoButton, redoButton, settingsButton;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttachment;
    bool controlsShown = true;
    Drawer drawer;
    JourneyPanel journeyPanel;
    Follower follower { *this };
    bool wasSteering = false;
    int shown = -1, shownSub = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DungeonCrawlerView)
};

// The plug-in window: the view at 960 x 560, scaled to the window (resizable, fixed
// aspect; the size menu offers 75-200 %). The scale is saved with the state.
class DungeonCrawlerEditor final : public juce::AudioProcessorEditor {
public:
    static constexpr int defaultWidth = DungeonCrawlerView::defaultWidth, defaultHeight = DungeonCrawlerView::defaultHeight;
    explicit DungeonCrawlerEditor(DungeonCrawlerProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override {}
    DungeonCrawlerView& view() noexcept { return content; }
    void setScale(float);
private:
    DungeonCrawlerProcessor& processor;
    DungeonCrawlerView content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DungeonCrawlerEditor)
};
