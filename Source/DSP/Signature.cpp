#include "Modules.h"

// VIII. SIGNATURE: one effect per place (Direction Update 1, Milestone 6). Every place
// of the world uses a different kind, on top of its room, so that any two places are
// tellable apart blind. A kind is a recipe of small primitives:
//
//   serial (crossfaded by Amount):  saturation flavour -> tone -> attack softening ->
//                                   wobble (modulated delay) -> tremolo
//   parallel (added, x Amount):     tuned feedback combs, a modal bank (optionally struck
//                                   only by attacks), pitch layers (continuous, on
//                                   attacks, or twinkling; optional vowel formants and
//                                   a slow bloom), a tap pattern (fixed, scattering,
//                                   crackling, circling, diffused), smear, reverse swell
//
// Tone (-1..1, 0 = as designed) transposes resonances by up to an octave in semitone
// steps and tilts filters; Rate (x0.25..x4) scales every modulation rate.
namespace dc {
using namespace dsp;

namespace sig {
enum Sat { SatNone, SatWarm, SatGrowl, SatMenace, SatGrit };
enum Tone { ToneNone, ToneLow, ToneHigh, ToneBell, ToneShelf };
enum Shape { Sine, SmoothRandom, Bubbling, Slosh, Crooked };
enum TapMode { TapsFixed, TapsScatter, TapsCrackle, TapsCircle, TapsDiffuse };
enum PitchMode { PitchContinuous, PitchTransient, PitchTwinkle };
enum Formant { FormantNone, FormantAh, FormantOh };

// Fields in declaration order (designated initialisers must follow it).
struct Recipe {
    Sat sat = SatNone; float drive = 0;                                     // dB
    Tone tone = ToneNone; float toneHz = 1000, toneQ = .7f, toneDb = 0;
    float swellMs = 0;                                                      // attack softening
    float wobbleMs = 0, wobbleHz = 0; Shape wobbleShape = Sine; float wobbleMix = 1;
    float tremDepth = 0, tremHz = 0; Shape tremShape = Sine; bool tremLayersOnly = false;
    int combs = 0; float combHz[Signature::maxCombs] {}; float combFeedback = 0, combDamp = 0, combGain = 0;
    int modes = 0; float modeHz[Signature::maxModes] {}; float modeT60 = 1, modeGain = 0; bool modeStruck = false;
    int pitches = 0; float pitchRatio[2] {}; float pitchGain = 0; PitchMode pitchMode = PitchContinuous;
    float pitchHp = 20, pitchLp = 20000; Formant formant = FormantNone; float bloomMs = 0;
    int taps = 0; float tapMs[Signature::maxTaps] {}; float tapGain[Signature::maxTaps] {}; float tapPan[Signature::maxTaps] {};
    float tapLp = 20000, tapDarken = 1; TapMode tapMode = TapsFixed; float tapRate = .1f;
    float smear = 0, smearMs = 30;
    float reverseMs = 0, reverseGain = 0, reverseLp = 20000;
};

// The 40 signatures, in kind order (Locations.cpp assigns one to each place).
const Recipe recipes[Signature::kinds] = {
    {},   // 0 None
    // ---- Dungeon
    // 1 Corridors, Receding Footsteps: a train of short reflections down the passage,
    // each darker than the last.
    { .taps = 4, .tapMs = { 63, 131, 204, 282 }, .tapGain = { .5f, .36f, .25f, .17f }, .tapPan = { -.4f, .35f, -.3f, .25f },
      .tapLp = 4500, .tapDarken = .7f },
    // 2 Entrance Hall, Stone Flutter: a flutter between parallel stone walls, and early
    // reflections that crackle like torchlight.
    { .combs = 1, .combHz = { 88 }, .combFeedback = .62f, .combDamp = .55f, .combGain = .5f,
      .taps = 3, .tapMs = { 17, 29, 43 }, .tapGain = { .3f, .25f, .2f }, .tapPan = { -.7f, .6f, -.2f },
      .tapLp = 6000, .tapDarken = .85f, .tapMode = TapsCrackle },
    // 3 Prison Cells, Bar Resonance: tuned iron bars ringing (combs), a muffled cell.
    { .tone = ToneLow, .toneHz = 2300, .toneQ = .8f,
      .combs = 3, .combHz = { 146.8f, 220.0f, 311.1f }, .combFeedback = .93f, .combDamp = .12f, .combGain = .55f },
    // 4 Treasure Room, Glitter Sparkle: an octave and an octave + fifth glitter on every
    // attack, twinkling.
    { .pitches = 2, .pitchRatio = { 2.0f, 3.0f }, .pitchGain = .8f, .pitchMode = PitchTransient, .pitchHp = 1800, .pitchLp = 8000 },
    // 5 Final Boss Lair, Sub Choir Menace: a sub-octave choir ("oh"), menacing drive and
    // slow ominous repeats.
    { .sat = SatMenace, .drive = 12,
      .pitches = 2, .pitchRatio = { .5f, .4983f }, .pitchGain = .6f, .pitchLp = 2400, .formant = FormantOh,
      .taps = 2, .tapMs = { 640, 1280 }, .tapGain = { .3f, .16f }, .tapPan = { .5f, -.5f }, .tapLp = 1800, .tapDarken = .7f },
    // ---- Castle
    // 6 Courtyard, Wall Ping-Pong: a quick bounce between the two walls, left-right.
    // Direction Update 2: fluttering (scattered), a little longer.
    { .taps = 6, .tapMs = { 38, 76, 114, 152, 190, 228 }, .tapGain = { .5f, .4f, .32f, .25f, .19f, .14f }, .tapPan = { -.9f, .9f, -.9f, .9f, -.9f, .9f },
      .tapLp = 9000, .tapDarken = .9f, .tapMode = TapsScatter, .tapRate = .5f },
    // 7 Great Hall, Banner Reflections: a long, soft spread of early reflections,
    // softened by banners and swaying gently.
    { .taps = 6, .tapMs = { 23, 37, 52, 71, 93, 118 }, .tapGain = { .3f, .28f, .26f, .23f, .2f, .17f }, .tapPan = { -.8f, .7f, -.5f, .9f, -.9f, .4f },
      .tapLp = 4200, .tapDarken = .93f, .tapMode = TapsScatter, .tapRate = .15f },
    // 8 Throne Room, Regal Brass: brass-like harmonic resonances and a bright bite.
    { .tone = ToneBell, .toneHz = 1300, .toneQ = .9f, .toneDb = 2.5f,
      .modes = 6, .modeHz = { 220, 330, 440, 550, 660, 880 }, .modeT60 = .7f, .modeGain = .5f },
    // 9 Chapel, Choir Pad Bloom: a sung "ah" pad blooming slowly under the instrument.
    { .pitches = 2, .pitchRatio = { 1.0035f, 2.0f }, .pitchGain = .5f, .pitchLp = 3600, .formant = FormantAh, .bloomMs = 700,
      .smear = .55f, .smearMs = 45 },
    // 10 Tower Top, Wind-Blown Echoes: sparse long echoes that waver in gusts.
    { .tremDepth = .25f, .tremHz = .35f, .tremShape = SmoothRandom, .tremLayersOnly = true,
      .taps = 2, .tapMs = { 610, 1220 }, .tapGain = { .38f, .22f }, .tapPan = { .8f, -.8f }, .tapLp = 3500, .tapDarken = .7f,
      .tapMode = TapsScatter, .tapRate = .4f },
    // ---- Cathedral
    // 11 Cloister, Colonnade: evenly spaced reflections from the columns (a picket of
    // echoes).
    { .taps = 6, .tapMs = { 18, 36, 54, 72, 90, 108 }, .tapGain = { .26f, .24f, .22f, .2f, .18f, .16f }, .tapPan = { -.3f, .3f, -.3f, .3f, -.3f, .3f },
      .tapLp = 8000, .tapDarken = .95f },
    // 12 Nave, Vault Cascade: long, diffused cascades from the vault far above.
    { .taps = 4, .tapMs = { 170, 340, 530, 750 }, .tapGain = { .32f, .23f, .16f, .1f }, .tapPan = { -.9f, .9f, -.6f, .6f },
      .tapLp = 5000, .tapDarken = .82f, .tapMode = TapsDiffuse },
    // 13 Crypt, Stone Resonance: low-mid stone modes, dark and close.
    { .modes = 4, .modeHz = { 163, 217, 291, 384 }, .modeT60 = .9f, .modeGain = .65f },
    // 14 Bell Tower, Bell Resonators: a minor-third church bell struck by every attack.
    // Direction Update 2: a real bell's partials on D4 (hum 0.5, prime 1, minor-third
    // tierce 1.2, quint 1.5, nominal 2, superquint 3), longer and louder.
    { .modes = 6, .modeHz = { 146.8f, 293.7f, 352.4f, 440.5f, 587.3f, 881.0f }, .modeT60 = 6.0f, .modeGain = .7f, .modeStruck = true },
    // ---- Haunted Forest
    // 15 Forest Edge, Trunk Scatter: echoes scattered off tree trunks, drifting.
    // Direction Update 2: a dark shadow a semitone below, wavering.
    { .wobbleMs = 1.2f, .wobbleHz = .4f, .wobbleShape = SmoothRandom, .wobbleMix = .35f,
      .pitches = 1, .pitchRatio = { .9439f }, .pitchGain = .4f, .pitchMode = PitchContinuous, .pitchHp = 120, .pitchLp = 2800,
      .taps = 6, .tapMs = { 41, 77, 109, 158, 203, 262 }, .tapGain = { .22f, .2f, .2f, .17f, .15f, .12f }, .tapPan = { .8f, -.6f, .3f, -.9f, .6f, -.2f },
      .tapLp = 3500, .tapDarken = .9f, .tapMode = TapsScatter, .tapRate = .22f },
    // 16 Old Path, Spectral Smear: every note smeared into a ghostly blur, slightly adrift.
    { .wobbleMs = .8f, .wobbleHz = .2f, .wobbleShape = SmoothRandom, .wobbleMix = .5f,
      .smear = .8f, .smearMs = 70 },
    // 17 Witch's Hut, Crooked Wobble: a crooked, bubbling pitch wobble in a woody box.
    { .tone = ToneBell, .toneHz = 620, .toneQ = 1.1f, .toneDb = 3,
      .wobbleMs = 2.4f, .wobbleHz = 2.3f, .wobbleShape = Crooked, .wobbleMix = .5f },
    // 18 Ancient Grove, Breathing Shimmer: an octave of mystic shimmer, breathing slowly.
    { .tremDepth = .6f, .tremHz = .12f, .tremShape = Sine, .tremLayersOnly = true,
      .pitches = 1, .pitchRatio = { 2.0f }, .pitchGain = .6f, .pitchHp = 1200, .smear = .6f, .smearMs = 60 },
    // ---- Caverns
    // 19 Cave Mouth, Cave Hum: the deep hum of the cave's mouth and a far answer.
    // Direction Update 2: firelight flickering on the hum and the answer.
    { .tremDepth = .55f, .tremHz = 3.2f, .tremShape = SmoothRandom, .tremLayersOnly = true,
      .modes = 3, .modeHz = { 72, 108, 151 }, .modeT60 = 1.4f, .modeGain = .6f,
      .taps = 3, .tapMs = { 23, 41, 430 }, .tapGain = { .32f, .26f, .3f }, .tapPan = { -.7f, .7f, .4f }, .tapLp = 2600 },
    // 20 Crystal Cavern, Glass Ring: glassy, inharmonic modal ringing.
    { .modes = 5, .modeHz = { 1318.5f, 2489.0f, 3184.0f, 3638.0f, 4620.0f }, .modeT60 = 3.5f, .modeGain = .6f, .modeStruck = true },
    // 21 Underground Lake, Underwater: muffled as if underwater, a slow wobble, a long
    // watery tail.
    { .tone = ToneLow, .toneHz = 950, .toneQ = .9f,
      .wobbleMs = 2.6f, .wobbleHz = .35f, .wobbleShape = Sine, .wobbleMix = .6f,
      .taps = 2, .tapMs = { 720, 1440 }, .tapGain = { .3f, .18f }, .tapPan = { -.6f, .6f }, .tapLp = 900, .tapDarken = .8f,
      .tapMode = TapsScatter, .tapRate = .3f },
    // 22 Dragon's Hoard, Growl: low growl saturation with a sub-octave snarl.
    { .sat = SatGrowl, .drive = 9, .tone = ToneLow, .toneHz = 3200, .toneQ = .7f },
    // ---- Village
    // 23 Village Green, Folk Chorus: a lively doubling, like a second player.
    { .wobbleMs = 3.5f, .wobbleHz = .8f, .wobbleShape = Sine, .wobbleMix = .3f },
    // 24 Tavern, Warm Tavern: warm saturation, softened highs, a crowded-room smear.
    { .sat = SatWarm, .drive = 9, .tone = ToneShelf, .toneHz = 5500, .toneDb = -4, .smear = .32f, .smearMs = 18 },
    // 25 Market Square, Slap-Back: a hard slap off the buildings across the square.
    { .taps = 3, .tapMs = { 96, 129, 178 }, .tapGain = { .55f, .45f, .2f }, .tapPan = { -.8f, .8f, 0 }, .tapLp = 3000, .tapDarken = .75f },
    // 26 Blacksmith, Anvil: an anvil ringing on every attack, gritty.
    { .sat = SatGrit, .drive = 6,
      .modes = 6, .modeHz = { 880, 1294, 1839, 2253, 2983, 3670 }, .modeT60 = 1.4f, .modeGain = .75f, .modeStruck = true },
    // ---- Swamp
    // 27 Mire, Mud Drag: attacks dragged through mud, a sticky smear.
    { .tone = ToneShelf, .toneHz = 3000, .toneDb = -3, .swellMs = 60, .smear = .3f, .smearMs = 35 },
    // 28 Bog, Murky Wobble: murky, low-passed, wobbling.
    { .tone = ToneLow, .toneHz = 720, .toneQ = 1.3f, .wobbleMs = 1.8f, .wobbleHz = .6f, .wobbleShape = SmoothRandom, .wobbleMix = .65f },
    // 29 Sunken Ruins, Sloshing: half under water, filtered and sloshing back and forth.
    { .tone = ToneLow, .toneHz = 1400, .toneQ = 1.0f, .wobbleMs = 3.2f, .wobbleHz = .3f, .wobbleShape = Slosh, .wobbleMix = .5f,
      .tremDepth = .22f, .tremHz = .3f, .tremShape = Slosh },
    // ---- Mountains
    // 30 Foothills, Valley Answer: answers from hill after hill, scattered across the
    // stereo field and fluttering, each more distant (Direction Update 2).
    { .taps = 5, .tapMs = { 290, 530, 790, 1110, 1460 }, .tapGain = { .62f, .52f, .42f, .32f, .22f }, .tapPan = { -.9f, .85f, -.55f, .95f, -.7f },
      .tapLp = 3600, .tapDarken = .75f, .tapMode = TapsScatter, .tapRate = .35f },
    // 31 Mountain Pass, Canyon Echoes: discrete echoes receding down the canyon.
    { .taps = 5, .tapMs = { 380, 760, 1140, 1520, 1900 }, .tapGain = { .55f, .4f, .28f, .19f, .12f }, .tapPan = { .7f, -.6f, .5f, -.4f, .3f },
      .tapLp = 6000, .tapDarken = .72f },
    // 32 Frozen Peak, Icy Shimmer: a thin, icy shimmer two octaves up, twinkling.
    { .pitches = 1, .pitchRatio = { 4.0f }, .pitchGain = .24f, .pitchMode = PitchTwinkle, .pitchHp = 3500, .pitchLp = 8000,
      .taps = 1, .tapMs = { 900 }, .tapGain = { .2f }, .tapPan = { -.7f }, .tapLp = 8000 },
    // 33 Dwarven Hall, Forge Resonance: a warm, low harmonic resonance like a forge.
    { .tone = ToneBell, .toneHz = 180, .toneQ = .8f, .toneDb = 3.5f,
      .modes = 4, .modeHz = { 55, 110, 165, 220 }, .modeT60 = 1.5f, .modeGain = .5f },
    // ---- Necropolis
    // 34 Gates, Iron Groan: rusted iron gates groaning with every attack.
    { .modes = 4, .modeHz = { 98, 189, 281, 402 }, .modeT60 = 2.2f, .modeGain = .55f, .modeStruck = true,
      .smear = .2f, .smearMs = 25 },
    // 35 Graveyard, Reverse Swell: ghostly swells, each note sucked in backwards, and a
    // faint dissonant shimmer a tritone below (Direction Update 2).
    { .pitches = 1, .pitchRatio = { .7071f }, .pitchGain = .3f, .pitchMode = PitchContinuous, .pitchHp = 250, .pitchLp = 3200,
      .reverseMs = 460, .reverseGain = .65f, .reverseLp = 4200 },
    // 36 Mausoleum, Hollow Tomb: a tight, hollow stone tube, muffled.
    { .tone = ToneLow, .toneHz = 1900, .toneQ = .8f,
      .combs = 2, .combHz = { 238, 357 }, .combFeedback = .72f, .combDamp = .6f, .combGain = .6f },
    // ---- Wizard's Tower
    // 37 Stairwell, Spiral Echoes: echoes circling around you up the spiral stair.
    { .taps = 6, .tapMs = { 70, 140, 210, 280, 350, 420 }, .tapGain = { .4f, .34f, .28f, .23f, .18f, .14f },
      .tapLp = 6500, .tapDarken = .9f, .tapMode = TapsCircle, .tapRate = .25f },
    // 38 Library, Paper Hush: hushed and paper-soft: highs and attacks damped.
    { .tone = ToneShelf, .toneHz = 2600, .toneDb = -9, .swellMs = 10 },
    // 39 Alchemy Lab, Bubbling Alchemy: bubbling pitch and strange resonances.
    { .wobbleMs = .7f, .wobbleHz = 6, .wobbleShape = Bubbling, .wobbleMix = .35f,
      .modes = 4, .modeHz = { 523, 740, 906, 1170 }, .modeT60 = 1.0f, .modeGain = .4f },
    // 40 Observatory, Starry Shimmer: vast, slowly evolving starlight above the notes.
    { .pitches = 2, .pitchRatio = { 2.0f, 3.0f }, .pitchGain = .4f, .pitchMode = PitchTwinkle, .pitchHp = 2500,
      .smear = .6f, .smearMs = 80 },
};
}

const char* Signature::kindName(int k) noexcept {
    static const auto names = [] {
        std::array<std::array<char, 32>, kinds> out {};
        const char* s = signatureChoices;
        for (auto& name : out) {
            std::size_t i = 0;
            while (*s != 0 && *s != '|') { if (i + 1 < name.size()) name[i++] = *s; ++s; }
            if (*s == '|') ++s;
        }
        return out;
    }();
    return names[static_cast<std::size_t>(std::clamp(k, 0, kinds - 1))].data();
}

float Signature::Lfo::next(int shape, float hz, double rate, Random& random) noexcept {
    phase += hz / rate;
    phase -= std::floor(phase);
    const float p = static_cast<float>(phase);
    switch (shape) {
        case sig::Sine: return std::sin(twoPi * p);
        case sig::Slosh: return 0.6f * std::sin(twoPi * p) + 0.4f * std::sin(twoPi * (1.618f * p) + 1.0f);
        case sig::SmoothRandom:
        case sig::Bubbling:
        case sig::Crooked: {
            if (--countdown <= 0) {
                const float period = shape == sig::Bubbling ? 0.5f + random.unit() : 0.7f + 0.6f * random.unit();
                countdown = std::max(1, static_cast<int>(rate * period / std::max(0.01f, hz)));
                target = random.bipolar();
            }
            if (hz != followHz || shape != followShape) {   // exp() only when the rate changes
                followHz = hz; followShape = shape;
                follow = timeCoefficient(shape == sig::Bubbling ? 0.022 : 0.6 / std::max(0.01f, hz), rate);
            }
            value += follow * (target - value);
            return shape == sig::Crooked ? 0.6f * std::sin(twoPi * p) + 0.5f * value : value;
        }
        default: return 0;
    }
}

void Signature::prepare(double sampleRate) {
    sr = sampleRate;
    amount.setTime(0.04, sr);
    for (auto& d : wobbleLine) d.prepare(static_cast<std::size_t>(0.03 * sr));
    for (auto& ch : combLines) for (auto& d : ch) d.prepare(static_cast<std::size_t>(0.07 * sr));
    for (auto& ch : shifters) for (auto& s : ch) s.prepare(sr, 0.045);
    for (auto& d : tapLine) d.prepare(static_cast<std::size_t>(2.3 * sr));
    for (auto& ch : smearLines) for (auto& d : ch) d.prepare(static_cast<std::size_t>(0.1 * sr));
    for (auto& ch : diffuserLines) for (auto& d : ch) d.prepare(static_cast<std::size_t>(0.05 * sr));
    for (auto& d : reverseLine) d.prepare(static_cast<std::size_t>(2.1 * sr));
    for (auto& d : dc) d.prepare(sr);
    fastA = timeCoefficient(0.001, sr); fastR = timeCoefficient(0.06, sr);
    slowA = timeCoefficient(0.03, sr); slowR = timeCoefficient(0.3, sr);
    gateR = timeCoefficient(0.25, sr);
    bloomR = timeCoefficient(2.5, sr);
    tailDecay = 1.0f - timeCoefficient(0.05, sr);
    reset();
}

void Signature::reset() {
    for (auto& d : wobbleLine) d.clear();
    for (auto& ch : combLines) for (auto& d : ch) d.clear();
    for (auto& ch : combState) ch.fill(0);
    for (auto& ch : modes) for (auto& f : ch) f.clear();
    for (auto& ch : shifters) for (auto& s : ch) { s.clear(); s.setPhase(0); }
    for (auto& f : pitchHp) f.clear();
    for (auto& f : pitchLp) f.clear();
    for (auto& ch : formants) for (auto& f : ch) f.clear();
    for (auto& d : tapLine) d.clear();
    tapLpState0.fill(0); tapLpState1.fill(0); tapJitter.fill(1);
    for (auto& ch : smearLines) for (auto& d : ch) d.clear();
    for (auto& ch : diffuserLines) for (auto& d : ch) d.clear();
    for (auto& d : reverseLine) d.clear();
    for (auto& f : reverseLp) f.clear();
    for (auto* f : { &toneSvf, &growlLow, &growlSub }) for (auto& s : *f) s.clear();
    for (auto& b : toneEq) b.clear();
    for (auto& b : bite) b.clear();
    for (auto& d : dc) d.clear();
    growlPrev.fill(0); held.fill(0); holdCount.fill(0);
    growlFlip = 1;
    envFast = envSlow = envSwell = envBloom = gateEnv = 0;
    wobbleLfo = {}; wobbleLfoR = {}; wobbleLfoR.phase = 0.25; tremLfo = {};
    for (std::size_t k = 0; k < tapLfo.size(); ++k) { tapLfo[k] = {}; tapLfo[k].phase = 0.17 * static_cast<double>(k); }
    twinkle = { 1, 1 }; twinkleTarget = { 1, 1 }; twinkleCountdown = 0; crackleCountdown = 0;
    circlePhase = 0; reversePhase = 0;
    random.seed(0x5349474eu);
    kind = -1;
    amount.value = -1;   // not primed
}

void Signature::depart() noexcept {
    departing = true;
    heldOut = std::max(0.0f, amount.value);
}

void Signature::configure(int newKind, float newTone) noexcept {
    const bool kindChanged = newKind != kind;
    if (!kindChanged && std::abs(newTone - tone) < 1.0e-4f) return;
    if (kindChanged && kind >= 0) {
        // The engine dips the layer out for a kind change: start the new one clean.
        const float a = amount.value, t = amount.target;
        reset();
        amount.value = a; amount.target = t;
    }
    kind = newKind;
    tone = newTone;
    const auto& rc = sig::recipes[static_cast<std::size_t>(std::clamp(kind, 0, kinds - 1))];
    recipe = &rc;
    // Resonances move in semitones (no glide between pitches), filters tilt smoothly.
    toneShift = static_cast<float>(std::pow(2.0, std::round(tone * 12.0f) / 12.0));
    const double tilt = std::pow(2.0, static_cast<double>(tone));
    const auto keepSvf = [this](Svf& f, double hz, float damping) { const float s1 = f.s1, s2 = f.s2; f.set(hz, sr, damping); f.s1 = s1; f.s2 = s2; };
    for (int ch = 0; ch < 2; ++ch) {
        const auto C = static_cast<std::size_t>(ch);
        keepSvf(toneSvf[C], rc.toneHz * tilt, 1.0f / std::max(0.3f, rc.toneQ));
        if (rc.tone == sig::ToneBell) toneEq[C].peaking(sr, rc.toneHz * tilt, rc.toneQ, rc.toneDb);
        else if (rc.tone == sig::ToneShelf) toneEq[C].highShelf(sr, rc.toneHz * tilt, rc.toneDb);
        bite[C].peaking(sr, 1300.0 * tilt, 0.8, 4.0);
        keepSvf(growlLow[C], 280.0, 1.0f);
        keepSvf(growlSub[C], 420.0, 1.2f);
        keepSvf(pitchHp[C], rc.pitchHp * tilt, 1.0f);
        keepSvf(pitchLp[C], std::min(19000.0, rc.pitchLp * tilt), 1.0f);
        keepSvf(reverseLp[C], std::min(19000.0, rc.reverseLp * tilt), 1.0f);
        const double f1 = rc.formant == sig::FormantOh ? 450 : 750, f2 = rc.formant == sig::FormantOh ? 800 : 1200;
        formants[C][0].bandPass(sr, f1, 4.0);
        formants[C][1].bandPass(sr, f2, 5.0);
        for (int k = 0; k < rc.modes; ++k) {
            const auto K = static_cast<std::size_t>(k);
            const double hz = std::min(static_cast<double>(rc.modeHz[k] * toneShift), sr * 0.42);
            const double t60 = std::max(0.05, rc.modeT60 * (1.0 - 0.08 * k));
            const float damping = static_cast<float>(std::clamp(6.9078 / (pi * t60 * hz), 0.0005, 1.0));
            { auto& m = modes[C][K]; const float s1 = m.s1, s2 = m.s2; m.setResonant(hz * (ch == 0 ? 1.0 : 1.0015), sr, damping); m.s1 = s1; m.s2 = s2; }
            // Between unity peak gain (damping) and unity energy: narrow, long modes still
            // ring audibly from ordinary playing; the soft limit catches exact hits.
            modeGain[K] = std::sqrt(damping) * (rc.modeStruck ? 0.6f : 0.15f) / (1.0f + 0.25f * static_cast<float>(k)) * (hz < sr * 0.41 ? 1.0f : 0.0f);
        }
    }
    for (int k = 0; k < rc.combs; ++k)
        combSamples[static_cast<std::size_t>(k)] = static_cast<float>(std::clamp(sr / (rc.combHz[k] * toneShift), 2.0, 0.065 * sr));
    combCoefficient = 1.0f - rc.combDamp * 0.9f;
    for (int k = 0; k < maxTaps; ++k)
        tapCoefficient[static_cast<std::size_t>(k)] = poleForHz(std::min(19000.0, rc.tapLp * tilt * std::pow(rc.tapDarken, k)), sr);
    static constexpr float smearRatios[2][smearStages] = { { 1.0f, .73f, .53f, .37f }, { .91f, .67f, .47f, .41f } };
    static constexpr float diffuserMs[2][diffuserStages] = { { 13.7f, 7.3f, 4.1f }, { 11.9f, 8.3f, 3.7f } };
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < smearStages; ++k) {
            smearSamples[static_cast<std::size_t>(ch)][static_cast<std::size_t>(k)] = static_cast<float>(rc.smearMs * 0.001 * smearRatios[ch][k] * sr);
            if (k < diffuserStages) diffuserSamples[static_cast<std::size_t>(ch)][static_cast<std::size_t>(k)] = static_cast<float>(diffuserMs[ch][k] * 0.001 * sr);
        }
    swellA = timeCoefficient(std::max(0.001f, rc.swellMs * 0.001f), sr);
    bloomA = timeCoefficient(std::max(0.001f, rc.bloomMs * 0.001f), sr);
    const double driveDb = rc.drive;
    driveGain = dbToGain(static_cast<float>(driveDb));
    // Compensate the drive for a typical -12 dBFS signal so the serial blend stays level.
    driveComp = rc.sat == sig::SatNone ? 1.0f : 0.25f / std::max(1.0e-4f, std::tanh(driveGain * 0.25f));
}

