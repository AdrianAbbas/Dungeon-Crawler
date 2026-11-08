#include "PluginProcessor.h"
#include "World/Search.h"
#include "PluginEditor.h"
#include "World/Presets.h"
#include "Instrument/Presets.h"
#include "Instrument/DungeonSynthFit.h"
#include "UserData.h"
#include <cmath>
#include <optional>

juce::AudioProcessor::BusesProperties DungeonCrawlerProcessor::busesFor(PluginKind kind) {
    if (kind == PluginKind::Instrument) return BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
    return BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

namespace {
juce::StringArray instrumentNames(PluginKind kind) {
    juce::StringArray names;
    if (kind == PluginKind::Instrument)
        for (int k = 0; k < dc::inst::instrumentCount; ++k) names.add(dc::inst::design(k).name);
    return names;
}
}

DungeonCrawlerProcessor::DungeonCrawlerProcessor(PluginKind k)
    : AudioProcessor(busesFor(k)),
      pluginKind(k),
      parameters(*this, nullptr, "DUNGEON_CRAWLER", dc::createParameterLayout([this] { return locationIndex(); }, instrumentNames(k),
                                                                         { [this](float v) { return punchShownDb(v); }, [this](float db) { return punchFromDb(db); } })) {
    location = parameters.getRawParameterValue(dc::ids::location);
    subLocation = parameters.getRawParameterValue(dc::ids::subLocation);
    intensity = parameters.getRawParameterValue(dc::ids::intensity);
    mix = parameters.getRawParameterValue(dc::ids::mix);
    input = parameters.getRawParameterValue(dc::ids::input);
    output = parameters.getRawParameterValue(dc::ids::output);
    bypass = parameters.getRawParameterValue(dc::ids::bypass);
    travelTime = parameters.getRawParameterValue(dc::ids::travelTime);
    travelSync = parameters.getRawParameterValue(dc::ids::travelSync);
    for (std::size_t m = 0; m < macros.size(); ++m) macros[m] = parameters.getRawParameterValue(dc::ids::macros[m]);
    for (std::size_t i = 0; i < advanced.size(); ++i) advanced[i] = parameters.getRawParameterValue(dc::ids::advanced(i));
    journeyOn = parameters.getRawParameterValue(dc::ids::journeyOn);
    journeyMode = parameters.getRawParameterValue(dc::ids::journeyMode);
    journeyBars = parameters.getRawParameterValue(dc::ids::journeyBars);
    roamOn = parameters.getRawParameterValue(dc::ids::roamOn);
    roamBars = parameters.getRawParameterValue(dc::ids::roamBars);
    roamRandomness = parameters.getRawParameterValue(dc::ids::roamRandomness);
    roamSeed = parameters.getRawParameterValue(dc::ids::roamSeed);
    midiTravel = parameters.getRawParameterValue(dc::ids::midiTravel);
    midiOctave = parameters.getRawParameterValue(dc::ids::midiOctave);
    atmosLevel = parameters.getRawParameterValue(dc::ids::atmosLevel);
    atmosDensity = parameters.getRawParameterValue(dc::ids::atmosDensity);
    atmosSeed = parameters.getRawParameterValue(dc::ids::atmosSeed);
    quality = parameters.getRawParameterValue(dc::ids::quality);
    effects = parameters.getRawParameterValue(dc::ids::effects);
    for (std::size_t g = 0; g < mixGroups.size(); ++g) mixGroups[g] = parameters.getRawParameterValue(dc::ids::mixGroups[g]);
    mixLowCut = parameters.getRawParameterValue(dc::ids::mixLowCut);
    mixPreDelay = parameters.getRawParameterValue(dc::ids::mixPreDelay);
    mixDuck = parameters.getRawParameterValue(dc::ids::mixDuck);
    atmosOn = parameters.getRawParameterValue(dc::ids::atmosOn);
    dsAmount = parameters.getRawParameterValue(dc::ids::dungeonSynth);
    dsEra = parameters.getRawParameterValue(dc::ids::dungeonSynthEra);
    if (isInstrument()) {
        instPunch = parameters.getRawParameterValue(dc::ids::instPunch);
        instPunchTime = parameters.getRawParameterValue(dc::ids::instPunchTime);
        instrumentChoice = parameters.getRawParameterValue(dc::ids::instrument);
        instLock = parameters.getRawParameterValue(dc::ids::instLock);
        for (std::size_t m = 0; m < instMacros.size(); ++m) instMacros[m] = parameters.getRawParameterValue(dc::ids::instMacros[m]);
        bendRange = parameters.getRawParameterValue(dc::ids::bendRange);
        instLevel = parameters.getRawParameterValue(dc::ids::instLevel);
        instVowel = parameters.getRawParameterValue(dc::ids::instVowel);
        instEnsemble = parameters.getRawParameterValue(dc::ids::instEnsemble);
    }
    // The developer's recordings load on a background thread (once per process). The FX
    // ships the voice it uses in its places (Session 8); without them it sounds as before.
    wb::samples::Bank::get().load();
    // Session 10: the search index, built once (per plug-in kind and process) at load.
    dc::search::index(isInstrument());
    // The user's files move to ~/Library/Windward Audio/<plug-in>: copied once from the
    // folders older versions used (Source/UserData.h).
    dc::userdata::migrateOnce(isInstrument());
    markPresetBaseline();   // a new instance is "Init", unmodified
}

int DungeonCrawlerProcessor::instrumentIndex() const noexcept {
    if (!isInstrument()) return 0;
    if (instrumentLocked()) {
        const float v = instrumentChoice->load();
        return std::isfinite(v) ? juce::jlimit(0, dc::inst::instrumentCount - 1, static_cast<int>(std::lround(v))) : 0;
    }
    return dc::inst::designFor(currentDestination.load(), currentSubLocation.load());
}

void DungeonCrawlerProcessor::setInstrumentLock(bool on) {
    if (!isInstrument() || on == instrumentLocked()) return;
    pushUndo();
    const juce::ScopedValueSetter<bool> guard(applying, true);
    // Locking keeps what plays now.
    if (on) setPlain(dc::ids::instrument, static_cast<float>(playingInstrument.load()));
    setPlain(dc::ids::instLock, on ? 1.0f : 0.0f);
}

void DungeonCrawlerProcessor::chooseInstrument(int index) {
    if (!isInstrument()) return;
    pushUndo();
    const juce::ScopedValueSetter<bool> guard(applying, true);
    setPlain(dc::ids::instrument, static_cast<float>(juce::jlimit(0, dc::inst::instrumentCount - 1, index)));
    setPlain(dc::ids::instLock, 1.0f);
}

void DungeonCrawlerProcessor::keyboardNote(int key, float velocity) {
    if (key < 0 || key > 127) return;
    postPanel(key | (juce::jlimit(0, 127, juce::roundToInt(velocity * 127.0f)) << 8));
}

void DungeonCrawlerProcessor::panelBend(float bipolar) {
    postPanel((1 << 24) | juce::jlimit(0, 16383, juce::roundToInt((juce::jlimit(-1.0f, 1.0f, bipolar) + 1.0f) * 8191.5f)));
}
void DungeonCrawlerProcessor::panelMod(float unit) { postPanel((2 << 24) | juce::jlimit(0, 127, juce::roundToInt(unit * 127.0f))); }
void DungeonCrawlerProcessor::panelSustain(bool down) { postPanel((3 << 24) | (down ? 1 : 0)); }

bool DungeonCrawlerProcessor::previewNote(int instrument, int key, float velocity) {
    if (!isInstrument() || key < 0 || key > 127) return false;
    const int v = juce::jlimit(0, 127, juce::roundToInt(velocity * 127.0f));
    // A note-off always goes through (a preview let go while the host started).
    if (v > 0 && hostPlaying.load(std::memory_order_relaxed)) return false;
    postPanel((4 << 24) | (juce::jlimit(0, dc::inst::instrumentCount - 1, instrument) << 16) | (v << 8) | key);
    return true;
}

juce::StringArray DungeonCrawlerProcessor::searchRecent() const {
    const juce::SpinLock::ScopedLockType sl(viewLock);
    return recentSearches;
}

void DungeonCrawlerProcessor::setSearchRecent(const juce::StringArray& items) {
    juce::StringArray clean;
    for (const auto& item : items)
        if ((item.startsWith("q:") || item.startsWith("e:")) && item.length() < 120 && !clean.contains(item) && clean.size() < 8) clean.add(item);
    const juce::SpinLock::ScopedLockType sl(viewLock);
    recentSearches = clean;
}

void DungeonCrawlerProcessor::postPanel(int packed) {
    if (!isInstrument()) return;
    const auto scope = keyFifo.write(1);
    if (scope.blockSize1 + scope.blockSize2 < 1) return;   // full: dropped
    keyQueue[static_cast<std::size_t>(scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)] = packed;
}

DungeonCrawlerProcessor::ViewState DungeonCrawlerProcessor::viewState() const {
    const juce::SpinLock::ScopedLockType sl(viewLock);
    return view;
}

void DungeonCrawlerProcessor::updateViewState(const std::function<void(ViewState&)>& change) {
    const juce::SpinLock::ScopedLockType sl(viewLock);
    change(view);
    view.valid = true;
}

float DungeonCrawlerProcessor::effectsAmount() const noexcept {
    const float v = effects->load();
    return std::isfinite(v) ? juce::jlimit(0.0f, dc::effectsMax, v) : 1.0f;
}

float DungeonCrawlerProcessor::dungeonSynthAmount() const noexcept {
    const float v = dsAmount->load();
    return std::isfinite(v) ? juce::jlimit(0.0f, 1.0f, v) : 0.0f;
}

int DungeonCrawlerProcessor::dungeonSynthEra() const noexcept {
    const float v = dsEra->load();
    return std::isfinite(v) && v >= 0.5f ? dc::DungeonSynth::Modern : dc::DungeonSynth::Classic;
}

bool DungeonCrawlerProcessor::journeyRunning() const noexcept {
    return journeyOn->load() >= 0.5f && routeLength.load() > 0;
}

bool DungeonCrawlerProcessor::roamRunning() const noexcept { return roamOn->load() >= 0.5f; }

void DungeonCrawlerProcessor::setPlain(const char* id, float plain) {
    if (auto* p = parameters.getParameter(id)) {
        const float value = p->convertTo0to1(plain);
        if (std::abs(p->getValue() - value) <= 1.0e-7f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost(value);
        p->endChangeGesture();
    }
}

juce::String DungeonCrawlerProcessor::stopSteeringLocked() {
    const bool journey = journeyRunning(), roam = roamRunning();
    if (journeyOn->load() >= 0.5f) setPlain(dc::ids::journeyOn, 0.0f);
    if (roam) setPlain(dc::ids::roamOn, 0.0f);
    midiCancel.store(true);
    return journey && roam ? "Journey and Roam" : (journey ? "Journey" : (roam ? "Roam" : ""));
}

juce::String DungeonCrawlerProcessor::travelTo(int l, int sub) {
    l = juce::jlimit(0, dc::LocationCount - 1, l);
    sub = dc::clampSubLocation(l, sub);
    pushUndo();
    juce::String stopped;
    {
        const juce::ScopedValueSetter<bool> guard(applying, true);   // one undo step
        stopped = stopSteeringLocked();
        setPlain(dc::ids::location, static_cast<float>(l));
        setPlain(dc::ids::subLocation, static_cast<float>(sub));
    }
    // After the parameters: a block that sees this epoch also sees them.
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    return stopped;
}

juce::String DungeonCrawlerProcessor::stopSteering() {
    if (!journeyRunning() && !roamRunning()) return {};
    // Stay where the journey is now.
    const int l = currentDestination.load(), k = currentSubLocation.load();
    return travelTo(l, k);
}

void DungeonCrawlerProcessor::applyInit() {
    pushUndo();
    {
        const juce::ScopedValueSetter<bool> guard(applying, true);
        for (auto* p : getParameters()) {
            // Quality, Bypass and the bend range (the player's controller) are settings.
            if (p == getBypassParameter() || p == parameters.getParameter(dc::ids::quality) || p == parameters.getParameter(dc::ids::bendRange)) continue;
            if (std::abs(p->getValue() - p->getDefaultValue()) > 1.0e-7f) {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->getDefaultValue());
                p->endChangeGesture();
            }
        }
        setRoute({});
        midiCancel.store(true);
    }
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    soundGeneration.store(dc::inst::SoundLatest);
    presetIndex = -1;
    presetName = "Init";
    markPresetBaseline();
}

bool DungeonCrawlerProcessor::presetModified() const {
    if (!hasBaseline) return true;
    const auto now = snapshot();
    if (now.route.size() != presetBaseline.route.size()) return true;
    for (std::size_t k = 0; k < now.route.size(); ++k)
        if (now.route[k].location != presetBaseline.route[k].location || now.route[k].sub != presetBaseline.route[k].sub) return true;
    // Quality and Bypass are settings, not part of a preset.
    const auto& params = getParameters();
    for (int i = 0; i < params.size() && i < static_cast<int>(now.values.size()) && i < static_cast<int>(presetBaseline.values.size()); ++i) {
        auto* p = params[i];
        if (p == getBypassParameter() || p == parameters.getParameter(dc::ids::quality)) continue;
        if (std::abs(now.values[static_cast<std::size_t>(i)] - presetBaseline.values[static_cast<std::size_t>(i)]) > 1.0e-6f) return true;
    }
    return false;
}

int DungeonCrawlerProcessor::qualityMode() const noexcept {
    return juce::jlimit(0, 2, static_cast<int>(std::lround(quality != nullptr ? quality->load() : 1.0f)));
}

bool DungeonCrawlerProcessor::Snapshot::operator==(const Snapshot& o) const {
    if (values != o.values || route.size() != o.route.size()) return false;
    for (std::size_t k = 0; k < route.size(); ++k)
        if (route[k].location != o.route[k].location || route[k].sub != o.route[k].sub) return false;
    return true;
}

DungeonCrawlerProcessor::Snapshot DungeonCrawlerProcessor::snapshot() const {
    Snapshot snap;
    for (auto* p : getParameters()) snap.values.push_back(p->getValue());
    snap.route = route();
    return snap;
}

void DungeonCrawlerProcessor::restore(const Snapshot& snap) {
    const juce::ScopedValueSetter<bool> guard(applying, true);
    const auto& params = getParameters();
    for (int i = 0; i < params.size() && i < static_cast<int>(snap.values.size()); ++i) {
        auto* p = params[i];
        if (p == getBypassParameter() || p == parameters.getParameter(dc::ids::quality)) continue;
        if (std::abs(p->getValue() - snap.values[static_cast<std::size_t>(i)]) > 1.0e-7f) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(snap.values[static_cast<std::size_t>(i)]);
            p->endChangeGesture();
        }
    }
    setRoute(snap.route);
}

