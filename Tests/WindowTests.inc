// Session 9: the window (included by DungeonCrawlerTests.cpp, "window" group), copied from
// LIGHT SEEKER's Direction Update 2 tests and adapted.
//   frame     every partial repaint (an ember's, a button's) is drawn from the scene, never
//             a fill, also while the art at a new size is still being prepared
//   restore   a closed and reopened window comes back exactly as it was left (also from a
//             saved session), in both plug-ins: the view and scene, the panels and where
//             they were scrolled, the controls, the window size, the instrument
//   hover     hover clears when the mouse leaves; the selected place stays marked

namespace windowtests {
// Paints the editor into `backing` only inside `dirty` (editor coordinates): what the
// operating system does with a repaint of a region.
void paintRegion(juce::Component& editor, juce::Image& backing, juce::Rectangle<int> dirty) {
    juce::Graphics g(backing);
    g.reduceClipRegion(dirty);
    editor.paintEntireComponent(g, true);
}
// The share of pixels in `area` that differ from `reference` by more than `tolerance` (any channel).
double changedShare(const juce::Image& a, const juce::Image& reference, juce::Rectangle<int> area, int tolerance) {
    area = area.getIntersection(a.getBounds()).getIntersection(reference.getBounds());
    if (area.isEmpty()) return 0.0;
    const juce::Image::BitmapData da(a, juce::Image::BitmapData::readOnly), db(reference, juce::Image::BitmapData::readOnly);
    long long changed = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x) {
            const auto p = da.getPixelColour(x, y), q = db.getPixelColour(x, y);
            if (std::abs(p.getRed() - q.getRed()) > tolerance || std::abs(p.getGreen() - q.getGreen()) > tolerance
                || std::abs(p.getBlue() - q.getBlue()) > tolerance) ++changed;
        }
    return double(changed) / double(area.getWidth() * area.getHeight());
}
struct Window {
    std::unique_ptr<DungeonCrawlerProcessor> p;
    std::unique_ptr<juce::AudioProcessorEditor> base;
    DungeonCrawlerEditor* editor = nullptr;
    explicit Window(bool instrument = false) : p(std::make_unique<DungeonCrawlerProcessor>(instrument ? PluginKind::Instrument : PluginKind::Effect)) {
        p->setRateAndBufferSizeDetails(48000, 512);
        p->prepareToPlay(48000, 512);
        open();
    }
    void open() {
        base.reset(p->createEditor());
        editor = dynamic_cast<DungeonCrawlerEditor*>(base.get());
    }
    void close() { base.reset(); editor = nullptr; }
    DungeonCrawlerView& view() { return editor->view(); }
    void go(int location, int sub) {
        view().mapButton().onClick();
        view().settleForTest();
        view().locationButton(location).onClick();
        view().settleForTest();
        view().subLocationButton(sub).onClick();
        view().settleForTest();
    }
};
} // namespace windowtests

