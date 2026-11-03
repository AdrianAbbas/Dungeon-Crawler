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
#include "UI/InstrumentPanel.h"
#include "UI/Hover.h"
#include "UI/EffectsRoller.h"
#include "UI/SearchPanel.h"

class DungeonCrawlerView final : public juce::Component, private juce::Timer, private juce::AudioProcessorParameter::Listener {
public:
    static constexpr int defaultWidth = 960, defaultHeight = 560;
    explicit DungeonCrawlerView(DungeonCrawlerProcessor&);
    ~DungeonCrawlerView() override;
    // Asks the editor to change its size (scale of 960 x 560).
    std::function<void(float)> onScaleChosen;
    void paint(juce::Graphics&) override;
    void resized() override;
    // Panels close on Esc and on a click outside them (Hotfix 2).
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;
    // Every mouse move, enter and exit in the window (its children too) sweeps stale hovers.
    void mouseMove(const juce::MouseEvent& e) override { dc::ui::sweepHover(*this, e.getScreenPosition().toFloat()); }
    void mouseEnter(const juce::MouseEvent& e) override { dc::ui::sweepHover(*this, e.getScreenPosition().toFloat()); }
    void mouseExit(const juce::MouseEvent& e) override { dc::ui::sweepHover(*this, e.getScreenPosition().toFloat()); }
    int shownLocation() const noexcept { return shown; }
    int shownSubLocation() const noexcept { return shownSub; }
    juce::Button& locationButton(int index) { return *landmarks[static_cast<std::size_t>(index)]; }
    juce::Button& subLocationButton(int index) { return *subs[static_cast<std::size_t>(index)]; }
    juce::Button& hotspotButton(int index) { return *hotspots[static_cast<std::size_t>(index)]; }
    // Session 10: search. The magnifier (top right) or Cmd+F opens the panel over the
    // scene; a chosen result does exactly what choosing it by hand does (travel, the
    // instrument chooser, the preset menu) and the panel closes.
    void openSearch();
    void closeSearch();
    bool searchOpen() const { return searchPanel.isVisible(); }
    dc::ui::SearchPanel& searchPanelForTest() { return searchPanel; }
    juce::Button& searchButtonForTest() { return searchButton; }
    void chooseSearchResult(const dc::search::Entry&);
    // The editor refreshes the instrument panel when a search chose an instrument.
    std::function<void()> onInstrumentChosen;
    // Tests: the preview phrase's player (no message loop: stepped by hand).
    bool previewPlaying() const noexcept { return previewer.isTimerRunning() || previewer.instrument >= 0; }
    void stepPreviewForTest(double seconds) { previewer.advance(seconds); }
    std::vector<int> previewKeysForTest() const { return previewer.sounding; }
    // Tests (Session 9, hover): what each place-list entry, hotspot and landmark shows; a
    // sweep with the mouse at a screen point.
    bool placeHoverShown(int index) const { return subs[static_cast<std::size_t>(index)]->hot(); }
    bool hotspotHoverShown(int index) const { return hotspots[static_cast<std::size_t>(index)]->hot(); }
    bool landmarkHoverShown(int index) const { return landmarks[static_cast<std::size_t>(index)]->hot(); }
    int hoveredLandmark() const noexcept { return hovered; }
    int sweepHoverForTest(juce::Point<float> screen) { return dc::ui::sweepHover(*this, screen); }
    // Tests: the frame the view paints from (the last scene image that was ready), and
    // forgetting every rendered scene (as if the window's pixel size had just changed).
    const juce::Image& frameForTest() const noexcept { return frame; }
    void forgetScenesForTest() { cache.clearForTest(); }
    void closePanelsForTest() { closePanels(); }
    bool controlsShownForTest() const noexcept { return controlsShown; }
    // Session 9: the window's state into the processor (also on every change).
    void storeViewState();
    // The window's scale changed (the editor's transform): the art at the new pixel size.
    void scaleChanged();
    juce::Viewport* drawerViewportForTest() { return drawer.viewportForTest(); }
    juce::Viewport* journeyViewportForTest() { return journeyPanel.viewportForTest(); }
    dc::ui::EffectsRoller& effectsRollerForTest() { return effectsRoller; }
    juce::Button& advancedButton() { return advancedToggle; }
    juce::Button& mapButton() { return mapCrumb; }
    juce::Button& journeyButton() { return journeyToggle; }
    juce::Button& undoButtonForTest() { return undoButton; }
    juce::Button& slotButton(int k) { return k == 0 ? slotA : slotB; }
    juce::String presetLabel() const { return presetButton.getButtonText(); }
    void nextPresetForTest() { stepPreset(1); }
    bool journeyOpen() const { return journeyPanel.isVisible(); }
    juce::Button& journeyCloseButton() { return journeyPanel.closeButton(); }
    juce::Button& advancedCloseButton() { return drawer.closeButton(); }
    juce::Component& journeyPanelForTest() { return journeyPanel; }
    juce::Component& drawerForTest() { return drawer; }
    juce::Button& controlsButton() { return controlsToggle; }
    juce::String placeCrumbText() const { return placeCrumb.getText(); }
    // Tests: what a mouse-down on a component (anywhere in the window) does to the panels.
    void pointerDownForTest(juce::Component& c) { pointerDown(&c); }
    bool pressEscapeForTest() { return keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)); }
    // Hotfix 1: the "Journey active" badge, its Stop button, the short note.
    bool steerBadgeVisible() const { return steerBadge.isVisible(); }
    juce::String steerBadgeText() const { return steerBadge.text.getText(); }
    juce::Button& stopButton() { return steerBadge.stop; }
    juce::String noticeText() const { return notice.isVisible() ? notice.getText() : juce::String(); }
    // Tests: what the 10 Hz follower does (there is no message loop in the tests).
    void followForTest() { followSteering(); }
    // Tests: the preset menu's "Init / No preset" entry.
    void chooseInitForTest() { chooseInit(); }
    void choosePresetForTest(int index) { choosePreset(index); }
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
    class LandmarkButton final : public dc::ui::HoverButton {
    public:
        LandmarkButton(const juce::String& name) : dc::ui::HoverButton(name) {}
        void paintHover(juce::Graphics&, bool hover, bool down) override;
    };
    // A point of interest inside a scene: a small engraved ring, red for the place you
    // are in, its name on hover.
    class HotspotButton final : public dc::ui::HoverButton {
    public:
        HotspotButton() : dc::ui::HoverButton(juce::String()) {}
        void paintHover(juce::Graphics&, bool hover, bool down) override;
    };
    // An entry of the place list (top right): the selected place a crimson diamond, its
    // name in full ivory and a crimson rule under it (stable); a hovered one a soft glow,
    // cleared the moment the mouse leaves (Session 9, from LIGHT SEEKER).
    class PlaceButton final : public dc::ui::HoverButton {
    public:
        PlaceButton() : dc::ui::HoverButton(juce::String()) {}
        void paintHover(juce::Graphics&, bool hover, bool down) override;
    };
    // The Advanced drawer: one row per module parameter, grouped by module.
    class Drawer final : public juce::Component {
    public:
        explicit Drawer(juce::AudioProcessorValueTreeState&);
        void paint(juce::Graphics&) override;
        void resized() override;
        juce::Button& closeButton() { return close; }
        std::function<void()> onClose;
        juce::Viewport* viewportForTest() { return &viewport; }
        int scroll() const { return viewport.getViewPositionY(); }
        void setScroll(int y) { viewport.setViewPosition(0, y); }
    private:
        juce::TextButton close;
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
        juce::Button& closeButton() { return close; }
        std::function<void()> onClose;
        juce::Viewport* viewportForTest() { return &viewport; }
        int scroll() const { return viewport.getViewPositionY(); }
        void setScroll(int y) { viewport.setViewPosition(0, y); }
    private:
        // The panel's contents scroll under its title row (it ends above the controls).
        struct Body final : juce::Component {
            std::function<void(juce::Graphics&)> painter;
            void paint(juce::Graphics& g) override { if (painter) painter(g); }
        };
        juce::Viewport viewport;
        Body body;
        juce::TextButton close;
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
    // Session 10: the search preview's phrase (G3 C4 E4 G4, 190 ms apart, held while the
    // button is down, at most 3 s), through the processor's lock-free preview notes.
    class PreviewPlayer final : public juce::Timer {
    public:
        explicit PreviewPlayer(DungeonCrawlerProcessor& p) : processor(p) {}
        ~PreviewPlayer() override { stop(); }
        void start(int design);
        void stop();
        void timerCallback() override;
        void advance(double seconds);   // tests
        int instrument = -1;
        std::vector<int> sounding;
    private:
        void play(double elapsed);
        DungeonCrawlerProcessor& processor;
        double startedAt = 0, offset = 0;
        int step = 0;
    };
    // Hotfix 1: a manual choice of place (map, landmark, list, hotspot, breadcrumb)
    // always wins: it stops a Journey or Roam, sets the place and shows it.
    void travelManually(int location, int sub);
    void showNotice(const juce::String&);
    void refreshSteering();
    void chooseInit();
    void choosePreset(int factoryIndex);
    // Hotfix 2: the panels and how they close.
    void closePanels();
    void pointerDown(juce::Component*);
    int instrumentShownInCrumb = -1;
    // A small plate, top centre, while a Journey or Roam runs: what runs and Stop.
    struct SteerBadge final : juce::Component {
        juce::Label text;
        juce::TextButton stop { "Stop" };
        void paint(juce::Graphics&) override;
        void resized() override;
    };
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
    // The frame the view paints from (Session 9, LIGHT SEEKER's double buffer): the shown
    // scene at rest, kept until the next one is ready. A repaint of any region (an ember's,
    // a button's) draws it again from here, never a fill: if the art at a new pixel size is
    // still rendering, the previous frame is scaled meanwhile.
    juce::Image frame;
    void refreshFrame();
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
    std::array<std::unique_ptr<PlaceButton>, dc::placesPerLocation> subs;
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
    dc::ui::EffectsRoller effectsRoller;   // the FX's (the instrument's is on its panel)
    juce::TextButton presetButton, prevPreset, nextPreset, slotA { "A" }, slotB { "B" }, copySlot, undoButton, redoButton, settingsButton;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttachment;
    bool controlsShown = true;
    Drawer drawer;
    JourneyPanel journeyPanel;
    SteerBadge steerBadge;
    juce::Label notice;
    double noticeUntil = 0;
    Follower follower { *this };
    dc::ui::SearchButton searchButton;
    dc::ui::SearchPanel searchPanel;
    PreviewPlayer previewer { processor };
    bool wasSteering = false;
    int shown = -1, shownSub = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DungeonCrawlerView)
};

