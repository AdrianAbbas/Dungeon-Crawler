// Session 10: search (included by DungeonCrawlerTests.cpp, "search" group).
//   index     every place, instrument and preset is found by its name and by at least
//             three of its tags; typos and partial words find the right results; the
//             recordings; the FX has no instruments; the filters
//   panel     open (magnifier, Cmd+F), close (Esc, a click outside that still reaches the
//             map), results as you type, grouped, keyboard navigation, descriptions,
//             Recent (also saved with the session), chips, the preview
//   landing   Enter on a result lands exactly where choosing it by hand does (the
//             parameters, the route, the view and the window state compared), for every
//             place, instrument and preset, in both plug-ins; held notes ring on; Lock
//             instrument respected; the window reopens where the search took it
//   cost      the index is built once; a search and a keystroke's refresh are instant

namespace searchtests {
using dc::search::Kind;

int positionOf(const std::vector<dc::search::Result>& results, const dc::search::Index& idx, const std::string& key) {
    for (std::size_t k = 0; k < results.size(); ++k)
        if (idx.entries()[std::size_t(results[k].entry)].key == key) return int(k);
    return -1;
}
// The first result of a kind.
const dc::search::Entry* firstOf(const std::vector<dc::search::Result>& results, const dc::search::Index& idx, Kind kind) {
    for (const auto& r : results)
        if (idx.entries()[std::size_t(r.entry)].kind == kind) return &idx.entries()[std::size_t(r.entry)];
    return nullptr;
}

// Where a window has landed: what the processor and the view say.
struct Landed {
    std::vector<float> values;
    std::vector<dc::Stop> route;
    bool map = true;
    int location = -1, sub = -1, instrument = -1;
    bool locked = false;
    juce::String preset;
    DungeonCrawlerProcessor::ViewState state;
    std::string describe() const {
        return std::string(map ? "map" : "place") + " " + std::to_string(location) + ":" + std::to_string(sub) + " inst " + std::to_string(instrument)
               + (locked ? " locked" : "") + " preset " + preset.toStdString() + " view " + (state.place ? "place " : "map ") + std::to_string(state.location) + ":"
               + std::to_string(state.sub);
    }
    std::string differs(const Landed& o) const {
        if (values.size() != o.values.size()) return "parameter count";
        for (std::size_t k = 0; k < values.size(); ++k)
            if (std::abs(values[k] - o.values[k]) > 1.0e-6f) return "parameter " + std::to_string(k);
        if (route.size() != o.route.size()) return "route";
        for (std::size_t k = 0; k < route.size(); ++k)
            if (route[k].location != o.route[k].location || route[k].sub != o.route[k].sub) return "route";
        if (map != o.map || location != o.location || sub != o.sub) return "view: " + describe() + " vs " + o.describe();
        if (instrument != o.instrument || locked != o.locked) return "instrument: " + describe() + " vs " + o.describe();
        if (preset != o.preset) return "preset name: " + describe() + " vs " + o.describe();
        if (!(state == o.state)) return "window state: " + describe() + " vs " + o.describe();
        return {};
    }
};

Landed landed(windowtests::Window& w) {
    auto& v = w.view();
    v.settleForTest();
    Landed l;
    for (auto* p : w.p->getParameters()) l.values.push_back(p->getValue());
    l.route = w.p->route();
    l.map = v.showingMap();
    l.location = v.shownScene().location;
    l.sub = v.shownScene().sub;
    l.instrument = w.p->instrumentIndex();
    l.locked = w.p->instrumentLocked();
    l.preset = w.p->presetDisplayName();
    l.state = w.p->viewState();
    return l;
}

// Choosing a place by hand: the map's landmark, then the place list (or the list of the
// location already shown; another location: back to the map first).
void placeByHand(windowtests::Window& w, int l, int k) {
    auto& v = w.view();
    v.settleForTest();
    if (!v.showingMap() && v.shownLocation() != l) { v.mapButton().onClick(); v.settleForTest(); }
    if (v.showingMap()) {
        v.locationButton(l).onClick();
        v.settleForTest();
    }
    if (k != 0 || v.shownSubLocation() != 0) {
        v.subLocationButton(k).onClick();
        v.settleForTest();
    }
}

// The search: open it, type the name, arrow down to the result, Enter. Empty on success.
std::string bySearch(windowtests::Window& w, const dc::search::Entry& e, const juce::String& typed = {}) {
    auto& v = w.view();
    v.settleForTest();
    v.openSearch();
    auto& panel = v.searchPanelForTest();
    if (!panel.isVisible()) return "the panel did not open";
    panel.setQuery(typed.isNotEmpty() ? typed : juce::String::fromUTF8(e.name.c_str()));
    int at = -1;
    for (int k = 0; k < panel.resultCount(); ++k)
        if (const auto* r = panel.resultAt(k); r != nullptr && r->key == e.key) { at = k; break; }
    if (at < 0) return "not among the results";
    for (int k = 0; k < at; ++k) panel.handleKey(juce::KeyPress(juce::KeyPress::downKey));
    if (panel.selectedResult() != at) return "the arrow keys did not reach it";
    panel.handleKey(juce::KeyPress(juce::KeyPress::returnKey));
    if (panel.isVisible()) return "the panel stayed open";
    v.settleForTest();
    return {};
}

void render(windowtests::Window& w, int blocks) {
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int k = 0; k < blocks; ++k) { buffer.clear(); w.p->processBlock(buffer, midi); }
}
} // namespace searchtests

