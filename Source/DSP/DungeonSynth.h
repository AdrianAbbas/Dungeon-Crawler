#pragma once
// Session 15: DUNGEON SYNTH, a character stage after the instrument (or the FX's input) and
// before and alongside the place's space. One amount (0..1) scales the whole chain; the Era
// chooses its colour:
//
//   Classic  the 90s cassette sound: a cheap sampler/keyboard (gentle bit-depth and
//            sample-rate reduction, the aliasing softened by a filter in front of it), the
//            keyboard's chorus, cassette wow/flutter and a little hiss, a rolled-off top, a
//            narrower, nearly mono image, and a big dark grainy old-digital hall.
//   Modern   cleaner and warmer: tape saturation and low-mid warmth, a softer top, a vintage
//            ensemble chorus (strongest on pads, strings, choirs and organs), a lush dark
//            modulated hall, only a hint of lo-fi.
//
// Order (the pre-stage, in place): narrow -> sampler (anti-alias, sample-and-hold, bits) ->
// chorus -> tape saturation -> wow/flutter -> low cut, warmth, top roll-off -> hiss.
// The hall is a send from the pre-stage's output: an 8-line feedback delay network, its own
// output returned separately (the processor adds it to the place's wet signal, so Mix,
// Intensity, Effects and the MIX section's Reverb all move it). A slow level matcher keeps
// pre-stage + hall at the input's level (within +/-6 dB).
//
// At amount 0 (and once the hall has rung out) nothing runs: the processor skips the stage
// and the signal is exactly as before. Every stage fades in from the identity as the amount
// rises from 0 (the amount and the Era are smoothed), so moving it is click-free.
// Real-time safe: everything is allocated in prepare().
#include "Common.h"
#include <array>

namespace dc {

class DungeonSynth {
public:
    enum Era { Classic = 0, Modern = 1 };
    static constexpr int controlBlock = 16;   // the derived parameters update every 16 samples

    void prepare(double sampleRate);
    void reset() noexcept;
    // Once per host block. amount 0..1; era Classic/Modern; chorus: how much ensemble suits the
    // instrument (1 pads/strings/choirs/organs .. 0.1 drums); hallSend: how much of the hall the
    // processor lets through (its wet path: Intensity x Effects, the MIX Reverb); heard: of that,
    // what reaches the output (times Mix), for the level matcher.
    void setControls(float amount, int era, float chorus, float hallSend, float heard) noexcept;
    // How long the hall waits on top of its pre-delay (the engine's latency, so the hall stays
    // aligned with the place's output).
    void setHallDelay(int samples) noexcept { hallExtraDelay = std::max(0, samples); }
    // Whether anything has to run: the amount is on or still fading, or the hall still rings.
    bool active() const noexcept { return targetAmount > 0.0f || amount.value > 0.0f || hallRinging; }
    // Whether the pre-stage changes the signal right now (false: it passes exactly).
    bool shaping() const noexcept { return targetAmount > 0.0f || amount.value > 0.0f; }
    // n <= maxChunk samples. l/r in place through the pre-stage; the hall's return (already
    // scaled by hallSend) into hallL/hallR.
    void process(float* l, float* r, float* hallL, float* hallR, int n) noexcept;
    // Tests.
    float amountNow() const noexcept { return amount.value; }
    float eraNow() const noexcept { return era.value; }
    float matchGain() const noexcept { return gain; }

private:
    struct Derived {
        float side = 1, satDrive = 1, satAsym = 0, satMix = 0;
        float lofiMix = 0, holdStep = 1, bitStep = 0, aliasMix = 0;
        float chorusMix = 0, wowDepth = 0, flutterDepth = 0;
        float lowCutMix = 0, warmthDb = 0, topMix = 0, shelfDb = 0;
        float hissGain = 0, hallIn = 0, hallWidth = 1, grainMix = 0, grainStep = 1, grainBits = 0;
    };
    Derived derive(float a, float e, float chorus) const noexcept;
    void updateFilters(const Derived&, float a, float e) noexcept;
    void runHall(float in, float& outL, float& outR, const Derived&) noexcept;
    static float quantise(float x, float step) noexcept;

    double sr = 48000;
    float targetAmount = 0, targetEra = 0, targetChorus = 0.6f, targetSend = 0, targetHeard = 0;
    dsp::Smoothed amount, era, chorusAmount, send, heard;
    Derived now {};
    int counter = 0;
    bool hallRinging = false;
    int quiet = 0;
    // Sampler.
    std::array<dsp::Svf, 4> antiAlias {};    // before the hold (two per side, 24 dB/oct): the aliasing stays soft
    std::array<dsp::Svf, 4> reconstruct {};   // after the hold (two per side): the sampler's output filter
    std::array<float, 2> held {};
    int holdCount = 0, holdFactor = 2;   // the sampler holds each of its samples this many host samples
    // Chorus: three voices from the mid.
    dsp::DelayLine chorusLine;
    std::array<double, 3> chorusPhase { 0.0, 0.33, 0.67 };
    double vibratoPhase = 0;
    // Tape.
    std::array<dsp::DelayLine, 2> wowLine;
    double wowPhase = 0, flutterPhase = 0, flutter2Phase = 0;
    float drift = 0, driftTarget = 0;
    int driftCounter = 0;
    std::array<dsp::DcBlocker, 2> dc {};
    // EQ.
    std::array<dsp::Svf, 2> lowCut {}, top {}, top2 {};
    std::array<dsp::Biquad, 2> warmth {}, shelf {};
    // Hiss.
    dsp::Random noise;
    std::array<float, 2> hissLow {}, hissHigh {};
    float envelope = 0, envAttack = 0, envRelease = 0;
    // The hall.
    static constexpr int lines = 8, diffusers = 4;
    std::array<dsp::DelayLine, lines> hall;
    std::array<float, lines> hallLength {}, hallGain {}, hallDamp {}, hallState {};
    std::array<double, lines> hallModPhase {};
    std::array<dsp::DelayLine, diffusers> diffuser;
    std::array<float, diffusers> diffuserLength {};
    dsp::DelayLine preDelay;
    int hallExtraDelay = 0;
    dsp::Svf hallHigh, hallLow;
    std::array<dsp::Svf, 4> hallDark {};   // after the grain (two per side, 24 dB/oct): the hall stays dark
    float hallOut = 0.5f;
    float hallDampCoefficient = 0.3f, hallModDepth = 0, hallPreDelay = 0;
    std::array<float, 2> grainHeld {};
    float grainPhase = 1;
    double hallModRate = 0;
    // The level matcher (pre-stage + what is heard of the hall against the input).
    float refSlow = 0, refFast = 0, outSlow = 0, gain = 1, gainTarget = 1;
    float slowCoefficient = 0, fastCoefficient = 0, glideCoefficient = 0;
    int matchCounter = 0;
    // The matcher hears as a loudness meter does (a 60 Hz high-pass, +4 dB above 1.7 kHz), so
    // the low end the low cut takes away is not made up as level.
    std::array<dsp::Svf, 4> weightHigh {};
    std::array<dsp::Biquad, 4> weightShelf {};
};
}
