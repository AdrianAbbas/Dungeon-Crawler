#pragma once
// DUNGEON SYNTH Modern (Session 19): a warm, lush vintage sampler on the plug-in's whole output,
// as obvious at 100 % as Classic, warm instead of grimy. Its own chain (Classic's code never runs
// it, and it never runs Classic's); dc::DungeonSynth owns both and crossfades them on the Era.
//
// Order (at 100 %): the vintage sampler (24 kHz, 10 bits, band-limited both ways) -> the low-pass's
// first half -> tape (11 dB, its 2nd harmonic leading, 4x oversampled) -> the low-pass's second
// half (fourth order in all, 8.5 kHz) -> low-mid warmth, a warm tilt, the rumble cut -> the
// ensemble (deep, slow, its moving voices complementary between the ears) -> + the hall (a send
// from here: long, dark, modulated, blooming, as wet as Classic's) -> slow wow -> width -> level
// match. Every non-linear or time-varying part runs where it cannot fizz: the sampler's rate change
// through windowed-sinc kernels, the tape at 4x between linear-phase filters and after the
// sampler's band edge, the moving reads (ensemble, wow) through eight-point windowed sinc.
// Everything is allocated in prepare(); real-time safe. docs/DEVLOG.md, Session 19: why.
#include "Common.h"
#include <array>
#include <vector>

namespace dc {

// =================================================================================================
//  DUNGEON SYNTH Modern at 100 %: THE NUMBERS. One block, one number per tweak.
//  Every amount below 100 % follows from these through the one roller curve (shape(), below):
//  0 % is the input, 25 % subtle, 50-70 % the sweet spot, 100 % deliberately strong.
// =================================================================================================
namespace modern {
inline constexpr double samplerRateHz = 24000.0;  // the vintage sampler's rate (22-26 kHz); its band ends at 0.39 x (9.4 kHz)
inline constexpr float samplerBits = 10.0f;       // its bits (10-11), scaled to the music's envelope (1.5 s release): the grain
inline constexpr float tapeDriveDb = 11.0f;       // tape drive (9-12 dB) into the tape's -18 dBFS RMS reference level, 4x oversampled
inline constexpr double lowPassHz = 8500.0;       // the smooth low-pass (8-10 kHz), fourth order, around the tape
inline constexpr float chorusDepthMs = 9.0f;      // the ensemble's sweep, peak to peak (ms), three voices at 0.4 Hz
inline constexpr float hallWet = 1.0f;            // the hall's return; 1 = as wet as Classic's hall (measured: --ds-modern)
// The rest of 100 %:
inline constexpr float tapeEven = 0.3f;           // the tape's even bend (its 2nd harmonic leads: warmth)
inline constexpr float warmthDb = 3.0f;           // the fuller low-mid: a bump at 200 Hz
inline constexpr float headBumpDb = 5.0f;         // and a low shelf below 250 Hz
inline constexpr float tiltDb = -6.0f;            // a warm tilt: a gentle shelf above tiltHz (the low-pass does the rest)
inline constexpr double tiltHz = 1000.0;          // (its corner)
inline constexpr float ensembleMoving = 0.6f;     // the ensemble's moving pair (added to one ear, taken from the other)
inline constexpr float wowPercent = 0.22f;        // slow wow (peak, % of speed) at 0.4 Hz, with a slow drift
inline constexpr float hallSeconds = 10.0f;       // the hall's T60 in an average place (its place scales it)
inline constexpr float sideGain = 1.6f;           // the image: side x 1.6, the power kept
// Where the roller starts from (0 %+): the sampler at 48 kHz and 16 bits (inaudible), the low-pass
// at 20 kHz. The roller curve: s = a^rollerPower (25 % -> 0.18, 50 % -> 0.42, 70 % -> 0.64).
inline constexpr double samplerRateStartHz = 48000.0;
inline constexpr float samplerBitsStart = 16.0f;
inline constexpr double lowPassStartHz = 20000.0;
inline constexpr float rollerPower = 1.25f;
}

class DungeonSynthModern {
public:
    static constexpr int controlBlock = 16;

