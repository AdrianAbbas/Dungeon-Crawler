#pragma once
// Warmth pass (before 1.0 ships): two stages on the plug-in's whole output, after the place, the
// Mix and the Output gain, before the safety limiter.
//
//   Grit       the DUNGEON SYNTH roller (parameter "dungeonSynth", its ID kept), rebuilt to crush
//              warmly: 4x oversampled asymmetric tube/tape saturation (even harmonics, its drive
//              following the music's level so the colour is the same loud or soft), a gentle bit
//              crush that stays at 12 bits or more, a band limit, a tape modulation-noise grain
//              that rides the music, a dark tilt after it (never fizzy), and its own level match.
//              0 % passes the signal untouched (the stage is not called).
//   WarmChain  the warm character on every instrument and both Eras ("warmth", 100 % on the
//              instrument by default): low-end warmth and a broad 200-500 Hz cut, a decorrelated
//              three-voice ensemble, tape wow and flutter with a slow random drift, a sloped top
//              (a shelf and a 6 dB/oct low-pass whose corner drifts, never a wall), slow
//              amplitude breathing on pads, 2x oversampled tape saturation and a slow bus
//              compressor (the glue) before a decorrelated stereo hall, an envelope-following
//              air / tape-hiss bed (none in silence) shaped so 8-20 kHz sits about -29 to -33 dB
//              under the whole, the bass below 150 Hz kept mono, a slow width servo that holds the
//              image at the Era's L/R correlation (Classic about 0.55, Modern about 0.4), an optional
//              deliberate Mono switch, and a -1.3 dBFS ceiling (true peaks stay under -0.5 dBTP).
//
// The Era voices both: Classic a worn cassette (more hiss and wow, narrower but never mono, more
// drive), Modern lush and wide (a longer hall, less hiss, gentler drive).
//
// Both stages are allocation-free after prepare() and click-free: their amounts glide, and their
// constant latency (the linear-phase oversamplers: 23 samples, not reported; 0.5 ms on the whole
// output) is crossfaded in and out when a stage starts or stops.
#include "Common.h"
#include <array>

