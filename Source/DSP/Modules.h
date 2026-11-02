#pragma once
// The six modules imported from VEIL (see docs/DEVLOG.md, Milestone 0 audit). Each
// processes a stereo chunk in place:
//   process(left, right, numSamples, params, context)
// Parameters arrive once per chunk (a Frame from the engine: location, travel) and are
// smoothed inside the module. Chunks never exceed maxChunk samples.
#include "ModuleParams.h"
#include "Common.h"
#include <array>
#include <memory>
#include <vector>

namespace juce::dsp { template <typename> class Oversampling; }

namespace dc {
struct Context {
    double sampleRate = 48000, bpm = 120;
};
inline constexpr int maxChunk = 64;

//==============================================================================
// I. THE ABYSS: resonant low-pass (12/24 dB), high-pass, Darkness (cutoff, high
// shelf cut, low-mid warmth), slow Movement, and the Tomb Resonator.
class Abyss {
public:
    void prepare(double sampleRate);
    void reset();
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
private:
    double sr = 48000, phase = 0;
    std::array<dsp::Svf, 2> hp, lp1, lp2;
    std::array<dsp::Biquad, 2> shelf, warmth;
    dsp::Smoothed logCutoff, resonance, highPass, darkness, movement, slopeBlend, tombAmount;
    // Tomb: two tuned feedback combs per channel with damping in the loop.
    std::array<std::array<dsp::DelayLine, 2>, 2> combs;
    std::array<std::array<float, 2>, 2> combDamp{};
    dsp::Smoothed tombPeriod, tombFeedback;
    float tombDampCoefficient = 0.5f, compensation = 1;
    int counter = 0;
};

//==============================================================================
// II. ANCIENT BLOOD: oversampled saturation with loudness compensation, Age,
// Dust, Instability. It is the only module in the main path with latency; its
// own dry path is delayed to match and it never stops (off = aligned dry).
class AncientBlood {
public:
    AncientBlood();
    ~AncientBlood();
    void prepare(double sampleRate);
    void reset();
    int latencySamples() const noexcept { return latency; }
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
private:
    double sr = 48000;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int latency = 0;
    std::array<dsp::DelayLine, 2> dryDelay, wow;
    std::array<std::vector<float>, 2> wet;
    dsp::Smoothed drive, texture, age, dust, instability, mix, enable;
    std::array<dsp::Biquad, 2> ageFilter;
    std::array<float, 2> held{}, inMs{}, outMs{}, compensation{1, 1};
    float msCoefficient = 0.0003f, compensationCoefficient = 0.05f;
    std::array<dsp::DcBlocker, 2> dc;
    float holdPhase = 0, wowPhase = 0, drift = 0, driftTarget = 0, dustEnvelope = 0, crackle = 0;
    std::array<float, 2> hiss{};
    int driftCountdown = 0;
    dsp::Random random;
};

//==============================================================================
// VII. RELICS (DUNGEON CRAWLER addition): torch flicker (an irregular tremolo), a
// keyed resonator bank of struck metal (bars, a bell, crystal), and two subtle beds
// that follow the playing and fall silent with it: wind and water drops. It sits
// after Ancient Blood, so the echoes and the reverb carry what it adds.
class Relics {
public:
    static constexpr int partials = 6;
    void prepare(double sampleRate);
    void reset();
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
private:
    void configure(int kind, int note, int octave, float decaySeconds) noexcept;
    struct Drop { float phase = 0, frequency = 1000, amplitude = 0, pan = 0; };
    double sr = 48000;
    std::array<std::array<dsp::Svf, partials>, 2> resonators;
    std::array<float, partials> partialGain{};
    std::array<dsp::Svf, 2> windFilter;
    std::array<Drop, 4> dropsActive;
    dsp::Smoothed flicker, metal, wind, drips, whispers;
    // Whispers (Session 8): two readers of the developer's whispering, each a few seconds
    // from anywhere in one of his three takes, faded in and out.
    struct Whisper { const int16_t* data = nullptr; int length = 0; double pos = 0; float step = 1, age = 0, span = 0, gain = 0, pan = 0; bool on = false; };
    std::array<Whisper, 2> whisperVoices;
    int whisperCountdown = 0;
    std::array<dsp::Svf, 4> whisperBand;   // per side: a high-pass, then a low-pass
    float whisperLevel = 0;
    dsp::Random whisperRandom;
    float envelope = 0, attack = 0.01f, release = 0.0001f, gainSmooth = 0.01f;
    float flickerGain = 1, flickerTarget = 0, flickerValue = 0, flickerPhase = 0, windPhase = 0, configuredDecay = -1;
    int flickerCountdown = 0, dripCountdown = 0, configured = -1;
    dsp::Random random;
};

//==============================================================================
// VIII. SIGNATURE (DUNGEON CRAWLER, Milestone 6): the one effect that belongs to a
// single place, so any two places are tellable apart blind. Each kind is a recipe of
// small primitives (saturation flavour, tone, attack softening, wobble, tremolo,
// tuned combs, a modal bank, pitch sparkle, tap patterns, smear, reverse swell); see
// Signature.cpp. A kind change cannot morph: the engine dips the layer out, switches,
// and brings it back (as for the Choir interval).
namespace sig { struct Recipe; }
class Signature {
public:
    static constexpr int kinds = 41;   // 0 = none
    static constexpr int maxTaps = 6, maxModes = 6, maxCombs = 3, smearStages = 4, diffuserStages = 3;
    void prepare(double sampleRate);
    void reset();
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
    static const char* kindName(int kind) noexcept;
    // After a journey to another kind: stop taking input (as the amount falls) and ring
    // out; the engine runs the new kind in a second instance.
    void depart() noexcept;
    bool isDeparting() const noexcept { return departing; }
    bool ringing() const noexcept { return tail > 1.0e-4f; }
    float tailLevel() const noexcept { return tail; }
private:
    bool departing = false;
    float heldOut = 0, tail = 0, tailDecay = 0.999f;
    struct Lfo {
        double phase = 0;
        float value = 0, target = 0, follow = 0, followHz = -1;
        int countdown = 0, followShape = -1;
        float next(int shape, float hz, double sr, dsp::Random&) noexcept;
    };
    void configure(int kind, float tone) noexcept;
    double sr = 48000;
    int kind = -1;
    float tone = 0, toneShift = 1;
    const sig::Recipe* recipe = nullptr;
    dsp::Smoothed amount;
    // Serial stages.
    std::array<dsp::Svf, 2> toneSvf, growlLow, growlSub;
    std::array<dsp::Biquad, 2> toneEq, bite;
    std::array<dsp::DcBlocker, 2> dc;
    std::array<float, 2> growlPrev {}, held {};
    std::array<int, 2> holdCount {};
    float growlFlip = 1, driveGain = 1, driveComp = 1;
    float envFast = 0, envSlow = 0, envSwell = 0, envBloom = 0, fastA = 0, fastR = 0, slowA = 0, slowR = 0, swellA = 0, bloomA = 0, bloomR = 0, gateEnv = 0, gateR = 0;
    std::array<dsp::DelayLine, 2> wobbleLine;
    Lfo wobbleLfo, wobbleLfoR, tremLfo;
    // Parallel layers.
    std::array<std::array<dsp::DelayLine, maxCombs>, 2> combLines;
    std::array<std::array<float, maxCombs>, 2> combState {};
    std::array<float, maxCombs> combSamples {};
    float combCoefficient = 0.5f;
    std::array<std::array<dsp::Svf, maxModes>, 2> modes;
    std::array<float, maxModes> modeGain {};
    std::array<std::array<dsp::GranularShifter, 2>, 2> shifters;
    std::array<dsp::Svf, 2> pitchHp, pitchLp;
    std::array<std::array<dsp::Biquad, 2>, 2> formants;
    std::array<float, 2> twinkle { 1, 1 }, twinkleTarget { 1, 1 };
    int twinkleCountdown = 0;
    std::array<dsp::DelayLine, 2> tapLine;
    std::array<float, maxTaps> tapLpState0 {}, tapLpState1 {}, tapCoefficient {}, tapJitter {};
    std::array<Lfo, maxTaps> tapLfo;
    int crackleCountdown = 0;
    double circlePhase = 0;
    std::array<std::array<dsp::DelayLine, smearStages>, 2> smearLines;
    std::array<std::array<float, smearStages>, 2> smearSamples {};
    std::array<std::array<dsp::DelayLine, diffuserStages>, 2> diffuserLines;
    std::array<std::array<float, diffuserStages>, 2> diffuserSamples {};
    std::array<dsp::DelayLine, 2> reverseLine;
    std::array<dsp::Svf, 2> reverseLp;
    double reversePhase = 0;
    dsp::Random random;
};

//==============================================================================
// IX. THE VAULT (Direction Update 2): zero-latency convolution with generated impulse
// responses, one per space (Vault.cpp): huge halls with slow bloom and long pre-delay,
// reversed swells, caverns with far distinct echoes and glints, small warm rooms.
// The IRs are built once per sample rate and shared by every instance (not real-time:
// in prepare()). Wet-only (the engine adds the dry), with an octave shimmer fed back
// from the wet into the input and ducking under the playing.
namespace conv { class Convolver; struct IrSpectra; }
class Vault {
public:
    static constexpr int kinds = 34;   // 0 = none
    Vault();
    ~Vault();
    void prepare(double sampleRate);
    void reset();
    // The space to use; changing it is only real-time safe right after reset().
    void setKind(int kind) noexcept;
    int kindNow() const noexcept { return kind; }
    void process(const float* inL, const float* inR, float* wetL, float* wetR, int n, const Frame&) noexcept;
    bool ringing() const noexcept { return tail > 1.0e-4f; }
    float level() const noexcept { return tail; }
    // Seconds of IR per kind (tests and the CPU report).
    static double kindSeconds(int kind) noexcept;
    static const char* kindName(int kind) noexcept;
private:
    double sr = 48000;
    int kind = 0;
    std::unique_ptr<conv::Convolver> left, right;
    std::shared_ptr<const void> cache;   // keeps the shared IR set alive
    const std::vector<std::array<const conv::IrSpectra*, 2>>* irs = nullptr;
    dsp::Smoothed shimmer, toneHz, duck;
    std::array<dsp::GranularShifter, 2> shifters;
    std::array<float, 2> shimmerLp {}, toneState1 {}, toneState2 {};
    std::array<std::array<float, maxChunk>, 2> feed {}, back {};
    float envelope = 0, envAttack = 0, envRelease = 0, tail = 0, tailDecay = 0.999f;
};

//==============================================================================
// X. ENSEMBLE (Direction Update 2): a lush three-voice chorus, a rising (barber-pole)
// phaser, a tape echo that bounces between left and right with wow, darkening and
// saturation in the loop (each repeat optionally rising or falling in pitch), and
// realistic water drops (a resonant bubble that chirps upwards), very quiet.
class Ensemble {
public:
    void prepare(double sampleRate);
    void reset();
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
    bool ringing() const noexcept { return held > 1.0e-4f; }
private:
    static constexpr int voices = 3, phasers = 4, stages = 6;
    double sr = 48000;
    dsp::Smoothed mix, depth, rate, spiral, spiralRate, tapeMix, feedback, toneHz, wow, drops;
    std::array<dsp::DelayLine, 2> chorusLine;
    std::array<double, voices> chorusPhase {};
    // Rising phaser: four staggered sweeps whose notch frequencies climb, each faded in
    // and out by a window over its cycle (an endless upward spiral).
    double spiralPhase = 0;
    std::array<std::array<std::array<float, stages>, phasers>, 2> apState {};
    std::array<std::array<float, phasers>, 2> phaserFb {};
    // Tape: two heads crossfaded on time changes (no pitch glides), ping-pong cross.
    std::array<dsp::DelayLine, 2> tape;
    std::array<float, 2> tapeLp {}, tapeHp {}, tapeOut {};
    float headA = 18000, headB = 18000, headFade = 1;
    double wowPhase = 0, flutterPhase = 0;
    std::array<dsp::GranularShifter, 2> ascend;
    int ascendKind = 0;
    float held = 0, heldDecay = 0.9999f;
    // Drops.
    struct Drop { float phase = 0, hz = 1000, chirp = 0, amp = 0, decay = 0.999f, pan = 0, click = 0; };
    std::array<Drop, 4> dropPool;
    int dropCountdown = 0;
    float envelope = 0;
    dsp::Random random;
};

//==============================================================================
// III. THE CHOIR: 1-4 granular pitch-shifted voices with detune, movement and a
// vowel formant bank, plus Drone: a sub-octave layer that holds after notes end.
class Choir {
public:
    void prepare(double sampleRate);
    void reset();
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
private:
    double sr = 48000, lfoPhase = 0;
    std::array<dsp::GranularShifter, 4> voices;
    std::array<float, 4> voiceGain{}, ratios{ { 1, 1, 1, 1 } };
    std::array<std::array<dsp::Biquad, 3>, 2> formants;
    std::array<dsp::Biquad, 2> choirTone;
    dsp::Smoothed blend, detune, movement, droneLevel, formant;
    dsp::Svf inputLowpass;
    // Drone: sub-octave shifter feeding a capture buffer that grains play from.
    dsp::GranularShifter droneShifter;
    std::vector<float> capture;
    std::size_t captureWrite = 0, captureFrozenAt = 0, captureCount = 0;
    struct Grain { double position = 0; float age = 1, length = 1, pan = 0, rate = 1; };
    std::array<Grain, 4> grains;
    float inputEnvelope = 0, holdEnvelope = 0, droneLevelTrack = 0;
    bool captureActive = false;
    std::array<dsp::Biquad, 2> droneTone;
    dsp::Random random;
    int counter = 0;
    float formantCached = -1;
    void updateFormants(float value) noexcept;
    // Voice Imprint (Session 8): a bank of third-octave band-passes shaped by the
    // spectral envelope of the developer's choir stacks (ooh to aah with Formant).
    static constexpr int imprintBands = 19;
    std::array<std::array<dsp::Biquad, imprintBands>, 2> imprintBank;
    std::array<float, imprintBands> imprintGain{};
    dsp::Smoothed imprint;
    float imprintCached = -2, genericPower = 0, imprintPower = 0, powerCoefficient = 0.0005f;
    bool imprintShaped = false;
    void updateImprint(float formantValue) noexcept;
};

//==============================================================================
// IV. THE WRAITH: STFT phase vocoder (Hann, 4x overlap). Blur smears magnitudes
// over time and frequency, Detune/Drift shift pitch slightly, Ghost adds a delayed
// octave-up spectral copy, Freeze resynthesises a held spectrum with evolving
// phases. Wet-only layer: its STFT delay is part of the effect.
struct WraithFft;   // wraps juce::dsp::FFT, defined in Wraith.cpp
class Wraith {
public:
    Wraith();
    ~Wraith();
    void prepare(double sampleRate);
    void reset();
    int fftSize() const noexcept { return size; }
    // Eco quality: one spectral channel (the mid) instead of two: half the cost.
    void setMono(bool on) noexcept { mono = on; }
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
private:
    struct Channel {
        std::vector<float> inFifo, outAccum, frame, window;
        std::vector<float> lastPhase, sumPhase, ghostPhase, smoothMag, frozenMag, mag, freq, synthMag, synthFreq;
        std::vector<float> ghostRing, blurred;   // ghost: past frames of magnitudes
        int ghostWrite = 0;
        bool captured = false;                   // freeze spectrum taken
    };
    void processFrame(Channel&, int channel) noexcept;
    double sr = 48000;
    int size = 2048, hop = 512, order = 11, ghostFrames = 48, fifoPos = 0, hopCounter = 0;
    std::unique_ptr<WraithFft> fft;
    float scale = 1;
    std::array<Channel, 2> channels;
    bool mono = false;
    dsp::Smoothed mix;
    float blur = .5f, detuneCents = 15, drift = .4f, ghost = .45f, freezeAmount = 0, freezeTarget = 0;
    float driftValue = 0, driftTarget = 0, ratio = 1, blurTarget = .5f, ghostTarget = .45f;
    int framesSinceDrift = 0, nextDrift = 12;
    dsp::Random random;
};

//==============================================================================
// V. THE ECHOES: crossfaded time changes, damping, diffusion, Reverse, Descent
// (each repeat drops in pitch), stereo spread, limited feedback.
class Echoes {
public:
    void prepare(double sampleRate);
    void reset();
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
    bool ringing() const noexcept { return held > 1.0e-3f; }   // a tail still sounds
private:
    float held = 0;   // the return level, held while the tail decays
    double sr = 48000;
    std::array<double, 2> headPhase { 0.0, 0.5 };   // the two reverse heads
    std::array<dsp::DelayLine, 2> lines;
    std::array<std::array<dsp::DelayLine, 3>, 2> diffusers;
    std::array<std::array<std::size_t, 3>, 2> diffuserSamples{};
    std::array<float, 2> diffuserDelay{};   // total all-pass delay per channel, taken out of the tap
    std::array<dsp::GranularShifter, 2> descent;
    std::array<float, 2> damp{}, hpState{};
    float tapA = 20000, tapB = 20000, crossfade = 1;   // two taps for click-free time changes
    std::array<float, 2> headLength { 20000, 20000 };   // latched at each head's wrap
    dsp::Smoothed feedback, mix, damping, diffusion, spread, reverseBlend, descentBlend;
    float descentRatio = 1;
};

//==============================================================================
// VI. THE CRYPT: 8-line Householder FDN with input diffusion, early reflections,
// modulated lines, in-loop damping and shimmer, four characters that differ in
// structure, Rising (reverse-swell) mode and Eternal hold.
// DUNGEON CRAWLER changes (audit): line-length changes crossfade between two taps of
// the same line instead of gliding the length (no pitch warble in the tail), and
// chamber changes crossfade the early-reflection patterns, fade diffuser stages in and
// out and smooth the modulation depth and body EQ (no clicks while travelling).
class Crypt {
public:
    void prepare(double sampleRate);
    void reset();
    void process(float* l, float* r, int n, const Frame&, const Context&) noexcept;
    bool ringing() const noexcept { return held > 1.0e-3f; }   // a tail still sounds
    float level() const noexcept { return held; }
    // Wet only: the output is the reverb's return alone (the engine runs several rooms
    // in parallel while travelling and adds the dry signal itself).
    void setWetOnly(bool on) noexcept { wetOnly = on; }
private:
    float held = 0;   // the return level, held while the tail decays
    bool wetOnly = false;
    static constexpr int lines = 8;
    double sr = 48000;
    std::array<dsp::DelayLine, lines> delay;
    std::array<float, lines> lengthA{}, lengthB{}, targetLength{}, gain{}, gainTarget{}, lowpass{}, lfoPhase{}, lfoRate{};
    float tapFade = 1, fadeStep = 0.0003f;   // 0 = tap A, 1 = tap B
    std::array<dsp::DelayLine, 8> diffusers;
    std::array<float, 8> diffuserLength{}, diffuserWeight{};
    dsp::DelayLine preDelayL, preDelayR, early;
    std::array<dsp::Biquad, 2> inputTone;
    std::array<float, 2> lowCut{};
    dsp::GranularShifter shimmerShifter;
    dsp::Smoothed mix, width, shimmer, eternal, inputGain, dampingHz, preDelaySamples, modulation, bodyDb, reflectionScale;
    std::array<dsp::DelayLine, 2> rising;   // reverse-swell buffers of the wet signal
    double risePhase = 0;
    float riseLength = 48000;
    dsp::Smoothed risingBlend;
    int character = 1, previousCharacter = 1, diffuserCount = 6, counter = 0;
    float erFade = 1;   // 0 = previous chamber's reflections, 1 = current
    float injection = 0.3f, injectionTarget = 0.3f, gainGlide = 0.001f;
};
}
