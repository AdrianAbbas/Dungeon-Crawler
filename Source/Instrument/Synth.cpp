#include "Synth.h"

namespace dc::inst {

//==============================================================================
bool SampleVoice::start(const wb::samples::Instrument& inst, float noteMidi, float velocity, float pick, float sampleRate, float attackSeconds,
                        float decaySeconds, float skipSeconds, float even, float courseCents, float courseLvl, int lookupKey) {
    const auto* z = inst.find(lookupKey >= 0 ? lookupKey : int(std::lround(noteMidi)), velocity, pick);
    if (z == nullptr || z->length < 8) return false;
    rate = sampleRate;
    zone = z;
    midi = noteMidi;
    rateRatio = inst.rate / rate;
    pos = double(std::min(skipSeconds * inst.rate, float(z->length) * 0.25f));
    if (z->loops() && pos >= double(z->loopStart)) pos = double(z->loopStart);
    tailPos = -1;
    tailMix = 0;
    // Evened toward the median loudness of the note's own layer (the layers keep their steps).
    const auto evened = [&](const wb::samples::Zone& q) { return std::clamp(even * (inst.medianDb(q.layer) - q.loudnessDb), -12.0f, 12.0f); };
    float evenDb = evened(*z);
    if (smoothLayers && z->layer == 0 && z->hiVel > z->loVel) {
        // The softest layer rises 6 dB across its range (its hits from barely touched up).
        evenDb -= 6.0f * (1.0f - std::clamp((velocity - z->loVel) / (z->hiVel - z->loVel), 0.0f, 1.0f));
    } else if (smoothLayers && z->layer > 0 && z->hiVel > z->loVel) {
        // The take just below this layer (same key, same round-robin), as loud as it plays
        // at the top of its own layer: at this layer's lowest velocity the level starts
        // there and rises to this take's own by its highest.
        const int key = lookupKey >= 0 ? lookupKey : int(std::lround(noteMidi));
        if (const auto* below = inst.find(key, z->loVel - 0.001f, pick); below != nullptr && below->layer < z->layer) {
            const float t = std::clamp((velocity - z->loVel) / (z->hiVel - z->loVel), 0.0f, 1.0f);
            const float gap = (below->loudnessDb + evened(*below)) - (z->loudnessDb + evenDb);
            evenDb += (1.0f - t) * std::clamp(gap, -24.0f, 0.0f);
        }
    }
    gain = d::dbToGain(evenDb);
    amp = attackSeconds > 0.0005f ? 0.0f : 1.0f;
    attackStep = attackSeconds > 0.0005f ? 1.0f / (attackSeconds * rate) : 1.0f;
    fade = 1;
    fadeMul = decaySeconds > 0.0f ? std::exp(-6.9078f / (decaySeconds * rate)) : 1.0f;
    releaseMul = 1;
    releasing = false;
    courseLevel = courseLvl;
    courseZone = z;
    if (courseLvl > 0.0f && z->loops()) {
        // Another take for the second player, where there is one.
        const auto* other = inst.find(int(std::lround(noteMidi)), velocity, pick + 0.5f - std::floor(pick + 0.5f));
        if (other != nullptr && other->loops()) courseZone = other;
    }
    coursePos = pos - double(0.012f * inst.rate);
    courseRatio = std::exp2(courseCents / 1200.0f + (z->root - courseZone->root) / 12.0f);
    age = 0;
    vibratoPhase = pick;
    on = true;
    update();
    return true;
}

//==============================================================================
bool VocalVoice::start(float noteMidi, float velocity, float random, float sampleRate, float attack, float skip, uint64_t seed) {
    rate = sampleRate;
    for (auto& rd : reader) rd.s.reset(), rd.used = false, rd.delay = 0;
    readers = 0;
    shiftAbove = 0;
    const int key = int(std::lround(noteMidi));
    // Velocity crossfades the soft take into the full one (the layers were levelled a few
    // dB apart by the preparation, so the level rises smoothly with the timbre).
    const float full = d::smoothstep(0.25f, 0.85f, velocity);
    const int n = std::clamp(singers, 1, maxSingers);
    const float norm = 1.0f / std::sqrt(1.0f + 0.7f * float(n - 1));
    for (int v = 0; v < 2; ++v) {
        const auto& src = vowel[std::size_t(v)];
        if (src.lead == nullptr && src.ensemble == nullptr) continue;
        for (int i = 0; i < n; ++i) {
            const auto* inst = (i == 0 || src.ensemble == nullptr) ? (src.lead != nullptr ? src.lead : src.ensemble) : src.ensemble;
            if (inst == nullptr || inst->zones.empty()) continue;
            const int look = std::clamp(key, inst->lowKey, inst->highKey);
            if (v == 0 && i == 0) shiftAbove = std::max(0.0f, noteMidi - inst->highRoot);
            // Each singer: deterministic from the note's seed.
            const uint64_t h = d::mix64(seed + uint64_t(i) * 0x9E3779B97F4A7C15ull + 17u);
            const float u1 = d::unipolar(h), u2 = d::unipolar(d::mix64(h + 1)), u3 = d::unipolar(d::mix64(h + 2)), u4 = d::unipolar(d::mix64(h + 3));
            const float cents = i == 0 ? 0.0f : detuneCents * (2.0f * u1 - 1.0f);
            const int delay = i == 0 ? 0 : int(timingMs * 0.001f * rate * (0.25f + 0.75f * u2));
            const float side = (i & 1) ? 1.0f : -1.0f;
            const float pan = i == 0 ? 0.0f : side * spread * (0.4f + 0.6f * float((i + 1) / 2) / float(std::max(1, n / 2)));
            const auto pg = d::panGains(std::clamp(pan, -1.0f, 1.0f));
            const float pick = i == 0 ? random : random + 0.37f * float(i) + 0.11f * u3 - std::floor(random + 0.37f * float(i) + 0.11f * u3);
            const float singerGain = (i == 0 ? 1.0f : 0.85f) * norm;
            const bool layered = inst->layers > 1;
            for (int layer = 0; layer < (layered ? 2 : 1); ++layer) {
                const float wl = layered ? (layer == 0 ? 1.0f - full : full) : 1.0f;
                if (wl < 0.02f || readers >= maxReaders - 1) continue;
                auto& rd = reader[std::size_t(readers)];
                const float vel = layered ? (layer == 0 ? 0.1f : 0.95f) : velocity;
                const float a = i == 0 ? attack : attack * (1.0f + 0.6f * u4);
                if (!rd.s.start(*inst, noteMidi, vel, pick, rate, a, 0.0f, skip + (i == 0 ? 0.0f : 0.06f * u3), 0.6f, 0, 0, look)) continue;
                rd.used = true;
                rd.vowel = v;
                rd.weight = wl * singerGain;
                rd.delay = delay;
                rd.baseCents = cents;
                rd.drift = i == 0 ? 0.5f * driftCents : driftCents;
                rd.driftHz = 0.07f + 0.25f * u4;
                rd.driftPhase = u1;
                rd.l = pg.l;
                rd.r = pg.r;
                rd.s.cents = cents;
                rd.s.update();
                ++readers;
            }
        }
    }
    if (grit != nullptr && gritLevel > 0.0f && !grit->zones.empty() && readers < maxReaders) {
        auto& rd = reader[std::size_t(readers)];
        if (rd.s.start(*grit, noteMidi, velocity, random, rate, attack, 0.0f, skip, 0.8f, 0, 0, std::clamp(key, grit->lowKey, grit->highKey))) {
            rd.used = true;
            rd.vowel = 2;
            rd.weight = gritLevel * norm;
            rd.delay = 0;
            rd.baseCents = 0;
            rd.drift = driftCents;
            rd.driftHz = 0.13f;
            rd.l = rd.r = 0.7071f;
            ++readers;
        }
    }
    // Shifted up beyond the recordings: the formants rise with it; a gentle low-pass that
    // falls an octave per octave of shift keeps the voice from turning thin and bright.
    tameHz = shiftAbove > 0.05f ? std::min(0.45f * rate, 11000.0f * std::exp2(-shiftAbove / 12.0f)) : 0.0f;
    tame.reset();
    if (tameHz > 0.0f) tame.set(tameHz, 0.6f, rate);
    morph = morphTarget;
    return readers > 0;
}

//==============================================================================
bool BrassVoice::start(float noteMidi, float velocity, float pick, float sampleRate, float attackSeconds) {
    rate = sampleRate;
    for (auto& rd : reader) rd.s.reset(), rd.used = false, rd.delay = 0;
    readers = 0;
    beyond = 0;
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    // Where a velocity falls among an instrument's dynamics: the two adjacent layers, how
    // far between them, and the level there (between the layers' median loudness, a little
    // below the softest: -6 dB at velocity 0; above the loudest: +2 dB at 1).
    struct Place { int l0, l1; float t, levelDb; };
    const auto placeOf = [&](const wb::samples::Instrument& inst, float shift = 0.0f) {
        const int layers = std::max(1, inst.layers);
        // `shift`: a doubling player picks its dynamic a little softer (its layer changes at
        // another velocity than the lead's, so the two steps never add up); its level does not move.
        const float pos = std::clamp((velocity + shift) * float(layers) - 0.5f, -0.5f, float(layers) - 0.5f);
        const int l0 = std::clamp(int(std::floor(pos)), 0, layers - 1), l1 = std::min(l0 + 1, layers - 1);
        const float t = l1 == l0 ? 0.0f : std::clamp(pos - float(l0), 0.0f, 1.0f);
        // The level follows the velocity along a smooth curve (about 13 dB from velocity 0.2
        // to 1, never steeper than ~1.2 dB per 0.05 of velocity), from the loudest layer's
        // median (+1 dB at velocity 1): the library's dynamics are 16-18 dB apart between
        // adjacent layers, too steep to play; each layer keeps its own tone.
        const float level = inst.medianDb(layers - 1) + 1.0f + 20.0f * std::log10(0.16f + 0.84f * std::pow(velocity, 1.6f));
        return Place { l0, l1, t, level };
    };
    // The dynamic nearest the velocity, levelled to `levelDb` (the velocity curve): one take
    // per note. Two adjacent dynamics crossfaded would interfere (their attacks start alike,
    // so they are not independent) and dip in level between the layers; one take levelled
    // onto the curve rises smoothly, its tone changing at the layer's edge.
    const auto addReaders = [&](const wb::samples::Instrument& inst, const Player& p, float midi, float weight, float levelDb, float attack, bool duck, float pickHere, float decay) {
        const auto at = placeOf(inst, p.lookup != 0.0f ? -0.12f : 0.0f);
        const int layers = std::max(1, inst.layers);
        int key = std::clamp(int(std::lround(midi)), inst.lowKey, inst.highKey);
        if (p.lookup != 0.0f) {
            // The nearest other recorded note within 5 semitones, first on the `lookup` side.
            const float layerVel0 = (float(at.t < 0.5f ? at.l0 : at.l1) + 0.5f) / float(layers);
            const auto* own = inst.find(key, layerVel0, pickHere);
            const int dir = p.lookup > 0.0f ? 1 : -1;
            for (int d = 1; d <= 5 && own != nullptr; ++d) {
                bool found = false;
                for (int side : { dir, -dir }) {
                    const int k = int(std::lround(midi)) + side * d;
                    if (k < inst.lowKey || k > inst.highKey) continue;
                    if (const auto* z = inst.find(k, layerVel0, pickHere); z != nullptr && std::abs(z->root - own->root) > 0.5f) {
                        key = k;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }
        const auto pg = d::panGains(std::clamp(p.pan, -1.0f, 1.0f));
        const int delay = int(std::max(0.0f, p.delayMs) * 0.001f * rate);
        {
            const int layer = at.t < 0.5f ? at.l0 : at.l1;
            const float w = 1.0f;
            if (readers >= maxReaders) return;
            auto& rd = reader[std::size_t(readers)];
            const float layerVel = (float(layer) + 0.5f) / float(layers);
            rd.s.exactTail = true;
            if (!rd.s.start(inst, midi, layerVel, pickHere, rate, attack, decay, 0.0f, 1.0f, 0, 0, key)) return;   // evened fully: on the curve
            rd.used = true;
            rd.crestDb = inst.medianCrestDb;
            rd.weight = weight * w * d::dbToGain(levelDb - inst.medianDb(layer));
            rd.s.cents = p.cents;
            rd.s.update();
            rd.l = pg.l;
            rd.r = pg.r;
            rd.delay = delay;
            // The sustain dips under the staccato hit and rises into the held note (~80 ms).
            rd.duck = duck ? susDuck : 0.0f;
            rd.duckStep = duck ? susDuck / (0.08f * rate) : 0.0f;
            ++readers;
        }
    };
    const float stacWeight = stacLevel * (0.25f + 0.75f * std::pow(velocity, 1.5f));
    float reference = 0;
    for (int i = 0; i < players; ++i) {
        const auto& p = player[std::size_t(i)];
        const float midi = noteMidi + p.semis;
        const auto* ref = p.sus != nullptr ? p.sus : p.stac;
        if (i == 0 && ref != nullptr && !ref->zones.empty())
            beyond = std::max({ 0.0f, ref->lowRoot - midi, midi - ref->highRoot });
        // Each player its own takes (the round-robin pick moved along).
        const float pk = pick + 0.37f * float(i) - std::floor(pick + 0.37f * float(i));
        const bool hit = p.stac != nullptr && stacWeight > 0.01f;
        // The staccato is levelled against the sustain at the same velocity (the library's
        // staccatos and sustains were not recorded at matching levels), `stacLiftDb` above.
        const float susLevel = p.sus != nullptr ? placeOf(*p.sus).levelDb : (p.stac != nullptr ? placeOf(*p.stac).levelDb : -30.0f);
        if (p.sus != nullptr) addReaders(*p.sus, p, midi, p.gain, susLevel, attackSeconds, hit, pk, 0.0f);
        if (hit) addReaders(*p.stac, p, midi, p.gain * stacWeight, p.sus != nullptr ? susLevel + stacLiftDb : susLevel, 0.0f, false, pk, p.sus != nullptr ? stacDecaySeconds : 0.0f);
    }
    // The bite's ceiling: the section's peak as played, from every take's level and its
    // instrument's median crest (not the take's own peak: the ceiling then follows the
    // velocity curve, with no step where a take changes).
    for (int k = 0; k < readers; ++k) {
        const auto& rd = reader[std::size_t(k)];
        if (rd.used) reference += rd.weight * rd.s.gain * d::dbToGain(rd.s.zone->loudnessDb + rd.crestDb);
    }
    driveRef = std::max(1.0e-5f, 0.7f * reference);
    biteNow = bite;
    biteMul = std::exp(-1.0f / (std::max(biteSeconds, 0.005f) * rate));
    hitEnv = 1;
    hitMul = std::exp(-1.0f / (0.12f * rate));
    presL.reset();
    presR.reset();
    presL.set(presenceHz, 0.7f, rate);
    presR.set(presenceHz, 0.7f, rate);
    thinL.reset();
    thinR.reset();
    thinL.set(thinHz, 0.6f, rate);
    thinR.set(thinHz, 0.6f, rate);
    for (int c = 0; c < 2; ++c) {
        exIn[std::size_t(c)].reset();
        exOut[std::size_t(c)].reset();
        exTop[std::size_t(c)].reset();
        exIn[std::size_t(c)].set(900.0f, 0.7f, rate);
        exOut[std::size_t(c)].set(1800.0f, 0.7f, rate);
        exTop[std::size_t(c)].set(std::min(7000.0f, 0.45f * rate), 0.7f, rate);
    }
    return readers > 0;
}

void BrassVoice::tick(float& l, float& r) {
    float sl = 0, sr = 0;
    for (int k = 0; k < readers; ++k) {
        auto& rd = reader[std::size_t(k)];
        if (!rd.used) continue;
        if (rd.delay > 0) {
            --rd.delay;
            continue;
        }
        if (!rd.s.active()) continue;
        float w = rd.weight;
        if (rd.duck > 0.0f) {
            w *= 1.0f - rd.duck;
            rd.duck = std::max(0.0f, rd.duck - rd.duckStep);
        }
        const float y = rd.s.tick() * w;
        sl += y * rd.l;
        sr += y * rd.r;
    }
    // On the hit the low end thinned first (`hitThin`), then the bite saturates that leaner
    // hit, then the presence band (always `presence`, more on the hit) brightens the result:
    // EQ after the distortion, so the saturation does not flatten it away. The attack is
    // bright and dense, the held note keeps its body.
    const float h = hitEnv > 1.0e-4f ? hitEnv : 0.0f;
    if (hitThin > 0.0f && h > 0.0f) {
        sl -= hitThin * h * thinL.tick(sl).lp;
        sr -= hitThin * h * thinR.tick(sr).lp;
    }
    if (biteNow > 1.0e-4f) {
        // The lips pushed: the hit driven (1 + bite, at most 5) times harder into a tanh whose
        // ceiling is the note's own natural peak (`driveRef`, from its level, so a soft
        // recording is pushed as hard as a loud one): the hit louder and denser, its peak
        // no higher, never flattened into a square (tanh keeps bending). It fades out with
        // the bite (no step where it ends).
        const float g = 1.0f + std::min(biteNow, 4.0f), inv = 1.0f / driveRef, m = bite > 0.0f ? std::min(1.0f, biteNow / bite) : 0.0f;
        sl += m * (driveRef * std::tanh(sl * g * inv) - sl);
        sr += m * (driveRef * std::tanh(sr * g * inv) - sr);
        biteNow *= biteMul;
    }
    const float p = presence + hitPresence * h;   // a held `presence` below 0 rounds the held note
    if (std::abs(p) > 1.0e-4f) {
        sl += p * presL.bandpass(sl);
        sr += p * presR.bandpass(sr);
    }
    const float e = excite + hitExcite * h;
    if (e > 1.0e-4f) {
        // |band| is the band an octave up (and its DC, which the high-pass takes off).
        float* side[2] { &sl, &sr };
        for (int c = 0; c < 2; ++c) {
            const float band = exIn[std::size_t(c)].tick(*side[c]).hp;
            const float up = exTop[std::size_t(c)].tick(exOut[std::size_t(c)].tick(std::abs(band)).hp).lp;
            *side[c] += e * up;
        }
    }
    hitEnv *= hitMul;
    l += sl;
    r += sr;
}

namespace {
inline float readCubic(const int16_t* data, double pos) {
    const auto i = int(pos);
    const float t = float(pos - double(i));
    const float xm1 = data[i - 1], x0 = data[i], x1 = data[i + 1], x2 = data[i + 2];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return (((c3 * t + c2) * t + c1) * t + x0) * (1.0f / 32768.0f);
}
}

float SampleVoice::tick() {
    if (!on) return 0.0f;
    const auto& z = *zone;
    float y = readCubic(z.data, pos);
    if (courseLevel > 0.0f && coursePos >= 0.0) {
        const auto& c = *courseZone;
        if (coursePos < double(c.length - 1)) y += courseLevel * readCubic(c.data, coursePos);
    }
    if (tailPos >= 0.0) {
        // Crossing into the recorded release tail.
        y = y * (1.0f - tailMix) + readCubic(z.data, tailPos) * tailMix;
        tailPos += double(inc);
        tailMix += tailStep;
        if (tailMix >= 1.0f) {
            pos = exactTail ? tailPos - double(inc) : tailPos;   // pos += inc follows
            tailPos = -1;
        }
    }
    pos += double(inc);
    coursePos += double(inc * courseRatio);
    if (courseLevel > 0.0f && courseZone->loops() && !releasing && coursePos >= double(courseZone->loopEnd))
        coursePos -= double(courseZone->loopEnd - courseZone->loopStart);
    if (z.loops() && !releasing && pos >= double(z.loopEnd)) pos -= double(z.loopEnd - z.loopStart);
    if (tailPos >= double(z.length - 1)) tailPos = -1, tailMix = 0, pos = double(z.length);
    if (amp < 1.0f) amp = std::min(1.0f, amp + attackStep);
    fade *= fadeMul * releaseMul;
    const float out = y * amp * fade * gain;
    if (pos >= double(z.length - 1) || fade < 1.0e-4f) on = false;
    return out;
}

//==============================================================================
void Layer::prepare(float rate) {
    pluck.prepare(rate, 20.0f);
    strings.prepare(rate, 20.0f);
    reset();
}

void Layer::reset() {
    model = Model::None;
    cents = 0;
    synth.reset();
    pluck.reset();
    bell.reset();
    drum.reset();
    fm.reset();
    tine.reset();
    choir.reset();
    wind.reset();
    bowed.reset();
    strings.reset();
    organ.reset();
    sample.reset();
    vocal.reset();
    brass.reset();
    bellBent = 0;
}

bool Layer::active() const {
    switch (model) {
        case Model::Synth: return synth.active();
        case Model::Pluck: return pluck.active;
        case Model::Bell: return bell.active;
        case Model::Drum: return drum.active();
        case Model::Fm: return fm.active();
        case Model::Tine: return tine.active();
        case Model::Choir: return choir.active();
        case Model::Wind: return wind.active();
        case Model::Bowed: return bowed.active();
        case Model::Strings: return strings.active();
        case Model::Organ: return organ.active();
        case Model::Sample: return sample.active();
        case Model::Vocal: return vocal.active();
        case Model::Brass: return brass.active();
        case Model::None: break;
    }
    return false;
}

void Layer::release(float seconds, float rate) {
    switch (model) {
        case Model::Synth: synth.release(seconds, rate); break;
        case Model::Pluck: pluck.damp(seconds); break;
        case Model::Fm: fm.release(seconds, rate); break;
        case Model::Tine: tine.release(seconds); break;
        case Model::Choir: choir.release(seconds, rate); break;
        case Model::Wind: wind.release(seconds, rate); break;
        case Model::Bowed: bowed.release(seconds, rate); break;
        case Model::Strings: strings.release(seconds); break;
        case Model::Organ: organ.release(seconds); break;
        case Model::Sample: sample.release(seconds); break;
        case Model::Vocal: vocal.release(seconds); break;
        case Model::Brass: brass.release(seconds); break;
        case Model::Bell:
            // A struck bell or bar damped: its modes stop over `seconds` (the highs a little first).
            for (int k = 0; k < bell.modes; ++k) {
                auto& tau = bell.tau[std::size_t(k)];
                tau = std::min(tau, seconds / 6.91f * (k >= 2 ? 0.6f : 1.0f));
                bell.mode[std::size_t(k)].retune(bell.freq[std::size_t(k)] * std::exp2(bellBent / 12.0f), tau, rate);
            }
            bell.extras.glideSemis = 0;
            break;
        case Model::Drum:
        case Model::None: break;
    }
}

void Layer::update(float rate, float bendSemis, float vibrato) {
    bendSemis += cents / 100.0f;
    switch (model) {
        case Model::Synth: synth.bend = bendSemis; synth.update(rate); break;
        case Model::Pluck: pluck.bendTo(std::exp2(bendSemis / 12.0f)); break;
        case Model::Bell:
            if (std::abs(bendSemis - bellBent) > 1.0e-4f && bell.extras.glideSemis == 0.0f) {
                bellBent = bendSemis;
                const float ratio = std::exp2(bendSemis / 12.0f);
                for (int k = 0; k < bell.modes; ++k)
                    bell.mode[std::size_t(k)].retune(bell.freq[std::size_t(k)] * ratio, bell.tau[std::size_t(k)], rate);
            }
            bell.update();
            break;
        case Model::Drum: drum.endHz = hz * std::exp2(bendSemis / 12.0f); drum.update(rate); break;
        case Model::Fm: fm.bend = bendSemis; fm.update(rate); break;
        case Model::Tine: tine.bend = bendSemis; tine.update(); break;
        case Model::Choir: choir.bend = bendSemis; choir.update(rate); break;
        case Model::Wind: wind.bend = bendSemis; wind.update(rate); break;
        case Model::Bowed: bowed.bend = bendSemis; bowed.update(rate); break;
        case Model::Strings: strings.bend = bendSemis; strings.update(); break;
        case Model::Organ: organ.bend = bendSemis; organ.update(); break;
        case Model::Sample: sample.bend = bendSemis; sample.update(); break;
        case Model::Vocal: vocal.update(bendSemis, float(d::chunk) / rate); break;
        case Model::Brass: brass.update(bendSemis); break;
        case Model::None: break;
    }
    (void) vibrato;
}

void Layer::tick(const d::Stream& noise, float& l, float& r) {
    float y = 0;
    switch (model) {
        case Model::Synth: {
            float a = 0, b = 0;
            synth.tick(a, b);
            l += a * gain;
            r += b * gain;
            return;
        }
        case Model::Pluck: y = pluck.tick(noise); break;
        case Model::Bell: y = bell.active ? bell.tick() : 0.0f; break;
        case Model::Drum: y = drum.tick(noise); break;
        case Model::Fm: y = fm.tick(); break;
        case Model::Tine: y = tine.on ? tine.tick() : 0.0f; break;
        case Model::Choir: y = choir.tick(); break;
        case Model::Wind: y = wind.tick(noise); break;
        case Model::Bowed: y = bowed.tick(noise); break;
        case Model::Strings: y = strings.tick(noise); break;
        case Model::Organ: y = organ.tick(noise); break;
        case Model::Sample: y = sample.tick(); break;
        case Model::Vocal: {
            // Stereo already (the ensemble's spread).
            float a = 0, b = 0;
            vocal.tick(a, b);
            l += a * gain;
            r += b * gain;
            return;
        }
        case Model::Brass: {
            float a = 0, b = 0;
            brass.tick(a, b);
            l += a * gain;
            r += b * gain;
            return;
        }
        case Model::None: return;
    }
    y *= gain;
    l += y * panGains.l;
    r += y * panGains.r;
}

//==============================================================================
void Synth::prepare(double sampleRate) {
    rate = float(sampleRate);
    for (std::size_t k = 0; k < voices.size(); ++k) {
        auto& v = voices[k];
        for (auto& L : v.layer) L.prepare(rate);
        v.noise.setKey(0x5eed + uint32_t(k), 7);
    }
    captures.prepare(sampleRate);
    reset();
}

void Synth::reset() {
    for (auto& v : voices) {
        for (auto& L : v.layer) L.reset();
        v.on = v.keyDown = v.sustained = v.releasing = v.killing = false;
        v.key = -1;
        v.tone.reset();
        for (auto& h : v.highPass) h.reset();
        for (auto& b : v.body) b.reset();
    }
    captures.reset();
    perf = {};
    bendNow = modNow = 0;
    counter = 0;
    lit[0].store(0);
    lit[1].store(0);
}

void Synth::setDesign(int index) noexcept { designNow = std::clamp(index, 0, instrumentCount - 1); }

int Synth::activeVoices() const noexcept {
    int c = 0;
    for (const auto& v : voices) c += v.on ? 1 : 0;
    return c;
}

bool Synth::keyLit(int key) const noexcept {
    if (key < 0 || key > 127) return false;
    return ((lit[std::size_t(key >> 6)].load(std::memory_order_relaxed) >> (key & 63)) & 1u) != 0;
}

void Synth::kill(Voice& v) noexcept {
    if (!v.on || v.killing) return;
    // A quick fade (6 ms) so a stolen voice never clicks.
    v.killing = true;
    v.killStep = 1.0f / (0.006f * rate);
    v.keyDown = v.sustained = false;
}

int Synth::pickVoice() noexcept {
    // A free slot first.
    for (std::size_t k = 0; k < voices.size(); ++k)
        if (!voices[k].on) return int(k);
    // Otherwise the oldest released voice, then the oldest held one (it fades while the
    // new note starts in the slot of the quietest dying voice).
    int best = -1;
    float bestScore = 1.0e30f;
    for (std::size_t k = 0; k < voices.size(); ++k) {
        const auto& v = voices[k];
        const float score = (v.killing ? 0.0f : (v.releasing ? 1.0e6f : 2.0e6f)) + float(v.order);
        if (score < bestScore) bestScore = score, best = int(k);
    }
    auto& v = voices[std::size_t(best)];
    for (auto& L : v.layer) L.reset();
    v.on = false;
    return best;
}

void Synth::noteOn(int key, float velocity) noexcept {
    if (key < 0 || key > 127) return;
    if (velocity <= 0.0f) {
        noteOff(key);
        return;
    }
    const auto& des = design(designNow);
    // The same key again: the old note lets go quickly (a re-strike, not a pile-up).
    for (auto& v : voices)
        if (v.on && v.key == key && !v.killing) kill(v);
    // Polyphony: beyond the instrument's, the oldest note fades out.
    int sounding = 0;
    for (const auto& v : voices) sounding += v.on && !v.killing ? 1 : 0;
    while (sounding >= std::max(des.polyphony, 1)) {
        Voice* oldest = nullptr;
        for (auto& v : voices)
            if (v.on && !v.killing && (oldest == nullptr || (v.releasing && !oldest->releasing) || (v.releasing == oldest->releasing && v.order < oldest->order))) oldest = &v;
        if (oldest == nullptr) break;
        kill(*oldest);
        --sounding;
    }
    auto& v = voices[std::size_t(pickVoice())];
    for (auto& L : v.layer) L.reset();
    v.designIndex = designNow;
    v.key = key;
    v.on = true;
    v.keyDown = true;
    v.sustained = false;
    v.releasing = false;
    v.killing = false;
    v.killGain = 1;
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.order = ++orderCounter;
    v.age = 0;
    v.step = 0;
    v.state.fill(0);
    v.tremPhase = 0;
    v.wheelLevel = 1;
    // Punch: the hit's boost, harder the harder the key is struck; it settles over Punch
    // Time along a raised cosine (in dB), so it never steps.
    // Session 12: the horns follow the velocity more steeply (punchVelocityScale).
    v.punchPeakDb = punchDb(designNow, punch, sound) * punchVelocityScale(designNow, v.velocity, sound);
    v.punchSamples = std::max(1.0f, std::clamp(punchSeconds, 0.02f, 0.5f) * rate);
    v.punchShape = sound >= SoundHorns && brassPresence(designNow, sound) ? 4.0f : 1.0f;   // the horns, not the chant
    v.punchPos = 0;
    v.punchGain = v.punchPeakDb > 0.0f ? std::pow(10.0f, v.punchPeakDb * 0.05f) : 1.0f;
    v.punchStep = 0;
    Note n;
    n.key = std::clamp(key + int(std::lround(des.transpose)), 0, 127);
    n.hz = 440.0f * std::exp2((float(key) + des.transpose - 69.0f) / 12.0f);
    n.velocity = v.velocity;
    ++noteCount;
    n.random = d::unipolar(d::mix64(noteCount * 0x9E3779B97F4A7C15ull + uint64_t(key)));
    n.macro = macros;
    n.vowel = voiceVowel;
    n.ensemble = voiceEnsemble;
    n.classic = sound < SoundMix;
    n.sound = sound;
    n.rate = rate;
    n.bpm = tempo;
    n.noiseAt = d::mix64(noteCount + 77) << 8;
    v.note = n;
    // The post: tone follows the Tone macro (two octaves either way) and the velocity.
    const auto& p = des.post;
    v.toneHz = std::min(p.toneHz * std::exp2(2.0f * n.offset(0)) * std::exp2(-p.velocityTone * (1.0f - v.velocity)), 0.45f * rate);
    v.toneQ = p.toneQ;
    v.tone.reset();
    v.tone.set(v.toneHz, v.toneQ, rate);
    for (auto& h : v.highPass) {
        h.reset();
        h.set(p.highPassHz, 0.7f, rate);
    }
    const float bodyAmount = std::clamp(2.0f * macros[3], 0.0f, 2.0f);
    for (std::size_t k = 0; k < 3; ++k) {
        v.body[k].reset();
        if (p.body[k].hz > 0.0f) v.body[k].set(p.body[k].hz, p.body[k].q, rate);
        v.bodyGain[k] = p.body[k].hz > 0.0f ? p.body[k].gain * bodyAmount : 0.0f;
    }
    // Session 11: the brass's presence, a resonance at 2.9 kHz (its last body slot).
    const bool presence = brassPresence(designNow, sound);
    if (presence) {
        v.body[2].reset();
        v.body[2].set(2900.0f, 1.1f, rate);
        v.bodyGain[2] = 0.75f * std::max(bodyAmount, 0.5f);
    }
    // Session 12: the horns' attack edge, an upper-mid band (3.2 kHz) added on the hit and
    // fading over ~80 ms (more the harder the note): brightness and bite on the attack only,
    // the held note as it was. Its gain is stepped per chunk (an exponential, no edge).
    const float edge = attackEdge(designNow, sound);
    v.edge.reset();
    v.edgeGain = 0;
    v.edgeDecay = 0;
    v.hitClip = edge > 0.0f;
    // A horn's hit is shaped from its first sample (the clipper fades out only, when the
    // horns have stopped; the note itself starts from silence, so this never steps).
    if (v.hitClip) clipMix = clipTarget = 1.0f;
    if (edge > 0.0f) {
        v.edge.set(3200.0f, 0.9f, rate);
        v.edgeGain = edge * (0.5f + 2.5f * v.velocity * v.velocity);
        v.edgeDecay = std::exp(-float(d::chunk) / (0.08f * rate));
    }
    v.drive = v.baseDrive = p.drive;
    v.gain = d::dbToGain(designTrimDb(designNow));
    // The brass's presence adds to its sustain's level as well: taken back (measured; the
    // brass sustains as loud as before, the hit is what grows).
    const float presenceTrimDb = presence ? (designNow == 6 ? -1.95f : designNow == 9 ? -1.4f : -0.2f) : 0.0f;
    v.gain *= d::dbToGain(presenceTrimDb);
    const auto cap = captureOf(designNow);
    v.capture = captures.available(cap.capture) ? cap.capture : CaptureNone;
    v.captureSend = cap.send;
    des.start(v, n);
    // A sampled instrument playing its stand-in (no recordings): its own trim.
    if (usesRecordings(designNow)) {
        bool recorded = false;
        for (const auto& L : v.layer) recorded |= L.model == Model::Sample || L.model == Model::Vocal;
        if (!recorded) v.gain = d::dbToGain(standInTrimDb(designNow) + presenceTrimDb);
    }
    // Velocity sets the level of the blown, bowed, sung and held sources (struck and
    // plucked models already respond to it in their own way).
    // The brass (Session 11) answers harder: softer below, stronger above (equal at 0.8).
    const float velocityGain = presence ? 1.237f * (0.1f + 0.9f * v.velocity * v.velocity) : 0.35f + 0.65f * std::pow(v.velocity, 1.3f);
    for (auto& L : v.layer)
        switch (L.model) {
            case Model::Synth: case Model::Choir: case Model::Wind: case Model::Bowed: case Model::Strings: case Model::Organ: case Model::Fm: case Model::Sample: case Model::Vocal:
                L.gain *= velocityGain;
                break;
            // Session 13: the recorded brass (Model::Brass) carries its own dynamics: its
            // layers and level already follow the velocity.
            default: break;
        }
    // Whatever the layers started at, the first chunk's update tunes them now.
    const float bendSemis = bendNow * bendRange;
    for (auto& L : v.layer)
        if (L.model != Model::None) L.update(rate, bendSemis, modNow);
    if (key >= 0) lit[std::size_t(key >> 6)].fetch_or(uint64_t(1) << (key & 63), std::memory_order_relaxed);
}

void Synth::releaseVoice(Voice& v) noexcept {
    if (v.releasing || v.killing) return;
    v.releasing = true;
    const auto& des = design(v.designIndex);
    const float seconds = des.releaseSeconds * v.note.scale(2);
    for (auto& L : v.layer)
        if (L.model != Model::None) L.release(seconds, rate);
}

void Synth::noteOff(int key) noexcept {
    for (auto& v : voices) {
        if (!v.on || v.key != key || !v.keyDown) continue;
        v.keyDown = false;
        if (perf.sustain) v.sustained = true;
        else releaseVoice(v);
    }
    bool still = false;
    for (const auto& v : voices) still |= v.on && v.key == key && (v.keyDown || v.sustained) && !v.killing;
    if (!still && key >= 0 && key < 128) lit[std::size_t(key >> 6)].fetch_and(~(uint64_t(1) << (key & 63)), std::memory_order_relaxed);
}

void Synth::setSustain(bool down) noexcept {
    perf.sustain = down;
    if (down) return;
    for (auto& v : voices)
        if (v.on && v.sustained) {
            v.sustained = false;
            if (!v.keyDown) {
                releaseVoice(v);
                if (v.key >= 0) lit[std::size_t(v.key >> 6)].fetch_and(~(uint64_t(1) << (v.key & 63)), std::memory_order_relaxed);
            }
        }
}

void Synth::allNotesOff() noexcept {
    perf.sustain = false;
    for (auto& v : voices)
        if (v.on) {
            v.keyDown = v.sustained = false;
            releaseVoice(v);
        }
    lit[0].store(0);
    lit[1].store(0);
}

void Synth::allSoundOff() noexcept {
    perf.sustain = false;
    for (auto& v : voices) kill(v);
    lit[0].store(0);
    lit[1].store(0);
}

void Synth::controlChunk() noexcept {
    // The wheels glide over a chunk (no zipper noise).
    bendNow += 0.5f * (perf.bend - bendNow);
    modNow += 0.35f * (perf.mod - modNow);
    const float chunkSeconds = float(d::chunk) / rate;
    const float bendSemis = bendNow * bendRange;
    // The hit clipper follows the horns (in and out over ~5 ms).
    bool horn = false;
    for (const auto& v : voices) horn |= v.on && v.hitClip;
    clipTarget = horn ? 1.0f : 0.0f;
    for (auto& v : voices) {
        if (!v.on) continue;
        v.age += chunkSeconds;
        if (v.punchPeakDb > 0.0f) {
            // The boost over the next chunk: from where it is to where it will be.
            v.punchPos += float(d::chunk);
            if (v.punchPos >= v.punchSamples) {
                v.punchStep = (1.0f - v.punchGain) / float(d::chunk);
                if (v.punchPos >= v.punchSamples + float(d::chunk)) { v.punchPeakDb = 0; v.punchGain = 1; v.punchStep = 0; }
            } else {
                float shape = 0.5f * (1.0f + std::cos(3.14159265f * v.punchPos / v.punchSamples));
                if (v.punchShape != 1.0f) shape = std::pow(shape, v.punchShape);
                const float db = v.punchPeakDb * shape;
                v.punchStep = (std::pow(10.0f, db * 0.05f) - v.punchGain) / float(d::chunk);
            }
        }
        if (v.edgeGain > 0.0f) {
            v.edgeGain *= v.edgeDecay;
            if (v.edgeGain < 1.0e-4f) v.edgeGain = 0;
        }
        const auto& des = design(v.designIndex);
        if (des.control != nullptr) {
            v.note.bpm = tempo;
            des.control(v, v.note, chunkSeconds);
        }
        // The mod wheel, as the instrument wants it.
        float vibrato = 0;
        float toneMul = 1;
        v.wheelLevel = 1;
        switch (des.wheel) {
            case ModWheel::Vibrato: vibrato = 0.35f * modNow; break;
            case ModWheel::Brightness: toneMul = std::exp2(2.0f * modNow); break;
            case ModWheel::Tremolo: {
                v.tremPhase += 5.2f * chunkSeconds;
                v.tremPhase -= std::floor(v.tremPhase);
                v.wheelLevel = 1.0f - 0.6f * modNow * (0.5f + 0.5f * d::sinCycles(v.tremPhase));
                break;
            }
            case ModWheel::Growl: v.drive = v.baseDrive + 0.6f * modNow; break;
            case ModWheel::Swell: v.wheelLevel = 0.55f + 0.9f * modNow; break;
        }
        if (toneMul != 1.0f || des.wheel == ModWheel::Brightness) v.tone.set(std::min(v.toneHz * toneMul, 0.45f * rate), v.toneQ, rate);
        for (auto& L : v.layer) {
            if (L.model == Model::None) continue;
            // Vibrato by wheel: the models with a vibrato take it directly; the others
            // are bent (a small pitch wobble at 5.3 Hz).
            float bendExtra = 0;
            if (vibrato > 0.0f) {
                switch (L.model) {
                    case Model::Synth: L.synth.vibratoDepth = std::max(L.synth.vibratoDepth, vibrato); break;
                    case Model::Choir: L.choir.vibratoDepth = std::max(L.choir.vibratoDepth, vibrato); break;
                    case Model::Wind: L.wind.vibratoDepth = std::max(L.wind.vibratoDepth, vibrato); break;
                    case Model::Bowed: L.bowed.vibratoDepth = std::max(L.bowed.vibratoDepth, vibrato); break;
                    case Model::Strings: L.strings.vibratoDepth = std::max(L.strings.vibratoDepth, vibrato); break;
                    case Model::Fm: L.fm.vibratoDepth = std::max(L.fm.vibratoDepth, vibrato); break;
                    default: {
                        v.state[7] += 5.3f * chunkSeconds;
                        v.state[7] -= std::floor(v.state[7]);
                        bendExtra = vibrato * d::sinCycles(v.state[7]);
                        break;
                    }
                }
            }
            L.update(rate, bendSemis + bendExtra, vibrato);
        }
        bool any = false;
        for (const auto& L : v.layer) any |= L.active();
        if (!any) {
            v.on = false;
            if (v.key >= 0 && !v.keyDown && !v.sustained) {
                bool still = false;
                for (const auto& o : voices) still |= o.on && o.key == v.key;
                if (!still) lit[std::size_t(v.key >> 6)].fetch_and(~(uint64_t(1) << (v.key & 63)), std::memory_order_relaxed);
            }
        }
    }
}

float hitClipShape(float x) noexcept {
    constexpr float knee = 0.56f, ceiling = 0.944f;   // -5 dBFS, -0.5 dBFS
    const float a = std::abs(x);
    if (a <= knee) return x;
    const float y = knee + (ceiling - knee) * std::tanh((a - knee) / (ceiling - knee));
    return x < 0.0f ? -y : y;
}

void Synth::render(float* outL, float* outR, int n) noexcept {
    for (int at = 0; at < n; at += CaptureBus::block) renderBlock(outL + at, outR + at, std::min(CaptureBus::block, n - at));
}

void Synth::renderBlock(float* outL, float* outR, int n) noexcept {
    for (int i = 0; i < n; ++i) {
        if (counter == 0) controlChunk();
        counter = (counter + 1) % d::chunk;
        float sumL = 0, sumR = 0;
        for (auto& v : voices) {
            if (!v.on) continue;
            float l = 0, r = 0;
            for (auto& L : v.layer)
                if (L.model != Model::None) L.tick(v.noise, l, r);
            // The post: high-pass, body, tone, drive.
            l = v.highPass[0].tick(l).hp;
            r = v.highPass[1].tick(r).hp;
            float bl = l, br = r;
            const float mid = 0.5f * (l + r);
            for (std::size_t k = 0; k < 3; ++k)
                if (v.bodyGain[k] != 0.0f) {
                    const float b = v.bodyGain[k] * v.body[k].bandpass(mid);
                    bl += b;
                    br += b;
                }
            if (v.edgeGain > 0.0f) {
                const float e = v.edgeGain * v.edge.bandpass(mid);
                bl += e;
                br += e;
            }
            l = bl;
            r = br;
            // One low-pass for both sides would smear the image; the tone filter runs on the
            // mid and the side keeps its own share above it.
            const float m = v.tone.tick(0.5f * (l + r)).lp;
            const float s = 0.5f * (l - r);
            l = m + s * std::min(1.0f, v.toneHz / 6000.0f);
            r = m - s * std::min(1.0f, v.toneHz / 6000.0f);
            if (v.drive > 0.0f) {
                const float k = 1.0f + 6.0f * v.drive;
                l = d::softClip(l * k) / k * (1.0f + v.drive);
                r = d::softClip(r * k) / k * (1.0f + v.drive);
            }
            float g = v.gain * v.wheelLevel;
            if (v.punchPeakDb > 0.0f) {
                g *= v.punchGain;
                v.punchGain += v.punchStep;
            }
            if (v.killing) {
                v.killGain -= v.killStep;
                if (v.killGain <= 0.0f) {
                    v.killGain = 0;
                    v.on = false;
                    for (auto& L : v.layer) L.reset();
                }
                g *= v.killGain;
            }
            sumL += l * g;
            sumR += r * g;
            if (v.capture != CaptureNone) captures.send(v.capture, i, 0.5f * (l + r) * g * v.captureSend);
        }
        if (clipMix != clipTarget) clipMix = clipTarget > clipMix ? std::min(clipTarget, clipMix + 0.004f) : std::max(clipTarget, clipMix - 0.004f);
        if (clipMix > 0.0f) {
            const float yl = sumL * level, yr = sumR * level;
            outL[i] += yl + clipMix * (hitClipShape(yl) - yl);
            outR[i] += yr + clipMix * (hitClipShape(yr) - yr);
        } else {
            outL[i] += sumL * level;
            outR[i] += sumR * level;
        }
    }
    // The captures' wet joins the instrument (before its place), at the same level.
    if (level != 1.0f) {
        std::array<float, CaptureBus::block> wl {}, wr {};
        captures.process(wl.data(), wr.data(), n);
        for (int i = 0; i < n; ++i) {
            outL[i] += wl[std::size_t(i)] * level;
            outR[i] += wr[std::size_t(i)] * level;
        }
    } else {
        captures.process(outL, outR, n);
    }
}

}