void testWindowRestore(Suite& s, bool instrument) {
    using namespace windowtests;
    const std::string kind = instrument ? "instrument" : "FX";
    Window w(instrument);
    w.go(dc::Cathedral, 2);
    auto& view = w.view();
    view.advancedButton().setToggleState(true, juce::dontSendNotification);
    view.advancedButton().onClick();
    if (auto* vp = view.drawerViewportForTest()) vp->setViewPosition(0, 120);
    const int scrolled = view.drawerViewportForTest()->getViewPositionY();
    w.editor->setScale(1.25f);
    const int width = w.editor->getWidth(), height = w.editor->getHeight();
    int chosen = -1;
    if (instrument) {
        chosen = dc::inst::designFor(dc::Village, 1);   // the Tavern's guitar, played in the Cathedral
        w.p->chooseInstrument(chosen);
    }
    w.close();
    w.open();
    auto& again = w.view();
    again.settleForTest();
    s.check(!again.showingMap() && again.shownScene() == dc::art::SceneId { dc::Cathedral, 2 },
            "restore (" + kind + "): the reopened window shows the same place's scene (not the world map)");
    s.check(again.subLocationButton(2).getToggleState(), "restore (" + kind + "): the same place is selected in the list");
    s.check(again.advancedOpen() && !again.journeyOpen(), "restore (" + kind + "): the Advanced drawer is open again");
    s.check(again.drawerViewportForTest()->getViewPositionY() == scrolled, "restore (" + kind + "): ... scrolled where it was",
            std::to_string(again.drawerViewportForTest()->getViewPositionY()) + " vs " + std::to_string(scrolled));
    s.check(w.editor->getWidth() == width && w.editor->getHeight() == height && std::abs(width - juce::roundToInt(960 * 1.25f)) <= 1,
            "restore (" + kind + "): the same window size (125 %)", std::to_string(w.editor->getWidth()) + " x " + std::to_string(w.editor->getHeight()));
    if (instrument) {
        auto* panel = w.editor->instrumentPanel();
        s.check(panel != nullptr && panel->isVisible() && w.p->instrumentLocked() && w.p->instrumentIndex() == chosen && panel->titleText() == dc::inst::design(chosen).name,
                "restore (instrument): the same instrument, its panel shown", panel != nullptr ? panel->titleText().toStdString() : std::string("no panel"));
    }
    // The Journey panel and the controls hidden; then the map.
    again.advancedButton().setToggleState(false, juce::dontSendNotification);
    again.advancedButton().onClick();
    again.journeyButton().setToggleState(true, juce::dontSendNotification);
    again.journeyButton().onClick();
    w.close();
    w.open();
    s.check(w.view().journeyOpen() && !w.view().advancedOpen(), "restore (" + kind + "): the Journey panel open again (and only it)");
    w.view().closePanelsForTest();
    w.view().controlsButton().onClick();   // hide the controls
    w.close();
    w.open();
    s.check(!w.view().controlsShownForTest() && !w.view().journeyOpen(), "restore (" + kind + "): the controls stay hidden, the panel closed");
    w.view().controlsButton().onClick();
    // A location's own scene (sub-location 0), then the map.
    w.go(dc::Swamp, 0);
    w.close();
    w.open();
    w.view().settleForTest();
    s.check(!w.view().showingMap() && w.view().shownScene() == dc::art::SceneId { dc::Swamp, 0 }, "restore (" + kind + "): a location's own scene reopens as it was");
    w.view().mapButton().onClick();
    w.view().settleForTest();
    w.close();
    w.open();
    w.view().settleForTest();
    s.check(w.view().showingMap(), "restore (" + kind + "): left on the map, it reopens on the map");
    // A saved session (the project reopened): a new processor from the saved state.
    w.go(dc::Necropolis, 1);
    w.view().journeyButton().setToggleState(true, juce::dontSendNotification);
    w.view().journeyButton().onClick();
    w.editor->setScale(0.75f);
    juce::MemoryBlock block;
    w.p->getStateInformation(block);
    w.close();
    Window other(instrument);
    other.close();
    other.p->setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    other.open();
    other.view().settleForTest();
    s.check(!other.view().showingMap() && other.view().shownScene() == dc::art::SceneId { dc::Necropolis, 1 } && other.view().journeyOpen(),
            "restore (" + kind + "): a saved session reopens on the same scene with the Journey panel open");
    s.check(std::abs(other.editor->getWidth() - juce::roundToInt(960 * 0.75f)) <= 1, "restore (" + kind + "): ... at the same window size (75 %)");
    if (instrument)
        s.check(other.p->instrumentLocked() && other.p->instrumentIndex() == chosen, "restore (instrument): ... with the same instrument");
    // A user preset carries sounds, not the window.
    const auto keep = other.p->viewState();
    juce::File preset = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("dc-view-test.dcpreset");
    {
        juce::MemoryBlock data;
        Window third(instrument);
        third.go(dc::Castle, 0);
        third.p->getStateInformation(data);
        if (auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()))) xml->writeTo(preset);
    }
    other.p->loadUserPreset(preset);
    preset.deleteFile();
    const auto now = other.p->viewState();
    s.check(now.place == keep.place && now.journey == keep.journey && now.location == keep.location && now.sub == keep.sub,
            "restore (" + kind + "): loading a preset keeps the window as it is");
    // An older session without the window's state opens on the map, as before.
    DungeonCrawlerProcessor fresh(instrument ? PluginKind::Instrument : PluginKind::Effect);
    s.check(!fresh.viewState().valid, "restore (" + kind + "): a new instance has no stored window (opens on the map)");
}

