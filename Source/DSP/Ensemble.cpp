#include "Modules.h"

// X. ENSEMBLE (Direction Update 2). Serial in this order: chorus -> rising phaser ->
// tape echo (added) -> water drops (added). Every part is skipped at zero.
namespace dc {
using namespace dsp;

void Ensemble::prepare(double sampleRate) {
    sr = sampleRate;
    for (auto& line : chorusLine) line.prepare(static_cast<std::size_t>(0.05 * sr));
    for (auto& line : tape) line.prepare(static_cast<std::size_t>(1.7 * sr));
    for (auto& s : ascend) s.prepare(sr, 0.045);
    for (auto* s : { &mix, &depth, &rate, &spiral, &spiralRate, &tapeMix, &feedback, &toneHz, &wow, &drops }) s->setTime(0.05, sr);
    heldDecay = static_cast<float>(std::exp(-1.0 / (0.3 * sr)));
    reset();
}

void Ensemble::reset() {
    for (auto& line : chorusLine) line.clear();
    for (auto& line : tape) line.clear();
    for (auto& s : ascend) s.clear();
    chorusPhase = { 0.0, 1.0 / 3.0, 2.0 / 3.0 };
    spiralPhase = 0;
    apState = {};
    phaserFb = {};
    tapeLp = tapeHp = tapeOut = {};
    headA = headB = -1; headFade = 1;
    wowPhase = flutterPhase = 0;
    held = 0;
    for (auto& d : dropPool) d.amp = 0;
    dropCountdown = static_cast<int>(0.3 * sr);
    envelope = 0;
    random.seed(0x0e5e3b1eu);
    mix.value = -1;
}

void Ensemble::process(float* l, float* r, int n, const Frame& p, const Context&) noexcept {
    mix.set(clamp01(p[EnsMix])); depth.set(std::clamp(p[EnsDepth], 0.5f, 12.0f)); rate.set(std::clamp(p[EnsRate], 0.05f, 4.0f));
    spiral.set(clamp01(p[Spiral])); spiralRate.set(std::clamp(p[SpiralRate], 0.02f, 1.0f));
    tapeMix.set(clamp01(p[TapeMix])); feedback.set(std::clamp(p[TapeFeedback], 0.0f, 0.9f));
    toneHz.set(std::clamp(p[TapeTone], 600.0f, 16000.0f)); wow.set(clamp01(p[TapeWow])); drops.set(clamp01(p[Drops]));
    if (mix.value < 0)
        for (auto* s : { &mix, &depth, &rate, &spiral, &spiralRate, &tapeMix, &feedback, &toneHz, &wow, &drops }) s->snap(s->target);
    const float wantedHead = std::clamp(p[TapeTime], 40.0f, 1500.0f) * 0.001f * static_cast<float>(sr);
    if (headB < 0) { headA = headB = wantedHead; headFade = 1; }
    else if (headFade >= 1.0f && std::abs(wantedHead - headB) > 0.004f * headB) { headA = headB; headB = wantedHead; headFade = 0; }
    const float fadeStep = static_cast<float>(1.0 / (0.06 * sr));
    const int newAscend = static_cast<int>(std::lround(p[TapeAscend]));
    if (newAscend != ascendKind) { ascendKind = newAscend; for (auto& s : ascend) s.clear(); }
    static constexpr float ascendRatio[] = { 1.0f, 1.0594631f, 1.4983071f, 2.0f, 0.7491535f };
    const float ratio = ascendRatio[std::clamp(ascendKind, 0, 4)];

    // The phaser's sweeps are set once per chunk (they rise slowly).
    const float sr_ = static_cast<float>(sr);
    std::array<float, phasers> apCoeff {}, window {};
    const float sp = spiral.skip(0);
    if (sp > 1.0e-4f) {
        spiralPhase += spiralRate.skip(0) * n / sr;
        spiralPhase -= std::floor(spiralPhase);
        for (int k = 0; k < phasers; ++k) {
            double ph = spiralPhase + static_cast<double>(k) / phasers;
            ph -= std::floor(ph);
            const double hz = 90.0 * std::pow(2.0, 6.3 * ph);
            const double t = std::tan(pi * std::min(hz, sr * 0.45) / sr);
            apCoeff[static_cast<std::size_t>(k)] = static_cast<float>((t - 1.0) / (t + 1.0));
            const float s = static_cast<float>(std::sin(pi * ph));
            window[static_cast<std::size_t>(k)] = s * s;
        }
    }
    const float lpA = poleForHz(toneHz.skip(0), sr), hpA = poleForHz(110.0, sr);

    for (int i = 0; i < n; ++i) {
        float x[2] = { finiteOr(l[i]), finiteOr(r[i]) };
        const float level = std::max(std::abs(x[0]), std::abs(x[1]));
        envelope += (level > envelope ? 0.01f : 0.0002f) * (level - envelope);
        // Chorus: three voices per side, their delays swinging slowly out of phase.
        const float m = mix.next(), dp = depth.next(), rt = rate.next();
        if (m > 1.0e-4f) {
            chorusLine[0].push(x[0]); chorusLine[1].push(x[1]);
            float wet[2] = { 0, 0 };
            for (int v = 0; v < voices; ++v) {
                auto& ph = chorusPhase[static_cast<std::size_t>(v)];
                ph += rt * (1.0 + 0.13 * v) / sr;
                ph -= std::floor(ph);
                for (int ch = 0; ch < 2; ++ch) {
                    const float swing = 0.5f + 0.5f * std::sin(twoPi * static_cast<float>(ph + 0.25 * ch));
                    const float ms = 7.0f + 4.0f * v + dp * swing;
                    wet[ch] += chorusLine[static_cast<std::size_t>(ch)].read(ms * 0.001f * sr_);
                }
            }
            for (int ch = 0; ch < 2; ++ch) x[ch] = x[ch] * (1.0f - 0.35f * m) + wet[ch] * (0.9f / voices) * m * 1.25f;
        }
        // Rising phaser: four staggered six-stage sweeps, windowed, summed.
        const float spv = spiral.next(); spiralRate.next();
        if (sp > 1.0e-4f) {
            for (int ch = 0; ch < 2; ++ch) {
                const auto C = static_cast<std::size_t>(ch);
                float sum = 0;
                for (int k = 0; k < phasers; ++k) {
                    const auto K = static_cast<std::size_t>(k);
                    const float a = apCoeff[K];
                    float y = x[ch] + 0.45f * phaserFb[C][K];
                    for (int s = 0; s < stages; ++s) {
                        auto& z = apState[C][K][static_cast<std::size_t>(s)];
                        const float out = a * y + z;
                        z = y - a * out;
                        y = out;
                    }
                    phaserFb[C][K] = softLimit(y, 1.5f);
                    sum += window[K] * 0.5f * (x[ch] + y);
                }
                x[ch] = lerp(x[ch], sum * 0.5f * 1.2f, spv);
            }
        }
        // Tape echo: left and right lines cross-fed (ping-pong), darkening and
        // saturating with every pass, with wow and flutter.
        const float tm = tapeMix.next(), fb = feedback.next(), wo = wow.next(); toneHz.next();
        if (tm > 1.0e-4f || held > 1.0e-4f) {
            wowPhase += 0.55 / sr; wowPhase -= std::floor(wowPhase);
            flutterPhase += 5.7 / sr; flutterPhase -= std::floor(flutterPhase);
            const float mod = wo * 0.0018f * sr_ * (0.5f + 0.5f * std::sin(twoPi * static_cast<float>(wowPhase)))
                            + wo * 0.00012f * sr_ * std::sin(twoPi * static_cast<float>(flutterPhase));
            headFade = std::min(1.0f, headFade + fadeStep);
            float out[2];
            for (int ch = 0; ch < 2; ++ch) {
                const auto& line = tape[static_cast<std::size_t>(ch)];
                const float a = line.read(headA + mod), b = line.read(headB + mod);
                out[ch] = a + headFade * (b - a);
            }
            const float send = tm * 0.5f * (x[0] + x[1]);
            for (int ch = 0; ch < 2; ++ch) {
                const auto C = static_cast<std::size_t>(ch);
                // Each side feeds the other: the repeats bounce between the walls.
                float back = out[1 - ch] * fb;
                if (ratio != 1.0f) back = ascend[C].process(back, ratio);
                tapeLp[C] += lpA * (back - tapeLp[C]);
                tapeHp[C] += hpA * (tapeLp[C] - tapeHp[C]);
                float v = std::tanh(1.3f * (tapeLp[C] - tapeHp[C])) / 1.3f;
                if (ch == 0) v += send;
                tape[C].push(finiteOr(v));
            }
            for (int ch = 0; ch < 2; ++ch) x[ch] += 1.15f * out[ch];
            held = std::max(std::max(std::abs(out[0]), std::abs(out[1])), held * heldDecay);
        }
        // Water drops: a gas bubble's resonance (its pitch rising as it closes) after a
        // tiny impact, rare and quiet; they follow the playing.
        const float dr = drops.next();
        if (dr > 1.0e-4f) {
            if (--dropCountdown <= 0) {
                dropCountdown = static_cast<int>(sr * (0.5 + 2.4 * random.unit()) / (0.4 + dr));
                const float bed = std::min(1.0f, envelope * 5.0f);
                for (auto& d : dropPool)
                    if (d.amp <= 1.0e-5f && bed > 0.05f) {
                        const float radius = 0.0011f + 0.0022f * random.unit();         // metres
                        const float damping = 0.13f / radius + 0.0072f * std::pow(radius, -1.5f);
                        d.hz = 3.0f / radius;                                           // Minnaert, roughly
                        d.chirp = 0.11f * damping;                                      // relative rise per second
                        d.decay = std::exp(-damping / sr_);
                        d.amp = (0.5f + 0.5f * random.unit()) * bed;
                        d.click = 1.0f;
                        d.phase = 0;
                        d.pan = 0.8f * random.bipolar();
                        break;
                    }
            }
            float dl = 0, drr = 0;
            for (auto& d : dropPool) {
                if (d.amp <= 1.0e-5f) continue;
                d.phase += d.hz / sr_;
                d.phase -= std::floor(d.phase);
                d.hz *= 1.0f + d.chirp / sr_;
                float v = d.amp * std::sin(twoPi * d.phase);
                if (d.click > 1.0e-3f) { v += 0.3f * d.amp * d.click * random.bipolar(); d.click *= 0.93f; }
                d.amp *= d.decay;
                dl += v * (1.0f - d.pan); drr += v * (1.0f + d.pan);
            }
            x[0] += 0.02f * dr * dl; x[1] += 0.02f * dr * drr;
        }
        l[i] = finiteOr(x[0]); r[i] = finiteOr(x[1]);
    }
}
}
