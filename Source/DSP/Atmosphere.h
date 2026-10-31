#pragma once
// IX. ATMOSPHERE (Milestone 10): an optional layer of sparse, synthesised sounds that
// belong to a place: torch crackle, dripping water, distant wind, chains, a far-off
// growl, bubbling, crows... Original synthesis only (no samples). Off by default
// (Level 0); its own Level, Density and Seed. Events sit on an eighth-note grid of the
// musical position and are chosen by a hash of (seed, slot, place), so the layer is
// deterministic and follows the tempo. It ducks under the instrument and never rises
// or sweeps: one-shots and short beds only.
#include "Common.h"
#include <array>

namespace dc {
enum class AtmosSound : int {
    TorchCrackle, Drip, Wind, Chains, Growl, Bubbles, Crow, BellToll, Owl, Murmur,
    Clink, Frog, Howl, IceCreak, Rustle, Tinkle, WaterLap, FireCrackle, Whisper, Count
};
const char* atmosSoundName(AtmosSound) noexcept;
// The sounds of a place (up to three; their order is their weight, first = most often).
struct AtmosSet { std::array<AtmosSound, 3> sounds; int count; };
AtmosSet atmosphereOf(int location, int sub) noexcept;

class Atmosphere {
public:
    static constexpr int voiceCount = 8;
    void prepare(double sampleRate);
    void reset();
    // Adds the layer to l/r. `beats` = the musical position at the start of the block,
    // `beatsPerSample` its rate; `inputLevel` the instrument's level (for ducking).
    void process(float* l, float* r, int n, double beats, double beatsPerSample, int location, int sub,
                 float level, float density, int seed, const float* dryL, const float* dryR) noexcept;
    int activeVoices() const noexcept;

private:
    struct Voice {
        AtmosSound sound = AtmosSound::Count;
        double age = 0, length = 0;
        float gain = 0, pan = 0, pitch = 1, phase = 0, phase2 = 0, env = 0, state = 0, state2 = 0;
        int count = 0, next = 0;
        dsp::Svf filter, filter2;
        dsp::Random random;
        bool active = false;
    };
    void start(AtmosSound, std::uint32_t hash, float level) noexcept;
    float render(Voice&) noexcept;
    double sr = 48000;
    std::array<Voice, voiceCount> voices;
    long long lastSlot = -1;
    float duck = 1, envelope = 0, attack = 0, release = 0, smoothLevel = 0;
};
}