    void prepare(double sampleRate);
    void reset() noexcept;
    // As DungeonSynth's (amount 0..1, the instrument's chorus weight, the hall's send).
    void setControls(float amount, float chorus, float hallSend) noexcept;
    void setPlace(float decaySeconds, float cutoffHz, float wet) noexcept;
    // After reset(), start the smoothed controls from these values (the Era crossfading in).
    void snapControls(float amount, float chorus, float hallSend) noexcept;
    bool active() const noexcept { return targetAmount > 0.0f || amount.value > 0.0f || hallRinging; }
    bool shaping() const noexcept { return targetAmount > 0.0f || amount.value > 0.0f; }
    // n <= 64 samples, in place.
    void process(float* l, float* r, int n) noexcept;
    void setHallOnlyForTest(bool on) noexcept { hallOnly = on; }
    float amountNow() const noexcept { return amount.value; }
    float chorusNow() const noexcept { return chorusAmount.value; }
    float sendNow() const noexcept { return send.value; }
    float matchGain() const noexcept { return gain; }
    float samplerRate() const noexcept { return static_cast<float>(now.rate); }
    // The one roller curve.
    static float shape(float a) noexcept;

private:
    // The vintage sampler: a band-limited variable-rate resampler. Its samples are taken at its own
    // rate (any rate, moving with the roller) through a Kaiser windowed-sinc kernel (cutoff 0.445 of
    // its rate: passing 0.39, about 70 dB down at its Nyquist), quantised there, and drawn back at
    // the host's rate through the same kernel: nothing folds back, nothing images up. Its latency is
    // fixed (whatever the rate): latency() host samples.
    struct Sampler {
        static constexpr int half = 20, perUnit = 128;
        std::vector<float> kernel;                  // h(v), v in its own samples, -half .. half
        std::vector<float> inL, inR, lowL, lowR;
        std::vector<double> lowT;                   // each of its samples' instant (host samples)
        std::size_t inMask = 0, lowMask = 0;
        long long count = 0, lowCount = 0;
        double nextT = 0, fs = 48000;
        int lagDown = 0, lag = 0;
        void prepare(double sampleRate, double lowestRate);
        void clear() noexcept;
        int latency() const noexcept { return lag; }
        float h(double v) const noexcept {
            const double p = (v + half) * perUnit;
            if (p <= 0.0 || p >= 2.0 * half * perUnit) return 0.0f;
            const auto i = static_cast<std::size_t>(p);
            const float f = static_cast<float>(p - static_cast<double>(i));
            return kernel[i] + f * (kernel[i + 1] - kernel[i]);
        }
        // One host sample in; the output `lag` samples behind. step: the bits' step (0: none), scaled by env.
        void process(float xl, float xr, double rate, float step, float& yl, float& yr) noexcept;
    };
    // Tape: x4 oversampled (one 81-tap Kaiser low-pass, polyphase both ways: 20 host samples of latency).
    struct Tape {
        static constexpr int factor = 4, taps = 81, latency = (taps - 1) / factor;   // 20
        std::array<float, 84> h {};
        std::array<std::array<float, 32>, 2> in {};     // host samples (newest at pos)
        std::array<std::array<float, 128>, 2> over {};  // oversampled, saturated
        int pos = 0, opos = 0;
        void prepare();
        void clear() noexcept;
        // Both channels; norm: the level normaliser, drive: the gain, even: the asymmetry.
        void process(float& l, float& r, float norm, float drive, float even) noexcept;
    };
    struct Derived {
        float s = 0;
        double rate = modern::samplerRateStartHz;
        float bitStep = 0, sampMix = 0, align = 0;
        float satMix = 0, drive = 1, even = 0;
        double lowPass = 20000.0;
        float warmthDb = 0, tiltDb = 0, headDb = 0;
        float chorusMix = 0, chorusDepth = 0;
        float wowDepth = 0, side = 1, hallIn = 0, bloom = 0;
    };
    Derived derive(float a, float chorus) const noexcept;
    void updateFilters(const Derived&) noexcept;
    float runHall(float in, float& outR, const Derived&) noexcept;
    float readSinc(const dsp::DelayLine& line, float delay) const noexcept;
    void clearState() noexcept;

