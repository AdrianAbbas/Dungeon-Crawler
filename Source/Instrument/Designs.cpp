// The forty instruments (Direction Update 3, section 3): one per place, each location a
// family. Every instrument is a start function that sets up a voice's layers on
// WORLDBUILDER's models (and the developer's recordings where they fit), its post
// (tone, body, drive), and sometimes a control function that runs while it sounds.
//
// Macros (0.5 = as designed): 0 Tone, 1 Attack (Strike, Pluck, Bow...), 2 Release
// (Ring...), 3 Body, 4 the instrument's own signature control. Note::scale(m) is a time
// factor x1/4..x4, Note::offset(m) -1..1.
#include "Synth.h"
#include "Trims.h"
#include "../World/Locations.h"

namespace dc::inst {
namespace {
using M = Model;

float vel(const Note& n, float floor = 0.3f) { return floor + (1.0f - floor) * std::pow(d::clamp01(n.velocity), 1.25f); }
float tone01(const Note& n) { return d::clamp01(n.macro[0]); }
float sig(const Note& n) { return d::clamp01(n.macro[4]); }
// Striking harder with the Attack macro above the middle, softer below (struck and plucked).
float hardness(const Note& n, float base) { return d::clamp01(base + 0.35f * n.offset(1)); }

const wb::samples::Instrument* sampled(const char* id) {
    auto& bank = wb::samples::Bank::get();
    return bank.ready() ? bank.instrument(id) : nullptr;
}

float midiOf(float hz) { return 69.0f + 12.0f * std::log2(std::max(hz, 1.0f) / 440.0f); }

// --- Model starters -------------------------------------------------------------------
void pluck(Layer& L, float hz, float decay, float bright, float v, const d::Pluck::Shape& shape, const Note& n, float gain = 1, float pan = 0) {
    L.use(M::Pluck, gain, pan);
    L.hz = hz;
    L.pluck.start(hz, decay, bright, v, pan, n.rate, n.noiseAt, shape);
}

void bell(Layer& L, float hz, float decay, float hard, float amp, const float* ratios, const float* decays, const float* gains, int count, const Note& n,
          d::Bell::Extras extras = {}, float gain = 1, float pan = 0) {
    L.use(M::Bell, gain, pan);
    L.hz = hz;
    L.bell.extras = extras;
    L.bell.start(hz, decay, hard, amp, pan, n.rate, ratios, decays, gains, count);
}

void synth(Layer& L, int model, float hz, float attack, float decay, float sustain, const Note& n, float gain = 1, float pan = 0) {
    L.use(M::Synth, gain, pan);
    L.hz = hz;
    L.synth.model = model;
    L.synth.velocity = d::clamp01(n.velocity);
    L.synth.pan = d::panGains(pan);
    L.synth.bite = 0;
    L.synth.biteDrive = 0;
    L.synth.start(hz, attack, decay, sustain, n.rate);
}

// Session 11: brass that cuts through a mix. Its attack quicker, its brightness opening
// further for the first ~100 ms (more so the harder it is played), its own blat louder.
// Nothing in the classic sound (tests, the before/after renders).
// Session 12 (sound 12): the horns crack. The rise quicker again, the brightness opening
// much further on the hit and following the velocity steeply, the lips driven harder for
// the first ~120 ms (an edge that fades into the same held tone), a louder blat.
float brassAttack(float seconds, const Note& n) { return n.classic ? seconds : seconds * (n.sound >= SoundHorns ? 0.2f : 0.4f); }
void brassBite(Layer& L, const Note& n, float amount = 1.0f) {
    if (n.classic) return;
    if (n.sound >= SoundHorns) {
        const float v = d::clamp01(n.velocity);
        L.synth.bite = amount * (0.5f + 3.3f * std::pow(v, 1.5f));
        L.synth.biteSeconds = 0.07f;
        L.synth.biteDrive = amount * (1.0f + 4.0f * v * v);
        L.synth.blat *= 1.0f + 1.2f * v;   // start() set it; a louder burst of buzz
        return;
    }
    L.synth.bite = amount * (0.6f + 1.2f * n.velocity);
    L.synth.biteSeconds = 0.09f;
}

void choir(Layer& L, float hz, float attack, float vowel, float bright, float breath, float detune, const Note& n, float gain = 1, float pan = 0) {
    L.use(M::Choir, gain, pan);
    L.hz = hz;
    L.choir.vowel = vowel;
    L.choir.brightness = bright;
    L.choir.breath = breath;
    L.choir.detuneCents = detune;
    L.choir.velocity = d::clamp01(n.velocity);
    L.choir.vibratoDepth = 0.1f;
    L.choir.start(hz, attack, n.rate);
}

void wind(Layer& L, float hz, float attack, float h2, float h3, float h4, float h5, float breath, float chiff, const Note& n, float gain = 1, float pan = 0) {
    L.use(M::Wind, gain, pan);
    L.hz = hz;
    auto& w = L.wind;
    w.h2 = h2;
    w.h3 = h3;
    w.h4 = h4;
    w.h5 = h5;
    w.breath = breath;
    w.chiff = chiff;
    w.tone = 1;
    w.velocity = d::clamp01(n.velocity);
    w.start(hz, attack, 0.0f, n.rate, n.noiseAt);
}

void strings(Layer& L, float hz, float attack, int players, float position, float bright, float bowNoise, const Note& n, float gain = 1, float pan = 0) {
    L.use(M::Strings, gain, pan);
    L.hz = hz;
    auto& s = L.strings;
    s.players = std::clamp(players, 1, d::StringVoice::maxPlayers);
    s.position = position;
    s.brightness = bright;
    s.bowNoise = bowNoise;
    s.vibratoDepth = 0.16f;
    s.vibratoDelay = 0.3f;
    s.spreadCents = 9;
    s.start(hz, attack, n.velocity, n.random, n.noiseAt, false);
}

void bowed(Layer& L, float hz, float attack, std::initializer_list<float> ratios, std::initializer_list<float> gains, float ring, float rosin, float noisy, const Note& n,
           float gain = 1, float pan = 0) {
    L.use(M::Bowed, gain, pan);
    L.hz = hz;
    auto& b = L.bowed;
    b.modes = 0;
    auto g = gains.begin();
    for (float r : ratios) {
        if (b.modes >= d::BowedVoice::maxModes) break;
        b.ratio[std::size_t(b.modes)] = r;
        b.gain[std::size_t(b.modes)] = g != gains.end() ? *g++ : 0.5f;
        b.decay[std::size_t(b.modes)] = 1.0f;
        ++b.modes;
    }
    b.ringSeconds = ring;
    b.rosin = rosin;
    b.noisiness = noisy;
    b.tunedHz = 0;
    b.start(hz, attack, n.rate, n.noiseAt);
}

void fm(Layer& L, float hz, float attack, float decay, float sustain, float ratio, float index, float indexSeconds, const Note& n, float gain = 1, float pan = 0) {
    L.use(M::Fm, gain, pan);
    L.hz = hz;
    L.fm.ratio = ratio;
    L.fm.index = index;
    L.fm.indexFloor = 0.15f;
    L.fm.start(hz, attack, decay, sustain, n.rate, indexSeconds);
}

void tine(Layer& L, float hz, float decay, float bright, const Note& n, float gain = 1, float pan = 0) {
    L.use(M::Tine, gain, pan);
    L.hz = hz;
    L.tine.start(hz, decay, bright, n.velocity, n.rate);
}

OrganVoice& organ(Layer& L, float hz, float gain = 1, float pan = 0) {
    L.use(M::Organ, gain, pan);
    L.hz = hz;
    L.organ.ranks = 0;
    L.organ.reed = 0;
    L.organ.tremolo = 0;
    return L.organ;
}

void drum(Layer& L, int kind, float hz, float startBend, float decay, float amp, float noiseAmp, float noiseHz, float noiseQ, float noiseSeconds, const Note& n,
          float gain = 1, float pan = 0) {
    L.use(M::Drum, gain, pan);
    L.hz = hz;
    L.drum.kind = kind;
    L.drum.bendSeconds = 0.04f;
    L.drum.start(hz, startBend, decay, amp, noiseAmp, noiseHz, noiseQ, noiseSeconds, pan, n.rate, n.noiseAt);
}

// A recorded instrument, or false (the caller plays its synthesised fallback).
bool sample(Layer& L, const char* id, float hz, const Note& n, float attack, float decay, float skip, float even, float courseCents = 0, float courseLevel = 0,
            float gain = 1, float pan = 0, bool smoothLayers = false) {
    const auto* inst = sampled(id);
    if (inst == nullptr) return false;
    L.use(M::Sample, gain, pan);
    L.hz = hz;
    L.sample.smoothLayers = smoothLayers;
    if (!L.sample.start(*inst, midiOf(hz), n.velocity, n.random, n.rate, attack, decay, skip, even, courseCents, courseLevel)) return false;
    return true;
}

// The developer's voice (Session 8) on a layer: two vowels (each a lead set for the first
// singer and an ensemble set for the others, e.g. the choir stacks), `singers` voices.
// nullptr without the recordings (the caller plays its stand-in). Configure, then
// startVocal().
VocalVoice* vocal(Layer& L, float hz, const char* a, const char* aEnsemble, const char* b, const char* bEnsemble, int singers, float gain = 1, float pan = 0) {
    const auto* la = a != nullptr ? sampled(a) : nullptr;
    const auto* lae = aEnsemble != nullptr ? sampled(aEnsemble) : nullptr;
    if (la == nullptr && lae == nullptr) return nullptr;
    L.use(M::Vocal, gain, pan);
    L.hz = hz;
    auto& v = L.vocal;
    v.reset();
    v.vowel[0] = { la, lae };
    v.vowel[1] = { b != nullptr ? sampled(b) : nullptr, bEnsemble != nullptr ? sampled(bEnsemble) : nullptr };
    v.singers = singers;
    return &v;
}
bool startVocal(Layer& L, float hz, const Note& n, float attack, float velocity, float skip = 0.0f) {
    // Session 9: the panel's Vowel and Ensemble (0.5 leaves the design exactly as it was).
    // Vowel moves the morph towards the first vowel (0) or the second (1), where the layer
    // has two; Ensemble thins the choir to one singer (0) or fills it (1).
    auto& v = L.vocal;
    if (n.vowel != 0.5f && v.vowel[1].lead != nullptr) {
        const float o = std::clamp((n.vowel - 0.5f) * 2.0f, -1.0f, 1.0f);
        const float m = o > 0.0f ? v.morphTarget + o * (1.0f - v.morphTarget) : v.morphTarget * (1.0f + o);
        v.morph = v.morphTarget = std::clamp(m, 0.0f, 1.0f);
    }
    if (n.ensemble != 0.5f) {
        const float e = std::clamp(n.ensemble, 0.0f, 1.0f);
        const float singers = e < 0.5f ? 1.0f + float(v.singers - 1) * e * 2.0f : float(v.singers) + float(VocalVoice::maxSingers - v.singers) * (e - 0.5f) * 2.0f;
        v.singers = std::clamp(int(std::lround(singers)), 1, VocalVoice::maxSingers);
    }
    if (!L.vocal.start(midiOf(hz), velocity, n.random, n.rate, attack, skip, n.noiseAt ^ uint64_t(hz * 1000.0f))) {
        L.use(M::None);
        return false;
    }
    return true;
}
// A one-shot piece of an unpitched recording (whispers, breaths): the take chosen by
// `pick`, started `skip` seconds in, played at its own speed (`semis` away), dying over
// `decay` seconds.
bool texture(Layer& L, const char* id, const Note& n, float pick, float skip, float decay, float semis, float gain, float pan) {
    const auto* inst = sampled(id);
    if (inst == nullptr || inst->zones.empty()) return false;
    L.use(M::Sample, gain, pan);
    L.sample.smoothLayers = false;
    const float root = inst->zones.front().root;
    L.hz = 440.0f * std::exp2((root + semis - 69.0f) / 12.0f);
    if (!L.sample.start(*inst, root + semis, 0.5f, pick, n.rate, 0.03f, decay, skip, 0.0f, 0, 0, int(std::lround(root)))) {
        L.use(M::None);
        return false;
    }
    return true;
}

// Body resonances: a string family scale of the violin's (WORLDBUILDER's data).
Post stringPost(float scale, float tone, float gain) {
    Post p;
    p.toneHz = tone;
    p.highPassHz = std::max(20.0f, 60.0f * scale);
    p.body = { { { d::violinBody[0].hz * scale, 3.5f, gain }, { d::violinBody[1].hz * scale, 4.5f, gain * 0.9f }, { d::violinBody[4].hz * std::sqrt(scale), 1.4f, gain * 0.6f } } };
    return p;
}
Post bodyPost(int type, float tone, float gain, float hp = 30) {
    Post p;
    p.toneHz = tone;
    p.highPassHz = hp;
    const auto& b = d::bodies[std::size_t(type)];
    p.body = { { { b.mode[0].hz, b.mode[0].q * 0.5f, gain * b.mode[0].gain }, { b.mode[1].hz, b.mode[1].q * 0.5f, gain * b.mode[1].gain },
                 { b.mode[2].hz, b.mode[2].q * 0.5f, gain * b.mode[2].gain } } };
    return p;
}
Post plainPost(float tone, float hp = 25, float drive = 0, float velocityTone = 0.8f) {
    Post p;
    p.toneHz = tone;
    p.highPassHz = hp;
    p.drive = drive;
    p.velocityTone = velocityTone;
    return p;
}
Post withBody(Post p, std::initializer_list<BodyPeak> peaks) {
    std::size_t k = 0;
    for (const auto& b : peaks)
        if (k < 3) p.body[k++] = b;
    return p;
}

// Mode tables (beyond WORLDBUILDER's InstrumentData).
constexpr float flat[8] { 1, 1, 1, 1, 1, 1, 1, 1 };
// Iron chain links: a stiff, irregular little bar.
constexpr float chainRatios[6] { 1.0f, 2.32f, 3.73f, 5.18f, 6.94f, 8.6f };
constexpr float chainDecays[6] { 1.0f, 0.7f, 0.5f, 0.38f, 0.3f, 0.22f };
constexpr float chainGains[6] { 1.0f, 0.65f, 0.5f, 0.38f, 0.26f, 0.18f };
// Celesta: steel bars over tuned resonators (the fundamental strongly reinforced).
constexpr float celestaRatios[4] { 1.0f, 3.0f, 4.21f, 9.6f };
constexpr float celestaDecays[4] { 1.0f, 0.4f, 0.3f, 0.15f };
constexpr float celestaGains[4] { 1.0f, 0.22f, 0.12f, 0.05f };
// Gold coins: thin discs, a bright inharmonic ting.
constexpr float coinRatios[4] { 1.0f, 1.41f, 2.13f, 2.89f };
constexpr float coinDecays[4] { 1.0f, 0.8f, 0.6f, 0.45f };
constexpr float coinGains[4] { 1.0f, 0.7f, 0.5f, 0.3f };
// Church bell decays (the hum longest after the prime) and strengths.
constexpr float bellDecays[8] { 1.0f, 0.82f, 0.66f, 0.55f, 0.44f, 0.34f, 0.28f, 0.22f };
constexpr float bellGains[8] { 0.55f, 1.0f, 0.7f, 0.35f, 0.62f, 0.28f, 0.2f, 0.14f };
// Stone marimba (a lithophone): rougher, less exactly tuned overtones, shorter.
constexpr float stoneRatios[4] { 1.0f, 3.86f, 8.7f, 13.4f };
constexpr float stoneDecays[4] { 1.0f, 0.32f, 0.18f, 0.1f };
constexpr float stoneGains[4] { 1.0f, 0.36f, 0.16f, 0.07f };
constexpr float glassDecays[4] { 1.0f, 0.8f, 0.62f, 0.5f };
constexpr float glassGains[4] { 1.0f, 0.5f, 0.34f, 0.2f };
// Gong: a flat disc's crowded modes.
constexpr float gongRatios[6] { 1.0f, 1.51f, 1.98f, 2.48f, 2.96f, 3.53f };
constexpr float gongDecays[6] { 1.0f, 0.85f, 0.7f, 0.6f, 0.5f, 0.42f };
constexpr float gongGains[6] { 1.0f, 0.62f, 0.5f, 0.4f, 0.32f, 0.26f };
// Anvil: a heavy block, hard and long.
constexpr float anvilRatios[5] { 1.0f, 2.57f, 4.12f, 5.94f, 7.6f };
constexpr float anvilDecays[5] { 1.0f, 0.6f, 0.45f, 0.32f, 0.22f };
constexpr float anvilGains[5] { 1.0f, 0.72f, 0.5f, 0.34f, 0.2f };
constexpr float barDecays[4] { 1.0f, 0.5f, 0.34f, 0.2f };
constexpr float barGains[4] { 1.0f, 0.42f, 0.22f, 0.09f };
// Bells under water: soft, a little out of shape.
constexpr float drownedRatios[4] { 1.0f, 2.41f, 3.9f, 5.62f };
constexpr float marimbaDecays[3] { 1.0f, 0.35f, 0.18f };
constexpr float marimbaGains[3] { 1.0f, 0.3f, 0.1f };

// A slow drift in cents for a voice (water, ghosts): smooth noise.
float drift(Voice& v, float cents, float hz) { return cents * v.noise.smooth(double(v.age) + 13.0 * double(v.order % 7), double(hz)); }

// ======================================================================================
// DUNGEON: dark orchestral (low strings, iron, gold)

// Corridors: contrabass pizzicato. A gut string plucked by the side of a finger, the
// bass's body, a finger's thump; Snap: a Bartok snap (the string slapping the board).
void corridors(Voice& v, const Note& n) {
    d::Pluck::Shape sh;
    sh.position = 0.32f - 0.12f * n.offset(1);
    sh.loss = 0.72f - 0.2f * n.offset(0);
    sh.stiffness = 0.04f;
    sh.pickNoise = 0.12f + 0.5f * sig(n);
    sh.dampSeconds = 0;
    pluck(v.layer[0], n.hz, 2.2f * n.scale(2), 0.3f + 0.3f * tone01(n), vel(n), sh, n);
    drum(v.layer[1], d::DrumTwoTone, n.hz, 1.0f, 0.09f, 0.12f * vel(n), 0.05f * vel(n) * (0.4f + sig(n)), 900.0f + 2200.0f * sig(n), 1.2f, 0.03f, n, 1.0f);
}

// Entrance Hall: a cello drone-lead. One bowed cello; Drone: the same note an octave
// below on a bowed resonator, under it (only what is played sounds).
void entranceHall(Voice& v, const Note& n) {
    strings(v.layer[0], n.hz, 0.12f * n.scale(1), 1, 0.11f, 0.42f + 0.3f * n.offset(0), 0.14f, n);
    if (sig(n) > 0.02f) bowed(v.layer[1], n.hz * 0.5f, 0.3f * n.scale(1), { 1, 2, 3, 4 }, { 1.0f, 0.5f, 0.3f, 0.15f }, 0.5f, 0.6f, 0.12f, n, 0.55f * sig(n));
}

// Prison Cells: iron chains, struck and bowed. A struck link (soft velocity: a heavy
// scrape bowed across the iron; hard: the strike), a bowed iron hum that sustains while
// the key is held, and the links rattling after the strike (Rattle).
void prisonCells(Voice& v, const Note& n) {
    d::Bell::Extras ex;
    ex.beatHz = 1.3f;
    ex.variation = 0.45f;
    bell(v.layer[0], n.hz, 2.8f * n.scale(2), hardness(n, 0.75f), 0.55f * vel(n), chainRatios, chainDecays, chainGains, 6, n, ex);
    bowed(v.layer[1], n.hz, 0.25f * n.scale(1), { 1.0f, 2.76f, 5.4f, 8.93f }, { 1.0f, 0.55f, 0.32f, 0.18f }, 1.1f, 0.35f, 0.55f, n, 0.6f * (1.15f - n.velocity));
    v.state[0] = 0;      // time to the next rattle
    v.state[1] = 0;      // rattles left
    v.step = int(3 + 6 * sig(n));
}
void prisonCellsControl(Voice& v, const Note& n, float dt) {
    // The links clink against each other for a moment after the strike.
    if (v.step <= 0 || v.age > 0.6f) return;
    v.state[0] -= dt;
    if (v.state[0] > 0.0f) return;
    const float r = d::unipolar(d::mix64(uint64_t(v.order) * 31 + uint64_t(v.step)));
    v.state[0] = 0.03f + 0.07f * r;
    --v.step;
    Note m = n;
    m.noiseAt = n.noiseAt + uint64_t(v.step) * 1013;
    drum(v.layer[2], d::DrumTwoTone, n.hz * (2.3f + 0.7f * r), 1.0f, 0.05f, 0.05f * vel(n) * sig(n), 0.06f * vel(n) * sig(n), 3800.0f + 2500.0f * r, 2.5f, 0.025f, m,
         1.0f, 0.4f * (r - 0.5f));
}

// Treasure Room: a golden celesta with coin glints. Steel bars over resonators; a few
// coins ring high above each note (Glint).
void treasureRoom(Voice& v, const Note& n) {
    bell(v.layer[0], n.hz, 2.6f * n.scale(2), hardness(n, 0.5f), 0.5f * vel(n), celestaRatios, celestaDecays, celestaGains, 4, n);
    v.step = int(1 + 3 * sig(n));
    v.state[0] = 0.02f;
}
void treasureRoomControl(Voice& v, const Note& n, float dt) {
    if (v.step <= 0 || sig(n) < 0.02f) return;
    v.state[0] -= dt;
    if (v.state[0] > 0.0f) return;
    const float r = d::unipolar(d::mix64(uint64_t(v.order) * 977 + uint64_t(v.step)));
    v.state[0] = 0.05f + 0.12f * r;
    --v.step;
    Note m = n;
    d::Bell::Extras ex;
    ex.beatHz = 3.0f;
    bell(v.layer[1 + (v.step & 1)], n.hz * (4.0f + 2.0f * r), 0.45f, 0.95f, 0.1f * vel(n) * sig(n), coinRatios, coinDecays, coinGains, 4, m, ex, 1.0f, 0.8f * (r - 0.5f));
}

// Final Boss Lair: colossal doom brass with choir stabs. Two low horns and the same an
// octave below, driven; a choir shouting the note for a moment (Choir).
void finalBossLair(Voice& v, const Note& n) {
    const float a = brassAttack(0.04f * n.scale(1), n);
    synth(v.layer[0], d::SynthBrass, n.hz, a, 1.0f, 1.0f, n, 0.9f, -0.15f);
    brassBite(v.layer[0], n);
    v.layer[0].synth.detuneCents = 9;
    v.layer[0].synth.cutoff = 700.0f * std::exp2(n.offset(0));
    v.layer[0].synth.formantHz = 650;
    // Session 12: the octave below speaks with the upper horns (it lagged by half again).
    synth(v.layer[1], d::SynthBrass, n.hz * 0.5f, n.sound >= SoundHorns ? a : a * 1.5f, 1.0f, 1.0f, n, 0.8f, 0.15f);
    v.layer[1].synth.detuneCents = 6;
    v.layer[1].synth.cutoff = 600.0f * std::exp2(n.offset(0));
    v.layer[1].synth.formantHz = 420;
    brassBite(v.layer[1], n, 0.7f);
    if (sig(n) > 0.02f) {
        // The choir stab: the developer's full aah and ooh stacks shouted together, six
        // voices tight in time, the gritty voice under them (stand-in: the formant choir).
        if (auto* c = vocal(v.layer[2], n.hz, "voice-aah", "choir-aah", "voice-ooh", "choir-ooh", 6, 1.15f * sig(n) * vel(n, 0.2f))) {
            c->morph = c->morphTarget = 0.35f;
            c->detuneCents = 14;
            c->timingMs = 18;
            c->spread = 0.85f;
            c->grit = sampled("voice-grit");
            c->gritLevel = 0.35f;
            startVocal(v.layer[2], n.hz, n, 0.012f, 0.95f, 0.02f);
        } else {
            choir(v.layer[2], n.hz, 0.01f, 2.0f, 0.75f, 0.1f, 14, n, 0.9f * sig(n) * vel(n, 0.2f));
        }
    }
    // The doom piano: a low piano struck an octave below with the brass (Weight).
    const float weight = d::clamp01(2.0f * n.macro[3]);
    if (weight > 0.02f) sample(v.layer[3], "piano", n.hz * 0.5f, n, 0.0f, 3.0f * n.scale(2), 0.0f, 0.5f, 0, 0, 0.42f * weight, 0.1f, true);
}
void finalBossLairControl(Voice& v, const Note&, float) {
    // The stab: the choir lets go after a moment, the brass stays.
    if (v.age > 0.28f && v.state[0] == 0.0f) {
        if (v.layer[2].model == M::Choir && v.layer[2].choir.env.stage != d::Env::Release) v.layer[2].choir.release(0.35f, v.note.rate);
        if (v.layer[2].model == M::Vocal) v.layer[2].vocal.release(0.3f);
        v.state[0] = 1.0f;
    }
}

// ======================================================================================
// CASTLE: the royal court

// Courtyard: a lute (the developer's classical guitar, made a lute: paired courses a few
// cents apart, the lute's smaller, brighter body, a quicker decay). Course: the second
// string's level.
void courtyard(Voice& v, const Note& n) {
    const float course = 0.25f + 0.6f * sig(n);
    if (!sample(v.layer[0], "guitar", n.hz, n, 0.0f, 3.2f * n.scale(2), 0.002f, 0.6f, 4.0f + 4.0f * sig(n), course)) {
        d::Pluck::Shape sh;
        sh.position = 0.12f;
        sh.loss = 0.5f;
        sh.stiffness = 0.08f;
        sh.course = course;
        sh.courseCents = 3.0f + 4.0f * sig(n);
        sh.pickNoise = 0.3f;
        pluck(v.layer[0], n.hz, 2.6f * n.scale(2), 0.5f + 0.3f * n.offset(0), vel(n), sh, n);
    }
}

// Great Hall: a brass fanfare section: two trumpets a few cents apart and a trombone an
// octave below (Section: the trombone and the second player).
void greatHall(Voice& v, const Note& n) {
    const float a = brassAttack(0.012f * n.scale(1), n);
    synth(v.layer[0], d::SynthBrass, n.hz * 2.0f, a, 1.0f, 1.0f, n, 0.8f, -0.3f);
    v.layer[0].synth.detuneCents = 5;
    v.layer[0].synth.cutoff = 1700.0f * std::exp2(n.offset(0));
    v.layer[0].synth.formantHz = 1600;
    v.layer[0].synth.formantLevel = 0.9f;
    synth(v.layer[1], d::SynthBrass, n.hz * 2.0f * std::exp2(-7.0f / 1200.0f), a * 1.3f, 1.0f, 1.0f, n, 0.55f + 0.3f * sig(n), 0.3f);
    v.layer[1].synth.detuneCents = 0;
    v.layer[1].synth.cutoff = 1500.0f * std::exp2(n.offset(0));
    v.layer[1].synth.formantHz = 1500;
    synth(v.layer[2], d::SynthBrass, n.hz, a * 1.6f, 1.0f, 1.0f, n, 0.45f * sig(n) * 2.0f, 0.0f);
    v.layer[2].synth.detuneCents = 0;
    v.layer[2].synth.cutoff = 900.0f * std::exp2(n.offset(0));
    v.layer[2].synth.formantHz = 700;
    // Session 12: the trumpets are bright and hot already; their bite stays lighter (the hit
    // comes from Punch and the clipper; the instruments' level check holds).
    const float bite = n.sound >= SoundHorns ? 0.55f : 1.0f;
    for (int k = 0; k < 3; ++k) brassBite(v.layer[std::size_t(k)], n, k == 2 ? 0.6f * bite : bite);
}

// Throne Room: a regal string ensemble, violins and below them violas and cellos an
// octave down (Low Section).
void throneRoom(Voice& v, const Note& n) {
    const float a = 0.2f * n.scale(1);
    strings(v.layer[0], n.hz, a, 3, 0.12f, 0.5f + 0.3f * n.offset(0), 0.15f, n, 1.0f);
    if (sig(n) > 0.02f) strings(v.layer[1], n.hz * 0.5f, a * 1.2f, 2, 0.13f, 0.4f + 0.3f * n.offset(0), 0.2f, n, 0.6f * sig(n) * 2.0f);
}

// Chapel: a small chamber organ, flue stops 8' 4' 2' (Stops: adds the nazard and tierce).
void chapel(Voice& v, const Note& n) {
    auto& o = organ(v.layer[0], n.hz);
    o.addRank(1.0f, 1.0f);
    o.addRank(2.0f, 0.5f, 1.5f);
    o.addRank(4.0f, 0.22f, -2.0f);
    if (sig(n) > 0.55f) {
        o.addRank(3.0f, 0.3f * (sig(n) - 0.5f) * 2.0f, 1.0f);
        o.addRank(5.0f, 0.18f * (sig(n) - 0.5f) * 2.0f, -1.0f);
    } else {
        o.level[1] *= sig(n) * 2.0f;
        o.level[2] *= sig(n) * 2.0f;
    }
    o.chiff = 0.35f * (0.6f + 0.8f * (1.0f - n.macro[1]));
    o.wind = 0.012f;
    o.start(n.hz, 0.03f * n.scale(1), n.rate, n.noiseAt);
}

// Tower Top: heroic horn calls. A horn (dark bell, scooping lips) and a second horn
// doubling (Bell Up: the bell raised, brighter and brassier).
void towerTop(Voice& v, const Note& n) {
    const float a = brassAttack(0.09f * n.scale(1), n);
    synth(v.layer[0], d::SynthBrass, n.hz, a, 1.0f, 1.0f, n, 0.9f, -0.1f);
    auto& s = v.layer[0].synth;
    s.detuneCents = 0;
    s.cutoff = (300.0f + 700.0f * sig(n)) * std::exp2(n.offset(0));
    s.formantHz = 620;
    s.formantLevel = 0.3f;
    s.breath = 0.08f;
    s.vibratoDepth = 0.07f;
    s.vibratoDelay = 0.45f;
    synth(v.layer[1], d::SynthBrass, n.hz * std::exp2(4.0f / 1200.0f), a * 1.2f, 1.0f, 1.0f, n, 0.45f, 0.15f);
    v.layer[1].synth.cutoff = (280.0f + 600.0f * sig(n)) * std::exp2(n.offset(0));
    v.layer[1].synth.formantHz = 600;
    v.layer[1].synth.formantLevel = 0.3f;
    brassBite(v.layer[0], n, 1.2f);
    brassBite(v.layer[1], n, 1.0f);
}

// ======================================================================================
// CATHEDRAL: sacred voices and bells

// Cloister: a soft choir, two sections a little apart (Vowel: oo .. ah).
void cloister(Voice& v, const Note& n) {
    const float a = 0.45f * n.scale(1);
    // The developer's soft choir: his soft ooh and aah (the first voice) and the choir
    // stacks (the others: other takes), four voices; Vowel morphs ooh to aah. Above the
    // highest recorded note the formant choir takes over.
    if (auto* c = vocal(v.layer[0], n.hz, "voice-ooh", "choir-ooh", "voice-aah", "choir-aah", 4, 1.0f)) {
        c->morph = c->morphTarget = sig(n);
        c->detuneCents = 7;
        c->timingMs = 45;
        c->spread = 0.7f;
        // Started past the onset of each sung note (where he scoops up to the pitch), the
        // attack fading it in.
        if (startVocal(v.layer[0], n.hz, n, std::max(a, 0.08f), 0.15f + 0.55f * n.velocity, 0.3f)) {
            const float synthBlend = d::smoothstep(1.0f, 6.0f, v.layer[0].vocal.shiftAbove);
            v.layer[0].gain *= 1.0f - 0.7f * synthBlend;
            if (synthBlend > 0.01f) choir(v.layer[1], n.hz, a, 0.2f + 1.8f * sig(n), 0.32f + 0.3f * n.offset(0), 0.14f, 9, n, synthBlend, 0.2f);
            return;
        }
    }
    const float vowel = 0.2f + 1.8f * sig(n);
    choir(v.layer[0], n.hz, a, vowel, 0.32f + 0.3f * n.offset(0), 0.14f, 9, n, 1.0f, -0.25f);
    choir(v.layer[1], n.hz * std::exp2(6.0f / 1200.0f), a * 1.2f, vowel + 0.3f, 0.3f + 0.3f * n.offset(0), 0.12f, 12, n, 0.7f, 0.3f);
}

// Nave: a grand pipe organ, the full principal chorus 16' to a mixture (Reeds: a
// trumpet rank joins).
void nave(Voice& v, const Note& n) {
    auto& o = organ(v.layer[0], n.hz);
    o.addRank(0.5f, 0.55f);
    o.addRank(1.0f, 1.0f);
    o.addRank(2.0f, 0.6f, 1.2f);
    o.addRank(3.0f, 0.28f, -1.0f);
    o.addRank(4.0f, 0.34f, 2.0f);
    o.addRank(6.0f, 0.18f * std::exp2(n.offset(0)), -1.5f);
    o.addRank(8.0f, 0.14f * std::exp2(n.offset(0)), 1.0f);
    o.chiff = 0.25f;
    o.wind = 0.01f;
    o.start(n.hz, 0.05f * n.scale(1), n.rate, n.noiseAt);
    if (sig(n) > 0.02f) {
        auto& r = organ(v.layer[1], n.hz, 0.5f * sig(n) * 2.0f);
        r.addRank(1.0f, 0.8f, 2.5f);
        r.addRank(2.0f, 0.3f);
        r.reed = 0.8f;
        r.chiff = 0.1f;
        r.wind = 0.005f;
        r.start(n.hz, 0.06f * n.scale(1), n.rate, n.noiseAt + 99);
    }
}

// Crypt: a low monk chant, basses on "oo" and an octave below them a basso profundo
// (Vowel: oo .. oh).
void crypt(Voice& v, const Note& n) {
    const float a = 0.3f * n.scale(1);
    // The developer's monk chant (sounding an octave below the key): his low hummed drone
    // and his gravelly "ooh" drone for the low notes (recorded on G2, never shifted beyond
    // a fifth), his hummed and "oh" sweeps above; three monks a little apart. Vowel: hum
    // to chant. A faint formant choir an octave below is the basso profundo.
    const bool low = n.key - 12 <= 50;
    const float hz = 0.5f * n.hz;
    if (auto* c = vocal(v.layer[0], hz, low ? "drone-hum" : "voice-mm", nullptr, low ? "drone-chant" : "voice-oh", nullptr, 3, 1.0f, -0.05f)) {
        c->morph = c->morphTarget = sig(n);
        c->detuneCents = 5;
        c->timingMs = 70;
        c->spread = 0.45f;
        c->driftCents = 3;
        // Into the held drone (its first second slides onto the pitch), faded in.
        if (startVocal(v.layer[0], hz, n, std::max(a, 0.12f), n.velocity, 1.0f)) {
            choir(v.layer[1], hz, a * 1.3f, 0.1f + 1.2f * sig(n), 0.12f + 0.2f * n.offset(0), 0.05f, 5, n, 0.22f, 0.1f);
            v.layer[1].choir.vibratoDepth = 0.03f;
            return;
        }
    }
    const float vowel = 0.1f + 1.2f * sig(n);
    choir(v.layer[0], n.hz, a, vowel, 0.18f + 0.25f * n.offset(0), 0.06f, 6, n, 1.0f, -0.1f);
    v.layer[0].choir.vibratoDepth = 0.04f;
    choir(v.layer[1], n.hz * 0.5f, a * 1.3f, vowel, 0.12f + 0.2f * n.offset(0), 0.05f, 5, n, 0.5f, 0.1f);
    v.layer[1].choir.vibratoDepth = 0.03f;
}

// Bell Tower: tuned church bells (hum, prime, minor-third tierce, quint, nominal...), the
// lowest partials beating; Strike: the clapper's clang.
void bellTower(Voice& v, const Note& n) {
    d::Bell::Extras ex;
    ex.beatHz = 1.1f;
    ex.variation = 0.2f;
    bell(v.layer[0], n.hz, 7.0f * n.scale(2), hardness(n, 0.8f), 0.5f * vel(n), d::bellRatios.data(), bellDecays, bellGains, 8, n, ex);
    if (sig(n) > 0.02f) drum(v.layer[1], d::DrumTwoTone, n.hz * 2.0f, 1.0f, 0.06f, 0.05f * vel(n) * sig(n), 0.1f * vel(n) * sig(n), std::min(n.hz * 9.0f, 7000.0f), 1.5f, 0.04f, n);
}

// ======================================================================================
// CAVERNS: stone, crystal, water

// Cave Mouth: a firelit stone marimba: stone bars, a resonator under each, the firelight
// flickering on it (Flicker).
void caveMouth(Voice& v, const Note& n) {
    bell(v.layer[0], n.hz, 1.3f * n.scale(2), hardness(n, 0.45f), 0.55f * vel(n), stoneRatios, stoneDecays, stoneGains, 4, n);
    bell(v.layer[1], n.hz, 2.0f * n.scale(2), 0.2f, 0.25f * vel(n), flat, flat, flat, 1, n, {}, 0.8f);
}
void caveMouthControl(Voice& v, const Note& n, float) {
    // Firelight: a slow uneven flicker on the level.
    const float f = v.noise.smooth(double(v.age), 7.0);
    v.wheelLevel *= 1.0f - 0.18f * sig(n) * (0.5f + 0.5f * f);
}

// Crystal Cavern: glass and crystal bells, beating, with a shimmer an octave above that
// swells after the strike (Shimmer).
void crystalCavern(Voice& v, const Note& n) {
    d::Bell::Extras ex;
    ex.beatHz = 2.2f;
    bell(v.layer[0], n.hz, 4.0f * n.scale(2), hardness(n, 0.7f), 0.45f * vel(n), d::glassRatios.data(), glassDecays, glassGains, 4, n, ex);
    if (sig(n) > 0.02f) {
        d::Bell::Extras up;
        up.beatHz = 3.1f;
        up.bloom = 0.5f;
        bell(v.layer[1], n.hz * 2.0f, 3.5f * n.scale(2), 0.4f, 0.22f * vel(n) * sig(n), d::glassRatios.data(), glassDecays, glassGains, 4, n, up, 1.0f, 0.3f);
    }
}

// Underground Lake: a water kalimba (the developer's kalimba), its pitch rippling as if
// heard across water (Ripple).
void undergroundLake(Voice& v, const Note& n) {
    if (!sample(v.layer[0], "kalimba", n.hz, n, 0.0f, 3.5f * n.scale(2), 0.0f, 0.5f)) tine(v.layer[0], n.hz, 2.6f * n.scale(2), 0.55f + 0.3f * n.offset(0), n);
}
void undergroundLakeControl(Voice& v, const Note& n, float) {
    v.layer[0].cents = drift(v, 14.0f * sig(n), 0.8f);
}

// Dragon's Hoard: a huge tuned gong; played hard, a growling low brass rises out of it
// (Growl: how much brass).
void dragonsHoard(Voice& v, const Note& n) {
    d::Bell::Extras ex;
    ex.beatHz = 0.7f;
    ex.bloom = 0.7f;
    ex.variation = 0.2f;
    bell(v.layer[0], n.hz, 6.0f * n.scale(2), hardness(n, 0.35f), 0.6f * vel(n), gongRatios, gongDecays, gongGains, 6, n, ex);
    const float growl = sig(n) * std::pow(d::clamp01(n.velocity), 1.5f);
    if (growl > 0.01f) {
        synth(v.layer[1], d::SynthBrass, n.hz, 0.12f * n.scale(1), 1.0f, 1.0f, n, 1.2f * growl);
        v.layer[1].synth.detuneCents = 11;
        v.layer[1].synth.cutoff = 500.0f * std::exp2(n.offset(0));
        v.layer[1].synth.formantHz = 380;
    }
}

// ======================================================================================
// VILLAGE: weary folk

// Village Green: a tin whistle / recorder, a little tired (its pitch wandering); Breath.
void villageGreen(Voice& v, const Note& n) {
    wind(v.layer[0], n.hz, 0.03f * n.scale(1), 0.12f, 0.26f, 0.03f, 0.0f, 0.1f + 0.3f * sig(n), 0.4f, n);
    v.layer[0].wind.wander = 4;
    v.layer[0].wind.vibratoDepth = 0.08f;
    v.layer[0].wind.air = 0.06f + 0.15f * sig(n);
}

// Tavern: a weary guitar (the developer's classical guitar): warm, dull, slower to
// speak, dying a little sooner; Weariness: darker and a touch out of tune.
void tavern(Voice& v, const Note& n) {
    const float detune = 9.0f * sig(n) * (n.random - 0.5f);
    if (sample(v.layer[0], "guitar", n.hz * std::exp2(detune / 1200.0f), n, 0.004f, 2.6f * n.scale(2), 0.006f, 0.5f)) return;
    d::Pluck::Shape sh;
    sh.position = 0.2f;
    sh.loss = 0.62f;
    sh.stiffness = 0.06f;
    sh.pickNoise = 0.15f;
    pluck(v.layer[0], n.hz * std::exp2(detune / 1200.0f), 2.4f * n.scale(2), 0.35f + 0.3f * n.offset(0), vel(n), sh, n);
}

// Market Square: a fiddle (the developer's violin), with a raspy folk bow under it (Rasp).
void marketSquare(Voice& v, const Note& n) {
    const bool rec = sample(v.layer[0], "violin-sustain", n.hz, n, 0.05f * n.scale(1), 0.0f, 0.03f, 0.6f);
    if (rec) {
        v.layer[0].sample.vibratoDepth = 0.14f;
        v.layer[0].sample.vibratoHz = 5.6f;
    } else {
        strings(v.layer[0], n.hz, 0.06f * n.scale(1), 1, 0.1f, 0.6f + 0.3f * n.offset(0), 0.3f, n);
    }
    if (sig(n) > 0.02f) strings(v.layer[1], n.hz, 0.03f * n.scale(1), 1, 0.07f, 0.7f, 0.5f, n, (rec ? 0.18f : 0.14f) * sig(n) * 2.0f);
}

// Blacksmith: a tuned anvil, struck with a hammer (Clank: the hammer's iron).
void blacksmith(Voice& v, const Note& n) {
    bell(v.layer[0], n.hz, 2.4f * n.scale(2), hardness(n, 0.9f), 0.5f * vel(n), anvilRatios, anvilDecays, anvilGains, 5, n);
    if (sig(n) > 0.02f) drum(v.layer[1], d::DrumTwoTone, n.hz * 3.1f, 1.0f, 0.04f, 0.08f * vel(n) * sig(n), 0.14f * vel(n) * sig(n), 3500.0f, 1.8f, 0.035f, n);
}

// ======================================================================================
// HAUNTED FOREST: eerie wood and wind

// Forest Edge: a detuned wooden flute: two flutes not quite agreeing (Detune).
void forestEdge(Voice& v, const Note& n) {
    const float a = 0.06f * n.scale(1);
    wind(v.layer[0], n.hz, a, 0.32f, 0.1f, 0.02f, 0.0f, 0.3f, 0.25f, n, 1.0f, -0.3f);
    v.layer[0].wind.air = 0.14f;
    v.layer[0].wind.wander = 6;
    v.layer[0].wind.vibratoDepth = 0.1f;
    const float cents = -6.0f - 30.0f * sig(n);
    wind(v.layer[1], n.hz * std::exp2(cents / 1200.0f), a * 1.4f, 0.3f, 0.12f, 0.0f, 0.0f, 0.34f, 0.15f, n, 0.7f, 0.35f);
    v.layer[1].wind.wander = 8;
}

// Old Path: creaking cello harmonics: a thin flageolet bowed near the bridge, and the
// wood creaking now and then under the bow (Creak).
void oldPath(Voice& v, const Note& n) {
    strings(v.layer[0], n.hz, 0.25f * n.scale(1), 1, 0.045f, 0.22f + 0.25f * n.offset(0), 0.45f, n, 0.8f);
    v.layer[0].strings.vibratoDepth = 0.06f;
    bowed(v.layer[1], n.hz, 0.3f * n.scale(1), { 1.0f, 2.0f }, { 1.0f, 0.2f }, 0.8f, 0.45f, 0.08f, n, 0.6f);
    v.state[0] = 0.3f;
}
void oldPathControl(Voice& v, const Note& n, float dt) {
    if (sig(n) < 0.02f || !v.keyDown) return;
    v.state[0] -= dt;
    if (v.state[0] > 0.0f) return;
    const float r = d::unipolar(d::mix64(uint64_t(v.order) * 131 + uint64_t(++v.step)));
    v.state[0] = 0.35f + 1.4f * r;
    v.layer[0].strings.scratch = 1.4f * sig(n);
}

// Witch's Hut: an alluring music box: a worn comb's teeth, each tooth paired with a
// sweeter one a little sharp (Wear).
void witchsHut(Voice& v, const Note& n) {
    tine(v.layer[0], n.hz, 2.0f * n.scale(2), 0.8f + 0.2f * n.offset(0), n);
    tine(v.layer[1], n.hz * std::exp2((4.0f + 16.0f * sig(n)) / 1200.0f), 1.6f * n.scale(2), 0.7f, n, 0.5f, 0.3f);
    // The developer humming the lullaby softly under the box (his soft "mmm", at the key:
    // the box sounds an octave above it), rising slowly and fading with the box.
    if (auto* h = vocal(v.layer[2], n.hz * 0.5f, "voice-mm", nullptr, nullptr, nullptr, 1, 0.3f, -0.15f)) {
        h->driftCents = 5;
        startVocal(v.layer[2], n.hz * 0.5f, n, 0.18f * n.scale(1), 0.12f, 0.3f);
    }
}
void witchsHutControl(Voice& v, const Note& n, float) {
    // The hum follows the box down: let go with it, even while the key is held.
    if (v.layer[2].model == M::Vocal && v.age > 1.6f * n.scale(2) && v.state[0] == 0.0f) {
        v.layer[2].vocal.release(0.9f);
        v.state[0] = 1.0f;
    }
}

// Ancient Grove: an ancient harp, and a breath whispering the note (Whispers).
void ancientGrove(Voice& v, const Note& n) {
    d::Pluck::Shape sh;
    sh.position = 0.3f;
    sh.loss = 0.36f - 0.15f * n.offset(0);
    sh.stiffness = 0.02f;
    sh.pickNoise = 0.07f;
    pluck(v.layer[0], n.hz, 3.6f * n.scale(2), 0.45f + 0.3f * n.offset(0), vel(n), sh, n);
    if (sig(n) > 0.02f) {
        // The breathy whisper: a moment of the developer's unintelligible whispering (one
        // of three voices, from anywhere in it) or one of his slow breaths, as the string
        // is plucked (stand-in: a breath of noise at the octave).
        auto& L = v.layer[1];
        const float r2 = d::unipolar(d::mix64(n.noiseAt + 3));
        const bool whisper = n.random < 0.65f;
        const float pan = 0.5f * (r2 - 0.5f) * 2.0f;
        const bool rec = whisper ? texture(L, "voice-whispers", n, r2, 0.5f + 17.0f * d::unipolar(d::mix64(n.noiseAt + 5)), 1.1f, 0.0f, 0.55f * sig(n) * 2.0f, pan)
                                 : texture(L, "voice-breaths", n, r2, 0.0f, 1.3f, 0.0f, 0.5f * sig(n) * 2.0f, pan);
        if (!rec) {
            wind(L, n.hz * 2.0f, 0.12f, 0, 0, 0, 0, 0.9f, 0.0f, n, 0.6f * sig(n), 0.4f * (n.random - 0.5f));
            L.wind.tone = 0;
            L.wind.breathQ = 7;
            L.wind.air = 0.25f;
        }
    }
}
void ancientGroveControl(Voice& v, const Note&, float) {
    if (v.layer[1].model == M::Wind && v.age > 0.5f && v.layer[1].wind.env.stage != d::Env::Release) v.layer[1].wind.release(0.8f, v.note.rate);
    if (v.layer[1].model == M::Sample && v.age > 0.6f && !v.layer[1].sample.releasing) v.layer[1].sample.release(0.6f);
}

// ======================================================================================
// SWAMP: murky

// Mire: a detuned banjo: bright steel strings over a drum head, its paired strings badly
// out of tune with each other (Detune). (The Samples repository has no banjo: modelled.)
void mire(Voice& v, const Note& n) {
    // The developer's banjo (open G, through the SSL): soft, medium and hard sweeps as
    // velocity layers; round-robins from the three hits per dynamic of the open fifth
    // string around G4, elsewhere from the neighbouring recorded notes (a minor third
    // away, so no two repeats are the same take); a second string badly out of tune with
    // it (Detune), murky.
    const float detune = 8.0f + 30.0f * sig(n);
    const bool fifth = n.key >= 65 && n.key <= 69 && sampled("banjo-g5") != nullptr;
    if (fifth) {
        if (sample(v.layer[0], "banjo-g5", n.hz, n, 0.0f, 1.4f * n.scale(2), 0.0f, 0.5f, detune, 0.55f, 1, 0, true)) return;
    } else if (const auto* inst = sampled("banjo")) {
        const int rr = std::min(2, int(n.random * 3.0f));
        const int look = std::clamp(n.key + (rr == 1 ? -3 : (rr == 2 ? 3 : 0)), inst->lowKey, inst->highKey);
        auto& L = v.layer[0];
        L.use(M::Sample);
        L.hz = n.hz;
        L.sample.smoothLayers = true;
        if (L.sample.start(*inst, midiOf(n.hz), n.velocity, n.random, n.rate, 0.0f, 1.4f * n.scale(2), 0.0f, 0.5f, detune, 0.55f, look)) return;
        L.use(M::None);
    }
    d::Pluck::Shape sh;
    sh.position = 0.09f;
    sh.loss = 0.16f - 0.1f * n.offset(0);
    sh.stiffness = 0.12f;
    sh.course = 0.7f;
    sh.courseCents = 8.0f + 30.0f * sig(n);
    sh.pickNoise = 0.55f;
    pluck(v.layer[0], n.hz, 1.3f * n.scale(2), 0.75f, vel(n), sh, n);
}

// Bog: a murky low reed, like a bass clarinet (a cylindrical bore: odd harmonics), with
// mud in its breath (Murk).
void bog(Voice& v, const Note& n) {
    wind(v.layer[0], n.hz, 0.05f * n.scale(1), 0.03f, 0.55f, 0.0f, 0.28f, 0.08f + 0.2f * sig(n), 0.1f, n);
    v.layer[0].wind.wander = 3;
    v.layer[0].wind.vibratoDepth = 0.03f;
    v.layer[0].wind.air = 0.04f + 0.1f * sig(n);
}

// Sunken Ruins: bells under water, drifting in pitch, with a glassy pad below (Pad).
void sunkenRuins(Voice& v, const Note& n) {
    d::Bell::Extras ex;
    ex.beatHz = 0.9f;
    bell(v.layer[0], n.hz, 5.0f * n.scale(2), hardness(n, 0.3f), 0.5f * vel(n), drownedRatios, glassDecays, glassGains, 4, n, ex);
    if (sig(n) > 0.02f) fm(v.layer[1], n.hz, 0.6f * n.scale(1), 1.0f, 1.0f, 2.0f, 1.1f, 2.0f, n, 0.35f * sig(n) * 2.0f, 0.2f);
}
void sunkenRuinsControl(Voice& v, const Note&, float) {
    // Under water: the whole note drifts a few cents, slowly.
    const float c = drift(v, 9.0f, 0.5f);
    v.layer[0].cents = c;
    v.layer[1].cents = c;
}

// ======================================================================================
// MOUNTAINS: highland and stone

// Foothills: a shepherd's flute: a low end-blown flute, breathy, its second harmonic
// strong, with the shepherd's breath around it (Breath).
void foothills(Voice& v, const Note& n) {
    wind(v.layer[0], n.hz, 0.07f * n.scale(1), 0.42f, 0.2f, 0.06f, 0.0f, 0.3f, 0.32f, n);
    v.layer[0].wind.air = 0.1f;
    v.layer[0].wind.vibratoDepth = 0.16f;
    v.layer[0].wind.vibratoHz = 4.6f;
    if (sig(n) > 0.02f) {
        auto& L = v.layer[1];
        wind(L, n.hz, 0.1f, 0, 0, 0, 0, 1.0f, 0.0f, n, 0.25f * sig(n));
        L.wind.tone = 0;
        L.wind.breathQ = 2.5f;
    }
}

// Mountain Pass: war drums: big skins tuned to the note, struck with heavy sticks, a slap
// on the rim (Rim).
// The developer's two djembes, pitched down and huge: each stroke has its own keys, low to
// high (the big drum's bass at the bottom, its muted bass, tone, slap and muted slap,
// then the small drum's), pitched further down the lower the key within its stroke; the
// three hits per dynamic are round-robins, the dynamics velocity layers.
struct Stroke { int top; const char* id; float shift; };
constexpr Stroke strokes[] { { 47, "djembe-big-bass", -9 }, { 52, "djembe-big-mbass", -7 }, { 57, "djembe-big-tone", -6 }, { 62, "djembe-big-slap", -5 },
                             { 66, "djembe-big-mslap", -5 }, { 71, "djembe-small-bass", -6 }, { 75, "djembe-small-tone", -4 }, { 79, "djembe-small-slap", -4 },
                             { 83, "djembe-small-mbass", -3 }, { 127, "djembe-small-mslap", -3 } };
bool warDrum(Layer& L, int key, const Note& n, float gain, float decayScale) {
    const auto& st = *std::find_if(std::begin(strokes), std::end(strokes), [&](const Stroke& s) { return key <= s.top; });
    const auto* inst = sampled(st.id);
    if (inst == nullptr || inst->zones.empty()) return false;
    const float root = inst->zones.front().root;
    const float semis = st.shift + 0.4f * float(std::max(key - st.top, -30));
    L.use(M::Sample, gain);
    L.hz = 440.0f * std::exp2((root + semis - 69.0f) / 12.0f);
    L.sample.smoothLayers = true;
    if (!L.sample.start(*inst, root + semis, n.velocity, n.random, n.rate, 0.0f, 2.2f * decayScale, 0.0f, 0.35f, 0, 0, int(std::lround(root)))) {
        L.use(M::None);
        return false;
    }
    return true;
}
void mountainPass(Voice& v, const Note& n) {
    if (warDrum(v.layer[0], n.key, n, 1.0f, n.scale(2))) {
        // Rim: the small drum's slap cracking over the stroke.
        if (sig(n) > 0.02f && n.key <= 71) warDrum(v.layer[1], 77, n, 0.5f * sig(n) * 2.0f, 0.5f);
        return;
    }
    auto& L = v.layer[0];
    L.use(M::Drum);
    L.hz = n.hz;
    L.drum.kind = d::DrumTimpani;
    L.drum.position = 0.25f;
    L.drum.hardness = hardness(n, 0.6f) * (0.5f + 0.5f * n.velocity);
    L.drum.partial = 0.45f;
    L.drum.shellHz = 140;
    L.drum.shell = 0.25f;
    L.drum.bendSeconds = 0.05f;
    L.drum.start(n.hz, 1.0f + 0.05f * n.velocity, 1.8f * n.scale(2), 0.6f * vel(n), 0.04f * vel(n), 600.0f, 0.7f, 0.08f, 0, n.rate, n.noiseAt);
    if (sig(n) > 0.02f) drum(v.layer[1], d::DrumTwoTone, n.hz * 3.0f, 1.0f, 0.04f, 0.06f * vel(n) * sig(n), 0.12f * vel(n) * sig(n), 1700.0f, 1.2f, 0.05f, n);
}

// Frozen Peak: an ice glockenspiel: hard bright bars, ice crackling after the strike (Frost).
void frozenPeak(Voice& v, const Note& n) {
    bell(v.layer[0], n.hz, 2.8f * n.scale(2), hardness(n, 0.95f), 0.45f * vel(n), d::freeBarRatios.data(), barDecays, barGains, 4, n);
    if (sig(n) > 0.02f) drum(v.layer[1], d::DrumTwoTone, n.hz * 4.0f, 1.0f, 0.02f, 0.0f, 0.06f * vel(n) * sig(n), 8500.0f, 3.0f, 0.12f, n);
}

// Dwarven Hall: a dwarven chant (deep voices on "oh") over low brass an octave below
// (Brass).
void dwarvenHall(Voice& v, const Note& n) {
    const float a = 0.12f * n.scale(1);
    // The developer's dwarven chant (an octave below the key): his gravelly "ooh" drone
    // (G2) on the low notes turning into his "aah" drone (A3) above, the grungy voice under
    // them, three dwarves; the low brass an octave below (Brass).
    if (auto* c = vocal(v.layer[0], n.hz, "drone-chant", nullptr, "drone-aah", nullptr, 3, 1.0f)) {
        c->morph = c->morphTarget = d::smoothstep(46.0f, 54.0f, float(n.key));
        c->detuneCents = 10;
        c->timingMs = 50;
        c->spread = 0.5f;
        c->grit = sampled("voice-grit");
        c->gritLevel = 0.22f;
        if (startVocal(v.layer[0], n.hz, n, std::max(a, 0.1f), n.velocity, 1.0f)) {
            if (sig(n) > 0.02f) {
                synth(v.layer[1], d::SynthBrass, n.hz * 0.5f, brassAttack(a, n), 1.0f, 1.0f, n, 0.7f * sig(n) * 2.0f);
                v.layer[1].synth.cutoff = 520.0f * std::exp2(n.offset(0));
                v.layer[1].synth.formantHz = 420;
                v.layer[1].synth.detuneCents = 7;
                brassBite(v.layer[1], n);
            }
            return;
        }
    }
    choir(v.layer[0], n.hz, a, 1.1f, 0.3f + 0.25f * n.offset(0), 0.07f, 10, n, 1.0f);
    v.layer[0].choir.vibratoDepth = 0.05f;
    if (sig(n) > 0.02f) {
        synth(v.layer[1], d::SynthBrass, n.hz * 0.5f, brassAttack(a, n), 1.0f, 1.0f, n, 0.7f * sig(n) * 2.0f);
        v.layer[1].synth.cutoff = 520.0f * std::exp2(n.offset(0));
        v.layer[1].synth.formantHz = 420;
        v.layer[1].synth.detuneCents = 7;
        brassBite(v.layer[1], n);
    }
}

// ======================================================================================
// NECROPOLIS: death and ghosts

// Gates: a dark choir pad: slow, dark voices, an octave below them more, and a faint
// analog breath of a pad (Pad).
void gates(Voice& v, const Note& n) {
    const float a = 0.9f * n.scale(1);
    // The developer's full chest choir stacks (aah, a little ooh), six voices spread wide
    // and slow, and three more an octave below them; dark (the post), the dark hall behind.
    if (auto* c = vocal(v.layer[0], n.hz, "voice-aah", "choir-aah", "voice-ooh", "choir-ooh", 6, 1.0f)) {
        c->morph = c->morphTarget = 0.35f;
        c->detuneCents = 12;
        c->timingMs = 90;
        c->spread = 1.0f;
        c->driftCents = 6;
        if (startVocal(v.layer[0], n.hz, n, a, std::max(0.8f, n.velocity), 0.35f)) {
            if (auto* lo = vocal(v.layer[1], n.hz * 0.5f, "choir-aah", nullptr, "choir-ooh", nullptr, 3, 0.55f)) {
                lo->morph = lo->morphTarget = 0.5f;
                lo->detuneCents = 9;
                lo->timingMs = 120;
                lo->spread = 0.9f;
                startVocal(v.layer[1], n.hz * 0.5f, n, a * 1.3f, 0.9f, 0.35f);
            }
            if (sig(n) > 0.02f) {
                synth(v.layer[2], d::SynthAnalog, n.hz, a * 1.5f, 1.0f, 1.0f, n, 0.22f * sig(n) * 2.0f);
                v.layer[2].synth.cutoff = 380.0f * std::exp2(n.offset(0));
                v.layer[2].synth.envOctaves = 0.8f;
                v.layer[2].synth.detuneCents = 12;
                v.layer[2].synth.subLevel = 0.3f;
            }
            return;
        }
    }
    choir(v.layer[0], n.hz, a, 0.4f, 0.14f + 0.2f * n.offset(0), 0.1f, 14, n, 1.0f, -0.3f);
    choir(v.layer[1], n.hz * 0.5f, a * 1.3f, 0.6f, 0.1f + 0.2f * n.offset(0), 0.08f, 12, n, 0.5f, 0.3f);
    if (sig(n) > 0.02f) {
        synth(v.layer[2], d::SynthAnalog, n.hz, a * 1.5f, 1.0f, 1.0f, n, 0.22f * sig(n) * 2.0f);
        v.layer[2].synth.cutoff = 380.0f * std::exp2(n.offset(0));
        v.layer[2].synth.envOctaves = 0.8f;
        v.layer[2].synth.detuneCents = 12;
        v.layer[2].synth.subLevel = 0.3f;
    }
}

// Graveyard: a ghost violin (the developer's violin), its pitch never quite settling,
// and the bow's wood knocking on the strings around it (Haunt).
void graveyard(Voice& v, const Note& n) {
    const float a = 0.25f * n.scale(1);
    if (sample(v.layer[0], "violin-sustain", n.hz, n, a, 0.0f, 0.06f, 0.6f, 7.0f, 0.4f)) {
        v.layer[0].sample.vibratoDepth = 0.2f;
        v.layer[0].sample.vibratoHz = 4.2f;
    } else {
        strings(v.layer[0], n.hz, a, 1, 0.08f, 0.35f + 0.3f * n.offset(0), 0.35f, n);
        v.layer[0].strings.vibratoDepth = 0.22f;
    }
    if (sig(n) > 0.02f && !sample(v.layer[1], "violin-collegno", n.hz, n, 0.0f, 1.0f, 0.0f, 0.5f, 0, 0, 0.35f * sig(n) * 2.0f, 0.3f))
        drum(v.layer[1], d::DrumTwoTone, n.hz * 2.0f, 1.0f, 0.06f, 0.03f * vel(n) * sig(n), 0.03f * vel(n) * sig(n), 2500.0f, 2.0f, 0.03f, n);
}
void graveyardControl(Voice& v, const Note&, float) {
    v.layer[0].cents = drift(v, 16.0f, 0.4f);
}

// Mausoleum: a tolling bell with a low organ under it while the key is held (Organ).
void mausoleum(Voice& v, const Note& n) {
    d::Bell::Extras ex;
    ex.beatHz = 0.8f;
    ex.variation = 0.15f;
    bell(v.layer[0], n.hz, 6.0f * n.scale(2), hardness(n, 0.55f), 0.5f * vel(n), d::bellRatios.data(), bellDecays, bellGains, 8, n, ex);
    // The developer's piano struck low under the toll (its held notes, an octave below).
    sample(v.layer[2], "piano-long", n.hz * 0.5f, n, 0.0f, 4.0f * n.scale(2), 0.0f, 0.5f, 0, 0, 0.32f * vel(n, 0.4f), -0.1f);
    if (sig(n) > 0.02f) {
        auto& o = organ(v.layer[1], n.hz * 0.5f, 0.5f * sig(n) * 2.0f);
        o.addRank(1.0f, 1.0f);
        o.addRank(2.0f, 0.45f, 1.5f);
        o.addRank(3.0f, 0.15f);
        o.reed = 0.25f;
        o.chiff = 0.1f;
        o.wind = 0.008f;
        o.start(n.hz * 0.5f, 0.25f * n.scale(1), n.rate, n.noiseAt + 5);
    }
}

// ======================================================================================
// WIZARD'S TOWER: arcane

// Stairwell: an ascending harpsichord arpeggio: a held key climbs on its own, note,
// fifth, octave, twelfth, two octaves... at the host's tempo (Rate: eighths to 32nds).
constexpr float arpSteps[7] { 0, 7, 12, 19, 24, 31, 36 };
void harpsichordPluck(Layer& L, float hz, const Note& n, float level) {
    d::Pluck::Shape sh;
    sh.position = 0.08f;
    sh.loss = 0.2f - 0.12f * n.offset(0);
    sh.stiffness = 0.2f;
    sh.pickNoise = 0.6f;
    sh.dampSeconds = 0;
    pluck(L, hz, 2.4f * n.scale(2), 0.8f, level, sh, n);
}
void stairwell(Voice& v, const Note& n) {
    harpsichordPluck(v.layer[0], n.hz, n, 0.85f * (0.6f + 0.4f * n.velocity));
    v.step = 1;
    v.state[0] = 0;
}
void stairwellControl(Voice& v, const Note& n, float dt) {
    if (!v.keyDown && !v.sustained) return;
    v.state[0] += dt;
    // Eighths, sixteenths, sixteenth triplets, 32nds.
    static constexpr float perBeat[4] { 2, 4, 6, 8 };
    const float stepSeconds = float(60.0 / std::max(n.bpm, 20.0)) / perBeat[std::min(3, int(sig(n) * 3.999f))];
    if (v.state[0] < stepSeconds) return;
    v.state[0] -= stepSeconds;
    const int k = v.step % 7;
    ++v.step;
    Note m = n;
    m.noiseAt = n.noiseAt + uint64_t(v.step) * 7919;
    const float hz = n.hz * std::exp2(arpSteps[k] / 12.0f);
    if (hz > 6000.0f) return;
    // The octaves carry the note; the fifths between them are lighter.
    const bool fifth = k == 1 || k == 3 || k == 5;
    harpsichordPluck(v.layer[std::size_t(v.step % 3)], hz, m, 0.85f * (0.6f + 0.4f * n.velocity) * (k == 0 ? 1.0f : (fifth ? 0.45f : 0.75f)));
}

// Library: a soft clavichord: a brass tangent pressing the string (it stops the moment the
// key lets go); Tangent: how hard it presses (brighter).
void library(Voice& v, const Note& n) {
    d::Pluck::Shape sh;
    sh.position = 0.12f;
    sh.loss = 0.5f - 0.25f * sig(n);
    sh.stiffness = 0.05f;
    sh.pickNoise = 0.08f + 0.2f * sig(n);
    pluck(v.layer[0], n.hz, 1.8f * n.scale(2), 0.3f + 0.3f * n.offset(0) + 0.3f * sig(n), 0.7f * vel(n), sh, n);
}

// Alchemy Lab: a bubbling glass marimba synth: an FM bar and glass partials, and bubbles
// rising off each note (Bubbles).
void alchemyLab(Voice& v, const Note& n) {
    fm(v.layer[0], n.hz, 0.002f, 1.1f * n.scale(2), 0.0f, 3.5f, 2.2f + 1.5f * n.offset(0), 0.35f, n, 0.7f);
    bell(v.layer[1], n.hz, 1.6f * n.scale(2), hardness(n, 0.6f), 0.35f * vel(n), d::marimbaRatios.data(), marimbaDecays, marimbaGains, 3, n);
    v.step = int(2 + 5 * sig(n));
    v.state[0] = 0.05f;
}
void alchemyLabControl(Voice& v, const Note& n, float dt) {
    if (v.step <= 0 || sig(n) < 0.02f) return;
    v.state[0] -= dt;
    if (v.state[0] > 0.0f) return;
    const float r = d::unipolar(d::mix64(uint64_t(v.order) * 613 + uint64_t(v.step)));
    v.state[0] = 0.06f + 0.2f * r;
    --v.step;
    // A bubble: a single mode chirping upward (a little rising glide, as bubbles do).
    d::Bell::Extras ex;
    ex.glideSemis = 5.0f + 7.0f * r;
    ex.glideSeconds = 0.05f;
    bell(v.layer[2], n.hz * (2.0f + 2.0f * r), 0.18f, 0.9f, 0.06f * sig(n), flat, flat, flat, 1, n, ex, 1.0f, 0.7f * (r - 0.5f));
}

// Observatory: a celestial synth pad (the developer's soft pad patch), with starlight an
// octave above (Stars).
void observatory(Voice& v, const Note& n) {
    const float a = 0.35f * n.scale(1);
    if (!sample(v.layer[0], "mlg-softpad-long", n.hz, n, a, 0.0f, 0.0f, 0.5f, 7.0f, 0.5f)) {
        synth(v.layer[0], d::SynthAnalog, n.hz, a, 1.0f, 1.0f, n);
        v.layer[0].synth.cutoff = 900.0f * std::exp2(n.offset(0));
        v.layer[0].synth.envOctaves = 1.2f;
        v.layer[0].synth.detuneCents = 12;
        v.layer[0].synth.subLevel = 0.2f;
    }
    if (sig(n) > 0.02f) fm(v.layer[1], n.hz * 2.0f, a * 2.0f, 1.0f, 1.0f, 3.0f, 0.6f, 3.0f, n, 0.18f * sig(n) * 2.0f, 0.4f);
}

// --------------------------------------------------------------------------------------
// The table, in place order (location by location, the scene first).
constexpr std::array<const char*, macroCount> M5(const char* a, const char* b, const char* c, const char* d4, const char* e) { return { a, b, c, d4, e }; }

const std::array<Design, instrumentCount>& table() {
    static const std::array<Design, instrumentCount> t { {
        // Dungeon
        { "Contrabass Pizzicato", M5("Tone", "Pluck", "Ring", "Body", "Snap"), "Vibrato", ModWheel::Vibrato, 8, 0.25f, -12, false,
          withBody(stringPost(0.28f, 3600, 0.8f), { { 78, 3, 0.7f }, { 129, 4, 0.6f }, { 600, 1.2f, 0.25f } }), corridors, nullptr },
        { "Cello Drone-Lead", M5("Tone", "Bow", "Release", "Body", "Drone"), "Vibrato", ModWheel::Vibrato, 6, 0.35f, 0, true,
          stringPost(0.45f, 5000, 0.7f), entranceHall, nullptr },
        { "Iron Chains", M5("Tone", "Strike", "Ring", "Weight", "Rattle"), "Swell", ModWheel::Swell, 8, 0.6f, 0, false,
          plainPost(9000, 60), prisonCells, prisonCellsControl },
        { "Golden Celesta", M5("Tone", "Strike", "Ring", "Body", "Glint"), "Brightness", ModWheel::Brightness, 12, 0.5f, 12, false,
          withBody(plainPost(9500, 80), { { 2400, 1.5f, 0.25f } }), treasureRoom, treasureRoomControl },
        { "Doom Brass & Choir", M5("Tone", "Attack", "Release", "Weight", "Choir"), "Growl", ModWheel::Growl, 6, 0.4f, 0, true,
          withBody(plainPost(5000, 30, 0.35f), { { 180, 1.5f, 0.4f } }), finalBossLair, finalBossLairControl },
        // Castle
        { "Lute", M5("Tone", "Pluck", "Ring", "Body", "Course"), "Vibrato", ModWheel::Vibrato, 10, 0.35f, 0, false,
          bodyPost(d::BodyLute, 9000, 0.9f, 60), courtyard, nullptr },
        { "Brass Fanfare", M5("Tone", "Attack", "Release", "Body", "Section"), "Vibrato", ModWheel::Vibrato, 6, 0.25f, 0, true,
          plainPost(9000, 40), greatHall, nullptr },
        { "Royal Strings", M5("Tone", "Bow", "Release", "Body", "Low Section"), "Vibrato", ModWheel::Vibrato, 8, 0.5f, 0, true,
          stringPost(1.0f, 6500, 0.5f), throneRoom, nullptr },
        { "Chamber Organ", M5("Tone", "Speech", "Release", "Body", "Stops"), "Tremulant", ModWheel::Tremolo, 10, 0.12f, 0, true,
          withBody(plainPost(7500, 40, 0, 0.2f), { { 500, 1.0f, 0.2f } }), chapel, nullptr },
        { "Heroic Horn", M5("Tone", "Attack", "Release", "Body", "Bell Up"), "Vibrato", ModWheel::Vibrato, 6, 0.3f, 0, true,
          plainPost(6500, 40), towerTop, nullptr },
        // Cathedral
        { "Soft Choir", M5("Tone", "Attack", "Release", "Breath", "Open"), "Vibrato", ModWheel::Vibrato, 8, 0.6f, 0, true,
          plainPost(9000, 70, 0, 0.4f), cloister, nullptr },
        { "Pipe Organ", M5("Tone", "Speech", "Release", "Body", "Reeds"), "Swell", ModWheel::Swell, 10, 0.3f, 0, true,
          plainPost(10000, 25, 0, 0.1f), nave, nullptr },
        { "Monk Chant", M5("Tone", "Attack", "Release", "Breath", "Open"), "Vibrato", ModWheel::Vibrato, 6, 0.6f, 0, true,
          plainPost(5000, 35, 0, 0.4f), crypt, nullptr },
        { "Church Bells", M5("Tone", "Strike", "Ring", "Body", "Clapper"), "Brightness", ModWheel::Brightness, 8, 4.0f, 0, false,
          plainPost(9000, 40), bellTower, nullptr },
        // Haunted Forest
        { "Wooden Flute", M5("Tone", "Attack", "Release", "Body", "Detune"), "Vibrato", ModWheel::Vibrato, 4, 0.2f, 0, true,
          plainPost(6000, 100, 0, 0.5f), forestEdge, nullptr },
        { "Cello Harmonics", M5("Tone", "Bow", "Release", "Body", "Creak"), "Vibrato", ModWheel::Vibrato, 6, 0.5f, 0, true,
          stringPost(0.45f, 7000, 0.5f), oldPath, oldPathControl },
        { "Music Box", M5("Tone", "Pluck", "Ring", "Body", "Wear"), "Vibrato", ModWheel::Vibrato, 12, 0.8f, 12, false,
          withBody(plainPost(9000, 150), { { 900, 1.2f, 0.3f } }), witchsHut, witchsHutControl },
        { "Ancient Harp", M5("Tone", "Pluck", "Ring", "Body", "Whispers"), "Vibrato", ModWheel::Vibrato, 12, 0.7f, 0, false,
          bodyPost(d::BodyHarp, 8000, 0.8f, 40), ancientGrove, ancientGroveControl },
        // Caverns
        { "Stone Marimba", M5("Tone", "Mallet", "Ring", "Resonator", "Flicker"), "Brightness", ModWheel::Brightness, 10, 0.5f, 0, false,
          plainPost(6000, 40), caveMouth, caveMouthControl },
        { "Crystal Bells", M5("Tone", "Strike", "Ring", "Body", "Shimmer"), "Brightness", ModWheel::Brightness, 10, 1.5f, 12, false,
          plainPost(14000, 120), crystalCavern, nullptr },
        { "Water Kalimba", M5("Tone", "Pluck", "Ring", "Body", "Ripple"), "Vibrato", ModWheel::Vibrato, 10, 0.6f, 0, false,
          withBody(plainPost(7000, 60), { { 420, 2.0f, 0.3f } }), undergroundLake, undergroundLakeControl },
        { "Hoard Gong", M5("Tone", "Mallet", "Ring", "Weight", "Growl"), "Growl", ModWheel::Growl, 6, 3.0f, 0, false,
          plainPost(6000, 25, 0.2f), dragonsHoard, nullptr },
        // Village
        { "Tin Whistle", M5("Tone", "Attack", "Release", "Body", "Breath"), "Vibrato", ModWheel::Vibrato, 4, 0.12f, 12, true,
          plainPost(7000, 120, 0, 0.5f), villageGreen, nullptr },
        { "Weary Guitar", M5("Tone", "Pluck", "Ring", "Body", "Weariness"), "Vibrato", ModWheel::Vibrato, 10, 0.3f, 0, false,
          bodyPost(d::BodyGuitar, 3800, 0.6f, 50), tavern, nullptr },
        { "Fiddle", M5("Tone", "Bow", "Release", "Body", "Rasp"), "Vibrato", ModWheel::Vibrato, 6, 0.25f, 0, true,
          stringPost(1.0f, 7000, 0.6f), marketSquare, nullptr },
        { "Tuned Anvil", M5("Tone", "Hammer", "Ring", "Body", "Clank"), "Brightness", ModWheel::Brightness, 8, 1.0f, 0, false,
          plainPost(10000, 80), blacksmith, nullptr },
        // Swamp
        { "Detuned Banjo", M5("Tone", "Pluck", "Ring", "Head", "Detune"), "Vibrato", ModWheel::Vibrato, 10, 0.2f, 0, false,
          withBody(plainPost(8000, 90), { { 360, 3.0f, 0.6f }, { 2400, 1.5f, 0.4f } }), mire, nullptr },
        { "Murky Reed", M5("Tone", "Attack", "Release", "Body", "Murk"), "Vibrato", ModWheel::Vibrato, 4, 0.15f, 0, true,
          plainPost(2200, 30, 0.1f, 0.6f), bog, nullptr },
        { "Drowned Bells", M5("Tone", "Strike", "Ring", "Body", "Pad"), "Vibrato", ModWheel::Vibrato, 10, 1.5f, 0, false,
          plainPost(3200, 40), sunkenRuins, sunkenRuinsControl },
        // Mountains
        { "Shepherd's Flute", M5("Tone", "Attack", "Release", "Body", "Breath"), "Vibrato", ModWheel::Vibrato, 4, 0.25f, 0, true,
          plainPost(5000, 80, 0, 0.5f), foothills, nullptr },
        { "War Drums", M5("Tone", "Stick", "Ring", "Shell", "Rim"), "Brightness", ModWheel::Brightness, 8, 1.2f, 0, false,
          plainPost(5000, 25), mountainPass, nullptr },
        { "Ice Glockenspiel", M5("Tone", "Mallet", "Ring", "Body", "Frost"), "Brightness", ModWheel::Brightness, 12, 0.8f, 24, false,
          plainPost(16000, 200), frozenPeak, nullptr },
        { "Dwarven Chant", M5("Tone", "Attack", "Release", "Breath", "Brass"), "Vibrato", ModWheel::Vibrato, 6, 0.5f, -12, true,
          plainPost(5500, 30, 0.08f, 0.4f), dwarvenHall, nullptr },
        // Necropolis
        { "Dark Choir Pad", M5("Tone", "Attack", "Release", "Breath", "Pad"), "Swell", ModWheel::Swell, 8, 1.2f, 0, true,
          plainPost(4200, 40, 0, 0.3f), gates, nullptr },
        { "Ghost Violin", M5("Tone", "Bow", "Release", "Body", "Haunt"), "Vibrato", ModWheel::Vibrato, 6, 0.6f, 0, true,
          stringPost(1.0f, 7000, 0.5f), graveyard, graveyardControl },
        { "Tolling Bell & Organ", M5("Tone", "Strike", "Ring", "Body", "Organ"), "Tremolo", ModWheel::Tremolo, 6, 3.0f, 0, false,
          plainPost(7000, 30), mausoleum, nullptr },
        // Wizard's Tower
        { "Harpsichord Arpeggio", M5("Tone", "Pluck", "Ring", "Body", "Rate"), "Brightness", ModWheel::Brightness, 8, 0.25f, 0, false,
          bodyPost(d::BodyHarpsichord, 11000, 0.7f, 60), stairwell, stairwellControl },
        { "Clavichord", M5("Tone", "Touch", "Ring", "Body", "Tangent"), "Bebung", ModWheel::Vibrato, 10, 0.06f, 0, false,
          bodyPost(d::BodyZither, 6000, 0.7f, 80), library, nullptr },
        { "Glass Marimba", M5("Tone", "Mallet", "Ring", "Body", "Bubbles"), "Brightness", ModWheel::Brightness, 10, 0.6f, 0, false,
          plainPost(10000, 80), alchemyLab, alchemyLabControl },
        { "Celestial Pad", M5("Tone", "Attack", "Release", "Body", "Stars"), "Swell", ModWheel::Swell, 10, 1.4f, 0, true,
          plainPost(9000, 40, 0, 0.3f), observatory, nullptr },
    } };
    return t;
}
} // namespace

const Design& design(int index) noexcept { return table()[std::size_t(std::clamp(index, 0, instrumentCount - 1))]; }

namespace {
// Place order: location by location, the scene (sub-location 0) first.
int firstOf(int location) noexcept {
    int f = 0;
    for (int l = 0; l < location && l < LocationCount; ++l) f += subLocationCount(l) + 1;
    return f;
}
}

int designFor(int location, int sub) noexcept {
    location = std::clamp(location, 0, LocationCount - 1);
    return std::clamp(firstOf(location) + std::clamp(sub, 0, subLocationCount(location)), 0, instrumentCount - 1);
}
int locationOf(int index) noexcept {
    index = std::clamp(index, 0, instrumentCount - 1);
    for (int l = 0; l < LocationCount; ++l)
        if (index < firstOf(l + 1)) return l;
    return LocationCount - 1;
}
int subOf(int index) noexcept { return std::clamp(index, 0, instrumentCount - 1) - firstOf(locationOf(index)); }

float designTrimDb(int index) noexcept { return trims[std::size_t(std::clamp(index, 0, instrumentCount - 1))]; }
float standInTrimDb(int index) noexcept { return standInTrims[std::size_t(std::clamp(index, 0, instrumentCount - 1))]; }
bool usesRecordings(int index) noexcept {
    const int l = locationOf(index), k = subOf(index);
    return (l == Castle && k == 0) || (l == Caverns && k == 2) || (l == Village && (k == 1 || k == 2)) || (l == Necropolis && k == 1) || (l == WizardsTower && k == 3)
           // Session 8: the voice, the banjo, the djembes and the piano.
           || (l == Dungeon && k == 4) || (l == Cathedral && (k == 0 || k == 2)) || (l == HauntedForest && (k == 2 || k == 3)) || (l == Swamp && k == 0)
           || (l == Mountains && (k == 1 || k == 3)) || (l == Necropolis && (k == 0 || k == 2));
}
float placeTrimDb(int location, int sub) noexcept { return placeTrims[std::size_t(designFor(location, sub))]; }

// The instruments that carry one of the developer's effect captures with them.
CaptureSend captureOf(int index) noexcept {
    const int l = locationOf(index), k = subOf(index);
    if (l == Caverns && k == 1) return { CaptureShimmer, 0.2f };       // Crystal Bells: the shimmer (Direction Update 3)
    if (l == WizardsTower && k == 3) return { CaptureShimmer, 0.22f };  // Celestial Pad: starlight
    if (l == Dungeon && k == 4) return { CaptureDarkHall, 0.2f };       // Doom Brass: the dark hall behind it
    if (l == Necropolis && k == 0) return { CaptureDarkHall, 0.22f };   // Dark Choir Pad
    if (l == Caverns && k == 2) return { CaptureChorusVerb, 0.22f };    // Water Kalimba: rippling
    if (l == Swamp && k == 2) return { CaptureChorusVerb, 0.25f };      // Drowned Bells
    return {};
}
}