void DungeonCrawlerProcessor::pushUndo() {
    if (applying) return;
    auto snap = snapshot();
    if (!undoStack.empty() && undoStack.back() == snap) return;
    undoStack.push_back(std::move(snap));
    if (undoStack.size() > 64) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

bool DungeonCrawlerProcessor::undo() {
    if (undoStack.empty()) return false;
    auto now = snapshot();
    // Skip snapshots identical to the present (an edit that changed nothing).
    while (!undoStack.empty() && undoStack.back() == now) undoStack.pop_back();
    if (undoStack.empty()) return false;
    redoStack.push_back(now);
    restore(undoStack.back());
    undoStack.pop_back();
    return true;
}

bool DungeonCrawlerProcessor::redo() {
    if (redoStack.empty()) return false;
    undoStack.push_back(snapshot());
    restore(redoStack.back());
    redoStack.pop_back();
    return true;
}

void DungeonCrawlerProcessor::selectSlot(int slot) {
    slot = juce::jlimit(0, 1, slot);
    if (slot == abSlot) return;
    slots[static_cast<std::size_t>(abSlot)] = snapshot();
    slotUsed[static_cast<std::size_t>(abSlot)] = true;
    abSlot = slot;
    if (slotUsed[static_cast<std::size_t>(slot)]) { pushUndo(); restore(slots[static_cast<std::size_t>(slot)]); }
    else { slots[static_cast<std::size_t>(slot)] = snapshot(); slotUsed[static_cast<std::size_t>(slot)] = true; }
}

void DungeonCrawlerProcessor::copyCurrentToOtherSlot() {
    const int other = 1 - abSlot;
    slots[static_cast<std::size_t>(other)] = snapshot();
    slotUsed[static_cast<std::size_t>(other)] = true;
}

const std::vector<dc::FactoryPreset>& DungeonCrawlerProcessor::factoryPresetList() const {
    return isInstrument() ? dc::inst::instrumentPresets() : dc::factoryPresets();
}

void DungeonCrawlerProcessor::applyFactoryPreset(int index) {
    const auto& presets = factoryPresetList();
    if (index < 0 || index >= static_cast<int>(presets.size())) return;
    pushUndo();
    const auto& preset = presets[static_cast<std::size_t>(index)];
    const juce::ScopedValueSetter<bool> guard(applying, true);
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p)) {
            if (p == getBypassParameter() || p == parameters.getParameter(dc::ids::quality) || p == parameters.getParameter(dc::ids::bendRange)) continue;
            float value = ranged->getDefaultValue();
            for (const auto& [id, plain] : preset.values)
                if (ranged->getParameterID() == id) value = ranged->convertTo0to1(plain);
            if (std::abs(ranged->getValue() - value) > 1.0e-7f) {
                ranged->beginChangeGesture();
                ranged->setValueNotifyingHost(value);
                ranged->endChangeGesture();
            }
        }
    setRoute(preset.route);
    soundGeneration.store(dc::inst::SoundLatest);
    midiCancel.store(true);
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    presetIndex = index;
    presetName = preset.name;
    markPresetBaseline();
}