    double sr = 48000;
    float targetAmount = 0, targetChorus = 0.6f, targetSend = 0;
    dsp::Smoothed amount, chorusAmount, send, placeLength, placeTone, placeSend;
    Derived now {};
    bool hallRinging = false, hallOnly = false;
    int quiet = 0;
    // The sampler, its envelope (the bits follow it), the dry path delayed to its latency.
    Sampler sampler;
    float sampEnv = 0, sampAttack = 0, sampRelease = 0;
    std::array<dsp::DelayLine, 2> alignLine, satDry;
    int latencyTotal = 0;
    // Tape.
    Tape tape;
    float satEnv = 0, satPeak = 0, satAttack = 0, satRelease = 0, satRms = 0, satRmsDown = 0;
    std::array<dsp::DcBlocker, 2> dc {};
    // Tone.
    std::array<dsp::Svf, 2> lowPassA {}, lowPassB {}, rumble {};
    std::array<dsp::Biquad, 2> warmth {}, tilt {}, head {};
    // Eight-point windowed-sinc reads (the ensemble, the wow).
    static constexpr int sincTaps = 8, sincPhases = 256;
    std::array<float, (sincPhases + 1) * sincTaps> sincTable {};
    // The ensemble: three voices from the mid.
    dsp::DelayLine chorusLine;
    std::array<double, 3> chorusPhase { 0.0, 1.0 / 3.0, 2.0 / 3.0 };
    double slowPhase = 0, vibratoPhase = 0;
    // Wow.
    std::array<dsp::DelayLine, 2> wowLine;
    double wowPhase = 0;
    float drift = 0, driftTarget = 0, driftCoefficient = 0;
    int driftCounter = 0;
    dsp::Random noise;
    // The hall (an 8-line FDN; a bloom of two long diffusers before it).
    static constexpr int lines = 8, diffusers = 4, blooms = 2;
    std::array<dsp::DelayLine, lines> hall;
    std::array<float, lines> hallLength {}, hallGain {}, hallState {};
    std::array<double, lines> hallModPhase {};
    std::array<dsp::DelayLine, diffusers> diffuser;
    std::array<float, diffusers> diffuserLength {};
    std::array<dsp::DelayLine, blooms> bloomLine;
    std::array<float, blooms> bloomLength {};
    dsp::DelayLine preDelay;
    dsp::Svf hallHigh, hallLow;
    std::array<dsp::Svf, 4> hallDark {};
    float hallDampCoefficient = 0.3f, hallModDepth = 0, hallPreDelay = 0, hallCeiling = 0.8f;   // the hall's energy at most 1 dB under the music's on long chords (Classic: 2 dB)
    float hallInHold = 0, hallInPower = 0, hallPower = 0, hallCtl = 1, holdAttack = 0, holdRelease = 0, powerCoefficient = 0, ctlDown = 0, ctlUp = 0;
    // The image.
    float midPower = 0, sidePower = 0, widthGain = 1, widthCoefficient = 0;
    // The level matcher (as Classic's: K-weighted, learning while the music plays).
    float refSlow = 0, refFast = 0, outSlow = 0, outFast = 0, refHold = 0, gain = 1, gainTarget = 1;
    float ceiling = 1, ceilingTarget = 1, ceilingDown = 0, ceilingUp = 0;
    float slowCoefficient = 0, fastCoefficient = 0, glideCoefficient = 0;
    int matchCounter = 0;
    std::array<dsp::Svf, 6> weightHigh {}, weightHigh2 {};   // fourth order (the lows the warmth adds count as a meter counts them)
    std::array<dsp::Biquad, 6> weightShelf {};
    float directFast = 0;   // the chain before the hall's return (what the fast ceiling hears)
};
}
