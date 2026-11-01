#include "Synth.h"

namespace dc::inst {

//==============================================================================
bool SampleVoice::start(const wb::samples::Instrument& inst, float noteMidi, float velocity, float pick, float sampleRate, float attackSeconds,
                        float decaySeconds, float skipSeconds, float even, float courseCents, float courseLvl) {
    const auto* z = inst.find(int(std::lround(noteMidi)), velocity, pick);
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
    const float evenDb = std::clamp(even * (inst.medianDb(z->layer) - z->loudnessDb), -12.0f, 12.0f);
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
            pos = tailPos;
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
    Note n;
    n.key = std::clamp(key + int(std::lround(des.transpose)), 0, 127);
    n.hz = 440.0f * std::exp2((float(key) + des.transpose - 69.0f) / 12.0f);
    n.velocity = v.velocity;
    ++noteCount;
    n.random = d::unipolar(d::mix64(noteCount * 0x9E3779B97F4A7C15ull + uint64_t(key)));
    n.macro = macros;
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
    v.drive = v.baseDrive = p.drive;
    v.gain = d::dbToGain(designTrimDb(designNow));
    const auto cap = captureOf(designNow);
    v.capture = captures.available(cap.capture) ? cap.capture : CaptureNone;
    v.captureSend = cap.send;
    des.start(v, n);
    // Velocity sets the level of the blown, bowed, sung and held sources (struck and
    // plucked models already respond to it in their own way).
    const float velocityGain = 0.35f + 0.65f * std::pow(v.velocity, 1.3f);
    for (auto& L : v.layer)
        switch (L.model) {
            case Model::Synth: case Model::Choir: case Model::Wind: case Model::Bowed: case Model::Strings: case Model::Organ: case Model::Fm: case Model::Sample:
                L.gain *= velocityGain;
                break;
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
    for (auto& v : voices) {
        if (!v.on) continue;
        v.age += chunkSeconds;
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
        outL[i] += sumL * level;
        outR[i] += sumR * level;
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
