#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Parameters.h"

class DungeonCrawlerProcessor final : public juce::AudioProcessor {
public:
    DungeonCrawlerProcessor();
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
    const juce::String getName() const override { return "DUNGEON CRAWLER"; }
    bool acceptsMidi() const override { return false; }
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

public:
    juce::AudioProcessorValueTreeState parameters;
    // For the editor and tests (written on the audio thread, read anywhere).
    std::atomic<float> inputLevel { 0 }, outputLevel { 0 }, travelProgress { 1 }, cpuLoad { 0 };
    std::atomic<int> currentDestination { 0 }, currentSubLocation { 0 };
    const dc::Engine& engine() const noexcept { return world; }
    int locationIndex() const noexcept;
    // Calibration only (tests): the chain's raw level, without trims or level matcher.
    void setLevelMatching(bool on) noexcept { world.setLevelMatching(on); }

private:
    void process(juce::AudioBuffer<float>&, bool forcedBypass);
    static constexpr float neutralIntensity = 1.0e-4f;   // at or below: the dry path
    dc::Controls controls;
    dc::Engine world;
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
