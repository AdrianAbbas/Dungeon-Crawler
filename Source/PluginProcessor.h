#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Parameters.h"
#include "World/Journey.h"
#include "World/Presets.h"
#include "DSP/Atmosphere.h"
#include "Instrument/Synth.h"
#include "DSP/Warmth.h"

// Direction Update 3: one processor, two plug-ins. DUNGEON CRAWLER (the instrument: no
// input, MIDI plays the place's instrument, which then sounds in the place) and DUNGEON
// CRAWLER FX (the effect, exactly as before).
enum class PluginKind { Effect, Instrument };

class DungeonCrawlerProcessor final : public juce::AudioProcessor {
public:
    explicit DungeonCrawlerProcessor(PluginKind kind = PluginKind::Effect);
    PluginKind kind() const noexcept { return pluginKind; }
    bool isInstrument() const noexcept { return pluginKind == PluginKind::Instrument; }
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    // Hosts call reset() on transport stops and locates; clearing the reverb then would
    // cut every tail when playback stops. Only prepareToPlay starts afresh.
    void reset() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return isInstrument() ? "DUNGEON CRAWLER" : "DUNGEON CRAWLER FX"; }
    // The instrument plays MIDI; the effect's MIDI travel (Milestone 9): notes choose
    // places where the host sends MIDI to effects.
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }
    // One program whose setCurrentProgram does nothing: some hosts call it on load and
    // that must never change the user's settings. Factory journeys come in Milestone 5.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current Place"; }
    void changeProgramName(int, const juce::String&) override {}
    juce::AudioProcessorParameter* getBypassParameter() const override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

private:
    static BusesProperties busesFor(PluginKind);
    PluginKind pluginKind;
    // Raw parameter values (declared before `parameters`: the Sub-location display
    // function reads `location`).
    std::atomic<float>* location = nullptr;
    std::atomic<float>* subLocation = nullptr;
    std::atomic<float>* intensity = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* input = nullptr;
    std::atomic<float>* output = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* travelTime = nullptr;
    std::atomic<float>* travelSync = nullptr;
    std::array<std::atomic<float>*, dc::macroCount> macros {};
    std::array<std::atomic<float>*, dc::Count> advanced {};
    std::atomic<float>* journeyOn = nullptr;
    std::atomic<float>* journeyMode = nullptr;
    std::atomic<float>* journeyBars = nullptr;
    std::atomic<float>* roamOn = nullptr;
    std::atomic<float>* roamBars = nullptr;
    std::atomic<float>* roamRandomness = nullptr;
    std::atomic<float>* roamSeed = nullptr;
    std::atomic<float>* midiTravel = nullptr;
    std::atomic<float>* midiOctave = nullptr;
    std::atomic<float>* atmosLevel = nullptr;
    std::atomic<float>* atmosDensity = nullptr;
    std::atomic<float>* atmosSeed = nullptr;
    std::atomic<float>* quality = nullptr;
    // The instrument's (null in the FX).
    std::atomic<float>* instrumentChoice = nullptr;
    std::atomic<float>* instLock = nullptr;
    std::array<std::atomic<float>*, dc::inst::macroCount> instMacros {};
    std::atomic<float>* bendRange = nullptr;
    std::atomic<float>* instLevel = nullptr;
    std::atomic<float>* instVowel = nullptr;
    std::atomic<float>* instEnsemble = nullptr;
    std::atomic<float>* effects = nullptr;
    // Session 11: the MIX section, the reverb helpers, Atmosphere On, Punch.
    std::array<std::atomic<float>*, 5> mixGroups {};
    std::atomic<float>* mixLowCut = nullptr;
    std::atomic<float>* mixPreDelay = nullptr;
    std::atomic<float>* mixDuck = nullptr;
    std::atomic<float>* atmosOn = nullptr;
    std::atomic<float>* instPunch = nullptr;
    std::atomic<float>* instPunchTime = nullptr;
    // Session 15: DUNGEON SYNTH and its Era.
    std::atomic<float>* dsAmount = nullptr;
    std::atomic<float>* dsEra = nullptr;
    std::atomic<float>* warmthParam = nullptr;   // Warmth pass
    std::atomic<float>* monoParam = nullptr;

