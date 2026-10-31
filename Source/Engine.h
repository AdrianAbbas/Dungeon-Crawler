#pragma once
// The sound engine: the six imported modules in a fixed chain, the travel between
// locations and the level matcher. Real-time safe: everything is allocated in
// prepare(); process() never allocates, locks or logs.
#include "DSP/GainStage.h"
#include "DSP/Modules.h"
#include "World/Locations.h"

namespace dc {
class Engine {
public:
    static constexpr double travelSeconds = 1.5;   // Milestone 1 makes this the Travel Time control

    void prepare(double sampleRate);
    // Clears every tail. Hosts call AudioProcessor::reset() on transport stops, so the
    // processor only calls this from prepareToPlay.
    void reset();
    // Constant for a given sample rate: Ancient Blood's oversampling and wow centre.
    int latencySamples() const noexcept { return blood.latencySamples(); }

    // One chunk (n <= maxChunk) in place. dryL/dryR are the unprocessed input aligned
    // with the latency (the level matcher's reference).
    void process(float* l, float* r, int n, const float* dryL, const float* dryR,
                 int location, float intensity, double bpm) noexcept;

    int destination() const noexcept { return target; }
    float travelProgress() const noexcept { return travel; }
    const Frame& frame() const noexcept { return current; }

    // A location's settings scaled by Intensity: 0 is neutral (transparent), 1 the place as designed.
    static Frame targetFrame(int location, float intensity) noexcept;
    static Frame interpolate(const Frame& a, const Frame& b, float t) noexcept;

private:
    struct Slot { dsp::Ramp gain; bool idle = true; int warmUp = 0; };
    template <class Module> void runSlot(Module&, Slot&, bool enabled, float* l, float* r, int n) noexcept;

    double sr = 48000;
    Abyss abyss;
    AncientBlood blood;
    Choir choir;
    Wraith wraith;
    Echoes echoes;
    Crypt crypt;
    std::array<Slot, 5> slots;   // abyss, choir, wraith, echoes, crypt (Blood runs always: it carries the latency)
    LevelMatcher levelMatcher;
    Context context;
    std::array<std::array<float, maxChunk>, 2> pre {};
    Frame from {}, to {}, current {};
    int target = -1;
    float targetIntensity = -1, travel = 1, travelStep = 0;
    int warmUpSamples = 240, settleSamples = 24000, settle = 0;
};
}
