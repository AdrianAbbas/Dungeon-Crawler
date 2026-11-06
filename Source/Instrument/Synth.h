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

enum class Model : uint8_t { None, Synth, Pluck, Bell, Drum, Fm, Tine, Choir, Wind, Bowed, Strings, Organ, Sample, Vocal, Brass, Woodwind };
// What the mod wheel does on an instrument.
// Session 17: Expression (the recorded woodwinds, sound 15): the wheel swells the note (a
// little louder, brighter, and on the flutes into their vibrato); at rest the note as played.
enum class ModWheel : uint8_t { Vibrato, Brightness, Tremolo, Growl, Swell, Expression };

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
    // Session 13 (the brass only, so the other recordings play exactly as before): crossing
    // into the release tail lands on the tail's next sample (the original skipped one).
    bool exactTail = false;
    // Set before start() (Session 8): a one-shot velocity layer glides from the layer below's
    // level up to its own across its velocity range, so the level never jumps at a layer
    // boundary (each layer keeps its own tone).
    bool smoothLayers = false;
    // How far a take is evened towards its layer's median at most (dB; Session 17: the
    // woodwinds 24: a flute's lowest notes are 20-30 dB softer than its top).
    float evenLimitDb = 12;
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

// Session 13: the recorded brass (horn, trumpet, tenor trombone, tuba: VSCO 2 Community
// Edition, CC0; tools/sample-prep/brass.py), played as DUNGEON CRAWLER's own section. Up to
// three players (each an instrument, an interval, a detune, a pan, a late entry); each
// plays its staccato take as the hit and its sustain under it (the sustain dipped under
// the hit, rising into the held note over ~80 ms), the dynamic nearest the velocity, its
// level on a smooth velocity curve (each take evened towards its layer's loudness), a
// looped sustain and its recorded release. On top, the lips pushed on the attack (`bite`: a soft clip driven
// harder for the first ~70 ms, the extra level kept: grit and density on the hit, the held
// note clean). Keys beyond the recordings are pitched from the nearest note; `beyond`
// tells the instrument how far, so it can hand over to its synthesis.
struct BrassVoice {
    static constexpr int maxPlayers = 3;
    static constexpr int maxReaders = maxPlayers * 2;   // per player: its sustain and its staccato
    struct Player {
        const wb::samples::Instrument* sus = nullptr;
        const wb::samples::Instrument* stac = nullptr;
        float semis = 0, cents = 0, gain = 1, pan = 0, delayMs = 0;
        // A doubling player plays another recording: the note `lookup` semitones away,
        // transposed to its pitch (the sustains have one take per note, and the same take
        // doubled a few ms late would comb-filter).
        float lookup = 0;
    };
    struct Reader {
        SampleVoice s;
        float weight = 0, l = 0.7071f, r = 0.7071f, duck = 0, duckStep = 0, crestDb = 12;
        int delay = 0;
        bool used = false;
    };
    // Set before start().
    std::array<Player, maxPlayers> player {};
    int players = 0;
    float stacLevel = 1;      // the staccato hit (times its velocity curve)
    float susDuck = 0.5f;     // how far the sustain dips under the hit (0 none)
    float bite = 0, biteSeconds = 0.07f;
    // The section's presence: a band at `presenceHz` (Q 0.8) added to the held note
    // (`presence`) and more on the hit (`hitPresence`, fading over ~120 ms), so the recorded
    // brass speaks through a mix (the recordings are darker than the synthesis was); a held
    // `presence` below 0 rounds the held note instead (the bright trumpets), the hit still lifted.
    float presence = 0, hitPresence = 0, presenceHz = 2900, hitThin = 0;
    // The staccato's level against the sustain at the same velocity (dB), and how fast it
    // dies away under the held note (60 dB over `stacDecaySeconds`: only its transient counts).
    float stacLiftDb = 2, stacDecaySeconds = 0.5f;
    float thinHz = 320;
    // The brass's own brightness (the recordings have little above 2 kHz; a horn played
    // hard does): the 0.9-2 kHz band full-wave rectified (an octave up: 2-4 kHz harmonics of
    // what is there), band-limited and added, `excite` always and `hitExcite` more on the hit.
    float excite = 0, hitExcite = 0;
    // Read after start(): semitones the first player's note lies outside its recordings.
    float beyond = 0;
    std::array<Reader, maxReaders> reader {};
    int readers = 0;
    float biteNow = 0, biteMul = 1, rate = 48000;
    float hitEnv = 0, hitMul = 1, driveRef = 0.1f;
    d::Svf presL, presR, thinL, thinR;
    std::array<d::Svf, 2> exIn {}, exOut {}, exTop {};
    bool active() const {
        for (int k = 0; k < readers; ++k)
            if (reader[std::size_t(k)].used && (reader[std::size_t(k)].s.active() || reader[std::size_t(k)].delay > 0)) return true;
        return false;
    }
    void reset() {
        for (auto& rd : reader) rd.s.reset(), rd.used = false, rd.delay = 0;
        readers = 0;
        players = 0;
        player = {};
        stacLevel = 1;
        susDuck = 0.5f;
        bite = 0;
        biteSeconds = 0.07f;
        presence = hitPresence = hitThin = 0;
        presenceHz = 2900;
        stacLiftDb = 2;
        stacDecaySeconds = 0.5f;
        thinHz = 320;
        excite = hitExcite = 0;
        for (auto* f : { &exIn, &exOut, &exTop })
            for (auto& x : *f) x.reset();
        beyond = 0;
        biteNow = hitEnv = 0;
        presL.reset();
        presR.reset();
        thinL.reset();
        thinR.reset();
    }
    // Adds a player (before start()); false when full or the instrument is missing.
    bool addPlayer(const wb::samples::Instrument* sus, const wb::samples::Instrument* stac, float semis, float cents, float gain, float pan, float delayMs = 0, float lookup = 0) {
        if (players >= maxPlayers || (sus == nullptr && stac == nullptr)) return false;
        player[std::size_t(players++)] = { sus, stac, semis, cents, gain, pan, delayMs, lookup };
        return true;
    }
    bool start(float noteMidi, float velocity, float pick, float sampleRate, float attackSeconds);
    void release(float seconds) {
        for (int k = 0; k < readers; ++k) reader[std::size_t(k)].s.release(seconds);
    }
    void update(float bendSemis) {
        for (int k = 0; k < readers; ++k) {
            auto& rd = reader[std::size_t(k)];
            if (!rd.used || !rd.s.active()) continue;
            rd.s.bend = bendSemis;
            rd.s.update();
        }
    }
    void tick(float& l, float& r);
};

