#include "Warmth.h"

namespace dc {
namespace {
inline float mix(float a, float b, float t) noexcept { return a + (b - a) * t; }
// Cents of pitch swing to the fractional rate change.
inline float centsToFraction(float cents) noexcept { return std::exp2(cents / 1200.0f) - 1.0f; }
// A rational tanh (Pade 7/6, within 2e-4 of tanh; exactly +/-1 beyond +/-4.97).
inline float fastTanh(float x) noexcept {
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2))) / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2)));
}
// sin(2 pi p) for a phase p in [0, 1): a parabola, refined once (within 0.1 %, smooth; for the LFOs).
inline float fastSinCycles(float p) noexcept {
    const float t = p - std::floor(p + 0.5f);
    const float y = 8.0f * t - 16.0f * t * std::abs(t);
    return 0.225f * (y * std::abs(y) - y) + y;
}
// Asymmetric tube / tape transfer: unity slope at zero, the bias tilts it (even harmonics).
inline float tube(float u, float k, float bias, float t0, float inv) noexcept { return (fastTanh(k * (u + bias)) - t0) * inv; }
// In-place 8-point Walsh-Hadamard transform, normalised.
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
constexpr float hallMs[8] { 29.3f, 34.1f, 39.7f, 44.9f, 51.1f, 57.7f, 63.9f, 71.3f };
constexpr float hallRates[8] { 0.21f, 0.33f, 0.27f, 0.41f, 0.37f, 0.47f, 0.53f, 0.61f };
constexpr float diffuserMs[4] { 4.3f, 11.9f, 5.1f, 13.7f };   // two for each side
// The ensemble: three voices (base delays per side, ms), their sweep rates (Hz) and weights per side.
constexpr float ensembleBaseL[3] { 7.2f, 11.6f, 16.1f }, ensembleBaseR[3] { 8.1f, 12.9f, 17.3f };
constexpr float ensembleRates[3] { 0.15f, 0.27f, 0.43f };
constexpr float ensembleOffset[3] { 1.9f, 2.6f, 3.4f };   // the right side's sweep against the left's (radians)
constexpr float ensembleWeightL[3] { 0.5f, 0.35f, 0.3f }, ensembleWeightR[3] { 0.3f, 0.5f, 0.35f };
// The hiss: the dry signal's level against the whole output's (the glue's make-up and the hall
// lie between them) and the hiss's own power below 8 kHz, measured on the test phrase (four
// instruments in their places, both Eras: -8.5 dB) so the share lands where it is asked to.
constexpr float airCalibration = 0.1413f;
// The Grit's harmonics at Grit 100 % (of the rectified band, against the music).
constexpr float gritExciter = 0.6f;
}