namespace dc::inst {

// Session 11. Indices in place order: see the table.
namespace {
enum class Role { Other, Brass, Strings, Drums, Pluck, Chant };
Role roleOf(int index) noexcept {
    switch (index) {
        case 4: case 6: case 9: return Role::Brass;                    // Doom Brass, Fanfare, Horn
        case 0: case 1: case 7: case 15: case 24: case 34: return Role::Strings;
        case 30: case 25: case 21: return Role::Drums;                 // War Drums, Anvil, Gong
        case 5: case 17: case 23: case 26: case 36: case 37: return Role::Pluck;   // Lute, Harp, Guitar, Banjo, Harpsichord, Clavichord
        case 32: return Role::Chant;                                   // Dwarven Chant (with its low brass)
        default: return Role::Other;
    }
}
}

MixFit mixFit(int index, int sound) noexcept {
    // The Weary Guitar's Tavern is a small, nearly dry room: nothing to take back there.
    if (sound < SoundMix || index == 23) return {};
    const bool horns = sound >= SoundHorns;
    switch (roleOf(index)) {
        // Session 12: the brass drier still, its reverb later and dipping under the hit
        // (the dry attack lands first; the tail blooms between the notes).
        case Role::Brass: {
            if (!horns) return { 0.6f, 0.75f, 30.0f, 0.0f };
            // Make-up in its place (measured with the horn phrase: the in-place change equals
            // the dry one): the fanfare 4.4 dB, the doom brass 3.8, the horn 2.3.
            const float makeup = index == 6 ? 4.4f : index == 4 ? 3.8f : 2.3f;
            return { 0.5f, 0.65f, 45.0f, 0.35f, makeup };
        }
        case Role::Strings: return { 0.75f, 0.85f, 15.0f };
        case Role::Drums: return { 0.6f, 0.7f, 20.0f };
        case Role::Pluck: return { 0.75f, 0.85f, 10.0f };
        case Role::Chant: return horns ? MixFit { 0.75f, 0.85f, 30.0f, 0.15f, 2.1f } : MixFit { 0.8f, 0.9f, 20.0f, 0.0f };
        default: return {};
    }
}

float designPunchDb(int index, int sound) noexcept {
    if (sound < SoundMix) return 0.0f;
    const bool horns = sound >= SoundHorns;
    switch (roleOf(index)) {
        // Session 12: the horns hit much harder (9 dB in Session 11).
        // The Final Boss Lair's doom brass under its choir stab and piano: more (17 dB); the
        // Great Hall's fanfare, already bright and hot, +10 (the instruments' level check).
        case Role::Brass: return horns ? (index == 4 ? 17.0f : index == 6 ? 10.0f : 12.0f) : 9.0f;
        // The War Drums' recorded djembes change velocity layer as they get harder; a large
        // boost on the hard slap's sharp transient made that step jump (CI: 8.4 dB at 7 dB of
        // Punch, 6.8 at 3.5; the limit is 6.5): 2 dB.
        case Role::Drums: return index == 30 ? 2.0f : 4.0f;
        case Role::Chant: return horns ? 7.0f : 4.0f;   // its low brass
        case Role::Pluck: return 2.0f;
        case Role::Strings: return index == 0 ? 2.0f : 0.0f;   // the pizzicato is a pluck
        default: return 0.0f;
    }
}

float punchMaxDb(int index, int sound) noexcept { return sound >= SoundHorns && roleOf(index) == Role::Brass ? 18.0f : 12.0f; }

float punchDb(int index, float punch, int sound) noexcept {
    const float own = designPunchDb(index, sound), most = punchMaxDb(index, sound);
    punch = std::clamp(std::isfinite(punch) ? punch : 1.0f, 0.0f, 2.0f);
    return std::clamp(punch <= 1.0f ? own * punch : own + (punch - 1.0f) * (most - own), 0.0f, most);
}

bool brassPresence(int index, int sound) noexcept { return sound >= SoundMix && roleOf(index) == Role::Brass; }

float attackEdge(int index, int sound) noexcept {
    if (sound < SoundHorns) return 0.0f;
    const auto role = roleOf(index);
    // The fanfare is bright and hot already: a lighter edge (the instruments' level check).
    return role == Role::Brass ? (index == 6 ? 0.5f : 1.0f) : role == Role::Chant ? 0.45f : 0.0f;
}

float punchVelocityScale(int index, float velocity, int sound) noexcept {
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    // Session 12: the horns' hit follows the velocity more steeply (a soft note blows in,
    // a hard one cracks); the others as in Session 11.
    const auto role = roleOf(index);
    if (sound >= SoundHorns && (role == Role::Brass || role == Role::Chant)) return 0.12f + 0.88f * std::pow(velocity, 1.6f);
    return 0.3f + 0.7f * velocity;
}

// The instruments that sing with the recorded voice (their start functions use vocal()):
// the panel shows Vowel and Ensemble for these (dimmed for the others).
bool designUsesVoice(int index) noexcept {
    const auto start = design(index).start;
    for (auto* f : { &finalBossLair, &cloister, &crypt, &witchsHut, &dwarvenHall, &gates })
        if (start == f) return true;
    return false;
}
}
