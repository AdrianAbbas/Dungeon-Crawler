#pragma once
// DUNGEON SYNTH (Session 15; rebuilt in Session 16): a character stage on the plug-in's whole
// output (after the place, the Mix and the Output gain, before the safety limiter). One amount
// (0..1) scales the whole chain; the Era chooses its colour:
//
//   Classic  the 90s cassette demo: a cheap sampler (the rate down to about 13 kHz with its
//            aliasing let through, about 8.5 bits, no dither), the keyboard's light chorus, a
//            big dark grainy old-digital hall at a high wet level, cassette wow (0.45 %, slow
//            and drifting) and flutter, tape hiss (riding under the music's treble, as a cassette's
//            noise reduction lets it: about -45 dBFS on bright material), a low-pass
//            at 5.5 kHz, a nearly mono image.
//   Modern   (Session 18) a lush, warm vintage sampler, not a broken one: gentle bit and rate
//            reduction (12 bits, 30 kHz at 100 %) between steep, clean eighth-order filters
//            (smooth, rounded, no fizz), the bits following the music's envelope closely (the
//            decays stay smooth); warm, gentle tape saturation; a fuller low-mid, a smooth top
//            (not muffled); a deep, slow vintage ensemble chorus, wide; a long, dark,
//            modulated hall that blooms (two long diffusers build it up); a hint of slow wow,
//            very little hiss. (Session 15-17's Modern, heavy saturation, 6th-order 10 kHz top,
//            sounded low quality: Tests/legacy keeps it for the before/after render.)
//
// Session 16: the stage was before the place (Session 15). There the place's own reverb,
// air, shimmer and ambience, which never went through it, re-widened and re-brightened the
// output and covered the stage: in its place the Dark Choir Pad at Classic 100 % came out
// brighter than at 0 %. A cassette records everything; so does the stage now.
//
// Order: (the hiss, before the place: processHiss) sampler -> chorus/ensemble -> (+ the hall, a
// send from here) -> tape saturation -> wow/flutter -> low cut, warmth (Modern) or the boxy middle (Classic), low-pass, shelf ->
// width -> level match. (Session 18, Modern only: its sampler is the Vintage below, the direct
// path delayed to its latency; the tape saturates the body below 2.5 kHz; the wow and the
// ensemble read through eight-point windowed sinc; the hall blooms. In Classic none of it runs:
// Classic is bit for bit Session 17's, tested against Tests/legacy.)
// A slow level matcher keeps the output at the input's loudness (+/-6 dB, K-weighted).
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
    // processor lets through (Intensity x Effects, the MIX Reverb, Mix).
    void setControls(float amount, int era, float chorus, float hallSend) noexcept;
    // Once per host block (Session 16): the place the music is in, so the hall takes on its
    // character instead of laying the same hall over every place: its reverb's decay (s), its
    // filter's cutoff (Hz) and how wet it already is (0..1, its reverbs' mixes).
    void setPlace(float decaySeconds, float cutoffHz, float wet) noexcept;
    // Whether anything has to run: the amount is on or still fading, or the hall still rings.
    bool active() const noexcept { return targetAmount > 0.0f || amount.value > 0.0f || hallRinging; }
    // Whether the stage changes the signal right now (false: it passes exactly, the hall rings out).
    bool shaping() const noexcept { return targetAmount > 0.0f || amount.value > 0.0f; }
    // n <= maxChunk samples, in place.
    void process(float* l, float* r, int n) noexcept;
    // The hiss, on the input before the place (n <= maxChunk, in place; at the amount and Era of
    // the last process() call).
    void processHiss(float* l, float* r, int n) noexcept;
    // Tests.
    float amountNow() const noexcept { return amount.value; }
    float eraNow() const noexcept { return era.value; }
    float matchGain() const noexcept { return gain; }
    float samplerRate() const noexcept { return static_cast<float>(now.rateStep * sr); }
    void setHallOnlyForTest(bool on) noexcept { hallOnly = on; }   // the output is the hall alone
    // Session 18 (tests): what the processor last set (the instrument's chorus weight, the hall's
    // send, the place: decay s, cutoff Hz, wet), to drive another copy of the stage the same way.
    float chorusForTest() const noexcept { return targetChorus; }
    float sendForTest() const noexcept { return targetSend; }
    std::array<float, 3> placeForTest() const noexcept { return lastPlace; }