void testSearchIndex(Suite& s, bool instrument) {
    using namespace searchtests;
    const std::string kind = instrument ? "instrument" : "FX";
    const auto& idx = dc::search::index(instrument);
    int instruments = 0, places = 0, presets = 0;
    for (const auto& e : idx.entries()) (e.kind == Kind::Instrument ? instruments : e.kind == Kind::Place ? places : presets)++;
    const int presetCount = instrument ? int(dc::inst::instrumentPresets().size()) : int(dc::factoryPresets().size());
    s.check(instruments == (instrument ? dc::inst::instrumentCount : 0) && places == 40 + dc::LocationCount && presets == presetCount,
            kind + ": the index holds every instrument, location, place and preset (" + std::to_string(instruments) + " / " + std::to_string(places) + " / "
                + std::to_string(presets) + ")");
    // Every entry by its own name (among the first three; a place and a preset may share
    // a name), and by at least three of its tags (each tag searched alone, in its kind).
    std::string nameFail, tagFail, fewTags, describedFail;
    for (const auto& e : idx.entries()) {
        const auto byName = idx.search(e.name, {}, 1000);
        const int at = positionOf(byName, idx, e.key);
        if (at < 0 || at > 2) nameFail += e.name + " (" + std::to_string(at) + "); ";
        if (e.tags.size() < 3) fewTags += e.name + "; ";
        if (e.description.size() < 20 || e.where.empty()) describedFail += e.name + "; ";
        dc::search::Filter f;
        f.instruments = e.kind == Kind::Instrument;
        f.places = e.kind == Kind::Place;
        f.presets = e.kind == Kind::Preset;
        int found = 0;
        for (const auto& t : e.tags)
            if (positionOf(idx.search(t, f, 1000), idx, e.key) >= 0) ++found;
        if (found < 3) tagFail += e.name + " (" + std::to_string(found) + "); ";
    }
    s.check(nameFail.empty(), kind + ": every place, instrument and preset is found by its name (first three)", nameFail);
    s.check(fewTags.empty() && tagFail.empty(), kind + ": every one has at least three tags and is found by at least three of them", fewTags + tagFail);
    s.check(describedFail.empty(), kind + ": every one has a description and where it belongs", describedFail);
    // Typos, partial words, case.
    struct Typo { const char* typed; Kind kind; const char* expected; };
    std::vector<Typo> typos {
        { "crpyt", Kind::Place, "Crypt" }, { "dwarvn", Kind::Place, "Dwarven Hall" }, { "cathedrl", Kind::Place, "Cathedral" },
        { "graveyrd", Kind::Place, "Graveyard" }, { "obsevatory", Kind::Place, "Observatory" }, { "WITCH", Kind::Place, "Witch's Hut" },
        { "crys", Kind::Place, "Crystal Cavern" }, { "obs", Kind::Place, "Observatory" }, { "mausoluem", Kind::Place, "Mausoleum" },
        { "pilgrimmage", Kind::Preset, "Pilgrimage" }, { "dragns road", Kind::Preset, "The Dragon's Road" },
    };
    if (instrument) {
        typos.push_back({ "dwarvn", Kind::Instrument, "Dwarven Chant" });
        typos.push_back({ "kalimab", Kind::Instrument, "Water Kalimba" });
        typos.push_back({ "harpsicord", Kind::Instrument, "Harpsichord Arpeggio" });
        typos.push_back({ "war drums", Kind::Instrument, "War Drums" });
        typos.push_back({ "dark choir", Kind::Instrument, "Dark Choir Pad" });
        typos.push_back({ "glock", Kind::Instrument, "Ice Glockenspiel" });
        typos.push_back({ "Celestial", Kind::Instrument, "Celestial Pad" });
    }
    std::string typoFail;
    for (const auto& t : typos) {
        const auto r = idx.search(t.typed, {}, 60);
        const auto* first = firstOf(r, idx, t.kind);
        const bool top = !r.empty() && positionOf(r, idx, first != nullptr ? first->key : "") <= 3;
        if (first == nullptr || first->name != t.expected || !top)
            typoFail += std::string(t.typed) + " -> " + (first != nullptr ? first->name : "nothing") + "; ";
    }
    s.check(typoFail.empty(), kind + ": typos, partial words and capitals find the right result (crpyt, dwarvn, kalimab, crys, WITCH...)", typoFail);
    // The words a search is made of: the edit distance and a word's match.
    s.check(dc::search::editDistance("crpyt", "crypt", 2) == 1 && dc::search::editDistance("dwarvn", "dwarven", 2) == 1
                && dc::search::editDistance("abc", "xyz", 1) == 2 && dc::search::wordMatch("cho", "choir") > 0.8f && dc::search::wordMatch("zzz", "choir") == 0.0f,
            kind + ": the fuzzy matcher (transpositions, a missing letter, prefixes)");
    // A nonsense word finds nothing; every word of a query must match.
    s.check(idx.search("qwxzv", {}, 60).empty() && idx.search("crypt qwxzv", {}, 60).empty(), kind + ": nonsense finds nothing (every word must match)");
    // Instruments: by family and by location; the recordings.
    if (instrument) {
        std::string familyFail, locationFail;
        for (int i = 0; i < dc::inst::instrumentCount; ++i) {
            const auto* e = idx.find("i:" + std::to_string(i));
            dc::search::Filter f;
            f.instruments = true;
            if (positionOf(idx.search(e->family, f, 1000), idx, e->key) < 0) familyFail += e->name + "; ";
            if (positionOf(idx.search(dc::locationNames[std::size_t(e->location)], f, 1000), idx, e->key) < 0) locationFail += e->name + "; ";
        }
        s.check(familyFail.empty() && locationFail.empty(), "instrument: every instrument is found by its family and by its location", familyFail + locationFail);
        const std::vector<std::pair<const char*, std::vector<const char*>>> recordings {
            { "guitar", { "Lute", "Weary Guitar" } }, { "banjo", { "Detuned Banjo" } }, { "djembe", { "War Drums" } },
            { "piano", { "Doom Brass & Choir", "Tolling Bell & Organ" } }, { "violin", { "Fiddle", "Ghost Violin" } }, { "kalimba", { "Water Kalimba" } },
            { "voice", { "Soft Choir", "Monk Chant", "Dwarven Chant", "Dark Choir Pad", "Music Box" } }, { "whispers", { "Ancient Harp" } },
        };
        std::string recFail;
        for (const auto& [word, names] : recordings) {
            dc::search::Filter f;
            f.instruments = true;
            const auto r = idx.search(word, f, 1000);
            for (const auto* n : names) {
                bool hit = false;
                for (const auto& x : r) hit = hit || idx.entries()[std::size_t(x.entry)].name == n;
                if (!hit) recFail += std::string(word) + " -> " + n + "; ";
            }
        }
        s.check(recFail.empty(), "instrument: the developer's recordings find the instruments that play them (guitar, banjo, djembe, piano, violin, kalimba, voice, whispers)", recFail);
    } else {
        s.check(firstOf(idx.search("choir", {}, 1000), idx, Kind::Instrument) == nullptr && firstOf(idx.search("", {}, 1000), idx, Kind::Instrument) == nullptr,
                "FX: no instruments among the results");
    }
    {
        // Whispers in the places that whisper.
        dc::search::Filter f;
        f.places = true;
        const auto r = idx.search("whispers", f, 1000);
        s.check(positionOf(r, idx, "p:2:2") >= 0 && positionOf(r, idx, "p:8:1") >= 0 && positionOf(r, idx, "p:3:3") >= 0,
                kind + ": \"whispers\" finds the Crypt, the Graveyard and the Ancient Grove");
    }
    // The filters: a kind, a mood.
    std::string filterFail;
    for (const auto& mood : dc::search::moodChips()) {
        dc::search::Filter f;
        f.moods = { mood };
        const auto r = idx.search("", f, 1000);
        if (r.size() < 5) filterFail += mood + " (too few); ";
        for (const auto& x : r) if (!idx.entries()[std::size_t(x.entry)].hasTag(mood)) { filterFail += mood + "; "; break; }
    }
    {
        dc::search::Filter f;
        f.presets = true;
        for (const auto& x : idx.search("dark", f, 1000)) if (idx.entries()[std::size_t(x.entry)].kind != Kind::Preset) { filterFail += "presets; "; break; }
    }
    s.check(filterFail.empty(), kind + ": the kind and mood filters (Dark, Epic, Eerie, Sacred) admit only what they say", filterFail);
    {
        // Grouped: one heading per kind, every kind's results together.
        const auto r = idx.search("dark", {}, 1000);
        int changes = 0;
        for (std::size_t k = 1; k < r.size(); ++k) changes += idx.entries()[std::size_t(r[k].entry)].kind != idx.entries()[std::size_t(r[k - 1].entry)].kind;
        s.check(changes <= 2, kind + ": results are grouped by kind");
    }
    // Cost: building the index, a search.
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        dc::search::Index fresh(instrument);
        const double buildMs = juce::Time::getMillisecondCounterHiRes() - t0;
        const char* queries[] { "c", "cr", "cry", "cryp", "crypt", "dark choir", "dwarvn", "bells", "eerie whispers", "guitar", "the", "s" };
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        std::size_t total = 0;
        for (int k = 0; k < 200; ++k)
            for (const auto* q : queries) total += fresh.search(q).size();
        const double searchMs = (juce::Time::getMillisecondCounterHiRes() - t1) / (200.0 * 12.0);
        s.metric(kind + ": building the search index", buildMs, " ms");
        s.metric(kind + ": one search (average)", searchMs, " ms");
        s.check(total > 0 && buildMs < 50.0 && searchMs < 2.0, kind + ": the index builds in under 50 ms, a search takes under 2 ms",
                fmt(buildMs, 2) + " / " + fmt(searchMs, 3) + " ms");
    }
}

