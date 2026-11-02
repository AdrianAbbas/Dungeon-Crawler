#pragma once
// DUNGEON CRAWLER's instrument engine (Direction Update 3): one instrument at a time,
// played from MIDI, every sub-location its own instrument.
//
// The sound models are WORLDBUILDER's (Source/Instrument/wb, copied unchanged): the
// analog / brass / reed synth voice, the plucked waveguide string, the struck modal
// object (bells, bars, gongs), membranes and timpani, FM, the tine, the formant choir,
// the wind voice, the bowed resonator and the bowed string, plus its sample bank (the
// developer's recordings). Added here: a pipe-organ voice (additive ranks), a sample
// player for the bank (adapted from WORLDBUILDER's SamplePart), and the playing layer
// of a normal software instrument: a voice pool, velocity, sustain pedal, pitch bend,
// mod wheel, click-free voice stealing.
//
// Every voice remembers the instrument it was started on, so changing instrument (a new
// place, a preset) never cuts or retunes a note that is held: it rings on as it was, and
// the next note plays the new instrument. Nothing allocates after prepare().
#include "SampleBank.h"
#include "Captures.h"
#include "wb/Instruments.h"
#include <array>
#include <memory>
#include <atomic>
#include <cstdint>

namespace dc::inst {
namespace d = wb::dsp;

inline constexpr int instrumentCount = 40;   // one per place (ten locations, their scenes and sub-locations)
inline constexpr int maxVoices = 24;         // slots (some fading out after a steal)
inline constexpr int macroCount = 5;         // Tone, Attack, Release, Body, the signature

enum class Model : uint8_t { None, Synth, Pluck, Bell, Drum, Fm, Tine, Choir, Wind, Bowed, Strings, Organ, Sample, Vocal };
// What the mod wheel does on an instrument.
enum class ModWheel : uint8_t { Vibrato, Brightness, Tremolo, Growl, Swell };

// A pipe organ (or chamber organ, or harmonium-like reed rank): up to eight ranks of
// pipes, each a sine at its footage's ratio with a little of its own detune, the flue's
// "chiff" (a noise burst at the second harmonic as the pipe speaks), wind noise, a soft
// attack as the pallet opens and a short release. Reed ranks add odd harmonics.
struct OrganVoice {
    static constexpr int maxRanks = 8;
    std::array<float, maxRanks> ratio {}, level {}, phase {}, inc {}, cents {};
    int ranks = 0;
    d::Env env;
    d::Svf chiffBand, windBand;
    float hz = 220, bend = 0, chiff = 0.3f, wind = 0.02f, reed = 0, age = 0, rate = 48000, gain = 1, tremolo = 0, tremPhase = 0;
    uint64_t noiseIndex = 0;
    bool active() const { return env.active(); }
    void reset() {
        env.stop();
        chiffBand.reset();
        windBand.reset();
        phase.fill(0);
        age = 0;
    }
    void addRank(float r, float lvl, float detuneCents = 0) {
        if (ranks >= maxRanks) return;
        ratio[std::size_t(ranks)] = r;
        level[std::size_t(ranks)] = lvl;
        cents[std::size_t(ranks)] = detuneCents;
        ++ranks;
    }
    void start(float noteHz, float attack, float sampleRate, uint64_t noiseAt) {
        rate = sampleRate;
        hz = noteHz;
        age = 0;
        noiseIndex = noiseAt;
        for (int k = 0; k < ranks; ++k) phase[std::size_t(k)] = 0.17f * float(k);
        env.trigger(attack, 1.0f, 1.0f, rate);
    }
    void release(float seconds) { env.release(seconds, rate); }
    void update() {
        const float chunkSeconds = float(d::chunk) / rate;
        age += chunkSeconds;
        const float f = hz * std::exp2(bend / 12.0f);
        for (int k = 0; k < ranks; ++k) {
            const float rf = f * ratio[std::size_t(k)] * std::exp2(cents[std::size_t(k)] / 1200.0f);
            inc[std::size_t(k)] = rf < 0.45f * rate ? rf / rate : 0.0f;
        }
        chiffBand.set(std::min(f * 2.0f, 0.4f * rate), 3.0f, rate);
        windBand.set(std::min(f * 4.0f, 0.4f * rate), 0.8f, rate);
        tremPhase += 5.6f * chunkSeconds;
        tremPhase -= std::floor(tremPhase);
    }
    float tick(const d::Stream& noise) {
        const float e = env.tick();
        if (e <= 0.0f) return 0.0f;
        float y = 0;
        for (int k = 0; k < ranks; ++k) {
            auto& p = phase[std::size_t(k)];
            const float s = d::sinCycles(p);
            // A reed rank: the sine pushed towards a clipped square (odd harmonics).
            y += level[std::size_t(k)] * (reed > 0.0f ? d::softClip(s * (1.0f + 4.0f * reed)) / (1.0f + 0.6f * reed) : s);
            p += inc[std::size_t(k)];
            if (p >= 1.0f) p -= 1.0f;
        }
        const float n = noise.white(noiseIndex++);
        const float speak = age < 0.15f ? std::exp(-age / 0.03f) : 0.0f;
        y += chiffBand.bandpass(n) * chiff * speak + windBand.bandpass(n) * wind;
        const float trem = tremolo > 0.0f ? 1.0f - tremolo * (0.5f + 0.5f * d::sinCycles(tremPhase)) : 1.0f;
        return y * e * gain * trem;
    }
};

// Plays one recording of the sample bank: the zone for the key and velocity, pitched
// from its detected root, looping sustained recordings and crossing into the recorded
// release tail when the key lets go (WORLDBUILDER's SamplePart voice, without patterns).
struct SampleVoice {
    const wb::samples::Zone* zone = nullptr;
    const wb::samples::Zone* courseZone = nullptr;
    double pos = 0, coursePos = 0, tailPos = -1;
    float rateRatio = 1, inc = 1, courseRatio = 1, courseLevel = 0;
    float midi = 60, amp = 1, attackStep = 1, fade = 1, fadeMul = 1, releaseMul = 1;
    float tailMix = 0, tailStep = 0, gain = 1, bend = 0, cents = 0, vibratoDepth = 0, vibratoHz = 5, vibratoPhase = 0, age = 0, rate = 48000;
    bool on = false, releasing = false, recordedTail = true;
    // Set before start() (Session 8): a one-shot velocity layer glides from the layer below's
    // level up to its own across its velocity range, so the level never jumps at a layer
    // boundary (each layer keeps its own tone).
    bool smoothLayers = false;
    bool active() const { return on; }
    void reset() { on = false; zone = courseZone = nullptr; }
    // skipSeconds: start that far into the recording; decaySeconds > 0: die away faster.
    // lookupKey >= 0: the zone is chosen for that key (not folded by octaves) and pitched
    // from there to noteMidi (the voice's top and bottom shifted instead of folded).
    bool start(const wb::samples::Instrument& inst, float noteMidi, float velocity, float pick, float sampleRate, float attackSeconds,
               float decaySeconds, float skipSeconds, float even, float courseCents, float courseLvl, int lookupKey = -1);
    void release(float seconds) {
        if (!on || releasing) return;
        releasing = true;
        if (recordedTail && zone->release > 0 && pos < double(zone->release)) {
            tailPos = double(zone->release);
            tailMix = 0;
            tailStep = 1.0f / (0.04f * rate);
            releaseMul = std::exp(-6.9078f / (std::max(seconds * 2.0f, 0.05f) * rate));
        } else {
            releaseMul = std::exp(-6.9078f / (std::max(seconds, 0.01f) * rate));
        }
    }
    void update() {
        age += float(d::chunk) / rate;
        float semis = midi - zone->root + bend + cents / 100.0f;
        if (vibratoDepth > 0.0f) {
            vibratoPhase += vibratoHz * float(d::chunk) / rate;
            vibratoPhase -= std::floor(vibratoPhase);
            semis += vibratoDepth * d::smoothstep(0.15f, 0.6f, age) * d::sinCycles(vibratoPhase);
        }
        inc = rateRatio * std::exp2(semis / 12.0f);
    }
    float tick();
};

// The developer's voice (Session 8): a sung sampler. Every singer reads the recordings
// (SampleVoice readers): the soft and the full take crossfaded by velocity (no step as the
// velocity rises), two vowels morphed while the note sounds (`morph` 0 = the first), the
// sustain looped and the recorded release tail on note off. An ensemble: one voice up to
// a full choir, each singer another take of the choir stacks where there are several,
// detuned, drifting slowly, entering a little late, spread across the stereo field. Keys
// beyond the recorded range are not folded by octaves: the nearest note is shifted (its
// brightness tamed as it rises), and the instrument blends its synthesis in up there.
struct VocalVoice {
    static constexpr int maxSingers = 6;
    static constexpr int maxReaders = maxSingers * 4 + 1;   // singers x vowels x (soft, full), + grit
    struct Source { const wb::samples::Instrument* lead = nullptr; const wb::samples::Instrument* ensemble = nullptr; };
    struct Reader {
        SampleVoice s;
        float weight = 0, l = 0.7071f, r = 0.7071f, baseCents = 0, drift = 0, driftPhase = 0, driftHz = 0.2f;
        int vowel = 0, delay = 0;
        bool used = false;
    };
    // Set before start().
    std::array<Source, 2> vowel {};
    const wb::samples::Instrument* grit = nullptr;   // a rough voice under the note (its pieces are takes)
    float gritLevel = 0;
    int singers = 1;
    float detuneCents = 8, timingMs = 30, spread = 0.7f, driftCents = 4, morph = 0, morphTarget = 0, morphGlide = 0.002f;
    // Read after start(): how far the note is shifted beyond the recordings (semitones,
    // > 0 above the highest recorded note), for the instrument's synthesis blend.
    float shiftAbove = 0;
    std::array<Reader, maxReaders> reader {};
    int readers = 0;
    d::Svf tame;
    float tameHz = 0, rate = 48000;
    bool active() const {
        for (int k = 0; k < readers; ++k)
            if (reader[std::size_t(k)].used && (reader[std::size_t(k)].s.active() || reader[std::size_t(k)].delay > 0)) return true;
        return false;
    }
    void reset() {
        for (auto& rd : reader) rd.s.reset(), rd.used = false, rd.delay = 0;
        readers = 0;
        tame.reset();
        vowel = {};
        grit = nullptr;
        gritLevel = 0;
        singers = 1;
        morph = morphTarget = 0;
        shiftAbove = 0;
    }
    bool start(float noteMidi, float velocity, float random, float sampleRate, float attack, float skip, uint64_t seed);
    void release(float seconds) {
        for (int k = 0; k < readers; ++k) reader[std::size_t(k)].s.release(seconds);
    }
    void update(float bendSemis, float chunkSeconds) {
        for (int k = 0; k < readers; ++k) {
            auto& rd = reader[std::size_t(k)];
            if (!rd.used || !rd.s.active()) continue;
            rd.driftPhase += rd.driftHz * chunkSeconds;
            rd.driftPhase -= std::floor(rd.driftPhase);
            rd.s.bend = bendSemis;
            rd.s.cents = rd.baseCents + rd.drift * d::sinCycles(rd.driftPhase);
            rd.s.update();
        }
    }
    void tick(float& l, float& r) {
        morph += morphGlide * (morphTarget - morph);
        const float w[2] = { 1.0f - morph, morph };
        float sl = 0, sr = 0;
        for (int k = 0; k < readers; ++k) {
            auto& rd = reader[std::size_t(k)];
            if (!rd.used) continue;
            if (rd.delay > 0) {
                --rd.delay;
                continue;
            }
            if (!rd.s.active()) continue;
            const float y = rd.s.tick() * rd.weight * (rd.vowel < 2 ? w[rd.vowel] : 1.0f);
            sl += y * rd.l;
            sr += y * rd.r;
        }
        if (tameHz > 0.0f) {
            // One low-pass on the mid keeps the image (as the post does).
            const float m = tame.tick(0.5f * (sl + sr)).lp, side = 0.5f * (sl - sr);
            sl = m + side;
            sr = m - side;
        }
        l += sl;
        r += sr;
    }
};

// One sound source of a voice: whichever model the instrument chose for it.
struct Layer {
    Model model = Model::None;
    float gain = 1, pan = 0;          // pan -1..1
    float hz = 220;                   // the pitch the layer was started at (before the wheel)
    float cents = 0;                  // a detune the instrument moves while it sounds (drift)
    d::SynthVoice synth;
    d::Pluck pluck;
    d::Bell bell;
    d::Drum drum;
    d::FmVoice fm;
    d::TineVoice tine;
    d::ChoirVoice choir;
    d::WindVoice wind;
    d::BowedVoice bowed;
    d::StringVoice strings;
    OrganVoice organ;
    SampleVoice sample;
    VocalVoice vocal;
    d::PanGains panGains { 0.7071f, 0.7071f };
    float bellBent = 0;
    void prepare(float rate);
    void reset();
    // Chooses the model for a new note (the others stay silent).
    Layer& use(Model m, float layerGain = 1, float layerPan = 0) {
        model = m;
        gain = layerGain;
        pan = layerPan;
        panGains = d::panGains(std::clamp(layerPan, -1.0f, 1.0f));
        return *this;
    }
    bool active() const;
    void release(float seconds, float rate);
    void update(float rate, float bendSemis, float vibrato);
    void tick(const d::Stream& noise, float& l, float& r);
};

// The instrument's character after its sources, per voice: tone (low-pass), a high-pass,
// up to three body resonances, drive.
struct BodyPeak { float hz = 0, q = 1, gain = 0; };
struct Post {
    float toneHz = 18000, toneQ = 0.7f, highPassHz = 25, drive = 0, velocityTone = 0.8f;
    std::array<BodyPeak, 3> body {};
};

struct Voice;
struct Note {
    int key = 60;              // MIDI note (after the instrument's transposition)
    float hz = 261.63f;        // its frequency at concert pitch
    float velocity = 0.8f;     // 0..1
    float random = 0.5f;       // 0..1, per note (round robins, small variations), deterministic
    // The macros (0.5 = as designed): tone, attack (strike), release (ring), body, signature.
    std::array<float, macroCount> macro { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    float rate = 48000;
    double bpm = 120;
    uint64_t noiseAt = 0;
    // Macro helpers: a time scale x1/4 .. x4, a -1..1 offset.
    float scale(int m) const { return std::exp2((macro[std::size_t(m)] - 0.5f) * 4.0f); }
    float offset(int m) const { return (macro[std::size_t(m)] - 0.5f) * 2.0f; }
};

struct Design {
    const char* name;                         // the instrument ("Contrabass Pizzicato")
    std::array<const char*, macroCount> macros;   // its macros' labels
    const char* modLabel;                     // what the mod wheel does, shown in the panel
    ModWheel wheel;
    int polyphony;
    float releaseSeconds;                     // a key let go (before the Release macro)
    float transpose;                          // semitones the instrument sounds from the key
    bool sustains;                            // held notes sustain (else they ring out)
    Post post;
    void (*start)(Voice&, const Note&);       // sets up the layers for a note
    void (*control)(Voice&, const Note&, float chunkSeconds);   // optional, once per chunk while the voice sounds
};
const Design& design(int index) noexcept;
// The instrument of a place (location, sub-location 0..4) and back.
int designFor(int location, int sub) noexcept;
int locationOf(int design) noexcept;
int subOf(int design) noexcept;
// The measured level trims (dB, Trims.h) that bring every instrument to the same loudness.
float designTrimDb(int index) noexcept;
// The same for an instrument that plays its synthesised stand-in (a build without the
// recordings): only the sampled instruments differ.
float standInTrimDb(int index) noexcept;
bool usesRecordings(int index) noexcept;
// The instrument's wet correction in a place (dB, Trims.h), measured with the place's own
// instrument: the instrument in its place as loud as it is dry.
float placeTrimDb(int location, int sub) noexcept;
// Which of the developer's effect captures an instrument sends part of itself into, and
// how much (Captures.h). Most send nothing.
struct CaptureSend { int capture = CaptureNone; float send = 0; };
CaptureSend captureOf(int index) noexcept;

struct Voice {
    std::array<Layer, 4> layer;
    int designIndex = 0, key = -1, order = 0;
    bool on = false, keyDown = false, sustained = false, releasing = false, killing = false;
    float velocity = 0.8f, gain = 1, killGain = 1, killStep = 0, age = 0;
    Note note {};
    // Post (filled from the design at the start, the Tone and Body macros applied).
    d::Svf tone;
    std::array<d::Svf, 2> highPass;
    std::array<d::Svf, 3> body;
    std::array<float, 3> bodyGain {};
    float drive = 0, baseDrive = 0, toneHz = 18000, toneQ = 0.7f;
    int capture = CaptureNone;
    float captureSend = 0;
    d::Stream noise;
    // Free for the instrument's own control function (arpeggios, wobble, bubbles...).
    std::array<float, 8> state {};
    int step = 0;
    float tremPhase = 0, wheelLevel = 1;
    bool active() const { return on; }
};

struct Performance {
    float bend = 0;          // -1..1 (the wheel)
    float mod = 0;           // 0..1
    bool sustain = false;
};

class Synth {
public:
    void prepare(double sampleRate);
    void reset();
    // Which instrument new notes play (voices already sounding keep their own).
    void setDesign(int index) noexcept;
    int currentDesign() const noexcept { return designNow; }
    void setMacros(const std::array<float, macroCount>& m) noexcept { macros = m; }
    void setBendRange(float semitones) noexcept { bendRange = semitones; }
    void setTempo(double bpm) noexcept { tempo = bpm; }
    void setLevel(float gain) noexcept { level = gain; }