void testWindow(Suite& s) {
    using namespace windowtests;
    s.section("window: reopens as it was left, hover clears, double-buffered repaints (Session 9)");
    // ---- Double buffer: a partial repaint shows the scene, also when the art at the
    // current pixel size is not ready (it used to fill the region with black).
    for (float scale : { 0.75f, 1.0f, 1.5f }) {
        Window w;
        w.editor->setScale(scale);
        w.go(dc::Dungeon, 4);
        auto& view = w.view();
        const auto label = "frame at " + std::to_string(int(scale * 100.0f)) + " %";
        view.setAnimation(false);
        s.check(view.frameForTest().isValid(), label + ": the scene's frame is ready");
        const auto bounds = w.editor->getLocalBounds();
        const auto scene = w.editor->createComponentSnapshot(bounds, true, 1.0f);
        view.forgetScenesForTest();
        // Regions the life repaints (embers, torches), or the middle of the art.
        auto regions = view.lifeRegions();
        regions.push_back(juce::Rectangle<int>(200, 120).withCentre(view.getLocalBounds().getCentre().translated(0, -60)));
        double worst = 0;
        auto stale = scene.createCopy();
        for (const auto& r : regions) {
            const auto region = w.editor->getLocalArea(&view, r).getIntersection(bounds);
            if (region.isEmpty()) continue;
            paintRegion(*w.editor, stale, region);
            worst = std::max(worst, changedShare(stale, scene, region, 6));
        }
        s.check(worst < 0.01, label + ": with the art not ready, a partial repaint shows the scene (never a fill)", fmt(worst * 100.0, 2) + " % differ");
    }
    {
        Window w;
        w.go(dc::Castle, 1);
        auto& view = w.view();
        const auto first = view.frameForTest();
        w.editor->setScale(1.5f);
        s.check(view.frameForTest().isValid(), "resized: the previous frame is kept (scaled) meanwhile");
        view.settleForTest();
        s.check(view.frameForTest().isValid() && view.frameForTest().getWidth() > first.getWidth(), "resized: then the art at the new size",
                std::to_string(first.getWidth()) + " -> " + std::to_string(view.frameForTest().getWidth()));
    }

    // ---- Restore: close and reopen; and a saved session; both plug-ins.
    testWindowRestore(s, false);
    testWindowRestore(s, true);

    // ---- Hover: clears when the mouse leaves; the selected place stays marked.
    {
        Window w;
        w.go(dc::Castle, 0);
        auto& view = w.view();
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto event = [&](juce::Component& c) {
            return juce::MouseEvent(src, c.getLocalBounds().getCentre().toFloat(), {}, juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                    &c, &c, juce::Time::getCurrentTime(), c.getLocalBounds().getCentre().toFloat(), juce::Time::getCurrentTime(), 1, false);
        };
        const auto enter = [&](juce::Component& c) { c.mouseEnter(event(c)); };
        const auto leave = [&](juce::Component& c) { c.mouseExit(event(c)); };
        const int count = dc::subLocationCount(dc::Castle) + 1;
        // Click the second place, then pass the mouse over every other entry and away.
        auto& clicked = view.subLocationButton(1);
        enter(clicked);
        clicked.onClick();
        view.settleForTest();
        leave(clicked);
        int stuck = 0;
        for (int k = 0; k < count; ++k) {
            auto& b = view.subLocationButton(k);
            enter(b);
            if (!view.placeHoverShown(k)) ++stuck;   // must show while over it
            leave(b);
            if (view.placeHoverShown(k)) ++stuck;
        }
        s.check(stuck == 0, "hover: every place in the list highlights while hovered and clears when the mouse leaves (after a click)");
        int selected = 0;
        for (int k = 0; k < count; ++k) selected += view.subLocationButton(k).getToggleState() ? 1 : 0;
        s.check(selected == 1 && view.subLocationButton(1).getToggleState(), "hover: the selected place stays marked (only it)");
        // A missed exit (the entry hidden and shown again under the mouse, or the host not
        // sending the exit): the next mouse move anywhere clears it.
        for (int k = 0; k < count; ++k) enter(view.subLocationButton(k));
        view.subLocationButton(0).setState(juce::Button::buttonOver);
        const int cleared = view.sweepHoverForTest({ -5000.0f, -5000.0f });
        int left = 0;
        for (int k = 0; k < count; ++k) left += view.placeHoverShown(k) || view.subLocationButton(k).getState() == juce::Button::buttonOver ? 1 : 0;
        s.check(cleared > 0 && left == 0, "hover: a missed exit is cleared by the next mouse move elsewhere", std::to_string(left) + " still lit");
        // Hidden while hovered (a travel's transition): the hover is gone when it shows again.
        enter(view.hotspotButton(0));
        view.hotspotButton(0).setVisible(false);
        view.hotspotButton(0).setVisible(true);
        s.check(!view.hotspotHoverShown(0), "hover: a hotspot hidden while hovered shows no hover when it comes back");
        // Every button of the window and the instrument panel: a stale hover state cleared.
        Window inst(true);
        inst.go(dc::Caverns, 1);
        std::vector<juce::Button*> all;
        std::function<void(juce::Component&)> collect = [&](juce::Component& c) {
            for (auto* child : c.getChildren()) {
                if (auto* b = dynamic_cast<juce::Button*>(child); b != nullptr && b->isVisible() && c.isVisible()) all.push_back(b);
                collect(*child);
            }
        };
        collect(*inst.editor);
        for (auto* b : all) b->setState(juce::Button::buttonOver);
        inst.view().sweepHoverForTest({ -5000.0f, -5000.0f });
        if (auto* panel = inst.editor->instrumentPanel()) dc::ui::sweepHover(*panel, { -5000.0f, -5000.0f });
        int lit = 0;
        for (auto* b : all) lit += b->getState() == juce::Button::buttonOver ? 1 : 0;
        s.check(all.size() > 20 && lit == 0, "hover: every hoverable button (" + std::to_string(all.size()) + ", the place list, toolbar, panel) clears",
                std::to_string(lit) + " still lit");
        // On the map: the landmark's name plate follows the hover and goes on exit.
        inst.view().mapButton().onClick();
        inst.view().settleForTest();
        auto& mark = inst.view().locationButton(dc::Mountains);
        enter(mark);
        const bool shownOver = inst.view().hoveredLandmark() == dc::Mountains && inst.view().landmarkHoverShown(dc::Mountains);
        leave(mark);
        s.check(shownOver && inst.view().hoveredLandmark() == -1 && !inst.view().landmarkHoverShown(dc::Mountains),
                "hover: a landmark's name plate shows while hovered and goes when the mouse leaves");
    }
}