void testSearchPanel(Suite& s, bool instrument) {
    using namespace searchtests;
    const std::string kind = instrument ? "instrument" : "FX";
    windowtests::Window w(instrument);
    auto& v = w.view();
    auto& panel = v.searchPanelForTest();
    v.settleForTest();
    s.check(!panel.isVisible() && v.searchButtonForTest().isVisible(), kind + ": the magnifier is at the top of the window, the panel closed");
    nav::click(v.searchButtonForTest());
    s.check(panel.isVisible() && panel.showingRecent(), kind + ": the magnifier opens the panel (Recent, nothing typed)");
    s.check(panel.getY() < 80 && panel.getBottom() <= v.getHeight() && panel.getRight() <= v.getWidth() && panel.getX() >= 0, kind + ": the panel sits under the magnifier, inside the window");
    v.pressEscapeForTest();
    s.check(!panel.isVisible(), kind + ": Esc closes it");
    s.check(v.keyPressed(juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)) && panel.isVisible(), kind + ": Cmd+F opens it");
    panel.handleKey(juce::KeyPress(juce::KeyPress::escapeKey));
    s.check(!panel.isVisible(), kind + ": Esc in the field closes it");
    if (instrument) {
        // Cmd+F with the focus on the instrument panel (the key goes up to the editor).
        s.check(w.editor->keyPressed(juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)) && panel.isVisible(), "instrument: Cmd+F from the instrument panel opens it");
        v.closeSearch();
    }
    // A click outside closes it and still does what it does: the map is never blocked.
    v.openSearch();
    v.pointerDownForTest(v.locationButton(dc::Caverns));
    v.locationButton(dc::Caverns).onClick();
    v.settleForTest();
    s.check(!panel.isVisible() && !v.showingMap() && v.shownLocation() == dc::Caverns, kind + ": a click on a landmark closes the search and travels there");
    v.openSearch();
    v.pointerDownForTest(panel.listForTest());
    s.check(panel.isVisible(), kind + ": a click inside the panel keeps it open");
    v.pointerDownForTest(v.searchButtonForTest());
    nav::click(v.searchButtonForTest());
    s.check(!panel.isVisible(), kind + ": the magnifier closes it again");
    s.check(panel.isOpaque(), kind + ": the panel is opaque (the scene behind is not repainted for it)");
    // Results as you type, grouped, with the description of the selected one.
    v.openSearch();
    panel.setQuery("cr");
    const int afterTwo = panel.resultCount();
    panel.setQuery("cry");
    s.check(afterTwo > 0 && panel.resultCount() > 0 && !panel.showingRecent(), kind + ": results as you type (partial words)");
    panel.setQuery("crypt");
    s.check(panel.resultCount() > 0 && panel.rowText(0) == "Crypt" && panel.groupHeadings().startsWith("Places"), kind + ": \"crypt\": the Crypt first, under Places",
            panel.rowText(0).toStdString() + " / " + panel.groupHeadings().toStdString());
    s.check(panel.selectedResult() == 0 && panel.descriptionText() == juce::String::fromUTF8(panel.resultAt(0)->description.c_str()),
            kind + ": the first result is selected and its description shown");
    panel.setQuery("dark");
    s.check(panel.groupHeadings().contains("Places") && panel.groupHeadings().contains("Presets") && (!instrument || panel.groupHeadings().contains("Instruments")),
            kind + ": results grouped by kind", panel.groupHeadings().toStdString());
    // Keyboard: down, up, clamped at the ends; hovering selects too.
    const int count = panel.resultCount();
    panel.handleKey(juce::KeyPress(juce::KeyPress::downKey));
    panel.handleKey(juce::KeyPress(juce::KeyPress::downKey));
    const bool down = panel.selectedResult() == 2;
    panel.handleKey(juce::KeyPress(juce::KeyPress::upKey));
    const bool up = panel.selectedResult() == 1;
    for (int k = 0; k < 5; ++k) panel.handleKey(juce::KeyPress(juce::KeyPress::upKey));
    const bool top = panel.selectedResult() == 0;
    for (int k = 0; k < count + 5; ++k) panel.handleKey(juce::KeyPress(juce::KeyPress::downKey));
    const bool bottom = panel.selectedResult() == count - 1;
    s.check(down && up && top && bottom, kind + ": arrow keys move through the results, held at the first and the last");
    panel.hoverResult(3);
    s.check(panel.selectedResult() == 3 && panel.descriptionText() == juce::String::fromUTF8(panel.resultAt(3)->description.c_str()),
            kind + ": hovering a result shows its description");
    // The field's own keys go to the panel (a real key press in the field).
    panel.setQuery("bell");
    panel.fieldForTest().keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    s.check(panel.selectedResult() == 1 && panel.query() == "bell", kind + ": the arrow keys in the field move the selection (the text stays)");
    // The chips.
    std::string chipFail;
    s.check(panel.chipCount() == (instrument ? 7 : 6) && panel.chipName(0) == (instrument ? "Instruments" : "Places"), kind + ": the chips (kinds, then Dark, Epic, Eerie, Sacred)");
    for (int c = 0; c < panel.chipCount(); ++c) {
        panel.setQuery("");
        panel.toggleChip(c);
        const auto name = panel.chipName(c);
        if (panel.resultCount() == 0) chipFail += name.toStdString() + " (nothing); ";
        for (int k = 0; k < panel.resultCount(); ++k) {
            const auto* e = panel.resultAt(k);
            const bool ok = name == "Instruments" ? e->kind == Kind::Instrument : name == "Places" ? e->kind == Kind::Place : name == "Presets" ? e->kind == Kind::Preset
                                                                                                                                       : e->hasTag(name.toLowerCase().toStdString());
            if (!ok) { chipFail += name.toStdString() + "; "; break; }
        }
        panel.toggleChip(c);
    }
    s.check(chipFail.empty(), kind + ": every chip filters (and a second click clears it)", chipFail);
    panel.toggleChip(instrument ? 1 : 0);   // Places
    panel.setQuery("bells");
    bool placesOnly = panel.resultCount() > 0;
    for (int k = 0; k < panel.resultCount(); ++k) placesOnly = placesOnly && panel.resultAt(k)->kind == Kind::Place;
    s.check(placesOnly, kind + ": a chip with a query: only places that ring");
    v.closeSearch();
    // Recent: the last eight results chosen and searches left unfinished, newest first,
    // kept by the processor (a reopened window, a saved session).
    w.p->setSearchRecent({});
    const auto& idx = dc::search::index(instrument);
    std::vector<const dc::search::Entry*> chosen;
    for (const auto* key : { "p:0:1", "p:1:2", "p:2:3", "r:0", "p:3:1", "p:4:2", "r:2", "p:5:3", "p:6:1", "p:7:2" })
        if (const auto* e = idx.find(key)) chosen.push_back(e);
    for (const auto* e : chosen) bySearch(w, *e);
    v.openSearch();
    bool recentOk = panel.showingRecent() && panel.resultCount() == 8;
    for (int k = 0; k < 8 && recentOk; ++k) recentOk = panel.resultAt(k) != nullptr && panel.resultAt(k)->key == chosen[chosen.size() - 1 - std::size_t(k)]->key;
    s.check(recentOk, kind + ": Recent shows the last eight results chosen, newest first", std::to_string(panel.resultCount()));
    panel.setQuery("eerie whis");
    v.closeSearch();
    v.openSearch();
    s.check(panel.resultCount() == 8 && panel.resultAt(0) == nullptr && panel.rowText(0) == "eerie whis", kind + ": a search left unfinished goes into Recent");
    panel.chooseSelected();
    s.check(panel.isVisible() && panel.query() == "eerie whis" && panel.resultCount() > 0, kind + ": choosing a recent search searches again");
    v.closeSearch();
    {
        juce::MemoryBlock state;
        w.p->getStateInformation(state);
        DungeonCrawlerProcessor other(instrument ? PluginKind::Instrument : PluginKind::Effect);
        other.setStateInformation(state.getData(), int(state.getSize()));
        s.check(other.searchRecent() == w.p->searchRecent() && other.searchRecent().size() == 8, kind + ": Recent is saved with the session");
        w.close();
        w.open();
        auto& again = w.view().searchPanelForTest();
        w.view().openSearch();
        s.check(again.showingRecent() && again.resultCount() == 8, kind + ": and comes back in a reopened window");
        w.view().closeSearch();
    }
    // The preview (instrument only): hold to hear a short phrase; never while the host plays.
    auto& view = w.view();
    auto& p2 = view.searchPanelForTest();
    view.openSearch();
    p2.setQuery(instrument ? "water kalimba" : "crypt");
    if (!instrument) {
        s.check(!p2.previewVisible() && !view.previewPlaying(), "FX: no preview (no instruments)");
        view.closeSearch();
        return;
    }
    s.check(p2.resultAt(0) != nullptr && p2.resultAt(0)->kind == Kind::Instrument && p2.previewVisible(), "instrument: an instrument shows its preview button");
    const int kalimba = p2.resultAt(0)->instrument;
    render(w, 2);   // the audio thread up to date with the place
    const int before = w.p->instrumentIndex();
    const bool lockedBefore = w.p->instrumentLocked();
    const int placeBefore = w.p->locationIndex();
    p2.pressPreview(true);
    view.stepPreviewForTest(0.7);
    render(w, 4);
    const auto& synth = w.p->instrumentEngine();
    bool lit = true;
    for (int key : { 55, 60, 64, 67 }) lit = lit && synth.keyLit(key);
    s.check(view.previewPlaying() && view.previewKeysForTest().size() == 4 && lit && synth.activeVoices() >= 4,
            "instrument: holding the preview plays the phrase (G3 C4 E4 G4)");
    s.check(w.p->instrumentIndex() == before && w.p->instrumentLocked() == lockedBefore && w.p->locationIndex() == placeBefore && kalimba != before
                && w.p->playingInstrument.load() == before,
            "instrument: the preview changes nothing (the instrument, the lock, the place)",
            std::to_string(before) + " -> " + std::to_string(w.p->instrumentIndex()) + ", kalimba " + std::to_string(kalimba));
    // Its level: the phrase peaks at a sensible level (not silent, not clipping).
    {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        float peak = 0;
        for (int k = 0; k < 20; ++k) { buffer.clear(); w.p->processBlock(buffer, midi); peak = std::max(peak, buffer.getMagnitude(0, 512)); }
        s.check(peak > 0.01f && peak < 1.0f, "instrument: the preview plays at a sensible level", fmt(toDb(peak)) + " dBFS");
    }
    p2.pressPreview(false);
    render(w, 2);
    bool released = true;
    for (int key : { 55, 60, 64, 67 }) released = released && !synth.keyLit(key);
    s.check(!view.previewPlaying() && released, "instrument: letting go stops it");
    // While the host plays: nothing.
    w.p->hostPlaying.store(true);
    p2.pressPreview(true);
    view.stepPreviewForTest(0.7);
    render(w, 2);
    bool silent = !view.previewPlaying();
    for (int key : { 55, 60, 64, 67 }) silent = silent && !synth.keyLit(key);
    s.check(silent && !w.p->previewNote(kalimba, 60, 0.6f), "instrument: no preview while the host plays");
    p2.pressPreview(false);
    w.p->hostPlaying.store(false);
    view.closeSearch();
    // Cost: a keystroke's refresh.
    view.openSearch();
    double worst = 0;
    for (const auto* q : { "d", "dw", "dwa", "dwar", "dwarv", "dwarve", "dwarven", "s", "sa", "sac", "sacr", "sacre", "sacred" }) {
        p2.setQuery(q);
        worst = std::max(worst, p2.lastSearchMs());
    }
    s.metric("instrument: one keystroke's search and refresh (worst)", worst, " ms");
    s.check(worst < 5.0, "instrument: a keystroke's search and refresh takes under 5 ms", fmt(worst, 3) + " ms");
    view.closeSearch();
}