public:
    juce::AudioProcessorValueTreeState parameters;
    // For the editor and tests (written on the audio thread, read anywhere).
    std::atomic<float> inputLevel { 0 }, outputLevel { 0 }, travelProgress { 1 }, cpuLoad { 0 };
    std::atomic<int> currentDestination { 0 }, currentSubLocation { 0 };
    const dc::Engine& engine() const noexcept { return world; }
    int locationIndex() const noexcept;
    // The journey route (Milestone 9): up to eight places, edited on the message thread,
    // read lock-free on the audio thread. Saved with the state.
    void setRoute(const std::vector<dc::Stop>&);
    std::vector<dc::Stop> route() const;
    // The place the sound is in or travelling to right now (journey, roam or MIDI may
    // differ from the Location parameters), and whether one of them is steering.
    bool steering() const noexcept { return steered.load(std::memory_order_relaxed); }
    int atmosphereVoices() const noexcept { return atmosphere.activeVoices(); }
    // Hotfix 1: whether a Journey (on, with a route) or Roam is set to steer.
    bool journeyRunning() const noexcept;
    bool roamRunning() const noexcept;
    // Whether the audio thread has processed a block since the last manual choice of
    // place: until then steering() and currentDestination still describe the journey
    // that was just stopped, and the editor must not follow them.
    bool steeringCurrent() const noexcept { return processedEpoch.load(std::memory_order_acquire) == manualEpoch.load(std::memory_order_acquire); }
    // A manual choice of place (message thread; map, landmark, place list, hotspot):
    // stops a running Journey or Roam and any place MIDI chose, then sets Location and
    // Sub-location. Takes effect in the next audio block. Returns what was stopped
    // ("Journey", "Roam", "Journey and Roam", or empty), for the editor's note.
    juce::String travelTo(int location, int sub);
    // Stops a running Journey or Roam where the sound is now (the Stop button):
    // Location and Sub-location take the journey's current place.
    juce::String stopSteering();

    // Milestone 11 (message thread). The window's scale (saved with the state).
    std::atomic<float> editorScale { 1.0f };
    // The window as it was left (Session 9, LIGHT SEEKER's fix copied): which view (the map
    // or a place's scene), the controls strip, the Advanced drawer and the Journey panel
    // (open, scrolled to). Kept here so a closed and reopened window comes back exactly as
    // it was, and saved with the session (a project reopens it too). The instrument, the
    // place and the window's size are already kept (parameters, editorScale).
    struct ViewState {
        bool valid = false;            // an editor has stored it (else: the defaults)
        bool place = false;            // a place's scene; false: the world map
        int location = 0, sub = 0;     // the scene shown then (the place follows the parameters)
        bool controls = true, advanced = false, journey = false, mix = false;   // mix: Session 11
        int advancedScroll = 0, journeyScroll = 0;
        bool operator==(const ViewState&) const = default;
    };
    ViewState viewState() const;
    void updateViewState(const std::function<void(ViewState&)>&);
    // The Effects roller (0..1.5; 1 = the place as designed).
    float effectsAmount() const noexcept;
    // Session 15: DUNGEON SYNTH (0..1; 0 = exactly as before) and its Era (0 Classic, 1 Modern).
    // Warmth pass: the roller is the Grit (dc::Grit), the Era also voices the warm chain.
    float dungeonSynthAmount() const noexcept;
    int dungeonSynthEra() const noexcept;
    // Warmth pass: the warm character chain (0..1; the instrument 100 % by default, the FX 0 %) and
    // the deliberate Mono switch (off by default).
    float warmthAmount() const noexcept;
    bool monoOn() const noexcept;
    const dc::WarmChain& warmChainForTest() const noexcept { return warm; }
    const dc::Grit& gritForTest() const noexcept { return grit; }
    dc::Grit& gritStageForTest() noexcept { return grit; }
    // Tests: false runs the code path exactly as before the warmth pass for every processor
    // (the warm chain and the voices' detune never run, whatever the Warmth parameter says); the
    // suite's older groups measure the engine so, the warmth group with it on.
    static void setWarmthAvailableForTests(bool on) noexcept { warmthAvailable().store(on); }
    static bool warmthAvailableForTests() noexcept { return warmthAvailable().load(); }
    int qualityMode() const noexcept;
    // Presets: factory (the effect: favourite spots, journeys; the instrument: instrument
    // presets, instruments elsewhere, journeys) and user presets (state files).
    const std::vector<dc::FactoryPreset>& factoryPresetList() const;
    void applyFactoryPreset(int index);
    // "Init / No preset": every parameter at its default (Quality and Bypass kept), no
    // route, no journey; the preset name is "Init".
    void applyInit();
    // Whether anything changed since the preset (or Init) was loaded; the name then
    // shows as "Name *".
    bool presetModified() const;
    juce::String presetDisplayName() const { return presetName + (presetModified() ? " *" : ""); }
    juce::File userPresetFolder() const;
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::File&);
    juce::String presetName { "Init" };
    int presetIndex = -1;   // factory index, or -1
    // A/B compare: two complete settings; switching stores the current one in its slot.
    void selectSlot(int slot);
    void copyCurrentToOtherSlot();
    int currentSlot() const noexcept { return abSlot; }
    // Undo / redo of edits: a snapshot is taken when an edit begins.
    void pushUndo();
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return !undoStack.empty(); }
    bool canRedo() const noexcept { return !redoStack.empty(); }
    bool applyingSnapshot() const noexcept { return applying; }
    // Calibration only (tests): the chain's raw level, without trims or level matcher.
    void setLevelMatching(bool on) noexcept { world.setLevelMatching(on); }

    // The instrument (DUNGEON CRAWLER). Which one plays: the place's, or with Lock
    // Instrument the one chosen. Read anywhere; written on the audio thread.
    std::atomic<int> playingInstrument { 0 };
    int instrumentIndex() const noexcept;   // as the parameters say (the place's unless locked)
    bool instrumentLocked() const noexcept { return instLock != nullptr && instLock->load() >= 0.5f; }
    // Lock Instrument (message thread): on, the instrument now playing is kept.
    void setInstrumentLock(bool on);
    void chooseInstrument(int index);   // locks to this instrument
    const dc::inst::Synth& instrumentEngine() const noexcept { return synth; }
    // The panel's keyboard (message thread): notes into the next audio block, lock-free.
    void keyboardNote(int key, float velocity);   // velocity 0: note off
    // The panel's wheels and pedal (message thread), into the next audio block.
    void panelBend(float bipolar);
    void panelMod(float unit);
    void panelSustain(bool down);
    // The wheels and pedal as the audio thread last saw them (for the panel).
    std::atomic<float> shownBend { 0 }, shownMod { 0 };
    std::atomic<bool> shownSustain { false };

    // Session 10: search. The last eight searches and results chosen ("q:text", "e:key"),
    // kept here so a reopened window has them, and saved with the session.
    juce::StringArray searchRecent() const;
    void setSearchRecent(const juce::StringArray&);
    // The search's preview (message thread): a note on any instrument, lock-free into the
    // next audio block, the place and everything else unchanged (voices keep their own
    // instrument, so what plays is not cut). Refused (false) while the host plays.
    bool previewNote(int instrument, int key, float velocity);
    // Whether the host's transport is playing (written on the audio thread).
    std::atomic<bool> hostPlaying { false };
    // Session 11: the instrument's sound before Session 11 (no fit to a mix, no designed
    // Punch, the old brass). A session or user preset saved before it opens with it, so
    // it sounds as it did; a new instance, Init and the factory presets use the new one.
    // Session 12: the generation of the sound (dc::inst::Sound: 10 classic, 11 Session 11's,
    // 12 the horns that hit harder). Saved as "sound"; a session keeps its own.
    void setClassicSound(bool on) noexcept { soundGeneration.store(on ? dc::inst::SoundClassic : dc::inst::SoundLatest); }
    bool usesClassicSound() const noexcept { return soundGeneration.load() < dc::inst::SoundMix; }
    void setSoundGeneration(int g) noexcept { soundGeneration.store(juce::jlimit(int(dc::inst::SoundClassic), int(dc::inst::SoundLatest), g)); }
    int soundGenerationUsed() const noexcept { return soundGeneration.load(); }
    // Session 14: a session moved from one sound to the next keeps the Level and Punch it
    // set (as heard: the same level, the same dB on the hit); left at their defaults, they
    // stay there (the new sound's defaults).
    void seatSession(int fromSound, int toSound);
    // The Punch control (0..2) as dB on the hit of the instrument shown, and back (its text).
    float punchShownDb(float control) const noexcept;
    float punchFromDb(float db) const noexcept;