// The new panel (and the FX's strip with the roller), written for review (docs/snapshots).
int panelSnapshots(const juce::String& directory) {
    using namespace windowtests;
    juce::File dir(directory);
    dir.createDirectory();
    int written = 0;
    const auto save = [&](const juce::Image& image, const char* name) {
        auto file = dir.getChildFile(name);
        file.deleteFile();
        juce::FileOutputStream stream(file);
        if (stream.openedOk() && juce::PNGImageFormat().writeImageToStream(image, stream)) ++written;
    };
    {
        Window w(true);
        w.go(dc::Cathedral, 0);   // the Cloister's Soft Choir: Vowel and Ensemble lit
        w.p->keyboardNote(60, 0.8f);
        w.p->keyboardNote(64, 0.8f);
        w.p->keyboardNote(67, 0.8f);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::pitchWheel(1, 11000), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 70), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        for (int k = 0; k < 8; ++k) { w.p->processBlock(buffer, midi); midi.clear(); }
        if (auto* panel = w.editor->instrumentPanel()) panel->pollForTest();
        w.view().setAnimation(false);
        const auto image = w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f);
        save(image, "s9-instrument-window.png");
        if (auto* panel = w.editor->instrumentPanel())
            save(w.editor->createComponentSnapshot(panel->getBounds(), true, 2.0f), "s9-instrument-panel.png");
        // A non-singing instrument: Vowel and Ensemble dimmed; the roller at 140 %.
        w.go(dc::Dungeon, 0);
        if (auto* fx = w.p->parameters.getParameter(dc::ids::effects)) fx->setValueNotifyingHost(fx->convertTo0to1(1.4f));
        for (int k = 0; k < 4; ++k) w.p->processBlock(buffer, midi);
        if (auto* panel = w.editor->instrumentPanel()) {
            panel->pollForTest();
            save(w.editor->createComponentSnapshot(panel->getBounds(), true, 2.0f), "s9-instrument-panel-pizzicato.png");
        }
    }
    {
        Window w(false);
        w.go(dc::Dungeon, 3);
        if (auto* fx = w.p->parameters.getParameter(dc::ids::effects)) fx->setValueNotifyingHost(fx->convertTo0to1(0.6f));
        w.view().setAnimation(false);
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s9-fx-window.png");
        const auto strip = juce::Rectangle<int>(0, 560 - 150, 960, 150);
        save(w.editor->createComponentSnapshot(strip, true, 2.0f), "s9-fx-controls.png");
    }
    return written == 5 ? 0 : 1;
}