// Enter on a result lands exactly where choosing it by hand does: two windows side by
// side, one chosen by hand, the other by search, compared after every step.
void testSearchLanding(Suite& s, bool instrument) {
    using namespace searchtests;
    const std::string kind = instrument ? "instrument" : "FX";
    const auto& idx = dc::search::index(instrument);
    windowtests::Window hand(instrument), search(instrument);
    std::string placeFail, presetFail, instFail, searchFail;
    int compared = 0;
    const auto compare = [&](const dc::search::Entry& e, std::string& fail) {
        const auto err = bySearch(search, e);
        if (!err.empty()) { searchFail += e.name + ": " + err + "; "; return; }
        const auto d = landed(hand).differs(landed(search));
        if (!d.empty()) fail += e.name + " (" + d + "); ";
        ++compared;
    };
    // Every place and location, from the map and from inside other places (the list's
    // order: the next place is often in the location shown, sometimes in another).
    for (const auto& e : idx.entries()) {
        if (e.kind != Kind::Place) continue;
        placeByHand(hand, e.location, e.sub);
        compare(e, placeFail);
        // And from the map.
        hand.view().mapButton().onClick();
        search.view().mapButton().onClick();
        hand.view().settleForTest();
        search.view().settleForTest();
        if (e.sub == 0) continue;
        placeByHand(hand, e.location, e.sub);
        compare(e, placeFail);
    }
    s.check(placeFail.empty() && searchFail.empty(), kind + ": every place and location lands where choosing it on the map and in the list does", placeFail + searchFail);
    searchFail.clear();
    // Every preset and journey, as the preset menu.
    for (const auto& e : idx.entries()) {
        if (e.kind != Kind::Preset) continue;
        hand.view().choosePresetForTest(e.preset);
        compare(e, presetFail);
    }
    s.check(presetFail.empty() && searchFail.empty(), kind + ": every preset and journey lands where the preset menu does", presetFail + searchFail);
    searchFail.clear();
    if (instrument) {
        // Every instrument, as the panel's chooser (locked, played here).
        auto* panel = hand.editor->instrumentPanel();
        for (const auto& e : idx.entries()) {
            if (e.kind != Kind::Instrument) continue;
            panel->chooser().setSelectedId(e.instrument + 1, juce::sendNotificationSync);
            compare(e, instFail);
        }
        auto* searchPanel = search.editor->instrumentPanel();
        searchPanel->pollForTest();
        s.check(instFail.empty() && searchFail.empty() && searchPanel->titleText() == panel->titleText(),
                "instrument: every instrument lands where the instrument chooser does (and the panel names it)", instFail + searchFail);
    }
    s.metric(kind + ": results compared with choosing by hand", compared);
    // The window reopens where the search took it (Session 9's restore).
    {
        windowtests::Window w(instrument);
        bySearch(w, *idx.find("p:2:2"));
        w.view().settleForTest();
        w.close();
        w.open();
        w.view().settleForTest();
        s.check(!w.view().showingMap() && w.view().shownScene().location == dc::Cathedral && w.view().shownScene().sub == 2,
                kind + ": closed and reopened, the window is where the search took it (the Crypt)");
    }
    if (!instrument) return;
    // Held notes ring on through every kind of jump; Lock instrument is respected.
    {
        windowtests::Window w(true);
        render(w, 2);
        w.p->keyboardNote(60, 0.8f);
        render(w, 4);
        const auto& synth = w.p->instrumentEngine();
        std::string cut;
        for (const auto* key : { "p:3:2", "l:7", "i:12", "r:4", "p:9:3" }) {
            const auto* e = idx.find(key);
            bySearch(w, *e);
            render(w, 4);
            if (!synth.keyLit(60)) cut += e->name + "; ";
        }
        s.check(cut.empty(), "instrument: a held note rings on through a search's travel, instrument and preset", cut);
        w.p->keyboardNote(60, 0.0f);
        render(w, 2);
        // Locked: a place found by search keeps the instrument.
        w.view().searchPanelForTest();
        bySearch(w, *idx.find("i:5"));   // the Lute, locked
        bySearch(w, *idx.find("p:8:1")); // the Graveyard
        render(w, 2);
        s.check(w.p->instrumentLocked() && w.p->instrumentIndex() == 5 && w.p->playingInstrument.load() == 5 && w.view().shownScene().location == dc::Necropolis,
                "instrument: with Lock instrument on, a place found by search keeps the instrument");
    }
}

