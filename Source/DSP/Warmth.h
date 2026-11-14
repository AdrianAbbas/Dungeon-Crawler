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
//              compressor (the glue) before a decorrelated stereo hall, the bass below 150 Hz kept
//              mono, a slow width servo that holds the image at the Era's L/R correlation (Classic
//              about 0.55, Modern about 0.4), an optional deliberate Mono switch, a faint tape-hiss
//              bed (the AIR control) and a -1.3 dBFS ceiling (true peaks stay under -0.5 dBTP).
//
// The hiss (hiss fix, build 3): driven by the notes and the DRY signal (before the hall and the
// leveller, never the output), at AIR 15 % (the default) about -46 dB (Classic) / -52 dB (Modern)
// of the whole: only heard soloed on headphones. It is gated: gone within 400 ms of the last
// note's release (the instrument) or of the input falling silent (the FX), even while the hall
// rings, and exactly 0 with no note held and a silent input. Under a note held longer than 2 s it
// ducks 10.5 dB. AIR 0 % adds no hiss at all. The air on top comes from the music instead: a 6-12 kHz
// shelf lift (the tape saturation's and the Grit's harmonics carry it).
//
// The Era voices both: Classic a worn cassette (a little more hiss, more wow, narrower but never
// mono, more drive), Modern lush and wide (a longer hall, less hiss, gentler drive).
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
    // Once per host block: the AIR control (0..1, 0.15 the default: the hiss's share above).
    void setAir(float air) noexcept { airTarget = std::isfinite(air) ? std::clamp(air, 0.0f, 1.0f) : 0.0f; }
    // Once per host block: the Grit's amount (0..1). Its harmonics carry the top: the music's
    // 1.5-5 kHz band, rectified, its products above 2.5 kHz added (none at Grit 0 %).
    void setGrit(float g) noexcept { gritTarget = std::isfinite(g) ? std::clamp(g, 0.0f, 1.0f) : 0.0f; grit.set(gritTarget); }
    // Once per host block, the instrument: whether notes are held (keys or the pedal) and whether
    // one started in this block. Without it (the FX) the dry signal opens the hiss's gate.
    void setNotes(bool held, bool started) noexcept { notesKnown = true; notesHeld = held; noteStarted = noteStarted || started; }
    bool active() const noexcept { return target > 0.0f || amount.value > 0.0f || engage > 0.0f; }
    // dryL / dryR: the dry signal aligned with l / r (before the place); null: l / r themselves.
    void process(float* l, float* r, int n, const float* dryL = nullptr, const float* dryR = nullptr) noexcept;
    // Tests and the report.
    float compressionDb() const noexcept { return grDb; }
    float widthGain() const noexcept { return sideGain; }
    float airGain() const noexcept { return airLevel * airGate * airDuck; }
    float airGateForTest() const noexcept { return airGate; }
    float airDuckForTest() const noexcept { return airDuck; }
    // Tests (`--hiss-renders`): the chain exactly as build 2 had it (the air bed following the
    // output at -29 / -33 dB, no gate; the 6.5 kHz shelf cut), for the before renders.
    static void setLegacyAirForTests(bool on) noexcept { legacyAir() = on; }
    static bool legacyAirForTests() noexcept { return legacyAir(); }

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
    // Air bed (the hiss).
    std::array<std::array<float, 3>, 2> pink {};
    std::array<dsp::Svf, 2> airHigh {}, airHigh2 {};
    std::array<dsp::Biquad, 2> airShelf {};
    std::array<dsp::Svf, 2> progHigh {}, progHigh2 {};
    float airUnit = 1, progPower = 0, progHf = 0, envAttack = 0, envRelease = 0, airLevel = 0, airGlide = 0;
    dsp::Random airNoise;   // its own generator: AIR changes nothing else in the chain
    // The Grit's harmonics (hiss fix): band in, rectified, high-passed out.
    std::array<dsp::Svf, 2> excHigh {}, excLow {}, excOut1 {}, excOut2 {};
    float gritTarget = 0;
    dsp::Smoothed grit;
    float airTarget = 0.15f;
    bool notesKnown = false, notesHeld = false, noteStarted = false;
    // The dry signal's level, its onsets, the gate (0..1, linear: shut within 300 ms) and the duck.
    float dryPower = 0, dryFast = 0, drySlow = 0, dryFastAttack = 0, dryFastRelease = 0, drySlowCoefficient = 0;
    float airGate = 0, gateOpenStep = 0, gateCloseStep = 0, airDuck = 1, duckDown = 0, duckUp = 0;
    int dryHold = 0, dryHoldSamples = 0;
    double heldSeconds = 0;
    static bool& legacyAir() noexcept { static bool on = false; return on; }
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