juce::File DungeonCrawlerProcessor::userPresetFolder() const {
    // ~/Library/Windward Audio/DUNGEON CRAWLER (FX)/Presets (Source/UserData.h).
    return dc::userdata::presets(isInstrument());
}

bool DungeonCrawlerProcessor::saveUserPreset(const juce::String& name) {
    const auto clean = juce::File::createLegalFileName(name.trim());
    if (clean.isEmpty()) return false;
    const auto folder = userPresetFolder();
    if (!folder.createDirectory()) return false;
    juce::MemoryBlock data;
    getStateInformation(data);
    auto xml = getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
    if (xml == nullptr) return false;
    xml->setAttribute("presetName", name.trim());
    const bool ok = xml->writeTo(folder.getChildFile(clean + ".dcpreset"));
    if (ok) { presetName = name.trim(); presetIndex = -1; markPresetBaseline(); }
    return ok;
}

bool DungeonCrawlerProcessor::loadUserPreset(const juce::File& file) {
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return false;
    pushUndo();
    const int keepQuality = qualityMode();
    const float keepScale = editorScale.load();
    const auto keepView = viewState();
    juce::MemoryBlock data;
    copyXmlToBinary(*xml, data);
    {
        const juce::ScopedValueSetter<bool> guard(applying, true);
        setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    }
    // Presets carry sounds, not the window or the quality setting.
    if (auto* q = parameters.getParameter(dc::ids::quality)) q->setValueNotifyingHost(q->convertTo0to1(static_cast<float>(keepQuality)));
    editorScale.store(keepScale);
    if (keepView.valid) updateViewState([&keepView](ViewState& v) { v = keepView; });
    presetName = xml->getStringAttribute("presetName", file.getFileNameWithoutExtension());
    presetIndex = -1;
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    markPresetBaseline();
    return true;
}

void DungeonCrawlerProcessor::setRoute(const std::vector<dc::Stop>& stops) {
    const int n = juce::jmin(static_cast<int>(stops.size()), dc::maxStops);
    routeLength.store(0);
    for (int k = 0; k < n; ++k) {
        const auto& st = stops[static_cast<std::size_t>(k)];
        const int l = juce::jlimit(0, dc::LocationCount - 1, st.location);
        routeStops[static_cast<std::size_t>(k)].store(l * 8 + dc::clampSubLocation(l, st.sub));
    }
    routeLength.store(n);
}

std::vector<dc::Stop> DungeonCrawlerProcessor::route() const {
    std::vector<dc::Stop> out;
    const int n = routeLength.load();
    for (int k = 0; k < n; ++k) {
        const int code = routeStops[static_cast<std::size_t>(k)].load();
        out.push_back({ code / 8, code % 8 });
    }
    return out;
}