void testSearch(Suite& s) {
    s.section("search (Session 10)");
    for (bool instrument : { false, true }) {
        testSearchIndex(s, instrument);
        testSearchPanel(s, instrument);
        testSearchLanding(s, instrument);
    }
}

// docs/snapshots/session10: the panel in both plug-ins.
int searchSnapshots(const juce::String& directory) {
    using namespace searchtests;
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
        windowtests::Window w(false);
        auto& v = w.view();
        v.setAnimation(false);
        v.settleForTest();
        v.openSearch();
        v.searchPanelForTest().setQuery("crpyt");
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s10-fx-map-search-typo.png");
        v.searchPanelForTest().setQuery("eerie");
        v.searchPanelForTest().handleKey(juce::KeyPress(juce::KeyPress::downKey));
        save(w.editor->createComponentSnapshot(v.searchPanelForTest().getBoundsInParent(), true, 2.0f), "s10-fx-panel-eerie.png");
        v.closeSearch();
    }
    {
        windowtests::Window w(true);
        auto& v = w.view();
        v.setAnimation(false);
        w.go(dc::Cathedral, 1);
        const auto& idx = dc::search::index(true);
        for (const auto* key : { "p:3:3", "i:30", "r:10" }) bySearch(w, *idx.find(key));
        w.go(dc::Cathedral, 1);
        render(w, 4);
        if (auto* p = w.editor->instrumentPanel()) p->pollForTest();
        v.setAnimation(false);
        v.openSearch();
        auto& panel = v.searchPanelForTest();
        panel.setQuery("");
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s10-instrument-recent.png");
        panel.setQuery("choir");
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s10-instrument-search-choir.png");
        save(w.editor->createComponentSnapshot(panel.getBoundsInParent(), true, 2.0f), "s10-instrument-panel-choir.png");
        panel.setQuery("");
        panel.toggleChip(0);   // Instruments
        panel.toggleChip(3);   // Dark
        save(w.editor->createComponentSnapshot(panel.getBoundsInParent(), true, 2.0f), "s10-instrument-chips-dark.png");
        panel.toggleChip(0);
        panel.toggleChip(3);
        panel.setQuery("dwarvn");
        save(w.editor->createComponentSnapshot(panel.getBoundsInParent(), true, 2.0f), "s10-instrument-panel-typo.png");
        v.closeSearch();
    }
    return written == 7 ? 0 : 1;
}