namespace dc {

// The Era of the DUNGEON SYNTH roller (the Grit) and of the warm chain.
enum class Era : int { Classic = 0, Modern = 1 };

// Linear-phase polyphase oversampler (factor 2 or 4) for one channel. Its prototype low-pass has
// factor * 24 taps (Kaiser), so up and down together delay by exactly 23 host samples.
class Oversampler {
public:
    static constexpr int tapsPerPhase = 24;
    static constexpr int latencySamples = tapsPerPhase - 1;
    void prepare(int factor);
    void clear() noexcept;
    int factor() const noexcept { return f; }
    // One host sample in, f high-rate samples out.
    void up(float x, float* out) noexcept;
    // f high-rate samples in (the ones up() made, processed), one host sample out.
    float down(const float* in) noexcept;

private:
    int f = 2, n = 48;
    std::vector<float> h;                 // n taps, its passband gain 1 (x f on the way up)
    std::array<float, 64> hist {};        // the host samples (up)
    std::array<float, 512> fast {};       // the high-rate samples (down)
    int histPos = 0, fastPos = 0;
};

// Hermite (4-point) read of a delay line, `delay` >= 2 samples.
float readCubic(const dsp::DelayLine& line, float delay) noexcept;

class Grit {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    // Once per host block: the amount (0..1), the Era (0 Classic, 1 Modern), Eco (the 2x path).
    void setControls(float amount, int era, bool eco) noexcept;
    bool active() const noexcept { return target > 0.0f || amount.value > 0.0f || engage > 0.0f; }
    void process(float* l, float* r, int n) noexcept;
    // Tests: the grain (modulation noise) off, to measure the saturation and crush alone.
    void setTextureForTest(bool on) noexcept { texture = on; }
    float matchGain() const noexcept { return gain; }

private:
    double sr = 48000;
    float target = 0, eraTarget = 0;
    bool eco = false, texture = true, fresh = true;
    dsp::Smoothed amount, era;
    float engage = 0, engageStep = 0;
    std::array<Oversampler, 2> os, osEco;   // 4x (Standard, High), 2x (Eco)
    std::array<dsp::DelayLine, 2> direct;   // the input, delayed to the oversamplers' latency
    std::array<dsp::DcBlocker, 2> dc {};
    float env = 0, envAttack = 0, envRelease = 0, fastEnv = 0, fastAttack = 0, fastRelease = 0;
    float crushScale = 0, crushRelease = 0;
    std::array<dsp::Biquad, 2> lowShelf {}, highShelf {};
    std::array<dsp::Svf, 2> band {}, band2 {};   // the band limit (12 dB/oct, its corner falls with the amount)
    std::array<dsp::Svf, 2> grainHigh {}, grainLow {}, bandHigh {}, bandLow {}, subCut {}, body {}, grainLow2 {};
    int prime = 0;   // samples run since a rest (the crossfade waits until the delay lines are full)
    float bandEnv = 0;
    dsp::Random noise;
    // Level match (the output at its input's loudness, heard through a loudness weighting).
    std::array<dsp::Svf, 4> weightHigh {};
    std::array<dsp::Biquad, 4> weightShelf {};
    float refPower = 0, outPower = 0, refFast = 0, gain = 1, gainTarget = 1, powerCoefficient = 0, fastCoefficient = 0, glide = 0;
    int counter = 0, filterCounter = 0;
    // What the amount, the Era and the envelopes set, every 16 samples.
    struct Control { float norm = 1, satMix = 0, k = 1, bias = 0, t0 = 0, inv = 1, stepScale = 0, grain = 0, a = -1, e = -1; } ctl;
};

class WarmChain {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    // Once per host block. amount 0..1 (the Warmth parameter); era 0 Classic, 1 Modern; mono: the
    // deliberate Mono switch; pad: how much ensemble and breathing suit the instrument (1 pads,
    // strings, choirs, organs .. 0.1 drums); hallSend: the place's wet path (Intensity x Effects,
    // the MIX Reverb, Mix); eco: the low-CPU path (two ensemble voices, a lighter hall).
    void setControls(float amount, int era, bool mono, float pad, float hallSend, bool eco) noexcept;
    // The place's reverb decay (s) and filter cutoff (Hz): the hall takes on its length and tone.
    void setPlace(float decaySeconds, float cutoffHz) noexcept;
    bool active() const noexcept { return target > 0.0f || amount.value > 0.0f || engage > 0.0f; }
    void process(float* l, float* r, int n) noexcept;
    // Tests and the report.
    float compressionDb() const noexcept { return grDb; }
    float widthGain() const noexcept { return sideGain; }
    float airGain() const noexcept { return airLevel; }

private:
    void updateFilters(float a, float e) noexcept;
    double sr = 48000;
    float target = 0, eraTarget = 0, padTarget = 1, sendTarget = 0;
    bool monoTarget = false, eco = false, fresh = true;
    dsp::Smoothed amount, era, pad, send;
    float engage = 0, engageStep = 0, mono = 0, monoStep = 0;
    int filterCounter = 0;
    int latency = 0;   // the oversampler and the tape's resting delay (samples)
    int prime = 0;     // samples run since a rest (the crossfade waits until the delay lines are full)
    // Tone.
    std::array<dsp::Biquad, 2> lowWarm {}, mudCut {}, topShelf {};
    std::array<float, 2> topState {};
    float topCoefficient = 1;
    // Slow random drifts (filter corner, wow, breathing, ensemble rates), each a smoothed random walk.
    struct Drift {
        float value = 0, goal = 0, coefficient = 0;
        int counter = 0, period = 1;
        float next(dsp::Random& r) noexcept {
            if (--counter <= 0) { counter = period; goal = r.bipolar(); }
            value += coefficient * (goal - value);
            return value;
        }
    };
    Drift cutoffDrift, wowDrift, breathDrift, ensembleDrift;
    dsp::Random random;
    // Ensemble: three voices, their own read heads for each side.
    std::array<dsp::DelayLine, 2> ensembleLine;
    std::array<double, 3> ensemblePhase { 0.0, 0.37, 0.71 };
    std::array<dsp::Svf, 2> ensembleHigh {};
    // Tape.
    std::array<dsp::DelayLine, 2> tapeLine;
    double wowPhase = 0, flutterPhase = 0;
    int tapeBase = 120;   // the tape's resting delay (samples), part of the chain's latency
    std::array<Oversampler, 2> os;
    std::array<dsp::DcBlocker, 2> dc {};
    std::array<dsp::DelayLine, 2> tapeAlign;   // the signal, delayed to the oversampler
    std::array<dsp::Svf, 2> tapeSub {};        // the saturation's products below 45 Hz out
    // Bus compressor.
    float tapeEnv = 0, tapeAttack = 0, tapeRelease = 0;
    float detector = 0, detectorCoefficient = 0, grDb = 0, attack = 0, release = 0, longTerm = 0, longCoefficient = 0, longRise = 0;
    float slowLevel = 0, levelCoefficient = 0, liftDb = 0, liftRise = 0, liftFall = 0;
    // Hall: eight lines, decorrelated stereo in and out.
    static constexpr int lines = 8;
    std::array<dsp::DelayLine, lines> hall;
    std::array<float, lines> hallLength {}, hallGain {}, hallState {};
    std::array<double, lines> hallPhase {};
    std::array<dsp::DelayLine, 4> diffuser;
    std::array<float, 4> diffuserLength {};
    std::array<dsp::DelayLine, 2> preDelay;
    std::array<dsp::Svf, 2> hallHigh {}, hallLow {};
    float hallDamp = 0.3f, hallMod = 0, hallPre = 0, hallWidth = 1;
    float placeLength = 1, placeTone = 1;
    float lastA = -1, lastE = -1, lastLength = 0, lastTone = 0;   // what the filters were last set for
    bool lastEco = false;
    // Air bed.
    std::array<std::array<float, 3>, 2> pink {};
    std::array<dsp::Svf, 2> airHigh {}, airHigh2 {};
    std::array<dsp::Biquad, 2> airShelf {};
    std::array<dsp::Svf, 2> progHigh {}, progHigh2 {};
    float airUnit = 1, progPower = 0, progHf = 0, envAttack = 0, envRelease = 0, airLevel = 0, airGlide = 0;
    // Image.
    dsp::Svf sideHigh, sideHigh2, sideHigh3, sideHigh4;
    float midPower = 0, sidePower = 0, sideGain = 1, powerGain = 1, widthCoefficient = 0, widthGlide = 0;
    // Ceiling.
    float ceilingGain = 1, ceilingRelease = 0;
    // What the controls and the envelopes set, every 16 samples.
    struct Control {
        float wowStep = 0, flutterStep = 0, wowDepth = 0, flutterDepth = 0, ensembleDepth = 0;
        float tapeNorm = 1, tapeK = 1, tapeBias = 0, tapeT0 = 0, tapeInv = 1, share = 0;
        float gainFrom = 1, gainTo = 1, at = 1, air = 0, side = 1, power = 1;
    } ctl;
};
}