    // MIDI, at a sample position within the block being rendered.
    void noteOn(int key, float velocity) noexcept;
    void noteOff(int key) noexcept;
    void setSustain(bool down) noexcept;
    void setPitchBend(float bipolar) noexcept { perf.bend = std::clamp(bipolar, -1.0f, 1.0f); }
    void setModWheel(float unit) noexcept { perf.mod = std::clamp(unit, 0.0f, 1.0f); }
    void allNotesOff() noexcept;   // releases everything (pedal too)
    void allSoundOff() noexcept;   // a quick fade on everything

    // Adds n samples to l and r.
    void render(float* l, float* r, int n) noexcept;

    int activeVoices() const noexcept;
    const Performance& performance() const noexcept { return perf; }
    // Whether the effect captures were found (tests, the panel).
    bool capturesReady(int capture) const noexcept { return captures.available(capture); }
    // For the panel: which keys sound (1 bit per key; written on the audio thread).
    bool keyLit(int key) const noexcept;

private:
    int pickVoice() noexcept;
    void kill(Voice&) noexcept;
    void releaseVoice(Voice&) noexcept;
    void controlChunk() noexcept;

    void renderBlock(float* l, float* r, int n) noexcept;
    // The voices live on the heap (about 2.3 MB: four layers of every model each), made
    // once with the engine, never on the audio thread: a processor made on a stack (a host
    // or a test) stays small.
    std::unique_ptr<std::array<Voice, maxVoices>> voiceStore = std::make_unique<std::array<Voice, maxVoices>>();
    std::array<Voice, maxVoices>& voices = *voiceStore;
    CaptureBus captures;
    std::array<float, macroCount> macros { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    Performance perf;
    float rate = 48000, bendRange = 2, level = 1, bendNow = 0, modNow = 0;
    double tempo = 120;
    int designNow = 0, counter = 0, orderCounter = 0;
    uint64_t noteCount = 0;
    std::array<std::atomic<uint64_t>, 2> lit {};
};
}