int DungeonCrawlerProcessor::locationIndex() const noexcept {
    const float v = location != nullptr ? location->load() : 0.0f;
    return std::isfinite(v) ? juce::jlimit(0, dc::LocationCount - 1, static_cast<int>(std::lround(v))) : 0;
}

void DungeonCrawlerProcessor::prepareToPlay(double sampleRate, int) {
    rate = std::isfinite(sampleRate) ? juce::jlimit(8000.0, 384000.0, sampleRate) : 48000.0;
    world.prepare(rate);
    atmosphere.prepare(rate);
    // The first notes should already be sampled (and the FX's places already have the
    // voice): wait briefly for the recordings.
    wb::samples::Bank::get().waitUntilLoaded(3000);
    if (isInstrument()) synth.prepare(rate);
    limiter.prepare(rate);
    latency = world.latencySamples();
    for (auto& d : dryDelay) d.prepare(static_cast<std::size_t>(latency + 8));
    for (auto& d : rawDelay) d.prepare(static_cast<std::size_t>(latency + 8));
    // Session 15/16: DUNGEON SYNTH (on the whole output, after the place).
    dungeon.setControls(dungeonSynthAmount(), dungeonSynthEra(), dc::inst::dungeonSynthChorusFx, 0.0f);
    dungeon.prepare(rate);
    for (auto* s : { &inputGain, &outputGain, &wetMix }) s->setTime(0.02, rate);
    placeGain.setTime(0.3, rate);
    placeGain.snap(1.0f);
    inputGain.snap(dc::dsp::dbToGain(input->load()));
    outputGain.snap(dc::dsp::dbToGain(output->load()));
    wetMix.snap(mix->load());
    bypassRamp.prepare(0.025, rate);
    bypassRamp.value = bypass->load() >= 0.5f ? 1.0f : 0.0f;
    neutralRamp.prepare(0.03, rate);
    neutralRamp.value = intensity->load() * effectsAmount() <= neutralIntensity ? 1.0f : 0.0f;
    instGain.setTime(0.03, rate);
    instGain.snap(instLevel != nullptr ? dc::dsp::dbToGain(juce::jlimit(-24.0f, 12.0f, instLevel->load())) : 1.0f);
    setLatencySamples(latency);
    inputLevel = 0;
    outputLevel = 0;
}

bool DungeonCrawlerProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    // mono->mono, stereo->stereo and mono->stereo (Logic offers it on mono tracks).
    const auto in = layout.getMainInputChannelSet(), out = layout.getMainOutputChannelSet();
    const auto monoOrStereo = [](const juce::AudioChannelSet& set) {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };
    if (isInstrument()) return layout.inputBuses.isEmpty() && monoOrStereo(out);
    return monoOrStereo(in) && monoOrStereo(out) && in.size() <= out.size();
}

void DungeonCrawlerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    // A manual choice of place cancels the place MIDI chose.
    if (midiCancel.exchange(false)) midiLocation = -1;
    // The instrument plays its MIDI (no MIDI travel: the notes are the music).
    if (isInstrument()) {
        midiLocation = -1;
        process(buffer, false, &midi);
        return;
    }
    // MIDI travel: the last note-on in the travel octave chooses the place.
    if (midiTravel->load() >= 0.5f) {
        const int base = juce::jlimit(0, 8, static_cast<int>(std::lround(midiOctave->load()))) * 12;
        for (const auto meta : midi) {
            const auto message = meta.getMessage();
            if (!message.isNoteOn()) continue;
            dc::Stop chosen;
            if (dc::midiPlace(message.getNoteNumber(), base, midiLocation >= 0 ? midiLocation : locationIndex(), chosen)) {
                midiLocation = chosen.location;
                midiSub = chosen.sub;
            }
        }
    } else midiLocation = -1;
    process(buffer, false, nullptr);
}
// Bypassed, the instrument still plays (dry, outside its place).
void DungeonCrawlerProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    process(buffer, true, isInstrument() ? &midi : nullptr);
}

void DungeonCrawlerProcessor::handleMidi(const juce::MidiMessage& m) noexcept {
    if (m.isNoteOn()) synth.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) synth.noteOff(m.getNoteNumber());
    else if (m.isSustainPedalOn()) synth.setSustain(true);
    else if (m.isSustainPedalOff()) synth.setSustain(false);
    else if (m.isPitchWheel()) synth.setPitchBend(static_cast<float>(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isController() && m.getControllerNumber() == 1) synth.setModWheel(static_cast<float>(m.getControllerValue()) / 127.0f);
    else if (m.isAllNotesOff()) synth.allNotesOff();
    else if (m.isAllSoundOff()) synth.allSoundOff();
    else if (m.isResetAllControllers()) {
        synth.setPitchBend(0);
        synth.setModWheel(0);
        synth.setSustain(false);
    }
}

void DungeonCrawlerProcessor::renderInstrument(juce::AudioBuffer<float>& buffer, int outputs, juce::MidiBuffer* midi) {
    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, numSamples);
    // The panel's keyboard first (at the block's start).
    {
        const auto scope = keyFifo.read(keyFifo.getNumReady());
        const auto play = [this](int packed) {
            // The panel: a key (kind 0), the bend and mod wheels and the pedal (kinds 1-3).
            switch (packed >> 24) {
                case 1: synth.setPitchBend(static_cast<float>((packed & 0x3fff) - 8192) / 8192.0f); return;
                case 2: synth.setModWheel(static_cast<float>(packed & 0x7f) / 127.0f); return;
                case 3: synth.setSustain((packed & 1) != 0); return;
                case 4: {
                    // The search's preview: the note starts on its own instrument (it keeps
                    // it while it rings); the next notes play the current one again.
                    const int key = packed & 0xff, velocity = (packed >> 8) & 0x7f, design = (packed >> 16) & 0xff;
                    if (velocity > 0) {
                        const int now = synth.currentDesign();
                        synth.setDesign(design);
                        synth.noteOn(key, static_cast<float>(velocity) / 127.0f);
                        synth.setDesign(now);
                    } else synth.noteOff(key);
                    return;
                }
                default: break;
            }
            const int key = packed & 0xff, velocity = (packed >> 8) & 0x7f;
            if (velocity > 0) synth.noteOn(key, static_cast<float>(velocity) / 127.0f);
            else synth.noteOff(key);
        };
        for (int k = 0; k < scope.blockSize1; ++k) play(keyQueue[static_cast<std::size_t>(scope.startIndex1 + k)]);
        for (int k = 0; k < scope.blockSize2; ++k) play(keyQueue[static_cast<std::size_t>(scope.startIndex2 + k)]);
    }
    auto* l = buffer.getWritePointer(0);
    // A mono output: render both sides into the one channel (halved later by the mix).
    std::array<float, dc::maxChunk> scratch {};
    float* r = outputs > 1 ? buffer.getWritePointer(1) : nullptr;
    int at = 0;
    const auto renderTo = [&](int end) {
        while (at < end) {
            const int n = juce::jmin(end - at, dc::maxChunk);
            if (r != nullptr) synth.render(l + at, r + at, n);
            else {
                std::fill(scratch.begin(), scratch.begin() + n, 0.0f);
                synth.render(l + at, scratch.data(), n);
                for (int i = 0; i < n; ++i) l[at + i] = 0.5f * (l[at + i] + scratch[static_cast<std::size_t>(i)]);
            }
            at += n;
        }
    };
    if (midi != nullptr)
        for (const auto meta : *midi) {
            renderTo(juce::jlimit(0, numSamples, meta.samplePosition));
            handleMidi(meta.getMessage());
        }
    renderTo(numSamples);
    if (midi != nullptr) midi->clear();
    const auto& perf = synth.performance();
    shownBend.store(perf.bend, std::memory_order_relaxed);
    shownMod.store(perf.mod, std::memory_order_relaxed);
    shownSustain.store(perf.sustain, std::memory_order_relaxed);
}