private:
    // Session 18: Modern's sampler, a warm vintage one done cleanly. It samples at a whole-number
    // ratio of the host's rate (U / D: 2 / 3 at 44.1 and 48 kHz, 29.4 and 32 kHz; 2 / 6 at 88.2 and
    // 96 kHz; 2 / 12 at 176.4 and 192 kHz), through one linear-phase polyphase low-pass (Kaiser,
    // passing 0.39 of its rate, about 70 dB down from 0.53) both ways: what folds back is a whisper
    // at the top of its band, rounded, never a fizz. Its bits are applied at its own rate (crush).
    // Latency: latency() host samples. Real-time safe (sized in prepare()).
    struct Vintage {
        int up = 2, down = 3, taps = 0, lat = 0;
        std::vector<float> h;            // taps, the DC gain 1
        std::vector<float> in, low;      // the host's samples, its own samples (rings)
        std::size_t inMask = 0, lowMask = 0;
        long long count = 0, lowCount = 0;
        void prepare(double sampleRate);
        void clear() noexcept;
        int latency() const noexcept { return lat; }
        template <class Crush>
        float process(float x, const Crush& crush) noexcept {
            in[std::size_t(count) & inMask] = x;
            // Its samples whose instants fall on this host sample (on the up-sampled grid).
            for (int r = 0; r < up; ++r) {
                const long long j = count * up + r;
                if (j % down != 0) continue;
                float acc = 0;
                for (int k = 0, t = r; t < taps; ++k, t += up) acc += h[std::size_t(t)] * in[std::size_t(count - k) & inMask];
                low[std::size_t(lowCount) & lowMask] = crush(acc * static_cast<float>(up));
                ++lowCount;
            }
            // Back to the host's rate (the newest of its samples is at or before this instant).
            const long long jj = count * up, newest = jj / down;
            float y = 0;
            for (long long k = 0, t = jj % down; t < taps; ++k, t += down) y += h[std::size_t(t)] * low[std::size_t(newest - k) & lowMask];
            ++count;
            return y * static_cast<float>(down);
        }
    };
    struct Derived {
        float lofiMix = 0, rateStep = 1, aliasOpen = 0, bitStep = 0;
        float chorusMix = 0, chorusDepth = 0;
        float satMix = 0, satDrive = 1, satAsym = 0;
        float wowDepth = 0, flutterDepth = 0;
        float hissGain = 0;
        float lowCutMix = 0, warmthDb = 0, honkDb = 0, topMix = 0, shelfDb = 0;
        float side = 1;
        float hallIn = 0, hallWidth = 1, grainMix = 0;
        // Session 18 (0 in Classic: nothing of it runs there): Modern's weight, the hall's bloom.
        float modern = 0, bloom = 0, align = 0, modernBitStep = 0;
    };
    Derived derive(float a, float e, float chorus) const noexcept;
    void updateFilters(const Derived&, float a, float e) noexcept;
    float runHallMono(float in, float& outR, const Derived&) noexcept;
    static float quantise(float x, float step, float scale) noexcept;

    double sr = 48000;
    float targetAmount = 0, targetEra = 0, targetChorus = 0.6f, targetSend = 0;
    dsp::Smoothed amount, era, chorusAmount, send;
    // The place's character (smoothed): the hall's length and tone scale, the send's share.
    dsp::Smoothed placeLength, placeTone, placeSend;
    std::array<float, 3> lastPlace { 3.0f, 12000.0f, 0.0f };
    Derived now {};
    bool hallRinging = false, hallOnly = false;
    int quiet = 0;
    // Sampler: anti-alias filters, sampling at the lower rate (the instant interpolated between
    // two host samples), the bits (scaled to the music's envelope, as a sample is normalised),
    // a line between its samples, the output filter.
    std::array<dsp::Svf, 4> antiAlias {}, reconstruct {};
    std::array<float, 2> held {}, previous {}, lastIn {};
    float holdPhase = 0;
    float crushEnv = 0, crushAttack = 0, crushRelease = 0;
    // Session 18, Modern's sampler (one a channel), the direct path delayed to its latency, the
    // envelope its bits follow (a quicker release than Classic's: smooth decays).
    std::array<Vintage, 2> vintage;
    // Session 18, Modern's moving reads (the wow, the ensemble): eight-point windowed sinc, its
    // kernels tabled at 256 fractions (flat to 0.4 of the host's rate: a moving fraction sweeps
    // no level in the treble, so no fizz around the high notes).
    static constexpr int sincTaps = 8, sincPhases = 256;
    std::array<float, (sincPhases + 1) * sincTaps> sincTable {};
    float readSinc(const dsp::DelayLine& line, float delay) const noexcept;
    std::array<dsp::DelayLine, 2> alignLine;
    float modernEnv = 0, modernRelease = 0;
    // Chorus / ensemble: three voices from the mid.
    dsp::DelayLine chorusLine;
    std::array<double, 3> chorusPhase { 0.0, 0.33, 0.67 };
    double vibratoPhase = 0;
    // Saturation: its drive follows the music's level (the same colour loud or soft).
    float satEnv = 0, satCoefficient = 0, satAttack = 0;
    std::array<dsp::DcBlocker, 2> dc {};
    // Session 18, Modern's: the band it saturates (below 2.5 kHz) and its own DC blockers.
    std::array<dsp::Svf, 2> satSplit {};
    std::array<dsp::DcBlocker, 2> satDc {};
    // Tape.
    std::array<dsp::DelayLine, 2> wowLine;
    double wowPhase = 0, flutterPhase = 0, flutter2Phase = 0;
    float drift = 0, driftTarget = 0, driftCoefficient = 0;
    int driftCounter = 0;
    // Hiss (about 25 dB under the music's treble, as a cassette's noise reduction: none in silence).
    dsp::Random noise;
    std::array<float, 2> hissLow {}, hissLow2 {}, hissHigh {};
    float gate = 0, gateAttack = 0, gateRelease = 0, trebleLow = 0, trebleCoefficient = 0, hissApplied = 0;
    // EQ.
    std::array<dsp::Svf, 2> lowCut {}, lowCut2 {}, top {}, top2 {}, top3 {};
    std::array<dsp::Biquad, 2> warmth {}, honk {}, shelf {};
    // The hall.
    static constexpr int lines = 8, diffusers = 4;
    std::array<dsp::DelayLine, lines> hall;
    std::array<float, lines> hallLength {}, hallGain {}, hallState {};
    std::array<double, lines> hallModPhase {};
    std::array<dsp::DelayLine, diffusers> diffuser;
    std::array<float, diffusers> diffuserLength {};
    dsp::DelayLine preDelay;
    // Session 18: Modern's bloom, two long diffusers before the tank (the hall builds up).
    static constexpr int blooms = 2;
    std::array<dsp::DelayLine, blooms> bloomLine;
    std::array<float, blooms> bloomLength {};
    dsp::Svf hallHigh, hallLow;
    std::array<dsp::Svf, 4> hallDark {};
    float hallOut = 1, hallDampCoefficient = 0.3f, hallModDepth = 0, hallPreDelay = 0, diffusion = 0.6f;
    std::array<float, 2> grainHeld {};
    // Its limiter: on long sustained chords the hall's energy stays at most 2 dB under the
    // music's (whose envelope releases over 1.5 s, slower than the hall decays: the tails ring freely).
    float hallInHold = 0, hallInPower = 0, hallPower = 0, hallCtl = 1, holdAttack = 0, holdRelease = 0, powerCoefficient = 0, ctlDown = 0, ctlUp = 0;
    float grainPhase = 1, grainStep = 1, grainBits = 0;
    // The image's power (what the narrowing takes away is given back).
    float midPower = 0, sidePower = 0, widthGain = 1, widthCoefficient = 0;
    // The level matcher (the output against the input).
    float refSlow = 0, refFast = 0, outSlow = 0, outFast = 0, refHold = 0, gain = 1, gainTarget = 1;
    float ceiling = 1, ceilingTarget = 1, ceilingDown = 0, ceilingUp = 0;
    float slowCoefficient = 0, fastCoefficient = 0, glideCoefficient = 0;
    int matchCounter = 0;
    // It hears as a loudness meter does (a 60 Hz high-pass, +4 dB above 1.7 kHz).
    std::array<dsp::Svf, 4> weightHigh {};
    std::array<dsp::Biquad, 4> weightShelf {};
};
}