void Signature::process(float* l, float* r, int n, const Frame& p, const Context&) noexcept {
    configure(std::clamp(static_cast<int>(std::lround(p[SigKind])), 0, kinds - 1), std::clamp(p[SigTone], -1.0f, 1.0f));
    amount.set(clamp01(p[SigAmount]));
    if (amount.value < 0) amount.snap(amount.target);
    const auto& rc = *recipe;
    if (kind == 0) return;
    const float rate = std::clamp(p[SigRate], 0.25f, 4.0f);
    const float wobbleCentre = rc.wobbleMs * 0.001f * static_cast<float>(sr) + 2.0f;
    const float wobbleDepth = rc.wobbleMs * 0.001f * static_cast<float>(sr);
    float* io[] = { l, r };

    for (int i = 0; i < n; ++i) {
        const float a = amount.next();
        // Departing (ringing out after a journey): the serial blend and the layers' input
        // fade with the amount, the layers' output stays, so their tails ring on.
        const float feed = departing ? (heldOut > 1.0e-5f ? std::min(1.0f, a / heldOut) : 0.0f) : 1.0f;
        const float layerAmount = departing ? heldOut : a;
        const float x[2] = { finiteOr(l[i]), finiteOr(r[i]) };
        const float level = std::max(std::abs(x[0]), std::abs(x[1]));
        envFast += (level > envFast ? fastA : fastR) * (level - envFast);
        envSlow += (level > envSlow ? slowA : slowR) * (level - envSlow);
        envSwell += (level > envSwell ? swellA : slowR) * (level - envSwell);
        envBloom += (level > envBloom ? bloomA : bloomR) * (level - envBloom);
        // Attack detector: the fast envelope well above the slow one.
        const float attack = clamp01((envFast - 1.4f * envSlow) / std::max(1.0e-5f, envFast));
        gateEnv = attack > gateEnv ? attack : gateEnv + gateR * (attack - gateEnv);

        const float wobble = rc.wobbleMs > 0 ? wobbleLfo.next(rc.wobbleShape, rc.wobbleHz * rate, sr, random) : 0.0f;
        const float wobbleR = rc.wobbleMs > 0 ? (rc.wobbleShape == sig::Sine || rc.wobbleShape == sig::Slosh
                                                     ? wobbleLfoR.next(rc.wobbleShape, rc.wobbleHz * rate, sr, random) : wobble) : 0.0f;
        float trem = 1;
        if (rc.tremDepth > 0) trem = 1.0f - rc.tremDepth * (0.5f + 0.5f * tremLfo.next(rc.tremShape, rc.tremHz * rate, sr, random));
        // Only real onsets (the peak well above the slower envelope) are softened, not the
        // beating of a sustained chord.
        const float swellGain = rc.swellMs > 0 ? std::min(1.0f, 1.4f * envSwell / std::max(1.0e-5f, envFast)) : 1.0f;

        // Pitch twinkle: each voice's level wanders (stars, ice).
        if (rc.pitchMode == sig::PitchTwinkle && --twinkleCountdown <= 0) {
            twinkleCountdown = static_cast<int>(sr * (0.05 + 0.12 * random.unit()) / rate);
            for (auto& t : twinkleTarget) t = random.unit() < 0.35f ? 1.0f : 0.15f * random.unit();
        }
        if (rc.tapMode == sig::TapsCrackle && --crackleCountdown <= 0) {
            crackleCountdown = static_cast<int>(sr * (0.008 + 0.03 * random.unit()));
            for (auto& j : tapJitter) j = 0.6f + 0.55f * random.unit();
        }
        for (int k = 0; k < 2; ++k) twinkle[static_cast<std::size_t>(k)] += 0.004f * (twinkleTarget[static_cast<std::size_t>(k)] - twinkle[static_cast<std::size_t>(k)]);

        float serial[2], layers[2] = { 0, 0 }, pre[2];
        for (int ch = 0; ch < 2; ++ch) {
            const auto C = static_cast<std::size_t>(ch);
            float y = x[ch];
            // Saturation flavours.
            switch (rc.sat) {
                case sig::SatWarm: {
                    const float b = 0.18f;
                    y = (std::tanh(driveGain * y + b) - std::tanh(b)) * driveComp;
                    y = dc[C].process(y);
                    break;
                }
                case sig::SatGrowl: {
                    const float low = growlLow[C].low(y);
                    if (low > 0 && growlPrev[C] <= 0 && ch == 0) growlFlip = -growlFlip;   // octave down: flip on every other cycle
                    growlPrev[C] = low;
                    const float sub = growlSub[C].low(growlFlip * std::min(1.0f, envFast * 1.5f) * 0.5f);
                    y = std::tanh(driveGain * (y + 0.2f * sub)) * driveComp;
                    y = dc[C].process(y);
                    break;
                }
                case sig::SatMenace: {
                    const float e = bite[C].process(y) * driveGain;
                    y = (e > 0 ? std::tanh(e) : std::tanh(1.5f * e) / 1.5f) * driveComp * 1.1f;
                    y = dc[C].process(y);
                    break;
                }
                case sig::SatGrit: {
                    y = std::tanh(driveGain * y) * driveComp;
                    if (++holdCount[C] >= 3) { holdCount[C] = 0; held[C] = y; }
                    y = 0.7f * y + 0.3f * held[C];
                    break;
                }
                default: break;
            }
            switch (rc.tone) {
                case sig::ToneLow: y = toneSvf[C].low(y); break;
                case sig::ToneHigh: y = toneSvf[C].highPass(y); break;
                case sig::ToneBell: case sig::ToneShelf: y = toneEq[C].process(y); break;
                default: break;
            }
            y *= swellGain;
            if (rc.wobbleMs > 0) {
                auto& line = wobbleLine[C];
                line.push(y);
                const float moved = line.read(wobbleCentre + wobbleDepth * (ch == 0 ? wobble : wobbleR));
                y = y + rc.wobbleMix * (moved - y);
            }
            if (!rc.tremLayersOnly) y *= trem;
            serial[ch] = y;
            pre[ch] = y * feed;
        }

        // Parallel layers, fed by the serial result.
        for (int ch = 0; ch < 2; ++ch) {
            const auto C = static_cast<std::size_t>(ch);
            float add = 0;
            for (int k = 0; k < rc.combs; ++k) {
                const auto K = static_cast<std::size_t>(k);
                auto& line = combLines[C][K];
                const float out = line.read(combSamples[K] * (ch == 0 ? 1.0f : 1.003f));
                combState[C][K] += combCoefficient * (out - combState[C][K]);
                line.push(pre[ch] + rc.combFeedback * combState[C][K]);
                add += out * rc.combGain * (1.0f - rc.combFeedback) * 2.5f;
            }
            if (rc.modes > 0) {
                // Struck modes: the attack itself plus a short noise "mallet" scaled by
                // the playing, so they ring whatever notes are played.
                const float excite = rc.modeStruck ? (pre[ch] + random.bipolar() * envFast * 0.7f) * gateEnv * gateEnv : pre[ch];
                float ring = 0;
                for (int k = 0; k < rc.modes; ++k) ring += modeGain[static_cast<std::size_t>(k)] * modes[C][static_cast<std::size_t>(k)].bandPass(excite);
                add += rc.modeGain * 2.2f * softLimit(ring, 1.5f);
            }
            if (rc.pitches > 0) {
                float voice = 0;
                for (int k = 0; k < rc.pitches; ++k) {
                    const float v = shifters[C][static_cast<std::size_t>(k)].process(pre[ch], rc.pitchRatio[k] * (ch == 0 ? 1.0f : 1.002f));
                    voice += rc.pitchMode == sig::PitchTwinkle ? v * twinkle[static_cast<std::size_t>(k)] : v;
                }
                if (rc.pitchHp > 25) voice = pitchHp[C].highPass(voice);
                if (rc.pitchLp < 19000) voice = pitchLp[C].low(voice);
                if (rc.formant != sig::FormantNone) voice = 2.2f * (formants[C][0].process(voice) + 0.7f * formants[C][1].process(voice));
                if (rc.pitchMode == sig::PitchTransient) voice *= gateEnv;
                if (rc.bloomMs > 0) voice *= std::min(1.5f, envBloom / std::max(1.0e-5f, envFast));
                add += voice * rc.pitchGain;
            }
            if (rc.smear > 0) {
                // Allpass smear: blurs attacks into a wash.
                float s = pre[ch] + 0.35f * add;
                for (int k = 0; k < smearStages; ++k) {
                    auto& line = smearLines[C][static_cast<std::size_t>(k)];
                    const float d = line.read(smearSamples[C][static_cast<std::size_t>(k)]);
                    const float v = s + 0.62f * d;
                    line.push(v);
                    s = d - 0.62f * v;
                }
                add += rc.smear * s;
            }
            if (rc.reverseMs > 0) {
                // Two reverse heads, sin^2 windows half a segment apart.
                auto& line = reverseLine[C];
                line.push(pre[ch]);
                const float length = rc.reverseMs * 0.001f * static_cast<float>(sr);
                float rev = 0;
                for (int head = 0; head < 2; ++head) {
                    double ph = reversePhase + 0.5 * head;
                    ph -= std::floor(ph);
                    const float w = std::sin(dsp::twoPi * 0.5f * static_cast<float>(ph));
                    rev += line.read(1.0f + 2.0f * length * static_cast<float>(ph)) * w * w;
                }
                add += reverseLp[C].low(rev) * rc.reverseGain;
            }
            layers[ch] = add;
        }
        if (rc.reverseMs > 0) {
            reversePhase += 1.0 / (rc.reverseMs * 0.001 * sr);
            reversePhase -= std::floor(reversePhase);
        }

        if (rc.taps > 0) {
            tapLine[0].push(pre[0]);
            tapLine[1].push(pre[1]);
            if (rc.tapMode == sig::TapsCircle) { circlePhase += rc.tapRate * rate / sr; circlePhase -= std::floor(circlePhase); }
            float tl = 0, tr = 0;
            for (int k = 0; k < rc.taps; ++k) {
                const auto K = static_cast<std::size_t>(k);
                float samples = rc.tapMs[k] * 0.001f * static_cast<float>(sr);
                if (rc.tapMode == sig::TapsScatter) samples *= 1.0f + 0.012f * tapLfo[K].next(sig::SmoothRandom, rc.tapRate * rate, sr, random);
                // Each channel's echo comes from the same channel (mixing left and right
                // would make stereo-detuned material beat); the pan is a balance.
                tapLpState0[K] += tapCoefficient[K] * (tapLine[0].read(samples) - tapLpState0[K]);
                tapLpState1[K] += tapCoefficient[K] * (tapLine[1].read(samples) - tapLpState1[K]);
                float g = rc.tapGain[k];
                if (rc.tapMode == sig::TapsCrackle) g *= tapJitter[K];
                float pan = rc.tapPan[k];
                if (rc.tapMode == sig::TapsCircle) pan = std::sin(twoPi * static_cast<float>(circlePhase + k / 6.0));
                const float angle = (pan + 1.0f) * 0.25f * static_cast<float>(pi);
                tl += tapLpState0[K] * g * std::cos(angle) * 1.41f;
                tr += tapLpState1[K] * g * std::sin(angle) * 1.41f;
            }
            if (rc.tapMode == sig::TapsDiffuse) {
                float* both[2] = { &tl, &tr };
                for (int ch = 0; ch < 2; ++ch)
                    for (int k = 0; k < diffuserStages; ++k) {
                        auto& line = diffuserLines[static_cast<std::size_t>(ch)][static_cast<std::size_t>(k)];
                        const float d = line.read(diffuserSamples[static_cast<std::size_t>(ch)][static_cast<std::size_t>(k)]);
                        const float v = *both[ch] + 0.6f * d;
                        line.push(v);
                        *both[ch] = d - 0.6f * v;
                    }
            }
            layers[0] += tl;
            layers[1] += tr;
        }
        const float layerGain = rc.tremLayersOnly ? trem : 1.0f;
        for (int ch = 0; ch < 2; ++ch)
            io[ch][i] = finiteOr(x[ch] + a * (serial[ch] - x[ch]) + layerAmount * layers[ch] * layerGain);
        tail = std::max(std::abs(layers[0]), std::abs(layers[1])) * layerAmount + tailDecay * tail;
    }
}
}