void DungeonCrawlerProcessor::process(juce::AudioBuffer<float>& buffer, bool forcedBypass, juce::MidiBuffer* midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto begin = juce::Time::getHighResolutionTicks();
    // Read before the parameters (they are written before the epoch moves on).
    const int epoch = manualEpoch.load(std::memory_order_acquire);
    double bpm = 120, beatsPerBar = 4;
    std::optional<double> hostBeats;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto tempo = pos->getBpm())
                if (std::isfinite(*tempo)) bpm = juce::jlimit(20.0, 400.0, *tempo);
            if (auto sig = pos->getTimeSignature())
                if (sig->numerator > 0 && sig->denominator > 0) beatsPerBar = juce::jlimit(1.0, 32.0, sig->numerator * 4.0 / sig->denominator);
            hostPlaying.store(pos->getIsPlaying(), std::memory_order_relaxed);
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    if (std::isfinite(*ppq)) hostBeats = *ppq;
        }

    // Inputs and outputs are counted separately: with mono->stereo the right output
    // carries no input and must still be written.
    const int numSamples = buffer.getNumSamples();
    const int bufferChannels = buffer.getNumChannels();
    const int outputs = juce::jmin(bufferChannels, getTotalNumOutputChannels());
    // The instrument's own sound is the place's input.
    const int inputs = isInstrument() ? outputs : juce::jmin(bufferChannels, getTotalNumInputChannels());
    for (int ch = inputs; ch < bufferChannels; ++ch) buffer.clear(ch, 0, numSamples);
    if (inputs < 1 || outputs < 1 || numSamples <= 0) return;
    auto* left = buffer.getWritePointer(0);
    auto* right = outputs > 1 ? buffer.getWritePointer(1) : nullptr;
    const bool stereoInput = inputs > 1;

    const auto finiteOr = [](float v, float fallback) { return std::isfinite(v) ? v : fallback; };
    // The Effects roller (Session 9) scales Intensity: 0 nearly dry, 1 as designed, up to
    // 1.5 stronger (Engine::strengthen). The engine's trims and level matcher keep the level.
    const float amount = juce::jlimit(0.0f, 1.0f, finiteOr(intensity->load(), 1)) * effectsAmount();
    controls.location = locationIndex();
    controls.sub = juce::jlimit(0, dc::maxSubLocations, static_cast<int>(std::lround(finiteOr(subLocation->load(), 0))));
    // Journeys, roaming and MIDI travel steer the place (Milestone 9). The position is
    // the host's when it plays (deterministic: the same bar, the same place), otherwise
    // a free-running count at the host's tempo.
    const double beats = hostBeats ? *hostBeats : freeBeats;
    freeBeats = hostBeats ? *hostBeats : freeBeats + static_cast<double>(buffer.getNumSamples()) / rate * bpm / 60.0;
    bool steering = false;
    if (roamOn->load() >= 0.5f) {
        const auto rateIndex = static_cast<std::size_t>(juce::jlimit(0, static_cast<int>(dc::roamBarChoices.size()) - 1, static_cast<int>(std::lround(roamBars->load()))));
        const double beatsPerStep = dc::roamBarChoices[rateIndex] * beatsPerBar;
        const auto step = static_cast<long long>(std::floor(std::max(0.0, beats) / beatsPerStep));
        controls.sub = dc::roamPlace(controls.location, step, juce::jlimit(0.0f, 1.0f, finiteOr(roamRandomness->load(), 0.5f)),
                                     juce::jlimit(1, 999, static_cast<int>(std::lround(roamSeed->load()))));
        steering = true;
    }
    const int stops = routeLength.load(std::memory_order_acquire);
    if (journeyOn->load() >= 0.5f && stops > 0) {
        const int index = dc::journeyStop(beats, stops, juce::jlimit(1.0f, 16.0f, journeyBars->load()) * beatsPerBar,
                                          juce::jlimit(0, 2, static_cast<int>(std::lround(journeyMode->load()))));
        const int code = routeStops[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
        controls.location = code / 8;
        controls.sub = code % 8;
        steering = true;
    }
    if (midiTravel->load() >= 0.5f && midiLocation >= 0) {
        controls.location = midiLocation;
        controls.sub = midiSub;
        steering = true;
    }
    steered.store(steering, std::memory_order_relaxed);
    controls.intensity = amount;
    for (std::size_t m = 0; m < macros.size(); ++m) controls.macros[m] = juce::jlimit(0.0f, 1.0f, finiteOr(macros[m]->load(), 0.5f));
    for (std::size_t i = 0; i < advanced.size(); ++i) controls.advanced[i] = finiteOr(advanced[i]->load(), 0);
    const auto sync = static_cast<std::size_t>(juce::jlimit(0, static_cast<int>(dc::travelSyncBeats.size()) - 1,
                                                            static_cast<int>(std::lround(finiteOr(travelSync->load(), 0)))));
    controls.travelSeconds = sync == 0 ? static_cast<double>(juce::jlimit(0.0f, 10.0f, finiteOr(travelTime->load(), 1.5f)))
                                       : dc::dsp::beatsToSeconds(dc::travelSyncBeats[sync], bpm);
    controls.bpm = bpm;
    // Session 11: the MIX section (100 % = as designed) and the reverb helpers.
    for (std::size_t g = 0; g < controls.mix.size(); ++g) controls.mix[g] = juce::jlimit(0.0f, dc::mixMax, finiteOr(mixGroups[g]->load(), 1.0f));
    // The user's MIX Reverb, before the instrument's fit below (DUNGEON SYNTH's hall follows it).
    const float userReverb = controls.mix[dc::MixReverb];
    controls.reverbLowCut = juce::jlimit(dc::mixLowCutOff, 600.0f, finiteOr(mixLowCut->load(), dc::mixLowCutOff));
    controls.reverbPreDelay = juce::jlimit(-40.0f, 120.0f, finiteOr(mixPreDelay->load(), 0.0f));
    controls.reverbDuck = juce::jlimit(0.0f, 1.0f, finiteOr(mixDuck->load(), 0.0f));
    // The place's instrument (as the journey, roam or the parameters choose it), or the
    // locked one; notes already sounding keep theirs.
    int index = 0;
    if (isInstrument()) {
        index = dc::inst::designFor(controls.location, controls.sub);
        if (instrumentLocked()) {
            const float v = finiteOr(instrumentChoice->load(), 0);
            index = juce::jlimit(0, dc::inst::instrumentCount - 1, static_cast<int>(std::lround(v)));
        }
        // How the instrument sits in a song (brass, strings, drums, plucks: less of the
        // place's reverb and echoes, the dry hit first); the MIX sliders scale on top.
        const int sound = soundGeneration.load(std::memory_order_relaxed);
        synth.setSound(sound);
        const auto fit = dc::inst::mixFit(index, sound);
        // 100 % is the fitted amount; above it the slider climbs to the place's full design
        // and beyond (150 % = 1.5 times the design, as for every other instrument).
        const auto fitted = [](float user, float f) { return user <= 1.0f ? user * f : f + (user - 1.0f) * 2.0f * (1.5f - f); };
        controls.mix[dc::MixReverb] = fitted(controls.mix[dc::MixReverb], fit.reverb);
        controls.mix[dc::MixEchoes] = fitted(controls.mix[dc::MixEchoes], fit.echoes);
        controls.reverbPreDelay += fit.preDelayMs;
        // Session 12: the brass's reverb dips under its own hits, with the user's Duck.
        if (fit.duck > 0.0f) controls.reverbDuck = 1.0f - (1.0f - controls.reverbDuck) * (1.0f - fit.duck);
        placeMakeupDb = fit.makeupDb;
    }
    world.setQuality(qualityMode());
    world.setControls(controls);
    if (isInstrument()) {
        synth.setDesign(index);
        synth.setPunch(juce::jlimit(0.0f, 2.0f, finiteOr(instPunch->load(), 1.0f)), juce::jlimit(20.0f, 500.0f, finiteOr(instPunchTime->load(), 120.0f)) * 0.001f);
        playingInstrument.store(index, std::memory_order_relaxed);
        std::array<float, dc::inst::macroCount> m {};
        for (std::size_t k = 0; k < m.size(); ++k) m[k] = juce::jlimit(0.0f, 1.0f, finiteOr(instMacros[k]->load(), 0.5f));
        synth.setMacros(m);
        synth.setBendRange(static_cast<float>(juce::jlimit(1, 24, static_cast<int>(std::lround(finiteOr(bendRange->load(), 2))))));
        synth.setTempo(bpm);
        synth.setVoiceControls(juce::jlimit(0.0f, 1.0f, finiteOr(instVowel->load(), 0.5f)), juce::jlimit(0.0f, 1.0f, finiteOr(instEnsemble->load(), 0.5f)));
        instGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 12.0f, finiteOr(instLevel->load(), 0.0f))));
        // The place's correction (its build-up on sustained and ringing notes) follows the
        // Effects roller: towards 0 % the place adds nothing, so the instrument meets its dry
        // self without a level step; above 100 % the stronger place builds up more and the
        // correction grows with it. At 100 %: exactly as designed.
        // The correction is mostly the reverbs' build-up: it follows their share too (the
        // instrument's fit and the MIX section's Reverb; exactly as before at 100 %).
        placeGain.set(dc::dsp::dbToGain(dc::inst::placeTrimDb(controls.location, controls.sub) * effectsAmount()
                                        * juce::jmin(1.25f, controls.mix[dc::MixReverb])
                                        + placeMakeupDb * effectsAmount()));
        renderInstrument(buffer, outputs, midi);
        // Level (Session 9): the instrument into its place, smoothed (no zipper, no click).
        auto* il = buffer.getWritePointer(0);
        auto* ir = outputs > 1 ? buffer.getWritePointer(1) : nullptr;
        if (instGain.value != 1.0f || instGain.target != 1.0f)
            for (int i = 0; i < numSamples; ++i) {
                const float g = instGain.next();
                il[i] *= g;
                if (ir != nullptr) ir[i] *= g;
            }
    }
    inputGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 24.0f, finiteOr(input->load(), 0))));
    outputGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 12.0f, finiteOr(output->load(), 0))));
    wetMix.set(juce::jlimit(0.0f, 1.0f, finiteOr(mix->load(), 1)));
    const float bypassTarget = forcedBypass || bypass->load() >= 0.5f ? 1.0f : 0.0f;
    const float neutralTarget = amount <= neutralIntensity ? 1.0f : 0.0f;

    // The Atmosphere: its level, its switch and the MIX section's Atmosphere (0-150 %).
    const float atmosAmount = atmosOn->load() < 0.5f ? 0.0f
                                                     : juce::jlimit(0.0f, 1.0f, finiteOr(atmosLevel->load(), 0)) * juce::jlimit(0.0f, dc::mixMax, finiteOr(mixGroups[4]->load(), 1.0f));
    const float atmosDense = juce::jlimit(0.0f, 1.0f, finiteOr(atmosDensity->load(), 0.35f));
    const int atmosSeedValue = juce::jlimit(1, 999, static_cast<int>(std::lround(finiteOr(atmosSeed->load(), 1))));
    const double beatsPerSample = bpm / 60.0 / rate;
    // Session 16: DUNGEON SYNTH works on the whole output (after the place, the Mix and the
    // Output gain, before the safety limiter), as a cassette records the whole sound. In Session 15
    // it was before the place, whose own reverb, air and ambience then covered it (in its place
    // the Dark Choir Pad at Classic 100 % came out brighter than at 0 %). Its hall follows the
    // place's wet path: Intensity x Effects (towards 0 %: the dry path, without it), the MIX
    // section's Reverb and Mix; its own matcher keeps the output at the input's loudness. The
    // hall is the genre's own: it follows the user's MIX Reverb, not the instrument's fit (which
    // keeps strings, brass and plucks drier in a song) that the place's reverbs follow.
    {
        const float hallSend = (amount <= neutralIntensity ? 0.0f : juce::jmin(1.25f, amount)) * juce::jlimit(0.0f, dc::mixMax, userReverb)
                             * juce::jlimit(0.0f, 1.0f, finiteOr(mix->load(), 1.0f));
        const float chorus = isInstrument() ? dc::inst::dungeonSynthChorus(index) : dc::inst::dungeonSynthChorusFx;
        dungeon.setControls(dungeonSynthAmount(), dungeonSynthEra(), chorus, hallSend);
        // The place the music is in (as it is now, travelling included): the hall takes on it.
        const auto& f = world.frame();
        dungeon.setPlace(f[dc::Decay], f[dc::Cutoff], juce::jlimit(0.0f, 1.0f, (f[dc::ReverbMix] + f[dc::VaultMix]) * amount));
    }
    const bool staging = dungeonEnabled && dungeon.active();
    float inPeak = 0, outPeak = 0;
    for (int start = 0; start < numSamples; start += dc::maxChunk) {
        const int n = juce::jmin(dc::maxChunk, numSamples - start);
        float* l = work[0].data();
        float* r = work[1].data();
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const float rawL = left[start + i], rawR = stereoInput ? right[start + i] : rawL;
            const float x0 = std::isfinite(rawL) ? juce::jlimit(-32.0f, 32.0f, rawL) : 0.0f;
            const float x1 = std::isfinite(rawR) ? juce::jlimit(-32.0f, 32.0f, rawR) : 0.0f;
            inPeak = juce::jmax(inPeak, std::abs(x0), std::abs(x1));
            // Bypass always crossfades to the untouched input.
            rawDelay[0].push(x0);
            rawDelay[1].push(x1);
            rawAligned[0][k] = rawDelay[0].readInteger(static_cast<std::size_t>(latency) + 1);
            rawAligned[1][k] = rawDelay[1].readInteger(static_cast<std::size_t>(latency) + 1);
            dryDelay[0].push(x0);
            dryDelay[1].push(x1);
            dryAligned[0][k] = dryDelay[0].readInteger(static_cast<std::size_t>(latency) + 1);
            dryAligned[1][k] = dryDelay[1].readInteger(static_cast<std::size_t>(latency) + 1);
            const float g = inputGain.next();
            l[i] = x0 * g;
            r[i] = x1 * g;
        }
        // DUNGEON SYNTH's hiss goes in before the place (the place shapes it as it shapes the music).
        if (staging) dungeon.processHiss(l, r, n);
        world.process(l, r, n, dryAligned[0].data(), dryAligned[1].data());
        if (isInstrument()) {
            // The instrument in its place: a measured correction per place (some spaces
            // build up on sustained or ringing notes more than on the effect's material).
            for (int i = 0; i < n; ++i) {
                const float g = placeGain.next();
                l[i] *= g;
                r[i] *= g;
            }
        }
        // The Atmosphere joins the wet signal (Mix, Intensity 0 and Bypass remove it too).
        if (atmosAmount > 0.0f || atmosphere.activeVoices() > 0)
            atmosphere.process(l, r, n, beats + beatsPerSample * start, beatsPerSample, world.destination(), world.destinationSub(),
                               atmosAmount, atmosDense, atmosSeedValue, dryAligned[0].data(), dryAligned[1].data());
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const float m = wetMix.next(), og = outputGain.next();
            bypassedAt[k] = bypassRamp.next(bypassTarget);
            const float dry = neutralRamp.next(neutralTarget);
            const float dl = dryAligned[0][k], dr = dryAligned[1][k];
            float ol = (dl + (l[i] - dl) * m) * og, orr = (dr + (r[i] - dr) * m) * og;
            ol += dry * (dl * og - ol);
            orr += dry * (dr * og - orr);
            post[0][k] = ol;
            post[1][k] = orr;
        }
        if (staging) dungeon.process(post[0].data(), post[1].data(), n);
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            float ol = post[0][k], orr = post[1][k];
            limiter.process(ol, orr);
            const float bypassed = bypassedAt[k];
            const float bl = rawAligned[0][k], br = rawAligned[1][k];
            ol += bypassed * (bl - ol);
            orr += bypassed * (br - orr);
            if (!std::isfinite(ol)) ol = 0;
            if (!std::isfinite(orr)) orr = 0;
            if (right != nullptr) { left[start + i] = ol; right[start + i] = orr; }
            else left[start + i] = 0.5f * (ol + orr);
            outPeak = juce::jmax(outPeak, std::abs(ol), std::abs(orr));
        }
    }
    inputLevel.store(juce::jmax(inPeak, inputLevel.load() * 0.85f), std::memory_order_relaxed);
    outputLevel.store(juce::jmax(outPeak, outputLevel.load() * 0.85f), std::memory_order_relaxed);
    travelProgress.store(world.travelProgress(), std::memory_order_relaxed);
    currentDestination.store(world.destination(), std::memory_order_relaxed);
    currentSubLocation.store(world.destinationSub(), std::memory_order_relaxed);
    processedEpoch.store(epoch, std::memory_order_release);
    const auto seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin);
    cpuLoad.store(0.9f * cpuLoad.load() + 0.1f * static_cast<float>(seconds * rate / numSamples), std::memory_order_relaxed);
}