// Session 17: the recorded woodwinds (flute, piccolo, oboe, clarinet, bassoon: VSCO 2
// Community Edition, CC0; tools/sample-prep/woodwinds.py), played as DUNGEON CRAWLER's own
// winds. Up to three players (each an instrument, an interval, a detune, a pan, a late entry,
// the sounding range it plays, a slow wander in pitch). Each plays its sustain (looped, its
// recorded release on note off) and, where the library has one, its vibrato sustain beside it
// (crossfaded at constant power: `vibBase`, and the wheel towards the vibrato); its staccato
// as a tongued onset (more the harder the key is struck, dying under the held note; none on a
// slurred note). The dynamic nearest the velocity, levelled onto a smooth velocity curve (as
// the brass). Then the winds' own character: the breath (noise in a band riding the note's
// own level, a puff on the attack), a woody body, a low cut (a small instrument), a darkening
// low-pass (lower played softly; the wheel opens it), the wheel's swell. A slurred note
// (`legato`) starts past its attack, fading in over `slurSeconds`, its pitch gliding from the
// note before.
struct WoodwindVoice {
    static constexpr int maxPlayers = 3;
    static constexpr int maxReaders = maxPlayers * 3;   // per player: sustain, vibrato sustain, staccato
    struct Player {
        const wb::samples::Instrument* sus = nullptr;
        const wb::samples::Instrument* vib = nullptr;
        const wb::samples::Instrument* stac = nullptr;
        float semis = 0, cents = 0, gain = 1, pan = 0, delayMs = 0;
        // A doubling player plays another recording (the nearest other note, `lookup`'s side
        // first), transposed: one take doubled a few ms late would comb-filter.
        float lookup = 0;
        // The sounding range it plays (MIDI), fading out over `fadeSemis` beyond it.
        float lowMidi = -1000, highMidi = 1000, fadeSemis = 3;
        // A slow wander in pitch (cents, peak) at about `wanderHz`.
        float wanderCents = 0, wanderHz = 0.17f;
    };
    struct Reader {
        SampleVoice s;
        float weight = 0, l = 0.7071f, r = 0.7071f, duck = 0, duckStep = 0;
        float baseCents = 0, wander = 0, wanderPhase = 0, wanderHz = 0.17f;
        int kind = 0, delay = 0;   // 0 sustain, 1 vibrato sustain, 2 staccato
        bool used = false, paired = false;   // paired: a sustain sharing with its vibrato take
    };
    // Set before start().
    std::array<Player, maxPlayers> player {};
    int players = 0;
    float stacLevel = 0.5f;          // the tongued onset (times its velocity curve)
    float stacLiftDb = 0;            // the staccato against the sustain at the same velocity
    float stacDecaySeconds = 0.3f;   // 60 dB under the held note over this
    float susDuck = 0.35f;           // how far the sustain dips under the tongued onset
    float vibBase = 0, vibWheel = 0; // the vibrato takes' share at rest, and how far the wheel moves it
    float pitchVibrato = 0;          // semitones of vibrato the wheel adds (sets without a vibrato take)
    float swellDb = 3, swellOpen = 1;   // the wheel at full: +dB, and octaves the low-pass opens
    float swellPresence = 0, presenceHz = 2500;   // and a presence band it lifts (0 none)
    float breath = 0, breathHz = 2400, breathQ = 0.8f, puff = 0;
    float darkHz = 0, darkSoft = 0, darkQ = 0.6f;   // a low-pass (0 none), `darkSoft` octaves lower at velocity 0
    float lowCutHz = 0;
    float woodHz = 0, woodQ = 1.3f, woodGain = 0;
    float curveFloor = 0.2f;         // the velocity curve's floor (0.2: about 14 dB from soft to loud)
    bool legato = false;
    float glideSemis = 0, glideSeconds = 0.05f, slurSeconds = 0.025f, slurSkipSeconds = 1.0f;   // the skip lands at most at the loop's start
    // Read after start(): semitones the note lies outside every player's recordings.
    float beyond = 0;
    std::array<Reader, maxReaders> reader {};
    int readers = 0;
    float rate = 48000, wheel = 0, glide = 0, glideMul = 1, darkNow = 0, velocity = 0.8f;
    float levelNow = 1, levelStep = 0, follow = 0, followUp = 0, followDown = 0, puffEnv = 0, puffMul = 1;
    float susW = 1, vibW = 0;
    float slurFade = 0, slurPhase = 0, slurStep = 0;   // slurring out: the fade's position
    uint32_t noiseState = 0x9E3779B9u;
    d::Svf darkL, darkR, lowL, lowR, lowL2, lowR2, wood, breathBand, presL, presR;   // the low cut: two stages (24 dB per octave)
    bool active() const {
        for (int k = 0; k < readers; ++k)
            if (reader[std::size_t(k)].used && (reader[std::size_t(k)].s.active() || reader[std::size_t(k)].delay > 0)) return true;
        return false;
    }
    void reset() {
        for (auto& rd : reader) rd.s.reset(), rd.used = false, rd.delay = 0;
        readers = 0;
        players = 0;
        player = {};
        stacLevel = 0.5f;
        stacLiftDb = 0;
        stacDecaySeconds = 0.3f;
        susDuck = 0.35f;
        vibBase = vibWheel = pitchVibrato = 0;
        swellDb = 3;
        swellOpen = 1;
        breath = puff = 0;
        breathHz = 2400;
        breathQ = 0.8f;
        darkHz = darkSoft = 0;
        darkQ = 0.6f;
        lowCutHz = 0;
        woodHz = woodGain = 0;
        woodQ = 1.3f;
        curveFloor = 0.2f;
        legato = false;
        glideSemis = glide = 0;
        glideSeconds = 0.05f;
        slurSeconds = 0.025f;
        slurSkipSeconds = 1.0f;
        beyond = 0;
        wheel = 0;
        follow = puffEnv = 0;
        slurFade = slurPhase = slurStep = 0;
        levelNow = 1;
        levelStep = 0;
        swellPresence = 0;
        presenceHz = 2500;
        for (auto* f : { &darkL, &darkR, &lowL, &lowR, &lowL2, &lowR2, &wood, &breathBand, &presL, &presR }) f->reset();
    }
    // Adds a player (before start()); false when full or the instrument is missing.
    bool addPlayer(const Player& p) {
        if (players >= maxPlayers || (p.sus == nullptr && p.stac == nullptr)) return false;
        player[std::size_t(players++)] = p;
        return true;
    }
    bool start(float noteMidi, float velocity, float pick, float sampleRate, float attackSeconds, uint64_t seed);
    void release(float seconds) {
        for (int k = 0; k < readers; ++k) reader[std::size_t(k)].s.release(seconds);
    }
    // Slurred into the next note: the held tone fades under it along a quarter cosine (an
    // exponential release falls 13 dB in its first 10 ms: a dip against the next note's
    // rise), no recorded release (the breath goes on into the next note); silent, it stops.
    void slurOut(float seconds) {
        slurFade = 1;
        slurPhase = 0;
        slurStep = 1.0f / std::max(1.0f, seconds * rate);
    }
    // The wheel (0..1, Expression) and the bend, once per chunk.
    void update(float bendSemis, float wheelNow, float chunkSeconds);
    void tick(float& l, float& r);
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
    BrassVoice brass;
    WoodwindVoice woodwind;
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
    // The voices' Vowel and Ensemble (Session 9; 0.5 = as designed, only the sung layers).
    float vowel = 0.5f, ensemble = 0.5f;
    bool classic = false;      // Session 11: the sound before it (older sessions)
    int sound = 12;            // Session 12: which generation of the sound (see Sound)
    // Session 17: a slurred note (a woodwind at sound 15 starting while its previous note is
    // still held): it starts past its attack, gliding from `legatoFromMidi`.
    bool legato = false;
    float legatoFromMidi = 60;
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
// Whether the instrument sings with the recorded voice (its panel shows Vowel and Ensemble).
bool designUsesVoice(int index) noexcept;
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
// Session 11: how an instrument sits in a song. The instruments mostly played in a mix
// (brass, strings, drums, plucks) send less to their place's reverbs and echoes and the
// reverb starts a little later (the dry hit first); the MIX sliders scale on top. 1, 1, 0
// for the others. Session 12: the brass's reverb also dips under its hits (`duck`).
// Which generation of the instrument's sound a note plays (saved with the session as
// "sound"): 10 before Session 11 (no fit, no designed Punch, the old brass); 11 Session 11's
// mix defaults, brass and Punch; 12 Session 12's horns (a harder, brighter, faster hit, more
// Punch, a drier attack). A session keeps the sound it was saved with; new instances, Init
// and the factory presets play the latest.
// 13 (Session 13): the brass plays the recorded horn, trumpet, trombone and tuba (when the
// samples are in the build; their synthesis otherwise), everything else as in 12.
// 14 (Session 14): the brass sits in the world (about 2.5 dB lower, a little less Punch, the
// place's space on its sustain, tape warmth, a soft top, a little width, darker played
// softly) and the War Drums about 2.5 dB lower; everything else as in 13.
// 15 (Session 17): the woodwinds (the Village Green's whistle, the Forest Edge's wooden flute,
// the Foothills' shepherd's flute, the Bog's low reed) play the recorded flute, piccolo, oboe,
// clarinet and bassoon (when the samples are in the build; their synthesis otherwise), the mod
// wheel their Expression, overlapping notes slurred; everything else as in 14.
enum Sound : int { SoundClassic = 10, SoundMix = 11, SoundHorns = 12, SoundRealBrass = 13, SoundSeated = 14, SoundWoodwinds = 15, SoundLatest = SoundWoodwinds };
// The instrument's reverb (and its pre-delay and duck under the hit) in a song.
// `makeupDb`: the level the drier, ducked space takes away from the instrument in its place,
// given back (measured: the same phrase in its place as loud, against its dry self, as in
// Session 11); it follows the Effects roller, so the dry instrument is unchanged.
struct MixFit { float reverb = 1, echoes = 1, preDelayMs = 0, duck = 0, makeupDb = 0; };
MixFit mixFit(int index, int sound = SoundLatest) noexcept;
// The instrument's own Punch (dB on the hit at Punch 100 %): high on the brass and the
// drums, a little on the plucks, none on pads, choirs and organs.
float designPunchDb(int index, int sound = SoundLatest) noexcept;
// The most Punch can add (Punch 200 %): 12 dB, 18 dB on the brass from sound 12 (the Doom
// Brass 21).
float punchMaxDb(int index, int sound = SoundLatest) noexcept;
// Punch (0..2: 0 none, 1 the instrument's own, 2 the most) to the hit's boost in dB.
float punchDb(int index, float punch, int sound = SoundLatest) noexcept;
// The brass's presence (2-4 kHz) and steeper velocity response (Session 11).
bool brassPresence(int index, int sound = SoundLatest) noexcept;
// Session 13: whether the instrument plays the recorded brass (sound 13 and the samples in
// the build), and the level its recorded section is set to (dB, measured: the brass as
// loud as the synthesis it replaces, the hit sharper).
bool designUsesRealBrass(int index, int sound = SoundLatest) noexcept;
float realBrassTrimDb(int index) noexcept;
// Calibration only (tests: `--realbrass-calibrate`): replaces an instrument's trim; NaN
// restores the measured one.
void overrideRealBrassTrimDb(int index, float db) noexcept;
// Session 12: the brass's attack edge (an upper-mid lift on the first ~100 ms only), 0 for
// the others; and how strongly velocity scales the hit (the Punch's share of velocity).
float attackEdge(int index, int sound = SoundLatest) noexcept;
float punchVelocityScale(int index, float velocity, int sound = SoundLatest) noexcept;
// Session 12: the hit clipper's curve (identity below -5 dBFS, never above -0.5 dBFS).
float hitClipShape(float x) noexcept;
// Session 14 (sound 14): how much lower an instrument sits by default (dB on the whole
// instrument: the horn, the fanfare, the doom brass, the War Drums), and on its brass layer
// only (the Dwarven Chant's low brass, the Hoard Gong's growl); 0 before sound 14.
// `recorded`: false for a sampled instrument playing its synthesised stand-in.
float seatLevelDb(int index, int sound = SoundLatest, bool recorded = true) noexcept;
float seatLayerDb(int index, int sound = SoundLatest) noexcept;
// Session 14: the brass's baseline processing (0 none, 1 the brass instruments): the soft
// top (a gentle high shelf above ~7 kHz, deeper played softly), tape warmth, a little width.
float seatAmount(int index, int sound = SoundLatest) noexcept;
// The seated brass's make-up in its place (dB, on top of sound 13's; in mixFit). Calibration
// only (tests: `--seat-calibrate`): replaces an instrument's level offset and make-up; NaN
// restores the measured ones.
float seatMakeupDb(int index) noexcept;
void overrideSeatDb(int index, float levelDb, float makeupDb) noexcept;
// Session 17: whether the instrument plays the recorded woodwinds (sound 15 and the samples
// in the build), the level of its recorded winds (dB, measured: its melody as loud as the
// synthesis it replaces), and what its mod wheel does (Expression from sound 15 when it plays
// the recordings, the design's own otherwise) with the panel's label for it.
bool designUsesRealWinds(int index, int sound = SoundLatest) noexcept;
float realWindTrimDb(int index) noexcept;
void overrideRealWindTrimDb(int index, float db) noexcept;   // calibration only; NaN restores
ModWheel wheelOf(int index, int sound = SoundLatest) noexcept;
const char* wheelLabel(int index, int sound = SoundLatest) noexcept;
// Session 14: the tape warmth's curve (gentle: under 0.1 dB at the held level, about 1 dB
// off a hit near full scale).
float seatWarmth(float x) noexcept;

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
    // Session 11, Punch: the hit's boost (dB at the start), how long it takes to settle,
    // where it is, and its gain over the current chunk (interpolated per sample).
    float punchPeakDb = 0, punchSamples = 1, punchPos = 0, punchGain = 1, punchStep = 0;
    // Session 12: the horns' Punch falls steeply after the crack (the raised cosine to the
    // power `punchShape`; 1 = Session 11's curve), settling at the same Punch Time.
    float punchShape = 1;
    // Session 12, the brass's attack edge: a 3.2 kHz band added on the hit, fading out.
    d::Svf edge;
    float edgeGain = 0, edgeDecay = 0;
    bool hitClip = false;   // a sound-12 horn: the synth's hit clipper engages while it sounds
    // Session 14, the brass seated: its amount (0 = none), the soft top (a one-pole low-pass
    // per side; the shelf takes `shelfDepth` of what lies above it) and the warmth.
    float seat = 0, shelfCoef = 0, shelfDepth = 0, shelfL = 0, shelfR = 0;
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
    // The voices' Vowel and Ensemble (0.5 = as designed), for notes that start from now.
    void setVoiceControls(float vowel, float ensemble) noexcept { voiceVowel = vowel; voiceEnsemble = ensemble; }
    // Session 11: Punch (0..2, 1 = the instrument's own) and Punch Time (seconds), for
    // notes that start from now.
    void setPunch(float amount, float seconds) noexcept { punch = amount; punchSeconds = seconds; }
    // Which generation of the sound (Sound: 10, 11, 12), for notes from now.
    void setSound(int generation) noexcept { sound = std::clamp(generation, int(SoundClassic), int(SoundLatest)); }
    int soundGeneration() const noexcept { return sound; }

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
    float voiceVowel = 0.5f, voiceEnsemble = 0.5f;
    float punch = 1.0f, punchSeconds = 0.12f;
    int sound = SoundLatest;
    // Session 12: the hit clipper. While a sound-12 horn sounds, the synth's sum is shaped
    // above -5 dBFS by a soft knee towards -0.5 dBFS: the punched hit turns into density
    // (louder for the same peak) instead of meeting the output limiter, which would pull
    // it down and pump back. Below the knee (the held notes) nothing changes.
    float clipMix = 0, clipTarget = 0;
    // Session 14: the brass's width (an ensemble of the seated voices' mid, delayed 8-12 ms and
    // slowly moving, added to the side only: the mono sum is unchanged).
    static constexpr int widthSize = 4096;   // 12 ms at up to 192 kHz, and room
    std::array<float, widthSize> widthLine {};
    int widthPos = 0, widthActive = 0;   // widthActive: samples the line still has to empty
    float widthPhase = 0;
    double tempo = 120;
    int designNow = 0, counter = 0, orderCounter = 0;
    uint64_t noteCount = 0;
    std::array<std::atomic<uint64_t>, 2> lit {};
};
}
