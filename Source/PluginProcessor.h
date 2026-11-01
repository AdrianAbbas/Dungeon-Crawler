#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Parameters.h"
#include "World/Journey.h"
#include "DSP/Atmosphere.h"
#include "Instrument/Synth.h"

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
    int qualityMode() const noexcept;
    // Presets: factory (favourite spots, journeys) and user presets (state files).
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
    // The wheels and pedal as the audio thread last saw them (for the panel).
    std::atomic<float> shownBend { 0 }, shownMod { 0 };
    std::atomic<bool> shownSustain { false };

private:
    void process(juce::AudioBuffer<float>&, bool forcedBypass, juce::MidiBuffer* midi);
    void renderInstrument(juce::AudioBuffer<float>&, int outputs, juce::MidiBuffer* midi);
    void handleMidi(const juce::MidiMessage&) noexcept;
    dc::inst::Synth synth;
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
    dc::dsp::Smoothed inputGain, outputGain, wetMix;
    dc::dsp::Ramp bypassRamp;
    // Intensity 0 is a true dry path: the output crossfades (30 ms) to the latency-aligned
    // input, so it is exactly transparent instead of a neutral-sounding chain.
    dc::dsp::Ramp neutralRamp;
    double rate = 48000;
    int latency = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DungeonCrawlerProcessor)
};