juce::AudioProcessorParameter* DungeonCrawlerProcessor::getBypassParameter() const { return parameters.getParameter(dc::ids::bypass); }
float DungeonCrawlerProcessor::punchShownDb(float control) const noexcept {
    return dc::inst::punchDb(isInstrument() && instrumentChoice != nullptr ? instrumentIndex() : 0, control, soundGeneration.load(std::memory_order_relaxed));
}

float DungeonCrawlerProcessor::punchFromDb(float db) const noexcept {
    const int index = isInstrument() && instrumentChoice != nullptr ? instrumentIndex() : 0, sound = soundGeneration.load(std::memory_order_relaxed);
    const float own = dc::inst::designPunchDb(index, sound), most = dc::inst::punchMaxDb(index, sound);
    db = juce::jlimit(0.0f, most, std::isfinite(db) ? db : 0.0f);
    if (own < 1.0e-4f) return db > 0.0f ? 1.0f + db / most : 0.0f;
    if (db <= own) return db / own;
    return 1.0f + (db - own) / juce::jmax(1.0e-4f, most - own);
}

void DungeonCrawlerProcessor::seatSession(int from, int to) {
    const int index = instrumentIndex();
    if (auto* level = parameters.getParameter(dc::ids::instLevel)) {
        const float db = level->convertFrom0to1(level->getValue()), offset = dc::inst::seatLevelDb(index, to) - dc::inst::seatLevelDb(index, from);
        if (std::abs(db) > 1.0e-4f && offset != 0.0f) level->setValueNotifyingHost(level->convertTo0to1(juce::jlimit(-24.0f, 12.0f, db - offset)));
    }
    if (auto* punch = parameters.getParameter(dc::ids::instPunch)) {
        const float control = punch->convertFrom0to1(punch->getValue());
        if (std::abs(control - 1.0f) > 1.0e-4f && dc::inst::designPunchDb(index, from) != dc::inst::designPunchDb(index, to)) {
            const float db = dc::inst::punchDb(index, control, from);
            const float own = dc::inst::designPunchDb(index, to), most = dc::inst::punchMaxDb(index, to);
            const float kept = own < 1.0e-4f ? (db > 0.0f ? 1.0f + db / most : 0.0f) : db <= own ? db / own : 1.0f + (db - own) / juce::jmax(1.0e-4f, most - own);
            punch->setValueNotifyingHost(punch->convertTo0to1(juce::jlimit(0.0f, 2.0f, kept)));
        }
    }
}