private:
    void process(juce::AudioBuffer<float>&, bool forcedBypass, juce::MidiBuffer* midi);
    void renderInstrument(juce::AudioBuffer<float>&, int outputs, juce::MidiBuffer* midi);
    void handleMidi(const juce::MidiMessage&) noexcept;
    dc::inst::Synth synth;
    void postPanel(int packed);
    mutable juce::SpinLock viewLock;
    ViewState view;
    juce::StringArray recentSearches;   // guarded by viewLock
    std::atomic<int> soundGeneration { dc::inst::SoundLatest };
    float placeMakeupDb = 0;   // Session 12: the instrument's make-up in its place (audio thread)
    dc::dsp::Smoothed instGain;   // the instrument's Level
    juce::AbstractFifo keyFifo { 256 };
    std::array<int, 256> keyQueue {};
    static constexpr float neutralIntensity = 1.0e-4f;   // at or below: the dry path
    dc::Controls controls;
    struct Snapshot { std::vector<float> values; std::vector<dc::Stop> route; bool operator==(const Snapshot& o) const; };
    Snapshot snapshot() const;
    void restore(const Snapshot&);
    std::vector<Snapshot> undoStack, redoStack;
    std::array<Snapshot, 2> slots;
    std::array<bool, 2> slotUsed { false, false };
    int abSlot = 0;
    bool applying = false;
    std::array<std::atomic<int>, dc::maxStops> routeStops {};
    std::atomic<int> routeLength { 0 };
    std::atomic<bool> steered { false };
    std::atomic<int> manualEpoch { 0 }, processedEpoch { 0 };
    std::atomic<bool> midiCancel { false };
    juce::String stopSteeringLocked();
    void setPlain(const char* id, float plain);
    // The settings the current preset loaded (for "modified"); none after loading a
    // project saved with a modified preset.
    Snapshot presetBaseline;
    bool hasBaseline = false;
    void markPresetBaseline() { presetBaseline = snapshot(); hasBaseline = true; }
    double freeBeats = 0;          // the position when the host does not play
    int midiLocation = -1, midiSub = 0;
    dc::Engine world;
    dc::Atmosphere atmosphere;
    dc::SafetyLimiter limiter;
    std::array<dc::dsp::DelayLine, 2> dryDelay;   // aligns the dry path with the engine's latency
    std::array<std::array<float, dc::maxChunk>, 2> dryAligned {}, work {};
    // Session 15: the stage (before the dry path and the place), the input aligned for Bypass
    // (which is always the untouched input), the stage's output and its hall's return.
    // Warmth pass: the Grit (the DUNGEON SYNTH roller) and the warm chain, on the whole output.
    dc::Grit grit;
    dc::WarmChain warm;
    static std::atomic<bool>& warmthAvailable() noexcept { static std::atomic<bool> on { true }; return on; }
    std::array<dc::dsp::DelayLine, 2> rawDelay;
    std::array<std::array<float, dc::maxChunk>, 2> rawAligned {}, post {};
    std::array<float, dc::maxChunk> bypassedAt {};
    dc::dsp::Smoothed inputGain, outputGain, wetMix, placeGain;
    dc::dsp::Ramp bypassRamp;
    // Intensity 0 is a true dry path: the output crossfades (30 ms) to the latency-aligned
    // input, so it is exactly transparent instead of a neutral-sounding chain.
    dc::dsp::Ramp neutralRamp;
    double rate = 48000;
    int latency = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DungeonCrawlerProcessor)
};
