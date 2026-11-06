#include "DungeonSynth.h"

namespace dc {
namespace {
constexpr float hallLengthsMs[8] { 31.7f, 37.3f, 41.9f, 47.3f, 53.9f, 59.3f, 67.1f, 73.7f };
constexpr float hallModRates[8] { 0.31f, 0.43f, 0.37f, 0.53f, 0.47f, 0.61f, 0.71f, 0.97f };
constexpr float diffuserMs[4] { 4.7f, 3.6f, 12.7f, 9.3f };
constexpr float inSigns[8] { 1, -1, 1, -1, -1, 1, -1, 1 };
inline float mix(float a, float b, float t) noexcept { return a + (b - a) * t; }
// In-place 8-point Walsh-Hadamard transform, normalised (energy preserving).
inline void hadamard8(float* v) noexcept {
    for (int h = 1; h < 8; h <<= 1)
        for (int i = 0; i < 8; i += h << 1)
            for (int j = i; j < i + h; ++j) {
                const float a = v[j], b = v[j + h];
                v[j] = a + b;
                v[j + h] = a - b;
            }
    for (int i = 0; i < 8; ++i) v[i] *= 0.35355339f;
}
}

void DungeonSynth::prepare(double sampleRate) {
    sr = std::isfinite(sampleRate) && sampleRate > 1000 ? sampleRate : 48000;
    amount.setTime(0.08, sr);
    era.setTime(0.25, sr);
    chorusAmount.setTime(0.2, sr);
    send.setTime(0.05, sr);
    heard.setTime(0.05, sr);
    chorusLine.prepare(static_cast<std::size_t>(0.025 * sr) + 8);
    for (auto& w : wowLine) w.prepare(static_cast<std::size_t>(0.008 * sr) + 8);
    for (auto& d : dc) d.prepare(sr);
    for (int i = 0; i < lines; ++i) {
        hallLength[std::size_t(i)] = static_cast<float>(hallLengthsMs[i] * 0.001 * sr);
        hall[std::size_t(i)].prepare(static_cast<std::size_t>(hallLength[std::size_t(i)] + 0.0005 * sr) + 32);
    }
    for (int k = 0; k < diffusers; ++k) {
        diffuserLength[std::size_t(k)] = std::max(1.0f, std::round(static_cast<float>(diffuserMs[k] * 0.001 * sr)));
        diffuser[std::size_t(k)].prepare(static_cast<std::size_t>(diffuserLength[std::size_t(k)]) + 8);
    }
    preDelay.prepare(static_cast<std::size_t>(0.06 * sr) + 8192);
    envAttack = dsp::timeCoefficient(0.02, sr);
    envRelease = dsp::timeCoefficient(0.6, sr);
    slowCoefficient = dsp::timeCoefficient(0.6, sr);
    fastCoefficient = dsp::timeCoefficient(0.08, sr);
    glideCoefficient = dsp::timeCoefficient(0.25, sr);
    for (auto& f : weightHigh) f.set(60.0, sr, 1.414f);
    for (auto& f : weightShelf) f.highShelf(sr, 1700.0, 4.0);
    noise.seed(0x44535945u);
    holdFactor = std::max(1, static_cast<int>(std::lround(sr / 22050.0)));
    reset();
}

void DungeonSynth::reset() noexcept {
    amount.snap(targetAmount);
    era.snap(targetEra);
    chorusAmount.snap(targetChorus);
    send.snap(targetSend);
    heard.snap(targetHeard);
    for (auto& f : antiAlias) f.clear();
    for (auto& f : reconstruct) f.clear();
    held = { 0, 0 };
    holdCount = 0;
    chorusLine.clear();
    for (auto& w : wowLine) w.clear();
    for (auto& d : dc) d.clear();
    for (auto* bank : { &lowCut, &top, &top2 }) for (auto& f : *bank) f.clear();
    for (auto* bank : { &warmth, &shelf }) for (auto& f : *bank) { f.clear(); f.setNormalised(1, 0, 0, 1, 0, 0); }
    hissLow = hissHigh = { 0, 0 };
    envelope = 0;
    for (auto& h : hall) h.clear();
    for (auto& d : diffuser) d.clear();
    preDelay.clear();
    hallHigh.clear();
    hallLow.clear();
    for (auto& f : hallDark) f.clear();
    hallState = {};
    grainHeld = { 0, 0 };
    grainPhase = 1;
    hallRinging = false;
    quiet = 0;
    refSlow = refFast = outSlow = 0;
    gain = gainTarget = 1;
    for (auto& f : weightHigh) f.clear();
    for (auto& f : weightShelf) f.clear();
    now = derive(amount.value, era.value, chorusAmount.value);
    updateFilters(now, amount.value, era.value);
}

void DungeonSynth::setControls(float a, int e, float chorus, float hallSend, float hallHeard) noexcept {
    targetAmount = std::isfinite(a) ? std::clamp(a, 0.0f, 1.0f) : 0.0f;
    targetEra = e == Modern ? 1.0f : 0.0f;
    targetChorus = std::isfinite(chorus) ? std::clamp(chorus, 0.0f, 1.0f) : 0.6f;
    targetSend = std::isfinite(hallSend) ? std::clamp(hallSend, 0.0f, 2.0f) : 0.0f;
    targetHeard = std::isfinite(hallHeard) ? std::clamp(hallHeard, 0.0f, 2.0f) : 0.0f;
    amount.set(targetAmount);
    era.set(targetEra);
    chorusAmount.set(targetChorus);
    send.set(targetSend);
    heard.set(targetHeard);
    // Off and still: the Era and the chorus weight follow at once (nothing to glide).
    if (!shaping()) { era.snap(targetEra); chorusAmount.snap(targetChorus); }
}

// Everything the amount (a), the Era (e: 0 Classic .. 1 Modern) and the instrument's chorus
// weight set. Every amount, mix and depth is 0 at a = 0 (the identity).
DungeonSynth::Derived DungeonSynth::derive(float a, float e, float chorus) const noexcept {
    Derived d;
    const float c = 1.0f - e;
    d.side = 1.0f - a * 0.4f * c;                                      // Classic: narrower (side -4.4 dB)
    d.satDrive = dsp::dbToGain(a * mix(3.5f, 8.0f, e));                // cheap keyboard .. tape
    d.satAsym = a * mix(0.04f, 0.22f, e);                              // even harmonics: warmth
    d.satMix = std::min(1.0f, 3.0f * a);
    // The sampler path takes over early (by 17 %; a partial blend with the direct path would
    // comb-filter the top); Modern's hint of lo-fi is its 16 bits against Classic's 12.
    d.lofiMix = std::min(1.0f, 6.0f * a);
    // The sampler runs at its own rate (the host's divided by a whole number: 22.05 kHz at
    // 44.1 kHz, 24 kHz at 48 kHz, so its steps fall on samples and add no jitter), blended in
    // with the amount; its output filter takes its images.
    const float bits = 24.0f - a * (24.0f - mix(12.0f, 16.0f, e));
    d.bitStep = std::exp2(1.0f - bits);
    d.chorusMix = a * chorus * mix(0.18f, 0.5f, e);
    d.wowDepth = static_cast<float>(a * mix(1.0f, 0.3f, e) * 0.0011 * sr);
    d.flutterDepth = static_cast<float>(a * mix(1.0f, 0.25f, e) * 0.00005 * sr);
    d.lowCutMix = a;
    d.warmthDb = a * mix(0.5f, 2.5f, e);
    d.topMix = std::min(1.0f, 4.0f * a);
    d.shelfDb = -a * mix(0.5f, 4.0f, e);
    d.hissGain = std::pow(a, 2.5f) * dsp::dbToGain(mix(-48.0f, -64.0f, e));
    d.hallIn = std::pow(a, 1.2f) * mix(1.0f, 0.85f, e);
    d.hallWidth = mix(0.5f, 1.0f, e);
    d.grainMix = c;
    d.grainStep = static_cast<float>(std::min(1.0, 16000.0 / sr));
    d.grainBits = std::exp2(1.0f - 12.0f);
    return d;
}

void DungeonSynth::updateFilters(const Derived& d, float a, float e) noexcept {
    const double rate = sr / holdFactor;
    for (auto& f : antiAlias) f.set(0.4 * rate, sr, 1.414f);
    for (auto& f : reconstruct) f.set(0.42 * rate, sr, 1.414f);
    for (auto& f : lowCut) f.set(mix(75.0f, 35.0f, e), sr, 1.414f);
    // Front-loaded (a^0.5): clearly darker by 50 %, the cassette's top at 100 %.
    const double fc = 20000.0 * std::pow(mix(4200.0f, 6800.0f, e) / 20000.0, std::sqrt(static_cast<double>(a)));
    for (auto& f : top) f.set(fc, sr, 1.3f);
    for (auto& f : top2) f.set(std::min(0.45 * sr, fc * mix(1.5f, 3.0f, e)), sr, 1.414f);
    warmth[0].peaking(sr, 220.0, 0.7, d.warmthDb);
    shelf[0].highShelf(sr, 3500.0, d.shelfDb);
    for (auto* bank : { &warmth, &shelf }) {
        auto& b = (*bank)[1];
        const auto& m = (*bank)[0];
        b.b0 = m.b0; b.b1 = m.b1; b.b2 = m.b2; b.a1 = m.a1; b.a2 = m.a2;
    }
    // The hall: longer and darker towards the full amount; Classic static (metallic, grainy),
    // Modern slowly modulated (lush).
    const float t60 = mix(1.8f, mix(5.5f, 6.5f, e), a);
    for (int i = 0; i < lines; ++i)
        hallGain[std::size_t(i)] = std::pow(10.0f, -3.0f * hallLength[std::size_t(i)] / (t60 * static_cast<float>(sr)));
    hallDampCoefficient = dsp::poleForHz(mix(5000.0f, 6500.0f, e), sr);
    hallModDepth = static_cast<float>(mix(0.6f, 9.0f, e) * sr / 48000.0);
    hallLow.set(mix(3400.0f, 4800.0f, e), sr, 1.0f);
    hallHigh.set(240.0, sr, 1.414f);
    hallPreDelay = static_cast<float>(mix(0.022f, 0.038f, e) * sr);
    for (auto& f : hallDark) f.set(mix(3400.0f, 4600.0f, e), sr, 1.414f);
    hallOut = mix(0.5f, 0.4f, e);
}

float DungeonSynth::quantise(float x, float step) noexcept {
    // Gently companded (mu = 4), as the old samplers did: quiet tails stay smoother.
    constexpr float mu = 4.0f, norm = 1.6094379f;   // log1p(4)
    const float m = std::abs(x);
    const float c = std::log1p(mu * std::min(m, 4.0f)) / norm;
    const float q = std::round(c / step) * step;
    return std::copysign(std::expm1(q * norm) / mu, x);
}

void DungeonSynth::runHall(float in, float& outL, float& outR, const Derived& d) noexcept {
    preDelay.push(in);
    float x = preDelay.read(1.0f + hallPreDelay + static_cast<float>(hallExtraDelay));
    x = hallLow.low(hallHigh.highPass(x));
    for (int k = 0; k < diffusers; ++k) {
        auto& line = diffuser[std::size_t(k)];
        const float z = line.readInteger(static_cast<std::size_t>(diffuserLength[std::size_t(k)]));
        const float v = x + 0.6f * z;
        x = z - 0.6f * v;
        line.push(v);
    }
    float o[lines];
    for (int i = 0; i < lines; ++i) {
        const auto k = std::size_t(i);
        auto& phase = hallModPhase[k];
        phase += hallModRates[i] / sr;
        if (phase >= 1.0) phase -= 1.0;
        const float mod = hallModDepth * (1.0f + std::sin(dsp::twoPi * static_cast<float>(phase)));
        const float rd = hall[k].read(hallLength[k] - mod);
        hallState[k] += hallDampCoefficient * (rd - hallState[k]);
        o[i] = hallState[k] * hallGain[k];
    }
    const float l = 0.5f * (o[0] - o[2] + o[4] - o[6]), r = 0.5f * (o[1] - o[3] + o[5] - o[7]);
    hadamard8(o);
    for (int i = 0; i < lines; ++i) hall[std::size_t(i)].push(o[i] + x * inSigns[i]);
    // Width (Classic narrower), then the old-digital grain: a lower rate, ten bits.
    const float m = 0.5f * (l + r), s = 0.5f * (l - r) * d.hallWidth;
    outL = m + s;
    outR = m - s;
    if (d.grainMix > 0.0f) {
        grainPhase += d.grainStep;
        if (grainPhase >= 1.0f) {
            grainPhase -= 1.0f;
            grainHeld[0] = quantise(outL, d.grainBits);
            grainHeld[1] = quantise(outR, d.grainBits);
        }
        outL += d.grainMix * (grainHeld[0] - outL);
        outR += d.grainMix * (grainHeld[1] - outR);
    }
    outL = hallDark[2].low(hallDark[0].low(outL)) * hallOut;
    outR = hallDark[3].low(hallDark[1].low(outR)) * hallOut;
}

void DungeonSynth::process(float* l, float* r, float* hallL, float* hallR, int n) noexcept {
    float hallPeak = 0;
    int at = 0;
    while (at < n) {
        const int m = std::min(controlBlock, n - at);
        amount.skip(m);
        era.skip(m);
        chorusAmount.skip(m);
        send.skip(m);
        heard.skip(m);
        bool justStopped = false;
        if (targetAmount <= 0.0f && amount.value < 1.0e-5f && amount.value != 0.0f) { amount.snap(0.0f); justStopped = true; }
        if (std::abs(era.value - targetEra) < 1.0e-5f) era.snap(targetEra);
        const float a = amount.value, e = era.value;
        const Derived next = derive(a, e, chorusAmount.value);
        updateFilters(next, a, e);
        const float sendNow = send.value, heardNow = heard.value;
        if (!shaping()) {
            // Off: the signal passes exactly; only the hall rings out.
            if (justStopped) {
                for (auto& f : antiAlias) f.clear();
                for (auto& f : reconstruct) f.clear();
                chorusLine.clear();
                for (auto& w : wowLine) w.clear();
                for (auto& dcb : dc) dcb.clear();
                for (auto* bank : { &lowCut, &top, &top2 }) for (auto& f : *bank) f.clear();
                for (auto* bank : { &warmth, &shelf }) for (auto& f : *bank) f.clear();
                envelope = 0;
                refSlow = refFast = outSlow = 0;
                gain = gainTarget = 1;
                for (auto& f : weightHigh) f.clear();
                for (auto& f : weightShelf) f.clear();
            }
            for (int i = at; i < at + m; ++i) {
                float hl = 0, hr = 0;
                if (hallRinging) runHall(0.0f, hl, hr, next);
                hallL[i] = hl * sendNow;
                hallR[i] = hr * sendNow;
                hallPeak = std::max(hallPeak, std::abs(hl) + std::abs(hr));
            }
            now = next;
            at += m;
            continue;
        }
        const float step = 1.0f / static_cast<float>(m);
        const float matchWeight = std::min(1.0f, 4.0f * a);
        for (int j = 0; j < m; ++j) {
            const int i = at + j;
            const float t = static_cast<float>(j + 1) * step;
            // The derived values glide across the control block (no steps).
            Derived d;
            d.side = mix(now.side, next.side, t);
            d.satDrive = mix(now.satDrive, next.satDrive, t);
            d.satAsym = mix(now.satAsym, next.satAsym, t);
            d.satMix = mix(now.satMix, next.satMix, t);
            d.lofiMix = mix(now.lofiMix, next.lofiMix, t);
            d.chorusMix = mix(now.chorusMix, next.chorusMix, t);
            d.wowDepth = mix(now.wowDepth, next.wowDepth, t);
            d.flutterDepth = mix(now.flutterDepth, next.flutterDepth, t);
            d.lowCutMix = mix(now.lowCutMix, next.lowCutMix, t);
            d.topMix = mix(now.topMix, next.topMix, t);
            d.hissGain = mix(now.hissGain, next.hissGain, t);
            d.hallIn = mix(now.hallIn, next.hallIn, t);
            d.hallWidth = mix(now.hallWidth, next.hallWidth, t);
            d.grainMix = mix(now.grainMix, next.grainMix, t);
            d.grainStep = next.grainStep;
            d.grainBits = next.grainBits;
            const float xl = l[i], xr = r[i];
            // The sampler: an anti-alias filter, sample-and-hold at the lower rate, fewer bits.
            float yl = xl, yr = xr;
            if (d.lofiMix > 0.0f) {
                const float al = antiAlias[2].low(antiAlias[0].low(xl)), ar = antiAlias[3].low(antiAlias[1].low(xr));
                if (++holdCount >= holdFactor) {
                    holdCount = 0;
                    held[0] = al;
                    held[1] = ar;
                }
                const bool coarse = next.bitStep > 1.0e-6f;
                const float ql = reconstruct[2].low(reconstruct[0].low(coarse ? quantise(held[0], next.bitStep) : held[0]));
                const float qr = reconstruct[3].low(reconstruct[1].low(coarse ? quantise(held[1], next.bitStep) : held[1]));
                yl += d.lofiMix * (ql - yl);
                yr += d.lofiMix * (qr - yr);
            }
            // The chorus: three voices from the mid, slow sweeps and a little vibrato.
            chorusLine.push(0.5f * (yl + yr));
            if (d.chorusMix > 0.0f) {
                vibratoPhase += 5.6 / sr;
                if (vibratoPhase >= 1.0) vibratoPhase -= 1.0;
                float v[3];
                static constexpr float base[3] { 0.007f, 0.010f, 0.013f }, rate[3] { 0.19f, 0.27f, 0.41f };
                for (int k = 0; k < 3; ++k) {
                    auto& ph = chorusPhase[std::size_t(k)];
                    ph += rate[k] / sr;
                    if (ph >= 1.0) ph -= 1.0;
                    const float delay = base[k] + 0.0016f * std::sin(dsp::twoPi * static_cast<float>(ph))
                                      + 0.00012f * std::sin(dsp::twoPi * static_cast<float>(vibratoPhase) + 2.1f * static_cast<float>(k));
                    v[k] = chorusLine.read(1.0f + delay * static_cast<float>(sr));
                }
                const float c = d.chorusMix;
                yl = yl * (1.0f - 0.3f * c) + c * (0.75f * v[0] + 0.45f * v[1]);
                yr = yr * (1.0f - 0.3f * c) + c * (0.75f * v[2] + 0.45f * v[1]);
            }
            // Tape saturation (small-signal gain 1; the asymmetry adds even harmonics).
            if (d.satMix > 0.0f) {
                const float g = d.satDrive, inv = 1.0f / g;
                const float sl = dc[0].process(std::tanh(g * (yl + d.satAsym * yl * yl)) * inv);
                const float sr2 = dc[1].process(std::tanh(g * (yr + d.satAsym * yr * yr)) * inv);
                yl += d.satMix * (sl - yl);
                yr += d.satMix * (sr2 - yr);
            }
            // Wow and flutter: one modulated delay for both sides (the tape moves for both).
            {
                wowPhase += 0.55 / sr;
                if (wowPhase >= 1.0) wowPhase -= 1.0;
                flutterPhase += 6.4 / sr;
                if (flutterPhase >= 1.0) flutterPhase -= 1.0;
                flutter2Phase += 11.3 / sr;
                if (flutter2Phase >= 1.0) flutter2Phase -= 1.0;
                if (--driftCounter <= 0) {
                    driftCounter = static_cast<int>(0.4 * sr);
                    driftTarget = noise.bipolar();
                }
                drift += 1.0e-4f * (driftTarget - drift);
                const float delay = d.wowDepth * (1.0f + 0.7f * std::sin(dsp::twoPi * static_cast<float>(wowPhase)) + 0.3f * drift)
                                  + d.flutterDepth * (1.0f + 0.7f * std::sin(dsp::twoPi * static_cast<float>(flutterPhase))
                                                     + 0.3f * std::sin(dsp::twoPi * static_cast<float>(flutter2Phase)));
                wowLine[0].push(yl);
                wowLine[1].push(yr);
                yl = wowLine[0].read(1.0f + std::max(0.0f, delay));
                yr = wowLine[1].read(1.0f + std::max(0.0f, delay));
            }
            // Hiss that follows the music (none in silence), on the tape: the roll-off below darkens it too.
            const float level = std::max(std::abs(xl), std::abs(xr));
            envelope += (level > envelope ? envAttack : envRelease) * (level - envelope);
            if (d.hissGain > 0.0f) {
                for (int ch = 0; ch < 2; ++ch) {
                    const float w = noise.bipolar();
                    hissLow[std::size_t(ch)] += 0.28f * (w - hissLow[std::size_t(ch)]);
                    hissHigh[std::size_t(ch)] += 0.05f * (hissLow[std::size_t(ch)] - hissHigh[std::size_t(ch)]);
                    const float h = (hissLow[std::size_t(ch)] - hissHigh[std::size_t(ch)]) * d.hissGain * envelope;
                    (ch == 0 ? yl : yr) += h;
                }
            }
            // Low cut, low-mid warmth, the rolled-off top and the softer shelf.
            yl += d.lowCutMix * (lowCut[0].highPass(yl) - yl);
            yr += d.lowCutMix * (lowCut[1].highPass(yr) - yr);
            yl = warmth[0].process(yl);
            yr = warmth[1].process(yr);
            const float tl = top2[0].low(top[0].low(yl)), tr = top2[1].low(top[1].low(yr));
            yl += d.topMix * (tl - yl);
            yr += d.topMix * (tr - yr);
            yl = shelf[0].process(yl);
            yr = shelf[1].process(yr);
            // Classic: a narrower image.
            {
                const float mid = 0.5f * (yl + yr), side = 0.5f * (yl - yr) * d.side;
                yl = mid + side;
                yr = mid - side;
            }
            // The hall (a send from here).
            float hl = 0, hr = 0;
            runHall(0.5f * (yl + yr) * d.hallIn, hl, hr, d);
            hallPeak = std::max(hallPeak, std::abs(hl) + std::abs(hr));
            // The level matcher: the stage and what is heard of the hall at the input's level.
            const float wl = weightShelf[0].process(weightHigh[0].highPass(xl)), wr = weightShelf[1].process(weightHigh[1].highPass(xr));
            const float ref = 0.5f * (wl * wl + wr * wr);
            const float pl = weightShelf[2].process(weightHigh[2].highPass(yl + heardNow * hl));
            const float pr = weightShelf[3].process(weightHigh[3].highPass(yr + heardNow * hr));
            const float out = 0.5f * (pl * pl + pr * pr);
            refSlow += slowCoefficient * (ref - refSlow);
            refFast += fastCoefficient * (ref - refFast);
            outSlow += slowCoefficient * (out - outSlow);
            if (--matchCounter <= 0) {
                matchCounter = 16;
                if (refSlow > 1.0e-7f && refFast > 0.3f * refSlow && outSlow > 1.0e-12f)
                    gainTarget = std::clamp(std::sqrt(refSlow / outSlow), 0.5f, 2.0f);
            }
            gain += glideCoefficient * (gainTarget - gain);
            const float g = 1.0f + (gain - 1.0f) * matchWeight;
            l[i] = yl * g;
            r[i] = yr * g;
            hallL[i] = hl * sendNow * g;
            hallR[i] = hr * sendNow * g;
        }
        now = next;
        at += m;
    }
    // The hall rings while it has input or is still audible; then it rests (cleared).
    if (shaping() && now.hallIn > 0.0f) { hallRinging = true; quiet = 0; }
    else if (hallRinging) {
        if (hallPeak < 1.0e-6f) quiet += n;
        else quiet = 0;
        if (quiet > static_cast<int>(0.5 * sr)) {
            hallRinging = false;
            for (auto& h : hall) h.clear();
            for (auto& dl : diffuser) dl.clear();
            preDelay.clear();
            hallHigh.clear();
            hallLow.clear();
            for (auto& f : hallDark) f.clear();
            hallState = {};
            grainHeld = { 0, 0 };
        }
    }
}
}