juce::AudioProcessorEditor* DungeonCrawlerProcessor::createEditor() { return new DungeonCrawlerEditor(*this); }

void DungeonCrawlerProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto state = parameters.copyState();
    state.setProperty("version", 4, nullptr);
    state.setProperty("editorScale", static_cast<double>(editorScale.load()), nullptr);
    state.setProperty("presetName", presetName, nullptr);
    state.setProperty("presetModified", presetModified(), nullptr);
    // Session 11: which instrument sound (10: as before Session 11; 11: Session 11's
    // defaults; 12: Session 12's horns).
    state.setProperty("sound", soundGeneration.load(), nullptr);
    juce::StringArray stops;
    for (const auto& st : route()) stops.add(juce::String(st.location) + ":" + juce::String(st.sub));
    state.setProperty("route", stops.joinIntoString(","), nullptr);
    // How many recorded notes the instrument has loaded (information only: the AU harness
    // reads it to see that the samples ship in the bundle).
    if (isInstrument()) state.setProperty("samples", wb::samples::Bank::get().zoneCount(), nullptr);
    // The window as it was left (Session 9, from LIGHT SEEKER): a child older versions ignore.
    if (const auto v = viewState(); v.valid) {
        juce::ValueTree w("EditorView");
        w.setProperty("place", v.place, nullptr);
        w.setProperty("location", v.location, nullptr);
        w.setProperty("sub", v.sub, nullptr);
        w.setProperty("controls", v.controls, nullptr);
        w.setProperty("advanced", v.advanced, nullptr);
        w.setProperty("journey", v.journey, nullptr);
        w.setProperty("mix", v.mix, nullptr);
        w.setProperty("advancedScroll", v.advancedScroll, nullptr);
        w.setProperty("journeyScroll", v.journeyScroll, nullptr);
        state.appendChild(w, nullptr);
    }
    // Session 10: the search's Recent list (older versions ignore it).
    if (const auto recent = searchRecent(); !recent.isEmpty()) {
        juce::ValueTree r("Search");
        r.setProperty("recent", recent.joinIntoString("\n"), nullptr);
        state.appendChild(r, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, data);
}

