#include "DungeonSynthModern.h"

namespace dc {
namespace {
constexpr float hallLengthsMs[8] { 31.7f, 37.3f, 41.9f, 47.3f, 53.9f, 59.3f, 67.1f, 73.7f };
constexpr float hallModRates[8] { 0.31f, 0.43f, 0.37f, 0.53f, 0.47f, 0.61f, 0.71f, 0.97f };
constexpr float diffuserMs[4] { 4.7f, 3.6f, 12.7f, 9.3f };
constexpr float bloomMs[2] { 23.3f, 41.1f };
constexpr float inSigns[8] { 1, -1, 1, -1, -1, 1, -1, 1 };
inline float mix(float a, float b, float t) noexcept { return a + (b - a) * t; }
inline double logMix(double a, double b, double t) noexcept { return a * std::pow(b / a, t); }
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
double besselI0(double x) {
    double sum = 1, term = 1;
    for (int k = 1; k < 50; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
    return sum;
}
// A smooth tanh (C1 at +/-3, where it reaches +/-1): the tape's curve.
inline float softSat(float x) noexcept {
    x = std::clamp(x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
// The tape's reference level, as a tape's VU reference: the music's RMS (rising over 20 ms,
// falling over 200 ms) is normalised to -18 dBFS RMS before the drive (so "11 dB of drive" puts
// its peaks into the curve as on a real machine). A peak follower guards the onsets: the level
// used is never under its peak power - 10 dB (after silence a note's attack is not overdriven
// while the RMS rises).
constexpr float tapeReference = 0.125f, crestAllowance = 0.1f;
// The hall's output at hallWet 1 (measured as wet as Classic's: --ds-modern).
constexpr float hallOutAtWet1 = 1.17f;
}

float DungeonSynthModern::shape(float a) noexcept {
    return std::pow(std::clamp(a, 0.0f, 1.0f), modern::rollerPower);
}

// ---- The sampler -------------------------------------------------------------------------------
void DungeonSynthModern::Sampler::prepare(double sampleRate, double lowestRate) {
    fs = sampleRate;
    // The kernel: cutoff 0.445 of its rate, Kaiser beta 6.76 (about 70 dB), 2 x half of its samples.
    constexpr double fc = 0.445, beta = 6.76;
    const int size = 2 * half * perUnit + 2;
    kernel.assign(std::size_t(size), 0.0f);
    std::vector<double> v(std::size_t(size), 0.0);
    for (int i = 0; i <= 2 * half * perUnit; ++i) {
        const double x = double(i) / perUnit - half, u = x / half;
        const double sinc = std::abs(x) < 1.0e-12 ? 2.0 * fc : std::sin(2.0 * dsp::pi * fc * x) / (dsp::pi * x);
        v[std::size_t(i)] = sinc * besselI0(beta * std::sqrt(std::max(0.0, 1.0 - u * u))) / besselI0(beta);
    }
    // Its samples at whole offsets sum to 1 (DC gain 1 both ways).
    double sum = 0;
    for (int k = -half; k <= half; ++k) sum += v[std::size_t((k + half) * perUnit)];
    for (int i = 0; i < size; ++i) kernel[std::size_t(i)] = static_cast<float>(v[std::size_t(i)] / sum);
    // Its samples are taken lagDown behind the newest input (the kernel's reach at the lowest rate);
    // the output is drawn `lag` behind (both reaches): fixed, whatever the rate.
    const int reach = static_cast<int>(std::ceil(half * fs / lowestRate));
    lagDown = reach + 1;
    lag = lagDown + reach + 2;
    std::size_t n = 16;
    while (n < std::size_t(lag + reach + 8)) n <<= 1;
    inL.assign(n, 0.0f);
    inR.assign(n, 0.0f);
    inMask = n - 1;
    n = 16;
    while (n < std::size_t(4 * half + 16)) n <<= 1;   // its samples within reach of the output, and the newest beyond
    lowL.assign(n, 0.0f);
    lowR.assign(n, 0.0f);
    lowT.assign(n, -1.0e18);
    lowMask = n - 1;
    clear();
}

void DungeonSynthModern::Sampler::clear() noexcept {
    std::fill(inL.begin(), inL.end(), 0.0f);
    std::fill(inR.begin(), inR.end(), 0.0f);
    std::fill(lowL.begin(), lowL.end(), 0.0f);
    std::fill(lowR.begin(), lowR.end(), 0.0f);
    std::fill(lowT.begin(), lowT.end(), -1.0e18);
    count = lowCount = 0;
    nextT = 0;
}

void DungeonSynthModern::Sampler::process(float xl, float xr, double rate, float step, float& yl, float& yr) noexcept {
    inL[std::size_t(count) & inMask] = xl;
    inR[std::size_t(count) & inMask] = xr;
    const long long newest = count++;
    const double ratio = rate / fs, reach = half / ratio;   // its samples a host sample; the kernel's reach (host samples)
    // Its samples whose instants lie lagDown behind the newest input (the kernel reaches them).
    while (nextT <= double(newest - lagDown)) {
        const auto m0 = static_cast<long long>(std::ceil(nextT - reach)), m1 = static_cast<long long>(std::floor(nextT + reach));
        float al = 0, ar = 0;
        for (long long m = m0; m <= m1; ++m) {
            const float w = h((nextT - double(m)) * ratio);
            al += w * inL[std::size_t(m) & inMask];
            ar += w * inR[std::size_t(m) & inMask];
        }
        al *= static_cast<float>(ratio);
        ar *= static_cast<float>(ratio);
        if (step > 0.0f) {
            al = std::round(al / step) * step;
            ar = std::round(ar / step) * step;
        }
        const auto k = std::size_t(lowCount++) & lowMask;
        lowL[k] = al;
        lowR[k] = ar;
        lowT[k] = nextT;
        nextT += 1.0 / ratio;
    }
    // Drawn back at the host's rate, `lag` behind the newest input.
    const double t = double(newest - lag);
    float ol = 0, orr = 0;
    for (long long k = lowCount - 1; k >= 0 && k > lowCount - 1 - static_cast<long long>(lowMask); --k) {
        const auto i = std::size_t(k) & lowMask;
        const double d = t - lowT[i];
        if (d <= -reach) continue;   // beyond the output's instant (not yet within reach)
        if (d >= reach) break;
        const float w = h(d * ratio);
        ol += w * lowL[i];
        orr += w * lowR[i];
    }
    yl = ol;
    yr = orr;
}

// ---- Tape ----------------------------------------------------------------------------------------
void DungeonSynthModern::Tape::prepare() {
    // Low-pass at 0.46 of the host's Nyquist on the x4 grid (22 kHz at 48 kHz), Kaiser beta 7.
    constexpr double fc = 0.115, beta = 7.0;
    double sum = 0;
    std::array<double, taps> v {};
    for (int n = 0; n < taps; ++n) {
        const double x = n - 0.5 * (taps - 1), u = x / (0.5 * (taps - 1));
        const double sinc = x == 0.0 ? 2.0 * fc : std::sin(2.0 * dsp::pi * fc * x) / (dsp::pi * x);
        v[std::size_t(n)] = sinc * besselI0(beta * std::sqrt(std::max(0.0, 1.0 - u * u))) / besselI0(beta);
        sum += v[std::size_t(n)];
    }
    h.fill(0.0f);
    for (int n = 0; n < taps; ++n) h[std::size_t(n)] = static_cast<float>(v[std::size_t(n)] / sum);
    clear();
}

void DungeonSynthModern::Tape::clear() noexcept {
    for (auto& c : in) c.fill(0.0f);
    for (auto& c : over) c.fill(0.0f);
    pos = opos = 0;
}

void DungeonSynthModern::Tape::process(float& l, float& r, float norm, float drive, float even) noexcept {
    pos = (pos + 1) & 31;
    in[0][std::size_t(pos)] = l;
    in[1][std::size_t(pos)] = r;
    const float inv = 1.0f / (drive * norm);
    for (int ch = 0; ch < 2; ++ch) {
        const auto& x = in[std::size_t(ch)];
        auto& o = over[std::size_t(ch)];
        for (int p = 0; p < factor; ++p) {
            float u = 0;
            for (int m = 0, i = p; i < taps; ++m, i += factor) u += h[std::size_t(i)] * x[std::size_t((pos - m) & 31)];
            // The soft curve, then the even bend on its (bounded) output: t + c t^2 stays monotonic
            // for c < 0.5, and its 2nd harmonic leads at every level (warm, never a buzz).
            const float t = softSat(drive * u * static_cast<float>(factor) * norm);
            o[std::size_t((opos + p) & 127)] = (t + even * t * t) * inv;
        }
    }
    // Down: the low-pass at the first of these four (20 host samples behind, exactly).
    const int at = opos;
    opos = (opos + factor) & 127;
    for (int ch = 0; ch < 2; ++ch) {
        const auto& o = over[std::size_t(ch)];
        float y = 0;
        for (int i = 0; i < taps; ++i) y += h[std::size_t(i)] * o[std::size_t((at - i) & 127)];
        (ch == 0 ? l : r) = y;
    }
}

// ---- The stage -------------------------------------------------------------------------------------
float DungeonSynthModern::readSinc(const dsp::DelayLine& line, float delay) const noexcept {
    delay = std::clamp(delay, 5.0f, static_cast<float>(line.data.size() - 8));
    const float position = static_cast<float>(line.write) - delay + static_cast<float>(line.data.size());
    const auto index = static_cast<std::size_t>(position);
    const float fraction = (position - static_cast<float>(index)) * static_cast<float>(sincPhases);
    const auto row = std::min(static_cast<std::size_t>(fraction), static_cast<std::size_t>(sincPhases - 1));
    const float t = fraction - static_cast<float>(row);
    const float* k0 = &sincTable[row * sincTaps];
    const float* k1 = k0 + sincTaps;
    float y = 0;
    for (int k = 0; k < sincTaps; ++k) y += (k0[k] + t * (k1[k] - k0[k])) * line.data[(index + std::size_t(k) - 3) & line.mask];
    return y;
}

void DungeonSynthModern::prepare(double sampleRate) {
    sr = std::isfinite(sampleRate) && sampleRate > 1000 ? sampleRate : 48000;
    amount.setTime(0.08, sr);
    chorusAmount.setTime(0.2, sr);
    send.setTime(0.05, sr);
    for (auto* p : { &placeLength, &placeTone, &placeSend }) p->setTime(0.3, sr);
    sampler.prepare(sr, std::min(modern::samplerRateHz, modern::samplerRateStartHz));
    tape.prepare();
    latencyTotal = sampler.latency() + Tape::latency;
    for (auto& d : alignLine) d.prepare(std::size_t(sampler.latency()) + 8);
    for (auto& d : satDry) d.prepare(std::size_t(Tape::latency) + 8);
    for (auto& d : dc) d.prepare(sr);
    chorusLine.prepare(static_cast<std::size_t>(0.03 * sr) + 16);
    for (auto& w : wowLine) w.prepare(static_cast<std::size_t>(0.006 * sr) + 16);
    for (int p = 0; p <= sincPhases; ++p) {
        const double f = double(p) / sincPhases;
        double sum = 0, w[sincTaps];
        for (int k = 0; k < sincTaps; ++k) {
            const double x = double(k - 3) - f, u = x / 4.0;
            const double sinc = std::abs(x) < 1.0e-9 ? 1.0 : std::sin(dsp::pi * 0.92 * x) / (dsp::pi * 0.92 * x);
            w[k] = sinc * besselI0(6.0 * std::sqrt(std::max(0.0, 1.0 - u * u))) / besselI0(6.0);
            sum += w[k];
        }
        for (int k = 0; k < sincTaps; ++k) sincTable[std::size_t(p * sincTaps + k)] = static_cast<float>(w[k] / sum);
    }
    for (int i = 0; i < lines; ++i) {
        hallLength[std::size_t(i)] = static_cast<float>(hallLengthsMs[i] * 0.001 * sr);
        hall[std::size_t(i)].prepare(static_cast<std::size_t>(hallLength[std::size_t(i)] + 0.0008 * sr) + 32);
    }
    for (int k = 0; k < diffusers; ++k) {
        diffuserLength[std::size_t(k)] = std::max(1.0f, std::round(static_cast<float>(diffuserMs[k] * 0.001 * sr)));
        diffuser[std::size_t(k)].prepare(static_cast<std::size_t>(diffuserLength[std::size_t(k)]) + 8);
    }
    for (int k = 0; k < blooms; ++k) {
        bloomLength[std::size_t(k)] = std::max(1.0f, std::round(static_cast<float>(bloomMs[k] * 0.001 * sr)));
        bloomLine[std::size_t(k)].prepare(static_cast<std::size_t>(bloomLength[std::size_t(k)]) + 8);
    }
    preDelay.prepare(static_cast<std::size_t>(0.05 * sr) + 8);
    sampAttack = dsp::timeCoefficient(0.002, sr);
    sampRelease = dsp::timeCoefficient(1.5, sr);
    satAttack = dsp::timeCoefficient(0.001, sr);
    satRelease = dsp::timeCoefficient(0.5, sr);
    satRms = dsp::timeCoefficient(0.02, sr);
    satRmsDown = dsp::timeCoefficient(0.2, sr);
    driftCoefficient = dsp::timeCoefficient(0.6, sr);
    slowCoefficient = dsp::timeCoefficient(0.2, sr);   // (Classic 0.3 s: this hall builds up more)
    fastCoefficient = dsp::timeCoefficient(0.05, sr);
    glideCoefficient = dsp::timeCoefficient(0.08, sr);
    holdAttack = dsp::timeCoefficient(0.01, sr);
    // (4 s: Classic's 1.5 s is shorter than this hall's decay and would pull its tails down.)
    holdRelease = dsp::timeCoefficient(4.0, sr);
    powerCoefficient = dsp::timeCoefficient(0.2, sr);
    ctlDown = dsp::timeCoefficient(0.05, sr);
    ctlUp = dsp::timeCoefficient(0.5, sr);
    ceilingDown = dsp::timeCoefficient(0.015, sr);
    ceilingUp = dsp::timeCoefficient(0.4, sr);
    widthCoefficient = dsp::timeCoefficient(0.1, sr);
    for (auto& f : weightHigh) f.set(60.0, sr, 1.847f);
    for (auto& f : weightHigh2) f.set(60.0, sr, 0.765f);
    for (auto& f : weightShelf) f.highShelf(sr, 1700.0, 4.0);
    for (auto& f : rumble) f.set(22.0, sr, 1.414f);
    noise.seed(0x4d4f4452u);
    reset();
}

void DungeonSynthModern::clearState() noexcept {
    sampler.clear();
    tape.clear();
    for (auto* bank : { &alignLine, &satDry, &wowLine }) for (auto& d : *bank) d.clear();
    for (auto& d : dc) d.clear();
    sampEnv = satEnv = satPeak = 0;
    for (auto* bank : { &lowPassA, &lowPassB, &rumble }) for (auto& f : *bank) f.clear();
    for (auto* bank : { &warmth, &tilt, &head }) for (auto& f : *bank) f.clear();
    chorusLine.clear();
    midPower = sidePower = 0;
    widthGain = 1;
    refSlow = refFast = outSlow = outFast = refHold = outHold = directFast = 0;
    gain = gainTarget = ceiling = ceilingTarget = 1;
    for (auto& f : weightHigh) f.clear();
    for (auto& f : weightHigh2) f.clear();
    for (auto& f : weightShelf) f.clear();
}

void DungeonSynthModern::reset() noexcept {
    amount.snap(targetAmount);
    chorusAmount.snap(targetChorus);
    send.snap(targetSend);
    for (auto* p : { &placeLength, &placeTone, &placeSend }) p->snap(p->target == 0.0f ? 1.0f : p->target);
    clearState();
    for (auto& h : hall) h.clear();
    for (auto& d : diffuser) d.clear();
    for (auto& b : bloomLine) b.clear();
    preDelay.clear();
    hallHigh.clear();
    hallLow.clear();
    for (auto& f : hallDark) f.clear();
    hallState = {};
    hallInHold = hallInPower = hallPower = 0;
    hallCtl = 1;
    hallRinging = false;
    quiet = 0;
    now = derive(amount.value, chorusAmount.value);
    updateFilters(now);
}

void DungeonSynthModern::setControls(float a, float chorus, float hallSend) noexcept {
    targetAmount = std::isfinite(a) ? std::clamp(a, 0.0f, 1.0f) : 0.0f;
    targetChorus = std::isfinite(chorus) ? std::clamp(chorus, 0.0f, 1.0f) : 0.6f;
    targetSend = std::isfinite(hallSend) ? std::clamp(hallSend, 0.0f, 2.0f) : 0.0f;
    amount.set(targetAmount);
    chorusAmount.set(targetChorus);
    send.set(targetSend);
    if (!shaping()) chorusAmount.snap(targetChorus);
}

void DungeonSynthModern::snapControls(float a, float chorus, float hallSend) noexcept {
    amount.snap(std::clamp(a, 0.0f, 1.0f));
    chorusAmount.snap(std::clamp(chorus, 0.0f, 1.0f));
    send.snap(std::clamp(hallSend, 0.0f, 2.0f));
    for (auto* p : { &placeLength, &placeTone, &placeSend }) p->snap(p->target == 0.0f ? 1.0f : p->target);
    now = derive(amount.value, chorusAmount.value);
    updateFilters(now);
}

void DungeonSynthModern::setPlace(float decaySeconds, float cutoffHz, float wet) noexcept {
    const float decay = std::isfinite(decaySeconds) ? std::clamp(decaySeconds, 0.3f, 30.0f) : 3.0f;
    const float cutoff = std::isfinite(cutoffHz) ? std::clamp(cutoffHz, 40.0f, 20000.0f) : 12000.0f;
    const float w = std::isfinite(wet) ? std::clamp(wet, 0.0f, 1.0f) : 0.0f;
    placeLength.set(std::clamp(std::sqrt(decay / 3.0f), 0.6f, 1.6f));
    placeTone.set(std::clamp(std::sqrt(cutoff / 8000.0f), 0.35f, 1.2f));
    placeSend.set(1.0f - 0.35f * w);
    if (!shaping()) { placeLength.snap(placeLength.target); placeTone.snap(placeTone.target); placeSend.snap(placeSend.target); }
}

// Everything the amount sets, through the one roller curve s = shape(a): each amount and depth is
// s x its value at 100 % (amplitudes; the hall's send s^1.5), the rate and the low-pass glide in
// log frequency, the bits in bits (= in dB of grain). At a = 0 every part is the identity.
DungeonSynthModern::Derived DungeonSynthModern::derive(float a, float chorus) const noexcept {
    Derived d;
    const float s = shape(a);
    d.s = s;
    d.rate = logMix(modern::samplerRateStartHz, modern::samplerRateHz, s);
    const float bits = mix(modern::samplerBitsStart, modern::samplerBits, s);
    d.bitStep = std::exp2(1.0f - bits);
    // Everything runs a fixed latency behind (the sampler's and the tape's filters are linear
    // phase); the direct path joins it within the first 5 % (a partial blend of the two is brief).
    d.align = std::min(1.0f, 20.0f * a);
    d.sampMix = std::min(1.0f, 10.0f * s);
    d.satMix = std::min(1.0f, 6.0f * s);
    d.drive = dsp::dbToGain(s * modern::tapeDriveDb);
    d.even = s * modern::tapeEven;
    d.lowPass = logMix(modern::lowPassStartHz, modern::lowPassHz, s);
    d.warmthDb = s * modern::warmthDb;
    d.tiltDb = s * modern::tiltDb;
    d.headDb = s * modern::headBumpDb;
    d.chorusMix = s * chorus;
    d.chorusDepth = static_cast<float>(s * modern::chorusDepthMs * 0.001 * sr);
    // Wow: the delay's swing for wowPercent of peak speed at 0.4 Hz.
    d.wowDepth = static_cast<float>(s * modern::wowPercent * 0.01 / (dsp::twoPi * 0.4) * sr);
    d.side = mix(1.0f, modern::sideGain, s);
    d.hallIn = std::pow(s, 1.5f);
    d.bloom = std::sqrt(s);
    return d;
}

void DungeonSynthModern::updateFilters(const Derived& d) noexcept {
    // The low-pass, fourth order in two halves (around the tape): Butterworth.
    for (auto& f : lowPassA) f.set(d.lowPass, sr, 1.8478f);
    for (auto& f : lowPassB) f.set(d.lowPass, sr, 0.7654f);
    warmth[0].peaking(sr, 200.0, 0.7, d.warmthDb);
    tilt[0].highShelf(sr, modern::tiltHz, d.tiltDb);
    // The low shelf: +G overall and a -G high shelf at its corner.
    head[0].highShelf(sr, 250.0, -d.headDb);
    {
        const float g = dsp::dbToGain(d.headDb);
        head[0].b0 *= g;
        head[0].b1 *= g;
        head[0].b2 *= g;
    }
    for (auto* bank : { &warmth, &tilt, &head }) {
        auto& b = (*bank)[1];
        const auto& m = (*bank)[0];
        b.b0 = m.b0; b.b1 = m.b1; b.b2 = m.b2; b.a1 = m.a1; b.a2 = m.a2;
    }
    // The hall: longer and darker towards 100 % (its place scales it).
    const float t60 = mix(1.5f, modern::hallSeconds, d.s) * placeLength.value;
    for (int i = 0; i < lines; ++i)
        hallGain[std::size_t(i)] = std::pow(10.0f, -3.0f * hallLength[std::size_t(i)] / (t60 * static_cast<float>(sr)));
    hallDampCoefficient = dsp::poleForHz(5000.0, sr);
    hallModDepth = static_cast<float>(12.0 * sr / 48000.0);
    hallLow.set(3000.0f * placeTone.value, sr, 1.0f);
    hallHigh.set(100.0, sr, 1.414f);
    hallPreDelay = static_cast<float>(0.03 * sr);
    for (auto& f : hallDark) f.set(3200.0f * placeTone.value, sr, 1.414f);
}

float DungeonSynthModern::runHall(float in, float& outR, const Derived& d) noexcept {
    preDelay.push(in);
    float x = preDelay.read(1.0f + hallPreDelay);
    x = hallLow.low(hallHigh.highPass(x));
    for (int k = 0; k < diffusers; ++k) {
        auto& line = diffuser[std::size_t(k)];
        const float z = line.readInteger(static_cast<std::size_t>(diffuserLength[std::size_t(k)]));
        const float v = x + 0.72f * z;
        x = z - 0.72f * v;
        line.push(v);
    }
    {
        float y = x;
        for (int k = 0; k < blooms; ++k) {
            auto& line = bloomLine[std::size_t(k)];
            const float z = line.readInteger(static_cast<std::size_t>(bloomLength[std::size_t(k)]));
            const float v = y + 0.62f * z;
            y = z - 0.62f * v;
            line.push(v);
        }
        x += d.bloom * (y - x);
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
    const float out = hallOutAtWet1 * modern::hallWet;
    outR = hallDark[3].low(hallDark[1].low(r)) * out;
    return hallDark[2].low(hallDark[0].low(l)) * out;
}

void DungeonSynthModern::process(float* l, float* r, int n) noexcept {
    float hallPeak = 0;
    int at = 0;
    while (at < n) {
        const int m = std::min(controlBlock, n - at);
        amount.skip(m);
        chorusAmount.skip(m);
        send.skip(m);
        placeLength.skip(m);
        placeTone.skip(m);
        placeSend.skip(m);
        bool justStopped = false;
        if (targetAmount <= 0.0f && amount.value < 1.0e-5f && amount.value != 0.0f) { amount.snap(0.0f); justStopped = true; }
        const float a = amount.value;
        const Derived next = derive(a, chorusAmount.value);
        updateFilters(next);
        const float sendNow = send.value;
        if (!shaping()) {
            if (justStopped) clearState();
            if (hallRinging)
                for (int i = at; i < at + m; ++i) {
                    float hr = 0;
                    const float hl = runHall(0.0f, hr, next);
                    if (hallOnly) { l[i] = hl * sendNow; r[i] = hr * sendNow; }
                    else { l[i] += hl * sendNow; r[i] += hr * sendNow; }
                    hallPeak = std::max(hallPeak, std::abs(hl) + std::abs(hr));
                }
            else if (hallOnly)
                for (int i = at; i < at + m; ++i) l[i] = r[i] = 0.0f;
            now = next;
            at += m;
            continue;
        }
        const float step = 1.0f / static_cast<float>(m);
        const float matchWeight = std::min(1.0f, 20.0f * a);   // fully in by 5 %, with the chain (align)
        const double rate = next.rate;
        for (int j = 0; j < m; ++j) {
            const int i = at + j;
            const float t = static_cast<float>(j + 1) * step;
            Derived d;
            d.s = mix(now.s, next.s, t);
            d.bitStep = next.bitStep;
            d.sampMix = mix(now.sampMix, next.sampMix, t);
            d.align = mix(now.align, next.align, t);
            d.satMix = mix(now.satMix, next.satMix, t);
            d.drive = mix(now.drive, next.drive, t);
            d.even = mix(now.even, next.even, t);
            d.chorusMix = mix(now.chorusMix, next.chorusMix, t);
            d.chorusDepth = mix(now.chorusDepth, next.chorusDepth, t);
            d.wowDepth = mix(now.wowDepth, next.wowDepth, t);
            d.side = mix(now.side, next.side, t);
            d.hallIn = mix(now.hallIn, next.hallIn, t);
            d.bloom = mix(now.bloom, next.bloom, t);
            const float xl = l[i], xr = r[i];
            // 1. The vintage sampler (its bits scaled to the music's envelope: the grain stays
            //    whatever the level, silence stays silent; released over 0.8 s, so a decay keeps
            //    a little grain), blended with the input delayed to its latency.
            const float level = std::max(std::abs(xl), std::abs(xr));
            sampEnv += (level > sampEnv ? sampAttack : sampRelease) * (level - sampEnv);
            float sl = 0, srr = 0;
            sampler.process(xl, xr, rate, d.bitStep * std::max(sampEnv, 1.0e-4f), sl, srr);
            alignLine[0].push(xl);
            alignLine[1].push(xr);
            const auto back = static_cast<std::size_t>(sampler.latency() + 1);
            float yl = alignLine[0].readInteger(back), yr = alignLine[1].readInteger(back);
            yl += d.sampMix * (sl - yl);
            yr += d.sampMix * (srr - yr);
            // 2. The low-pass's first half, then the tape: normalised to the tape's reference level
            //    (the same colour loud or soft), driven, the
            //    asymmetry adding even harmonics, at x4; blended with its dry (delayed to match).
            yl = lowPassA[0].low(yl);
            yr = lowPassA[1].low(yr);
            {
                const float p = 0.5f * (yl * yl + yr * yr);
                satEnv += (p > satEnv ? satRms : satRmsDown) * (p - satEnv);
                satPeak += (p > satPeak ? satAttack : satRelease) * (p - satPeak);
                const float level = std::max(satEnv, crestAllowance * satPeak);
                const float norm = std::clamp(tapeReference / std::sqrt(std::max(level, 1.0e-10f)), 0.25f, 40.0f);
                satDry[0].push(yl);
                satDry[1].push(yr);
                float tl = yl, tr = yr;
                tape.process(tl, tr, norm, d.drive, d.even);
                tl = dc[0].process(tl);
                tr = dc[1].process(tr);
                const auto tb = static_cast<std::size_t>(Tape::latency + 1);
                yl = satDry[0].readInteger(tb);
                yr = satDry[1].readInteger(tb);
                yl += d.satMix * (tl - yl);
                yr += d.satMix * (tr - yr);
            }
            // 3. The low-pass's second half (the tape's upper harmonics rolled off again), warmth,
            //    a gentle warm tilt, the rumble cut.
            yl = rumble[0].highPass(head[0].process(tilt[0].process(warmth[0].process(lowPassB[0].low(yl)))));
            yr = rumble[1].highPass(head[1].process(tilt[1].process(warmth[1].process(lowPassB[1].low(yr)))));
            // 4. The ensemble: three voices from the mid sweeping slowly (0.4 Hz, a third apart, a
            //    slower drift and a light vibrato over them).
            chorusLine.push(0.5f * (yl + yr));
            if (d.chorusMix > 0.0f) {
                slowPhase += 0.11 / sr;
                if (slowPhase >= 1.0) slowPhase -= 1.0;
                vibratoPhase += 5.3 / sr;
                if (vibratoPhase >= 1.0) vibratoPhase -= 1.0;
                const float slow = std::sin(dsp::twoPi * static_cast<float>(slowPhase));
                float v[3];
                for (int k = 0; k < 3; ++k) {
                    auto& ph = chorusPhase[std::size_t(k)];
                    ph += (0.4 + 0.04 * k) / sr;
                    if (ph >= 1.0) ph -= 1.0;
                    const float sweep = std::sin(dsp::twoPi * static_cast<float>(ph)) + 0.25f * slow;
                    const float delay = static_cast<float>(0.0085 * sr) + 0.5f * d.chorusDepth * sweep
                                      + static_cast<float>(0.00012 * sr) * std::sin(dsp::twoPi * static_cast<float>(vibratoPhase) + 2.1f * static_cast<float>(k));
                    v[k] = readSinc(chorusLine, delay);
                }
                // A small common voice, and the moving pair added to one side and taken from the
                // other: the two ears' combs are complementary (it moves and widens; the level
                // does not pump, as an in-phase blend of equal voices would, notching a chord).
                const float c = d.chorusMix, common = 0.35f * v[1], moving = modern::ensembleMoving * (v[0] - v[2]);
                yl = yl * (1.0f - 0.3f * c) + c * (common + moving);
                yr = yr * (1.0f - 0.3f * c) + c * (common - moving);
            }
            // 5. The hall: a send from here (after the tape: the reverb is not saturated), its own
            //    limiter keeping its energy at most 2 dB under the music's on long chords.
            {
                float hr = 0;
                const float hl = runHall(0.5f * (yl + yr) * d.hallIn * placeSend.value, hr, d);
                hallPeak = std::max(hallPeak, std::abs(hl) + std::abs(hr));
                const float pin = 0.5f * (xl * xl + xr * xr), ph = 0.5f * (hl * hl + hr * hr);
                hallInPower += powerCoefficient * (pin - hallInPower);
                hallInHold += (hallInPower > hallInHold ? holdAttack : holdRelease) * (hallInPower - hallInHold);
                hallPower += powerCoefficient * (ph - hallPower);
                const float limit = hallCeiling * hallInHold;
                const float wanted = hallPower > limit && hallPower > 1.0e-12f ? std::sqrt(limit / hallPower) : 1.0f;
                hallCtl += (wanted < hallCtl ? ctlDown : ctlUp) * (wanted - hallCtl);
                const float g = hallOnly ? sendNow : sendNow * hallCtl;
                if (hallOnly) yl = yr = 0.0f;
                // The fast ceiling hears the chain and half of the hall: at a new note the last
                // one's tail is not taken for an overshoot (hearing all of it, it ducked every onset
                // by 4 dB on a low reed; hearing none, the hall built up 2 dB within a phrase).
                const float dl = weightShelf[4].process(weightHigh2[4].highPass(weightHigh[4].highPass(yl)));
                const float dr = weightShelf[5].process(weightHigh2[5].highPass(weightHigh[5].highPass(yr)));
                directFast += fastCoefficient * (0.5f * (dl * dl + dr * dr) * (1.0f - 0.55f * d.s) * (1.0f - 0.55f * d.s) - directFast);
                yl += hl * g;
                yr += hr * g;
            }
            // 6. Slow wow (the tape moves for both sides), read through eight-point sinc.
            {
                wowPhase += 0.4 / sr;
                if (wowPhase >= 1.0) wowPhase -= 1.0;
                if (--driftCounter <= 0) {
                    driftCounter = static_cast<int>(0.9 * sr);
                    driftTarget = noise.bipolar();
                }
                drift += driftCoefficient * (driftTarget - drift);
                const float delay = 5.0f + d.wowDepth * (1.4f + std::sin(dsp::twoPi * static_cast<float>(wowPhase)) + 0.35f * drift);
                wowLine[0].push(yl);
                wowLine[1].push(yr);
                yl = readSinc(wowLine[0], delay);
                yr = readSinc(wowLine[1], delay);
            }
            // 7. The image, wider (the power it adds taken back evenly, slowly).
            {
                const float mid = 0.5f * (yl + yr), side = 0.5f * (yl - yr);
                midPower += powerCoefficient * (mid * mid - midPower);
                sidePower += powerCoefficient * (side * side - sidePower);
                const float before = midPower + sidePower, after = midPower + d.side * d.side * sidePower;
                const float wanted = before > 1.0e-12f && after > 1.0e-12f ? std::clamp(std::sqrt(before / after), 0.7f, 1.6f) : 1.0f;
                widthGain += widthCoefficient * (wanted - widthGain);
                yl = (mid + side * d.side) * widthGain;
                yr = (mid - side * d.side) * widthGain;
            }
            if (hallOnly) { l[i] = yl; r[i] = yr; continue; }
            // 8. The level matcher (as Classic's): the output at the input's loudness. A fixed make-up
            //    first (what the hall, the ensemble and the warmth add at 100 %, about 7 dB), so it
            //    starts near its gain (the first notes after a reset are not too loud).
            {
                const float makeup = 1.0f - 0.55f * d.s;
                yl *= makeup;
                yr *= makeup;
            }
            const float wl = weightShelf[0].process(weightHigh2[0].highPass(weightHigh[0].highPass(xl)));
            const float wr = weightShelf[1].process(weightHigh2[1].highPass(weightHigh[1].highPass(xr)));
            const float ref = 0.5f * (wl * wl + wr * wr);
            const float pl = weightShelf[2].process(weightHigh2[2].highPass(weightHigh[2].highPass(yl)));
            const float pr = weightShelf[3].process(weightHigh2[3].highPass(weightHigh[3].highPass(yr)));
            const float out = 0.5f * (pl * pl + pr * pr);
            refSlow += slowCoefficient * (ref - refSlow);
            refFast += fastCoefficient * (ref - refFast);
            outSlow += slowCoefficient * (out - outSlow);
            outFast += fastCoefficient * (out - outFast);
            refHold += (refSlow > refHold ? holdAttack : holdRelease) * (refSlow - refHold);
            outHold += (outSlow > outHold ? holdAttack : holdRelease) * (outSlow - outHold);
            const bool playing = refSlow > 1.0e-7f && refFast > 0.3f * refSlow && refSlow > 0.2f * refHold;
            if (--matchCounter <= 0) {
                matchCounter = 16;
                // (-12 .. +6 dB: at 100 % the hall at the music's level, the ensemble and the warmth
                // add more than Classic's 6 dB of range.)
                // Its loud moments against the input's (both held, 1.5 s): loudness is what the loud
                // moments are, and the hall and the tape even the level out between them.
                if (playing && outHold > 1.0e-12f) gainTarget = std::clamp(std::sqrt(refHold / outHold), 0.25f, 2.0f);
                const float heard = 0.5f * (directFast + outFast);   // the chain, and half of the hall
                if (playing && heard > 1.0e-12f) ceilingTarget = std::clamp(std::sqrt(1.122f * refFast / heard), 0.25f, 2.0f);
            }
            gain += glideCoefficient * (gainTarget - gain);
            ceiling += (ceilingTarget < ceiling ? ceilingDown : ceilingUp) * (ceilingTarget - ceiling);
            const float g = 1.0f + (std::min(gain, ceiling) - 1.0f) * matchWeight;
            // The direct path joins the delayed chain within the first 5 %.
            l[i] = xl + d.align * (yl * g - xl);
            r[i] = xr + d.align * (yr * g - xr);
        }
        now = next;
        at += m;
    }
    if (shaping() && now.hallIn > 0.0f) { hallRinging = true; quiet = 0; }
    else if (hallRinging) {
        if (hallPeak < 1.0e-6f) quiet += n;
        else quiet = 0;
        if (quiet > static_cast<int>(0.5 * sr)) {
            hallRinging = false;
            for (auto& h : hall) h.clear();
            for (auto& dl : diffuser) dl.clear();
            for (auto& b : bloomLine) b.clear();
            preDelay.clear();
            hallHigh.clear();
            hallLow.clear();
            for (auto& f : hallDark) f.clear();
            hallState = {};
        }
    }
}
}