float readCubic(const dsp::DelayLine& line, float delay) noexcept {
    delay = std::clamp(delay, 3.0f, static_cast<float>(line.data.size() - 4));
    const float position = static_cast<float>(line.write) - delay + static_cast<float>(line.data.size());
    const auto index = static_cast<std::size_t>(position);
    const float f = position - static_cast<float>(index);
    const float xm1 = line.data[(index - 1) & line.mask], x0 = line.data[index & line.mask];
    const float x1 = line.data[(index + 1) & line.mask], x2 = line.data[(index + 2) & line.mask];
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

// ---------------------------------------------------------------------------------------------
void Oversampler::prepare(int factor) {
    f = factor >= 4 ? 4 : 2;
    n = f * tapsPerPhase;
    // Kaiser windowed sinc, cutoff just under the host's Nyquist (as a fraction of the fast rate),
    // about 50 dB down from 26 kHz at 48 kHz.
    const double cutoff = 0.5 / f * 0.96, beta = 4.6;
    const auto i0 = [](double x) {
        double sum = 1, term = 1;
        for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
        return sum;
    };
    std::vector<double> v(static_cast<std::size_t>(n));
    double total = 0;
    for (int k = 0; k < n; ++k) {
        const double x = k - 0.5 * (n - 1), w = i0(beta * std::sqrt(std::max(0.0, 1.0 - std::pow(2.0 * x / (n - 1), 2.0)))) / i0(beta);
        const double s = std::sin(2.0 * dsp::pi * cutoff * x) / (dsp::pi * x);
        v[std::size_t(k)] = s * w;
        total += v[std::size_t(k)];
    }
    h.assign(std::size_t(n), 0.0f);
    for (int k = 0; k < n; ++k) h[std::size_t(k)] = static_cast<float>(v[std::size_t(k)] / total);
    clear();
}

void Oversampler::clear() noexcept {
    hist.fill(0.0f);
    fast.fill(0.0f);
    histPos = fastPos = 0;
}

void Oversampler::up(float x, float* out) noexcept {
    histPos = (histPos + 1) & 63;
    hist[std::size_t(histPos)] = x;
    const float g = static_cast<float>(f);
    for (int j = 0; j < f; ++j) {
        float acc = 0;
        for (int m = 0; m < tapsPerPhase; ++m) acc += h[std::size_t(j + f * m)] * hist[std::size_t((histPos - m) & 63)];
        out[j] = acc * g;
    }
}

float Oversampler::down(const float* in) noexcept {
    for (int j = 0; j < f; ++j) {
        fastPos = (fastPos + 1) & 511;
        fast[std::size_t(fastPos)] = in[j];
    }
    float acc = 0;
    for (int k = 0; k < n; ++k) acc += h[std::size_t(k)] * fast[std::size_t((fastPos - k) & 511)];
    return acc;
}

// ---------------------------------------------------------------------------------------------
// Grit
void Grit::prepare(double sampleRate) {
    sr = std::isfinite(sampleRate) && sampleRate > 1000 ? sampleRate : 48000;
    amount.setTime(0.08, sr);
    era.setTime(0.25, sr);
    for (auto& o : os) o.prepare(4);
    for (auto& o : osEco) o.prepare(2);
    for (auto& d : direct) d.prepare(Oversampler::latencySamples + 8);
    for (auto& d : dc) d.prepare(sr);
    engageStep = static_cast<float>(1.0 / (0.02 * sr));
    envAttack = dsp::timeCoefficient(0.005, sr);
    envRelease = dsp::timeCoefficient(0.3, sr);
    fastAttack = dsp::timeCoefficient(0.002, sr);
    fastRelease = dsp::timeCoefficient(0.06, sr);
    crushRelease = dsp::timeCoefficient(0.6, sr);
    for (auto& f : weightHigh) f.set(60.0, sr, 1.414f);
    for (auto& f : weightShelf) f.highShelf(sr, 1700.0, 4.0);
    powerCoefficient = dsp::timeCoefficient(0.4, sr);
    fastCoefficient = dsp::timeCoefficient(0.05, sr);
    glide = dsp::timeCoefficient(0.15, sr);
    noise.seed(0x47524954u);
    for (auto& b : grainHigh) b.set(1000.0, sr, 0.9f);
    for (auto& b : grainLow) b.set(4000.0, sr, 1.848f);
    for (auto& b : grainLow2) b.set(4000.0, sr, 0.765f);
    for (auto& b : bandHigh) b.set(1300.0, sr, 0.9f);
    for (auto& b : bandLow) b.set(6500.0, sr, 0.9f);
    for (auto& b : subCut) b.set(45.0, sr, 1.414f);
    for (auto& b : body) b.set(2200.0, sr, 1.414f);
    reset();
    fresh = true;
}

void Grit::reset() noexcept {
    amount.snap(target);
    era.snap(eraTarget);
    engage = target > 0.0f ? 1.0f : 0.0f;
    for (auto& o : os) o.clear();
    for (auto& o : osEco) o.clear();
    for (auto& d : direct) d.clear();
    for (auto& d : dc) d.clear();
    for (auto* bank : { &lowShelf, &highShelf }) for (auto& b : *bank) { b.clear(); b.setNormalised(1, 0, 0, 1, 0, 0); }
    for (auto* bank : { &band, &band2, &grainHigh, &grainLow, &bandHigh, &bandLow, &subCut, &body, &grainLow2 }) for (auto& b : *bank) b.clear();
    bandEnv = 0;
    for (auto& f : weightHigh) f.clear();
    for (auto& f : weightShelf) f.clear();
    env = fastEnv = crushScale = 0;
    ctl = {};
    refPower = outPower = refFast = 0;
    gain = gainTarget = 1;
    counter = filterCounter = 0;
}

void Grit::setControls(float a, int e, bool lowCpu) noexcept {
    target = std::isfinite(a) ? std::clamp(a, 0.0f, 1.0f) : 0.0f;
    eraTarget = e != 0 ? 1.0f : 0.0f;
    amount.set(target);
    era.set(eraTarget);
    // The first settings after prepare() apply at once: nothing glides from the defaults the stage
    // was prepared with.
    if (fresh) { amount.snap(target); era.snap(eraTarget); engage = target > 0.0f ? 1.0f : 0.0f; fresh = false; }
    if (!active()) era.snap(eraTarget);
    if (lowCpu != eco) {
        eco = lowCpu;
        for (auto& o : os) o.clear();
        for (auto& o : osEco) o.clear();
    }
}

void Grit::process(float* l, float* r, int n) noexcept {
    float* io[2] { l, r };
    auto& over = eco ? osEco : os;
    const int factor = over[0].factor();
    for (int i = 0; i < n; ++i) {
        const float a = amount.next(), e = era.next();
        const float xl = l[i], xr = r[i];
        const float p = 0.5f * (xl * xl + xr * xr);
        env += (p > env ? envAttack : envRelease) * (p - env);
        const float level = std::max(std::abs(xl), std::abs(xr));
        crushScale += (level > crushScale ? fastAttack : crushRelease) * (level - crushScale);
        // The program's own presence band (what the grain is measured against).
        {
            const float vl = bandLow[0].low(bandHigh[0].highPass(xl)), vr = bandLow[1].low(bandHigh[1].highPass(xr));
            const float bp = 0.5f * (vl * vl + vr * vr);
            bandEnv += (bp > bandEnv ? fastAttack : envRelease) * (bp - bandEnv);
        }
        if (--filterCounter <= 0) {
            filterCounter = 16;
            // The dark tilt after the saturation, the crush and the grain: fuller lows, a softer top,
            // a 24 dB/oct band limit (nothing of the crush's or the grain's noise left up there).
            if (std::abs(a - ctl.a) > 1.0e-4f || std::abs(e - ctl.e) > 1.0e-4f) {
            ctl.a = a;
            ctl.e = e;
            for (auto& b : lowShelf) b.peaking(sr, 240.0, 0.6, a * mix(3.5f, 2.5f, e));
            for (auto& b : highShelf) b.highShelf(sr, 2500.0, -a * mix(7.0f, 5.0f, e));
            const double corner = 20000.0 * std::pow(mix(7000.0f, 10000.0f, e) / 20000.0, static_cast<double>(a));
            for (auto& b : band) b.set(corner, sr, 1.848f);
            for (auto& b : band2) b.set(corner, sr, 0.765f);
            }
            // The saturation (its drive follows the music's level: the same colour loud or soft).
            ctl.norm = std::clamp(0.3f / std::sqrt(std::max(env, 1.0e-10f)), 0.5f, 4.0f);
            ctl.satMix = std::min(1.0f, 3.0f * a);
            ctl.k = dsp::dbToGain(a * mix(12.0f, 9.5f, e));
            ctl.bias = a * mix(0.16f, 0.11f, e);
            ctl.t0 = std::tanh(ctl.k * ctl.bias);
            ctl.inv = 1.0f / (ctl.k * (1.0f - ctl.t0 * ctl.t0));
            // The crush: 12 bits at the most (13 in Modern).
            const float bits = 24.0f - std::sqrt(a) * (24.0f - mix(12.0f, 13.0f, e));
            ctl.stepScale = bits < 23.5f ? std::exp2(1.0f - bits) : 0.0f;
            ctl.grain = texture ? a * a * mix(0.45f, 0.32f, e) * std::sqrt(bandEnv) : 0.0f;
        }
        direct[0].push(xl);
        direct[1].push(xr);
        // The crush's step, scaled to the music's envelope as an old sampler normalised its samples
        // (the grain stays whatever the level; silence stays silent).
        const float step = ctl.stepScale > 0.0f ? std::max(crushScale, 1.0e-4f) * ctl.stepScale : 0.0f;
        const float norm = ctl.norm, back = 1.0f / norm;
        float y[2];
        for (int c = 0; c < 2; ++c) {
            const auto ch = std::size_t(c);
            float hi[4];
            // The body (below about 2.2 kHz) is saturated; the top passes clean (its harmonics would
            // land above 6 kHz: a fizz).
            over[ch].up(body[ch].low(c == 0 ? xl : xr), hi);
            for (int j = 0; j < factor; ++j) {
                const float u = hi[j] * norm;
                hi[j] = ctl.satMix * (tube(u, ctl.k, ctl.bias, ctl.t0, ctl.inv) - u) * back;   // what the saturation adds
            }
            // Added to the signal (delayed to the oversamplers), its products below 45 Hz (the
            // asymmetry's thumps) taken out.
            float v = direct[ch].readInteger(Oversampler::latencySamples + 1) + subCut[ch].highPass(dc[ch].process(over[ch].down(hi)));
            if (step > 0.0f) v = std::round(v / step) * step;
            // The grain: tape modulation noise riding the music's presence band (1-4 kHz, dark).
            if (ctl.grain > 0.0f) v += ctl.grain * grainLow2[ch].low(grainLow[ch].low(grainHigh[ch].highPass(noise.bipolar())));
            v = highShelf[ch].process(lowShelf[ch].process(v));
            v = band2[ch].low(band[ch].low(v));
            y[c] = v;
        }
        // The level match: the output at its input's loudness (a loudness meter's weighting).
        const float dl = direct[0].readInteger(Oversampler::latencySamples + 1), dr = direct[1].readInteger(Oversampler::latencySamples + 1);
        const float wl = weightShelf[0].process(weightHigh[0].highPass(dl)), wr = weightShelf[1].process(weightHigh[1].highPass(dr));
        const float pl = weightShelf[2].process(weightHigh[2].highPass(y[0])), pr = weightShelf[3].process(weightHigh[3].highPass(y[1]));
        const float ref = 0.5f * (wl * wl + wr * wr), out = 0.5f * (pl * pl + pr * pr);
        refPower += powerCoefficient * (ref - refPower);
        refFast += fastCoefficient * (ref - refFast);
        outPower += powerCoefficient * (out - outPower);
        if (--counter <= 0) {
            counter = 16;
            if (refPower > 1.0e-9f && refFast > 0.3f * refPower && outPower > 1.0e-12f) gainTarget = std::clamp(std::sqrt(refPower / outPower), 0.5f, 2.0f);
        }
        gain += glide * (gainTarget - gain);
        const float g = 1.0f + (gain - 1.0f) * std::min(1.0f, 4.0f * a);
        // Starting and stopping: the chain (delayed by its oversamplers) crossfades with the input,
        // once its delay lines hold the music again (after a rest they start empty).
        const bool on = target > 0.0f || a > 1.0e-3f;
        if (on && engage <= 0.0f && prime < 2 * Oversampler::latencySamples + 2) ++prime;
        else engage = on ? std::min(1.0f, engage + engageStep) : std::max(0.0f, engage - engageStep);
        for (int c = 0; c < 2; ++c) {
            const float x = c == 0 ? xl : xr;
            io[c][i] = engage >= 1.0f ? y[c] * g : x + engage * (y[c] * g - x);
        }
    }
    if (engage <= 0.0f && target <= 0.0f) {
        amount.snap(0.0f);
        reset();
        prime = 0;
    }
}

// ---------------------------------------------------------------------------------------------
// WarmChain
void WarmChain::prepare(double sampleRate) {
    sr = std::isfinite(sampleRate) && sampleRate > 1000 ? sampleRate : 48000;
    amount.setTime(0.15, sr);
    era.setTime(0.25, sr);
    pad.setTime(0.3, sr);
    send.setTime(0.05, sr);
    grit.setTime(0.08, sr);
    tapeBase = static_cast<int>(std::lround(0.0025 * sr));
    latency = Oversampler::latencySamples + tapeBase;
    for (auto& d : ensembleLine) d.prepare(static_cast<std::size_t>(0.025 * sr) + 8);
    for (auto& d : tapeLine) d.prepare(static_cast<std::size_t>(0.008 * sr) + 8);
    for (auto& o : os) o.prepare(2);
    for (auto& d : dc) d.prepare(sr);
    for (auto& d : tapeAlign) d.prepare(Oversampler::latencySamples + 8);
    for (auto& f : tapeSub) f.set(45.0, sr, 1.414f);
    for (int i = 0; i < lines; ++i) {
        hallLength[std::size_t(i)] = static_cast<float>(hallMs[i] * 0.001 * sr);
        hall[std::size_t(i)].prepare(static_cast<std::size_t>(hallLength[std::size_t(i)] + 0.002 * sr) + 16);
    }
    for (int k = 0; k < 4; ++k) {
        diffuserLength[std::size_t(k)] = std::max(1.0f, std::round(static_cast<float>(diffuserMs[k] * 0.001 * sr)));
        diffuser[std::size_t(k)].prepare(static_cast<std::size_t>(diffuserLength[std::size_t(k)]) + 8);
    }
    for (auto& d : preDelay) d.prepare(static_cast<std::size_t>(0.05 * sr) + 8);
    engageStep = static_cast<float>(1.0 / (0.02 * sr));
    monoStep = static_cast<float>(1.0 / (0.03 * sr));
    detectorCoefficient = dsp::timeCoefficient(0.01, sr);
    tapeAttack = dsp::timeCoefficient(0.005, sr);
    tapeRelease = dsp::timeCoefficient(0.4, sr);
    // (the compressor's and the leveller's dB glide every 16 samples)
    attack = dsp::timeCoefficient(0.03, sr / 16.0);
    release = dsp::timeCoefficient(0.3, sr / 16.0);
    longCoefficient = dsp::timeCoefficient(4.0, sr);
    longRise = dsp::timeCoefficient(1.0, sr);
    levelCoefficient = dsp::timeCoefficient(0.25, sr);
    liftRise = dsp::timeCoefficient(0.3, sr / 16.0);
    liftFall = dsp::timeCoefficient(0.2, sr / 16.0);
    envAttack = dsp::timeCoefficient(0.03, sr);
    envRelease = dsp::timeCoefficient(0.5, sr);
    airGlide = dsp::timeCoefficient(0.1, sr);
    // The hiss's gate and duck: the dry signal's level (30 ms / 300 ms), its onsets (a 2 ms / 40 ms
    // follower against a 500 ms one), the gate opening over 50 ms and shutting over 300 ms (after
    // 50 ms of hold on the FX's input), the duck falling over 0.35 s and recovering over 80 ms.
    dryFastAttack = dsp::timeCoefficient(0.002, sr);
    dryFastRelease = dsp::timeCoefficient(0.04, sr);
    drySlowCoefficient = dsp::timeCoefficient(0.5, sr);
    gateOpenStep = static_cast<float>(1.0 / (0.05 * sr));
    gateCloseStep = static_cast<float>(1.0 / (0.3 * sr));
    dryHoldSamples = static_cast<int>(0.05 * sr);
    duckDown = dsp::timeCoefficient(0.35, sr);
    duckUp = dsp::timeCoefficient(0.08, sr);
    presenceRise = dsp::timeCoefficient(0.05, sr);
    presenceFall = dsp::timeCoefficient(0.4, sr);
    widthCoefficient = dsp::timeCoefficient(1.5, sr);
    widthGlide = dsp::timeCoefficient(0.7, sr);
    ceilingRelease = dsp::timeCoefficient(0.08, sr);
    const auto drift = [this](Drift& d, double periodSeconds, double glideSeconds) {
        d.period = std::max(1, static_cast<int>(periodSeconds * sr));
        d.coefficient = dsp::timeCoefficient(glideSeconds, sr);
    };
    drift(cutoffDrift, 2.3, 1.2);
    drift(wowDrift, 1.7, 0.9);
    drift(breathDrift, 2.9, 1.1);
    drift(ensembleDrift, 3.7, 1.8);
    for (auto& f : excHigh) f.set(1500.0, sr, 1.414f);
    for (auto& f : excLow) f.set(5000.0, sr, 1.414f);
    for (auto& f : excOut1) f.set(2500.0, sr, 1.848f);
    for (auto& f : excOut2) f.set(2500.0, sr, 0.765f);
    for (auto& f : airHigh) f.set(7500.0, sr, 1.848f);
    for (auto& f : airHigh2) f.set(7500.0, sr, 0.765f);
    for (auto& f : airShelf) f.highShelf(sr, 11000.0, 3.0);
    for (auto& f : progHigh) f.set(8000.0, sr, 1.414f);
    for (auto& f : progHigh2) f.set(8000.0, sr, 1.414f);
    sideHigh.set(160.0, sr, 1.848f);
    sideHigh2.set(160.0, sr, 0.765f);
    sideHigh3.set(160.0, sr, 1.848f);
    sideHigh4.set(160.0, sr, 0.765f);
    // The air's own power above 8 kHz for a unit gain (measured on its own filters, so the level it
    // is given lands where it is asked to).
    {
        random.seed(0x41495231u);
        std::array<float, 3> b {};
        dsp::Svf h1 = airHigh[0], h2 = airHigh2[0], m1 = progHigh[0], m2 = progHigh2[0];
        dsp::Biquad sh = airShelf[0];
        double hf = 0;
        const int count = static_cast<int>(sr);
        for (int i = 0; i < count + 2000; ++i) {
            const float w = random.bipolar();
            b[0] = 0.99765f * b[0] + w * 0.0990460f;
            b[1] = 0.96300f * b[1] + w * 0.2965164f;
            b[2] = 0.57000f * b[2] + w * 1.0526913f;
            const float pk = (b[0] + b[1] + b[2] + w * 0.1848f) * 0.12f;
            const float s = sh.process(h2.highPass(h1.highPass(pk)));
            const float m = m2.highPass(m1.highPass(s));
            if (i >= 2000) hf += double(m) * m;
        }
        airUnit = static_cast<float>(std::max(1.0e-12, hf / count));
    }
    random.seed(0x5741524Du);
    airNoise.seed(0x48495353u);
    reset();
    fresh = true;
}

void WarmChain::reset() noexcept {
    amount.snap(target);
    era.snap(eraTarget);
    pad.snap(padTarget);
    send.snap(sendTarget);
    grit.snap(gritTarget);
    engage = target > 0.0f || monoTarget ? 1.0f : 0.0f;
    mono = monoTarget ? 1.0f : 0.0f;
    for (auto* bank : { &excHigh, &excLow, &excOut1, &excOut2 }) for (auto& f : *bank) f.clear();
    for (auto* bank : { &lowWarm, &mudCut, &topShelf, &airShelf }) for (auto& b : *bank) b.clear();
    for (auto* bank : { &lowWarm, &mudCut, &topShelf }) for (auto& b : *bank) b.setNormalised(1, 0, 0, 1, 0, 0);
    topState = { 0, 0 };
    topCoefficient = 1;
    for (auto& d : ensembleLine) d.clear();
    for (auto& f : ensembleHigh) f.clear();
    for (auto& d : tapeLine) d.clear();
    for (auto& o : os) o.clear();
    for (auto& d : dc) d.clear();
    for (auto& d : tapeAlign) d.clear();
    for (auto& f : tapeSub) f.clear();
    detector = grDb = longTerm = tapeEnv = slowLevel = liftDb = 0;
    for (auto& h : hall) h.clear();
    hallState = {};
    for (auto& d : diffuser) d.clear();
    for (auto& d : preDelay) d.clear();
    for (auto* bank : { &hallHigh, &hallLow, &airHigh, &airHigh2, &progHigh, &progHigh2 }) for (auto& f : *bank) f.clear();
    pink = {};
    progPower = progHf = airLevel = 0;
    dryPower = dryFast = drySlow = 0;
    airGate = 0;
    airDuck = 1;
    presence = 0;
    shelfDb = 1.0e9f;
    dryHold = 0;
    heldSeconds = 0;
    noteStarted = false;
    sideHigh.clear();
    sideHigh2.clear();
    sideHigh3.clear();
    sideHigh4.clear();
    midPower = sidePower = 0;
    sideGain = powerGain = 1;
    ceilingGain = 1;
    filterCounter = 0;
    ctl = {};
    lastA = lastE = -1.0f;
}

void WarmChain::setControls(float a, int e, bool m, float p, float hallSend, bool lowCpu) noexcept {
    target = std::isfinite(a) ? std::clamp(a, 0.0f, 1.0f) : 0.0f;
    eraTarget = e != 0 ? 1.0f : 0.0f;
    monoTarget = m;
    padTarget = std::isfinite(p) ? std::clamp(p, 0.0f, 1.0f) : 0.6f;
    sendTarget = std::isfinite(hallSend) ? std::clamp(hallSend, 0.0f, 2.0f) : 0.0f;
    eco = lowCpu;
    amount.set(target);
    era.set(eraTarget);
    pad.set(padTarget);
    send.set(sendTarget);
    if (fresh) {
        amount.snap(target);
        era.snap(eraTarget);
        pad.snap(padTarget);
        send.snap(sendTarget);
        engage = target > 0.0f || monoTarget ? 1.0f : 0.0f;
        mono = monoTarget ? 1.0f : 0.0f;
        fresh = false;
    }
    if (!active() && !monoTarget) { era.snap(eraTarget); pad.snap(padTarget); send.snap(sendTarget); }
}

void WarmChain::setPlace(float decaySeconds, float cutoffHz) noexcept {
    const float decay = std::isfinite(decaySeconds) ? std::clamp(decaySeconds, 0.3f, 30.0f) : 3.0f;
    const float cutoff = std::isfinite(cutoffHz) ? std::clamp(cutoffHz, 40.0f, 20000.0f) : 12000.0f;
    placeLength = std::clamp(std::sqrt(decay / 3.0f), 0.9f, 1.25f);
    placeTone = std::clamp(std::sqrt(cutoff / 8000.0f), 0.5f, 1.2f);
}

void WarmChain::updateFilters(float a, float e) noexcept {
    // The top's corner drifts: every time. The rest only when the amount, the Era or the place moved.
    {
        const double corner = 20000.0 * std::pow(mix(10500.0f, 14500.0f, e) / 20000.0, static_cast<double>(a)) * (1.0 + 0.12 * a * cutoffDrift.value);
        topCoefficient = dsp::poleForHz(std::min(corner, 0.45 * sr), sr);
    }
    if (std::abs(a - lastA) < 1.0e-4f && std::abs(e - lastE) < 1.0e-4f && placeLength == lastLength && placeTone == lastTone && eco == lastEco) return;
    lastA = a;
    lastE = e;
    lastLength = placeLength;
    lastTone = placeTone;
    lastEco = eco;
    for (auto& b : lowWarm) b.peaking(sr, 58.0, 0.7, a * 3.0);
    for (auto& b : mudCut) b.peaking(sr, 320.0, 0.75, -a * 3.0);
    // The top (build 3): a 6 kHz shelf that follows the playing (set in process(), every 16 samples):
    // lifted while notes sound (Classic +1.5 dB, Modern +2 dB), the 6-12 kHz air from the music and
    // its harmonics, not from noise; easing to a gentle cut in the tails (-5 / -4.5 dB), so the
    // hall's ring-out stays dark. Build 2: a fixed cut of 3 / 1.5 dB at 6.5 kHz.
    if (legacyAir()) for (auto& b : topShelf) b.highShelf(sr, 6500.0, -a * mix(3.0f, 1.5f, e));
    else shelfDb = 1.0e9f;   // (re-set below)
    // (The top's 6 dB/oct corner above: Classic about 10.5 kHz, Modern 14.5 kHz, drifting by +/-12 %.)
    for (auto& f : ensembleHigh) f.set(150.0, sr, 1.414f);
    // The hall: Classic a shorter, narrower room; Modern long and wide (no shimmer).
    const float t60 = mix(5.0f, 8.0f, e) * placeLength;
    for (int i = 0; i < lines; ++i)
        hallGain[std::size_t(i)] = std::pow(10.0f, -3.0f * hallLength[std::size_t(i)] / (t60 * static_cast<float>(sr)));
    hallDamp = dsp::poleForHz(mix(5000.0f, 6500.0f, e) * placeTone, sr);
    for (auto& f : hallHigh) f.set(170.0, sr, 1.414f);
    for (auto& f : hallLow) f.set(mix(5500.0f, 7000.0f, e) * placeTone, sr, 1.0f);
    hallMod = eco ? 0.0f : static_cast<float>(mix(0.25f, 0.9f, e) * 0.001 * sr);
    hallPre = static_cast<float>(mix(0.018f, 0.032f, e) * sr);
    hallWidth = mix(0.8f, 1.0f, e);
}

void WarmChain::process(float* l, float* r, int n, const float* dryL, const float* dryR) noexcept {
    const int voices = eco ? 2 : 3;
    const float tau = 2.0f * static_cast<float>(dsp::pi);
    const bool legacy = legacyAir();
    bool started = noteStarted;
    noteStarted = false;
    const float secondsPerSample = static_cast<float>(1.0 / sr);
    for (int i = 0; i < n; ++i) {
        const float a = amount.next(), e = era.next(), p = pad.next(), s = send.next();
        const float cd = cutoffDrift.next(random), wd = wowDrift.next(random), bd = breathDrift.next(random), ed = ensembleDrift.next(random);
        (void) cd;
        if (--filterCounter <= 0) {
            filterCounter = 16;
            updateFilters(a, e);
            if (!legacy) {
                const float db = a * mix(mix(-5.0f, -4.5f, e), mix(1.5f, 2.0f, e), presence);
                if (std::abs(db - shelfDb) > 0.02f) {
                    shelfDb = db;
                    for (auto& b : topShelf) b.highShelf(sr, 6000.0, db);
                }
            }
            // What changes slowly, every 16 samples.
            const float wowRate = mix(0.6f, 0.38f, e), flutterRate = mix(6.3f, 5.1f, e);
            ctl.wowStep = wowRate / static_cast<float>(sr);
            ctl.flutterStep = flutterRate / static_cast<float>(sr);
            ctl.wowDepth = a * centsToFraction(mix(8.0f, 4.0f, e)) / (tau * wowRate) * static_cast<float>(sr);
            ctl.flutterDepth = a * centsToFraction(mix(0.7f, 0.35f, e)) / (tau * flutterRate) * static_cast<float>(sr);
            ctl.ensembleDepth = mix(0.9f, 1.3f, e) * 0.001f * static_cast<float>(sr);
            ctl.tapeNorm = std::clamp(0.25f / std::sqrt(std::max(tapeEnv, 1.0e-10f)), 0.5f, 4.0f);
            ctl.tapeK = 1.0f + a * mix(0.9f, 0.6f, e);
            ctl.tapeBias = a * 0.12f;
            ctl.tapeT0 = std::tanh(ctl.tapeK * ctl.tapeBias);
            ctl.tapeInv = 1.0f / (ctl.tapeK * (1.0f - ctl.tapeT0 * ctl.tapeT0));
            ctl.share = std::pow(10.0f, 0.1f * (legacy ? mix(-29.0f, -33.0f, e) : mix(-46.0f, -52.0f, e)));
            // The compressor and the leveller (in dB), stepped every 16 samples, the gain glides between.
            const float level = 10.0f * std::log10(std::max(detector, 1.0e-12f));
            const float threshold = std::max(-50.0f, 10.0f * std::log10(std::max(longTerm, 1.0e-12f)) - 8.0f);
            const float over = level - threshold, knee = 6.0f;
            const float shaped = over <= -0.5f * knee ? 0.0f : over >= 0.5f * knee ? over : (over + 0.5f * knee) * (over + 0.5f * knee) / (2.0f * knee);
            const float wanted = shaped * (1.0f - 1.0f / 2.5f);
            grDb += (wanted > grDb ? attack : release) * (wanted - grDb);
            // The leveller (the haze between the notes): under the reference the quiet parts and tails
            // are lifted, 0.3 of how far they fall, up to 3 dB, letting go again 28-40 dB under it
            // (deep tails and silence die away as they did).
            const float below = 10.0f * std::log10(std::max(longTerm, 1.0e-12f) / std::max(slowLevel, 1.0e-12f));
            const float lift = longTerm > 1.0e-9f ? (below <= 0.0f ? 0.0f : below <= 10.0f ? 0.3f * below : below <= 28.0f ? 3.0f : std::max(0.0f, 3.0f * (40.0f - below) / 12.0f)) : 0.0f;
            liftDb += (lift > liftDb ? liftRise : liftFall) * (lift - liftDb);
            ctl.gainFrom = ctl.gainTo;
            ctl.gainTo = dsp::dbToGain(a * (4.5f - grDb + liftDb));
            // The air's level and the image's gains. Build 3: the hiss follows the DRY signal's level
            // (before the hall and the leveller), at the Era's share scaled by AIR (15 % = the share).
            if (legacy) {
                const float need = progPower > 1.0e-9f ? std::max(0.0f, ctl.share * progPower - progHf) : 0.0f;
                ctl.air = a * std::sqrt(need / airUnit);
            } else {
                ctl.air = airTarget > 0.0f && dryPower > 1.0e-12f ? a * (airTarget / 0.15f) * std::sqrt(airCalibration * ctl.share * dryPower / airUnit) : 0.0f;
            }
            const float rho = mix(0.64f, 0.47f, e), kk = (1.0f - rho) / (1.0f + rho);
            ctl.side = sidePower > 1.0e-14f && midPower > 1.0e-12f ? std::clamp(std::sqrt(kk * midPower / sidePower), 0.5f, 3.5f) : 1.0f;
            const float g = 1.0f + a * (sideGain - 1.0f);
            const float before = midPower + sidePower, after = midPower + g * g * sidePower;
            ctl.power = before > 1.0e-12f && after > 1.0e-12f ? std::clamp(std::sqrt(before / after), 0.8f, 1.4f) : 1.0f;
            ctl.at = 0;
        }
        ctl.at += 1.0f / 16.0f;
        const float xl = l[i], xr = r[i];
        float yl = xl, yr = xr;
        // The hiss's gate and duck, from the notes and the dry signal (never the output).
        if (!legacy) {
            const float dl = dryL != nullptr ? dryL[i] : xl, dr = dryR != nullptr ? dryR[i] : xr;
            const float dp = 0.5f * (dl * dl + dr * dr);
            dryPower += (dp > dryPower ? envAttack : envRelease) * (dp - dryPower);
            dryFast += (dp > dryFast ? dryFastAttack : dryFastRelease) * (dp - dryFast);
            drySlow += drySlowCoefficient * (dp - drySlow);
            bool open, onset;
            if (notesKnown) {
                open = notesHeld;
                onset = started;
                started = false;
            } else {
                // The FX: open while the input sounds (above -66 dBFS within the last 50 ms).
                if (std::max(std::abs(dl), std::abs(dr)) > 5.0e-4f) dryHold = dryHoldSamples;
                else if (dryHold > 0) --dryHold;
                open = dryHold > 0;
                onset = dryFast > 4.0f * drySlow && dryFast > 1.0e-6f;
            }
            airGate = open ? std::min(1.0f, airGate + gateOpenStep) : std::max(0.0f, airGate - gateCloseStep);
            heldSeconds = !open || onset ? 0.0 : heldSeconds + secondsPerSample;
            const float duckTarget = heldSeconds > 2.0 ? 0.3f : 1.0f;   // -10.5 dB under a note held over 2 s
            airDuck += (duckTarget < airDuck ? duckDown : duckUp) * (duckTarget - airDuck);
            presence += (airGate > presence ? presenceRise : presenceFall) * (airGate - presence);
        }
        // 2. Ensemble: three voices, each side its own read heads (decorrelated), above 150 Hz.
        ensembleLine[0].push(yl);
        ensembleLine[1].push(yr);
        const float ensembleMix = a * mix(0.45f, 0.6f, e) * (0.4f + 0.6f * p);
        if (ensembleMix > 0.0f) {
            const float depth = ctl.ensembleDepth;
            float wl = 0, wr = 0;
            for (int k = 0; k < voices; ++k) {
                auto& ph = ensemblePhase[std::size_t(k)];
                ph += ensembleRates[k] * (1.0 + 0.25 * ed) / sr;
                if (ph >= 1.0) ph -= 1.0;
                const float cycles = static_cast<float>(ph);
                const float dl = ensembleBaseL[k] * 0.001f * static_cast<float>(sr) + depth * fastSinCycles(cycles);
                const float dr = ensembleBaseR[k] * 0.001f * static_cast<float>(sr) + depth * fastSinCycles(cycles + ensembleOffset[k] / tau);
                wl += ensembleWeightL[k] * readCubic(ensembleLine[0], dl);
                wr += ensembleWeightR[k] * readCubic(ensembleLine[1], dr);
            }
            if (eco) { wl *= 1.25f; wr *= 1.25f; }
            yl = yl * (1.0f - 0.25f * ensembleMix) + ensembleMix * ensembleHigh[0].highPass(wl);
            yr = yr * (1.0f - 0.25f * ensembleMix) + ensembleMix * ensembleHigh[1].highPass(wr);
        }
        // 3. Tape wow (slow, drifting) and flutter: one transport for both sides.
        {
            wowPhase += ctl.wowStep;
            if (wowPhase >= 1.0) wowPhase -= 1.0;
            flutterPhase += ctl.flutterStep;
            if (flutterPhase >= 1.0) flutterPhase -= 1.0;
            const float delay = static_cast<float>(tapeBase + 1) + ctl.wowDepth * (fastSinCycles(static_cast<float>(wowPhase)) + 0.4f * wd)
                              + ctl.flutterDepth * fastSinCycles(static_cast<float>(flutterPhase));
            tapeLine[0].push(yl);
            tapeLine[1].push(yr);
            if (a > 0.0f) {
                yl = readCubic(tapeLine[0], delay);
                yr = readCubic(tapeLine[1], delay);
            } else {
                yl = tapeLine[0].readInteger(static_cast<std::size_t>(tapeBase + 1));
                yr = tapeLine[1].readInteger(static_cast<std::size_t>(tapeBase + 1));
            }
        }
        // 4. The sloped top: a shelf and a drifting 6 dB/oct corner.
        yl = topShelf[0].process(yl);
        yr = topShelf[1].process(yr);
        topState[0] += topCoefficient * (yl - topState[0]);
        topState[1] += topCoefficient * (yr - topState[1]);
        const float topMix = std::min(1.0f, 4.0f * a);
        yl += topMix * (topState[0] - yl);
        yr += topMix * (topState[1] - yr);
        // 5. Breathing on pads: a slow random +/-0.5 dB.
        {
            const float g = 1.0f + a * p * mix(0.06f, 0.05f, e) * bd;
            yl *= g;
            yr *= g;
        }
        // 5b. The Grit's harmonics (hiss fix): the music's own 1.5-5 kHz band, full-wave rectified
        // (its even harmonics and their intermodulation), above 2.5 kHz, added with the Grit.
        {
            const float gv = grit.next();
            if (gv > 1.0e-4f && !legacy) {
                const float k = a * gv * gritExciter;
                for (int c = 0; c < 2; ++c) {
                    const auto ch = std::size_t(c);
                    float& y = c == 0 ? yl : yr;
                    const float band = excLow[ch].low(excHigh[ch].highPass(y));
                    y += k * excOut2[ch].highPass(excOut1[ch].highPass(std::abs(band)));
                }
            }
        }
        // 6. Tape saturation, 2x oversampled, gentle and a little asymmetric.
        {
            // Its drive follows the music's level (a hot instrument is not squashed more than a quiet one).
            const float pw = 0.5f * (yl * yl + yr * yr);
            tapeEnv += (pw > tapeEnv ? tapeAttack : tapeRelease) * (pw - tapeEnv);
            const float norm = ctl.tapeNorm, back = 1.0f / norm;
            const float k = ctl.tapeK, bias = ctl.tapeBias, t0 = ctl.tapeT0, inv = ctl.tapeInv, satMix = std::min(1.0f, 3.0f * a);
            tapeAlign[0].push(yl);
            tapeAlign[1].push(yr);
            for (int c = 0; c < 2; ++c) {
                float hi[4];
                os[std::size_t(c)].up(c == 0 ? yl : yr, hi);
                for (int j = 0; j < 2; ++j) { const float u = hi[j] * norm; hi[j] = satMix * (tube(u, k, bias, t0, inv) - u) * back; }   // what it adds
                // Added to the signal (delayed to the oversampler), its products below 45 Hz (the
                // asymmetry's thumps) taken out.
                const float v = tapeAlign[std::size_t(c)].readInteger(Oversampler::latencySamples + 1)
                              + tapeSub[std::size_t(c)].highPass(dc[std::size_t(c)].process(os[std::size_t(c)].down(hi)));
                (c == 0 ? yl : yr) = v;
            }
        }
        // 7. The bus compressor (the glue): 2.5:1, 30 ms / 300 ms, a 6 dB knee, its threshold 8 dB
        // under the music's recent loud level (4-6 dB of gain reduction on dense parts), +4.5 dB of
        // make-up; then the leveller (their levels in dB above, every 16 samples).
        {
            const float power = 0.5f * (yl * yl + yr * yr);
            detector += detectorCoefficient * (power - detector);
            slowLevel += levelCoefficient * (power - slowLevel);
            // The reference: the 250 ms level's held peak. It rises with the music at once and falls
            // over four seconds while it plays; in the gaps and tails it holds.
            if (slowLevel > longTerm) longTerm = slowLevel;
            else if (detector > 0.1f * longTerm) longTerm += longCoefficient * (slowLevel - longTerm);
            const float g = ctl.gainFrom + (ctl.gainTo - ctl.gainFrom) * std::min(1.0f, ctl.at);
            yl *= g;
            yr *= g;
        }
        // 8. The hall: decorrelated stereo in and out, after the glue.
        {
            const float in = a * s * mix(0.42f, 0.5f, e);
            preDelay[0].push(yl * in);
            preDelay[1].push(yr * in);
            float hl = hallLow[0].low(hallHigh[0].highPass(preDelay[0].read(1.0f + hallPre)));
            float hr = hallLow[1].low(hallHigh[1].highPass(preDelay[1].read(1.0f + hallPre * 1.13f)));
            for (int k = 0; k < 2; ++k) {
                for (int side = 0; side < 2; ++side) {
                    auto& line = diffuser[std::size_t(2 * side + k)];
                    float& x = side == 0 ? hl : hr;
                    const float z = line.readInteger(static_cast<std::size_t>(diffuserLength[std::size_t(2 * side + k)]));
                    const float v = x + 0.6f * z;
                    x = z - 0.6f * v;
                    line.push(v);
                }
            }
            float o[lines];
            for (int k = 0; k < lines; ++k) {
                const auto q = std::size_t(k);
                float rd;
                if (hallMod > 0.0f) {
                    hallPhase[q] += hallRates[k] / sr;
                    if (hallPhase[q] >= 1.0) hallPhase[q] -= 1.0;
                    rd = hall[q].read(hallLength[q] - hallMod * (1.0f + fastSinCycles(static_cast<float>(hallPhase[q]))));
                } else rd = hall[q].readInteger(static_cast<std::size_t>(hallLength[q]));
                hallState[q] += hallDamp * (rd - hallState[q]);
                o[k] = hallState[q] * hallGain[q];
            }
            const float outL = 0.5f * (o[0] + o[3] - o[5] + o[6]), outR = 0.5f * (o[1] + o[2] - o[4] + o[7]);
            hadamard8(o);
            for (int k = 0; k < lines; ++k) hall[std::size_t(k)].push(o[k] + (k % 2 == 0 ? hl : hr) * ((k & 2) ? -1.0f : 1.0f));
            const float m = 0.5f * (outL + outR), sd = 0.5f * (outL - outR) * hallWidth;
            yl += m + sd;
            yr += m - sd;
        }
        // 1. Tone, on everything the hall has added: 40-80 Hz warmth, the broad 200-500 Hz cut.
        yl = mudCut[0].process(lowWarm[0].process(yl));
        yr = mudCut[1].process(lowWarm[1].process(yr));
        // (Build 2's air bed, kept for the before renders only: envelope-following pink hiss on the
        // output, filling 8-20 kHz up to -29 / -33 dB of the whole. Its level tracking also feeds
        // the width servo's silence test below.)
        {
            const float pw = 0.5f * (yl * yl + yr * yr);
            progPower += (pw > progPower ? envAttack : envRelease) * (pw - progPower);
            const float hl = progHigh2[0].highPass(progHigh[0].highPass(yl)), hr = progHigh2[1].highPass(progHigh[1].highPass(yr));
            const float ph = 0.5f * (hl * hl + hr * hr);
            progHf += (ph > progHf ? envAttack : envRelease) * (ph - progHf);
            airLevel += airGlide * (ctl.air - airLevel);
            if (legacy && airLevel > 1.0e-7f) {
                for (int c = 0; c < 2; ++c) {
                    auto& b = pink[std::size_t(c)];
                    const float w = random.bipolar();
                    b[0] = 0.99765f * b[0] + w * 0.0990460f;
                    b[1] = 0.96300f * b[1] + w * 0.2965164f;
                    b[2] = 0.57000f * b[2] + w * 1.0526913f;
                    const float pk = (b[0] + b[1] + b[2] + w * 0.1848f) * 0.12f;
                    const float hiss = airShelf[std::size_t(c)].process(airHigh2[std::size_t(c)].highPass(airHigh[std::size_t(c)].highPass(pk)));
                    (c == 0 ? yl : yr) += hiss * airLevel;
                }
            }
        }
        // 10-12. The image: the width servo (the Era's correlation), the bass below 150 Hz mono
        // (a fourth-order high-pass on the side), the power the narrowing takes given back, the
        // deliberate Mono switch.
        {
            const float mid = 0.5f * (yl + yr);
            float side = 0.5f * (yl - yr);
            side += a * (sideHigh2.highPass(sideHigh.highPass(side)) - side);
            // (the servo's gain below is applied after another fourth-order high-pass: the bass it
            // would lift stays out)
            if (progPower > 1.0e-8f) {
                midPower += widthCoefficient * (mid * mid - midPower);
                sidePower += widthCoefficient * (side * side - sidePower);
            }
            sideGain += widthGlide * (ctl.side - sideGain);
            const float g = 1.0f + a * (sideGain - 1.0f);
            powerGain += widthGlide * (ctl.power - powerGain);
            side *= g;
            side += a * (sideHigh4.highPass(sideHigh3.highPass(side)) - side);
            mono = monoTarget ? std::min(1.0f, mono + monoStep) : std::max(0.0f, mono - monoStep);
            side *= 1.0f - mono;
            const float pg = 1.0f + a * (powerGain - 1.0f);
            yl = (mid + side) * pg;
            yr = (mid - side) * pg;
        }
        // 13. The ceiling: -1.3 dBFS (sample peaks), so the true peaks stay under -0.5 dBTP.
        {
            const float ceiling = mix(0.966f, 0.861f, a);
            const float peak = std::max(std::abs(yl), std::abs(yr));
            const float needed = peak > ceiling ? ceiling / peak : 1.0f;
            ceilingGain = needed < ceilingGain ? needed : std::min(needed, ceilingGain + ceilingRelease * (1.0f - ceilingGain));
            yl *= ceilingGain;
            yr *= ceilingGain;
        }
        // 9. The hiss (build 3): pink, shaped to the top, its own generator, after the image (so
        // it never moves the width servo) and the ceiling (so it never moves the ceiling's gain:
        // at most -50 dBFS, it leaves the true peak where it was), folded by the Mono switch.
        // Gated: exactly 0 shut.
        if (!legacy && airGate > 0.0f && airTarget > 0.0f && airLevel > 1.0e-9f) {
            const float g = airLevel * airGate * airDuck;
            float hiss[2];
            for (int c = 0; c < 2; ++c) {
                auto& b = pink[std::size_t(c)];
                const float w = airNoise.bipolar();
                b[0] = 0.99765f * b[0] + w * 0.0990460f;
                b[1] = 0.96300f * b[1] + w * 0.2965164f;
                b[2] = 0.57000f * b[2] + w * 1.0526913f;
                const float pk = (b[0] + b[1] + b[2] + w * 0.1848f) * 0.12f;
                hiss[c] = g * airShelf[std::size_t(c)].process(airHigh2[std::size_t(c)].highPass(airHigh[std::size_t(c)].highPass(pk)));
            }
            const float hm = 0.5f * (hiss[0] + hiss[1]), hs = 0.5f * (hiss[0] - hiss[1]) * (1.0f - mono);
            yl += hm + hs;
            yr += hm - hs;
        }
        // Starting and stopping: crossfaded with the input (the chain runs `latency` samples behind).
        const bool on = target > 0.0f || a > 1.0e-3f || monoTarget || mono > 0.0f;
        if (on && engage <= 0.0f && prime < 2 * latency + 2) ++prime;
        else engage = on ? std::min(1.0f, engage + engageStep) : std::max(0.0f, engage - engageStep);
        if (engage >= 1.0f) { l[i] = yl; r[i] = yr; }
        else { l[i] = xl + engage * (yl - xl); r[i] = xr + engage * (yr - xr); }
    }
    if (engage <= 0.0f && target <= 0.0f && !monoTarget) {
        amount.snap(0.0f);
        reset();
        prime = 0;
    }
}
}