// The plug-in window: the view at 960 x 560, scaled to the window (resizable, fixed
// aspect; the size menu offers 75-200 %). The scale is saved with the state. The
// instrument (DUNGEON CRAWLER) adds its panel under the view (960 x 756 at 100 %), laid out
// as LIGHT SEEKER's (Session 9).
class DungeonCrawlerEditor final : public juce::AudioProcessorEditor {
public:
    static constexpr int defaultWidth = DungeonCrawlerView::defaultWidth, defaultHeight = DungeonCrawlerView::defaultHeight;
    explicit DungeonCrawlerEditor(DungeonCrawlerProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override {}
    DungeonCrawlerView& view() noexcept { return content; }
    void setScale(float);
    int baseHeight() const noexcept { return defaultHeight + (panel != nullptr ? dc::ui::InstrumentPanel::panelHeight : 0); }
    dc::ui::InstrumentPanel* instrumentPanel() noexcept { return panel.get(); }
    // Cmd+F from anywhere in the window (the instrument panel too).
    bool keyPressed(const juce::KeyPress&) override;
private:
    // A click on the instrument panel closes the search (it is outside it).
    struct PanelClicks final : juce::MouseListener {
        explicit PanelClicks(DungeonCrawlerView& v) : view(v) {}
        void mouseDown(const juce::MouseEvent&) override { view.closeSearch(); }
        DungeonCrawlerView& view;
    };
    DungeonCrawlerProcessor& processor;
    DungeonCrawlerView content;
    PanelClicks panelClicks { content };   // before the panel: it outlives it
    std::unique_ptr<dc::ui::InstrumentPanel> panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DungeonCrawlerEditor)
};
