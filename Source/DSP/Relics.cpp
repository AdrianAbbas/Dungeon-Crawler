#include "Modules.h"
#include "../Instrument/SampleBank.h"

namespace dc {
using namespace dsp;

namespace {
// Partial ratios and weights of the three metal kinds (DUNGEON CRAWLER addition).
// Bars: struck iron bars (plate-like modes); Bell: the minor-third church bell
// (hum, prime, tierce, quint, nominal...); Crystal: sparse, bright, glassy modes.
struct Kind { float ratio[Relics::partials]; float weight[Relics::partials]; float decayScale[Relics::partials]; };
constexpr Kind kinds[3] = {
    { { 1.0f, 1.51f, 2.76f, 3.62f, 5.40f, 6.73f }, { 1.0f, .7f, .55f, .4f, .3f, .2f }, { 1.0f, .8f, .6f, .5f, .4f, .3f } },
    { { .5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f }, { .6f, 1.0f, .65f, .45f, .55f, .3f }, { 1.6f, 1.0f, .8f, .7f, .55f, .4f } },
    { { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.1f }, { 1.0f, .6f, .45f, .3f, .2f, .12f }, { 1.0f, .9f, .75f, .6f, .5f, .4f } },
};
}

void Relics::prepare(double sampleRate) {
    sr = sampleRate;
    for (auto* s : { &flicker, &metal, &wind, &drips, &whispers }) s->setTime(0.04, sr);
    gainSmooth = timeCoefficient(0.008, sr);
    attack = timeCoefficient(0.02, sr);
    release = timeCoefficient(1.2, sr);
    reset();
}

void Relics::reset() {
    for (auto& channel : resonators) for (auto& f : channel) f.clear();
    for (auto& f : windFilter) f.clear();
    envelope = 0; flickerGain = 1; flickerTarget = 0; flickerValue = 0; flickerPhase = 0; flickerCountdown = 0;
    windPhase = 0;
    for (auto& d : dropsActive) d = Drop {};
    dripCountdown = static_cast<int>(0.3 * sr);
    random.seed(0x52454c43u);
    configured = -1;
    for (auto& w : whisperVoices) w = Whisper {};
    whisperCountdown = static_cast<int>(0.4 * sr);
    for (auto& f : whisperBand) f.clear();
    whisperLevel = 0;
    flicker.value = -1;   // not primed
}

void Relics::configure(int kind, int note, int octave, float decaySeconds) noexcept {
    const int key = (kind * 16 + note) * 16 + octave;
    if (key == configured && std::abs(decaySeconds - configuredDecay) < 1.0e-3f) return;
    configured = key;
    configuredDecay = decaySeconds;
    const auto& k = kinds[std::clamp(kind, 0, 2)];
    const double base = noteFrequency(std::clamp(note, 0, 11), std::clamp(octave, 0, 4) + 2);
    for (int i = 0; i < partials; ++i) {
        const auto I = static_cast<std::size_t>(i);
        const double hz = std::min(base * k.ratio[i], sr * 0.42);
        const double t60 = std::max(0.02, static_cast<double>(decaySeconds * k.decayScale[i]));
        // Bandwidth for a T60: ln(1000) / (pi * T60) Hz; damping k = bandwidth / f.
        const float damping = static_cast<float>(std::clamp(6.9078 / (pi * t60 * hz), 0.0005, 1.0));
        for (auto& channel : resonators) {
            const float s1 = channel[I].s1, s2 = channel[I].s2;
            channel[I].set(hz, sr, damping);
            channel[I].s1 = s1; channel[I].s2 = s2;
        }
        partialGain[I] = damping * k.weight[i] * (hz < sr * 0.4 ? 1.0f : 0.0f);
    }
}

void Relics::process(float* l, float* r, int n, const Frame& p, const Context&) noexcept {
    flicker.set(clamp01(p[Flicker]));
    metal.set(clamp01(p[Metal]));
    wind.set(clamp01(p[Wind]));
    drips.set(clamp01(p[Drips]));
    // Whispers need the developer's recordings; without them the place is as it was.
    const wb::samples::Instrument* whisperTakes = nullptr;
    if (p[Whispers] > 1.0e-4f && wb::samples::Bank::get().ready()) whisperTakes = wb::samples::Bank::get().instrument("voice-whispers");
    whispers.set(whisperTakes != nullptr && !whisperTakes->zones.empty() ? clamp01(p[Whispers]) : 0.0f);
    if (flicker.value < 0)
        for (auto* s : { &flicker, &metal, &wind, &drips, &whispers }) s->snap(s->target);
    // The whisper band: the breath of the voice, no rumble, no hiss above it.
    for (int side = 0; side < 2; ++side) {
        whisperBand[static_cast<std::size_t>(2 * side)].set(500.0, sr, 0.7f);
        whisperBand[static_cast<std::size_t>(2 * side + 1)].set(2800.0, sr, 1.0f);
    }
    configure(static_cast<int>(std::lround(p[MetalKind])), static_cast<int>(std::lround(p[MetalNote])),
              static_cast<int>(std::lround(p[MetalOctave])), std::clamp(p[MetalDecay], 0.05f, 8.0f));
    const float rate = std::clamp(p[FlickerRate], 0.3f, 12.0f);
    const float flickerFollow = timeCoefficient(0.35 / rate, sr);
    const float windCentre = 380.0f + 260.0f * std::sin(twoPi * windPhase);
    windPhase += static_cast<float>(n * 0.07 / sr);
    if (windPhase >= 1) windPhase -= 1;
    for (auto& f : windFilter) f.set(windCentre, sr, 0.9f);

    float* io[] = { l, r };
    for (int i = 0; i < n; ++i) {
        const float fl = flicker.next(), me = metal.next(), wi = wind.next(), dr = drips.next(), wh = whispers.next();
        const float x0 = finiteOr(l[i]), x1 = finiteOr(r[i]);
        const float level = std::max(std::abs(x0), std::abs(x1));
        envelope += (level > envelope ? attack : release) * (level - envelope);

        // Torch flicker: an irregular tremolo (a random walk plus a slow sine).
        if (--flickerCountdown <= 0) {
            flickerCountdown = static_cast<int>(sr / (rate * (0.6f + 0.8f * random.unit())));
            flickerTarget = random.bipolar();
        }
        flickerValue += flickerFollow * (flickerTarget - flickerValue);
        flickerPhase += rate * 0.37f / static_cast<float>(sr);
        if (flickerPhase >= 1) flickerPhase -= 1;
        const float lfo = 0.65f * flickerValue + 0.35f * std::sin(twoPi * flickerPhase);
        const float wantedGain = 1.0f - fl * 0.55f * (0.5f + 0.5f * lfo);
        flickerGain += gainSmooth * (wantedGain - flickerGain);

        // Beds follow the playing (never a constant noise floor).
        const float bed = std::min(1.0f, envelope * 4.0f);
        float windL = 0, windR = 0;
        if (wi > 1.0e-4f) {
            windL = windFilter[0].bandPass(random.bipolar()) * wi * 0.22f * bed;
            windR = windFilter[1].bandPass(random.bipolar()) * wi * 0.22f * bed;
        }
        float drop = 0;
        if (dr > 1.0e-4f) {
            if (--dripCountdown <= 0) {
                dripCountdown = static_cast<int>(sr * (0.25 + 1.6 * random.unit()) / (0.4 + dr));
                for (auto& d : dropsActive)
                    if (d.amplitude <= 1.0e-4f) {
                        d.phase = 0;
                        d.frequency = 900.0f + 1500.0f * random.unit();
                        d.amplitude = (0.4f + 0.6f * random.unit()) * bed;
                        d.pan = random.bipolar() * 0.7f;
                        break;
                    }
            }
        }
        float dropL = 0, dropR = 0;
        for (auto& d : dropsActive) {
            if (d.amplitude <= 1.0e-4f) continue;
            // A water drop: a sine whose pitch sweeps up while it dies away.
            d.frequency *= 1.0f + static_cast<float>(14.0 / sr);
            d.phase += d.frequency / static_cast<float>(sr);
            if (d.phase >= 1) d.phase -= 1;
            const float v = d.amplitude * std::sin(twoPi * d.phase);
            d.amplitude *= 1.0f - static_cast<float>(45.0 / sr);
            dropL += v * (1.0f - d.pan); dropR += v * (1.0f + d.pan);
        }
        drop = dr * 0.06f;

        // Whispers: a few seconds of the developer's whispering from anywhere in one of
        // his three takes, then another, never loud (they follow the bed: no playing, no
        // whispers), panned apart, band-limited.
        float whisperL = 0, whisperR = 0;
        if (wh > 1.0e-4f && whisperTakes != nullptr) {
            if (--whisperCountdown <= 0) {
                whisperCountdown = static_cast<int>(sr * (0.5 + 1.8 * random.unit()));
                for (auto& w : whisperVoices)
                    if (!w.on) {
                        const auto& z = whisperTakes->zones[static_cast<std::size_t>(random.unit() * 0.999f * static_cast<float>(whisperTakes->zones.size()))];
                        const float span = static_cast<float>(1.6 + 1.8 * random.unit());
                        const int need = static_cast<int>(span * whisperTakes->rate) + 8;
                        if (z.length <= need) break;
                        w.data = z.data;
                        w.length = z.length;
                        w.pos = static_cast<double>(random.unit()) * static_cast<double>(z.length - need);
                        w.step = static_cast<float>(whisperTakes->rate / sr) * (0.96f + 0.08f * random.unit());
                        w.age = 0;
                        w.span = span;
                        w.gain = 0.6f + 0.4f * random.unit();
                        w.pan = 0.75f * random.bipolar();
                        w.on = true;
                        break;
                    }
            }
            float wl = 0, wr = 0;
            for (auto& w : whisperVoices) {
                if (!w.on) continue;
                const auto i0 = static_cast<int>(w.pos);
                const float t = static_cast<float>(w.pos - i0);
                const float x = (w.data[i0] + t * static_cast<float>(w.data[i0 + 1] - w.data[i0])) * (1.0f / 32768.0f);
                const float edge = std::min({ 1.0f, w.age / 0.25f, (w.span - w.age) / 0.4f });
                const float y = x * w.gain * edge * edge;
                wl += y * (1.0f - w.pan) * 0.7071f;
                wr += y * (1.0f + w.pan) * 0.7071f;
                w.pos += w.step;
                w.age += static_cast<float>(1.0 / sr);
                if (w.age >= w.span || w.pos >= static_cast<double>(w.length - 2)) w.on = false;
            }
            // Per side: a high-pass (the filter's high output), then a gentle low-pass.
            whisperLevel = wh * 0.32f * bed;
            float out[2] = { wl, wr };
            for (int side = 0; side < 2; ++side) {
                float band = 0, high = 0, unused = 0;
                whisperBand[static_cast<std::size_t>(2 * side)].process(out[side], band, high);
                out[side] = whisperBand[static_cast<std::size_t>(2 * side + 1)].process(high, band, unused);
            }
            whisperL = out[0] * whisperLevel;
            whisperR = out[1] * whisperLevel;
        }

        float in[2] = { x0, x1 };
        for (int ch = 0; ch < 2; ++ch) {
            float y = in[ch];
            if (me > 1.0e-4f) {
                float ring = 0;
                for (int k = 0; k < partials; ++k) {
                    const auto K = static_cast<std::size_t>(k);
                    ring += partialGain[K] * resonators[static_cast<std::size_t>(ch)][K].bandPass(in[ch]);
                }
                y += me * 2.2f * softLimit(ring, 1.5f);
            }
            y = y * flickerGain + (ch == 0 ? windL + drop * dropL : windR + drop * dropR);
            if (wh > 1.0e-4f) y += ch == 0 ? whisperL : whisperR;
            io[ch][i] = finiteOr(y);
        }
    }
}
}