void DungeonCrawlerProcessor::setStateInformation(const void* data, int size) {
    // Malformed or foreign data is ignored; values are clamped; missing parameters keep
    // their defaults (a state from an older version stays loadable).
    if (data == nullptr || size <= 0 || size > 1024 * 1024) return;
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;
    const auto incoming = juce::ValueTree::fromXml(*xml);
    std::vector<std::pair<juce::RangedAudioParameter*, float>> values;
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
            values.push_back({ ranged, ranged->convertFrom0to1(ranged->getDefaultValue()) });
    for (const auto& child : incoming) {
        const auto id = child.getProperty("id").toString();
        for (auto& [parameter, value] : values)
            if (parameter->getParameterID() == id) {
                const float v = static_cast<float>(child.getProperty("value", value));
                if (!std::isfinite(v)) return;
                value = parameter->getNormalisableRange().snapToLegalValue(v);
            }
    }
    for (auto& [parameter, value] : values) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    // The window's scale and the preset name (version 4).
    if (incoming.hasProperty("editorScale")) {
        const auto scale = static_cast<float>(static_cast<double>(incoming.getProperty("editorScale")));
        if (std::isfinite(scale)) editorScale.store(juce::jlimit(0.5f, 2.0f, scale));
    }
    if (incoming.hasProperty("presetName")) presetName = incoming.getProperty("presetName").toString();
    // Saved before Session 11 (no "sound"): the instrument as it sounded then; saved in
    // Session 11 ("sound" 11): its brass and Punch, unchanged. The FX has nothing to keep.
    if (isInstrument()) {
        const int saved = static_cast<int>(incoming.getProperty("sound", int(dc::inst::SoundClassic)));
        setSoundGeneration(saved);
        // Session 14: a session saved with sound 13 (the recorded brass) opens with 14 (the
        // brass and the War Drums seated). Where its Level and Punch were left at their
        // defaults it gets the new defaults (lower); where they were set, they are kept as
        // they were heard (the Level raised by what the new default takes away, the Punch at
        // the same dB on the hit).
        if (saved == dc::inst::SoundRealBrass) {
            setSoundGeneration(dc::inst::SoundSeated);
            seatSession(dc::inst::SoundRealBrass, dc::inst::SoundSeated);
        }
    } else {
        soundGeneration.store(dc::inst::SoundLatest);
    }
    // The window as it was left (Session 9), when the state has it; a preset never has it.
    if (const auto w = incoming.getChildWithName("EditorView"); w.isValid()) {
        updateViewState([&w](ViewState& v) {
            v.place = static_cast<bool>(w.getProperty("place", false));
            v.location = juce::jlimit(0, dc::LocationCount - 1, static_cast<int>(w.getProperty("location", 0)));
            v.sub = juce::jlimit(0, dc::maxSubLocations, static_cast<int>(w.getProperty("sub", 0)));
            v.controls = static_cast<bool>(w.getProperty("controls", true));
            v.advanced = static_cast<bool>(w.getProperty("advanced", false));
            v.journey = static_cast<bool>(w.getProperty("journey", false)) && !v.advanced;
            v.mix = static_cast<bool>(w.getProperty("mix", false)) && !v.advanced && !v.journey;
            v.advancedScroll = juce::jlimit(0, 100000, static_cast<int>(w.getProperty("advancedScroll", 0)));
            v.journeyScroll = juce::jlimit(0, 100000, static_cast<int>(w.getProperty("journeyScroll", 0)));
        });
    }
    if (const auto r = incoming.getChildWithName("Search"); r.isValid())
        setSearchRecent(juce::StringArray::fromLines(r.getProperty("recent").toString()));
    // The journey route (version 3); older states have none.
    std::vector<dc::Stop> stops;
    for (const auto& token : juce::StringArray::fromTokens(incoming.getProperty("route").toString(), ",", ""))
        if (token.containsChar(':') && static_cast<int>(stops.size()) < dc::maxStops)
            stops.push_back({ juce::jlimit(0, dc::LocationCount - 1, token.upToFirstOccurrenceOf(":", false, false).getIntValue()),
                              juce::jlimit(0, dc::maxSubLocations, token.fromFirstOccurrenceOf(":", false, false).getIntValue()) });
    setRoute(stops);
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    // Unmodified when saved: what was loaded is the preset; otherwise it stays modified.
    if (!applying) {
        if (static_cast<bool>(incoming.getProperty("presetModified", false))) hasBaseline = false;
        else markPresetBaseline();
    }
}

// One source, two plug-ins: the build's own identity says which (CMakeLists.txt).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DungeonCrawlerProcessor(JucePlugin_IsSynth ? PluginKind::Instrument : PluginKind::Effect);
}
