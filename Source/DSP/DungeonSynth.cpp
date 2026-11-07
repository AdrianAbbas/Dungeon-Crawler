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
// The hiss filter's own level (white noise, high-passed near 1 kHz, two poles near 4 kHz: a
// cassette's hiss as it comes out of a dark player): dividing by it makes hissGain its RMS.
constexpr float hissNorm = 1.0f / 0.24f;
// Classic's sampler at 100 %: about 13 kHz (the low end of the cheap samplers), 8.5 bits.
constexpr float classicRate = 13000.0f, classicBits = 8.5f;
// Session 18, Modern's sampler at 100 %: 12 bits (a warm vintage sampler's; its rate is fixed,
// two thirds of a 44.1 / 48 kHz host's: 29.4 / 32 kHz).
constexpr float modernBits = 12.0f;
// Modern's bloom: two long diffusers before the tank.
constexpr float bloomMs[2] { 23.3f, 41.1f };
}

// Session 18: Modern's sampler (see the header).
void DungeonSynth::Vintage::prepare(double sampleRate) {
    const int m = std::max(1, static_cast<int>(std::lround(sampleRate / 48000.0)));
    up = 2;
    down = 3 * (m >= 3 ? 4 : m);   // 44.1 / 48 kHz: 3, 88.2 / 96: 6, 176.4 / 192 and above: 12
    const double fast = sampleRate * up, rate = fast / down;
    const double pass = 0.39 * rate, stop = 0.53 * rate, cutoff = 0.5 * (pass + stop) / fast;
    // Kaiser: 70 dB (beta 6.76), its length from the transition.
    const double beta = 6.76, transition = (stop - pass) / fast;
    taps = static_cast<int>(std::ceil((70.0 - 7.95) / (14.36 * transition))) | 1;   // odd: the latency is whole host samples
    taps = std::min(taps, 1023);
    const auto i0 = [](double x) {
        double sum = 1, term = 1;
        for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
        return sum;
    };
    h.assign(std::size_t(taps), 0.0f);
    double total = 0;
    std::vector<double> v(static_cast<std::size_t>(taps), 0.0);
    for (int n = 0; n < taps; ++n) {
        const double x = n - 0.5 * (taps - 1), w = i0(beta * std::sqrt(std::max(0.0, 1.0 - std::pow(2.0 * x / (taps - 1), 2.0)))) / i0(beta);
        const double sinc = x == 0.0 ? 2.0 * cutoff : std::sin(2.0 * dsp::pi * cutoff * x) / (dsp::pi * x);
        v[std::size_t(n)] = sinc * w;
        total += v[std::size_t(n)];
    }
    // DC gain 1: each of its phases sums to about 1 / up (or 1 / down) of it, which the up-
    // sampler's (x up) and the return's (x down) gains in process() make up: the path is unity.
    for (int n = 0; n < taps; ++n) h[std::size_t(n)] = static_cast<float>(v[std::size_t(n)] / total);
    lat = (taps - 1) / up;   // both filters' halves, on the up-sampled grid, in host samples
    std::size_t size = 16;
    while (size < std::size_t(taps / up + 4)) size <<= 1;
    in.assign(size, 0.0f);
    inMask = size - 1;
    size = 16;
    while (size < std::size_t(taps / down + 4)) size <<= 1;
    low.assign(size, 0.0f);
    lowMask = size - 1;
    clear();
}

void DungeonSynth::Vintage::clear() noexcept {
    std::fill(in.begin(), in.end(), 0.0f);
    std::fill(low.begin(), low.end(), 0.0f);
    count = lowCount = 0;
}

// Session 18: an eight-point windowed-sinc read (delay >= 5 samples; see the header).
float DungeonSynth::readSinc(const dsp::DelayLine& line, float delay) const noexcept {
    delay = std::clamp(delay, 5.0f, static_cast<float>(line.data.size() - 8));
    const float position = static_cast<float>(line.write) - delay + static_cast<float>(line.data.size());
    const auto index = static_cast<std::size_t>(position);
    const float fraction = (position - static_cast<float>(index)) * static_cast<float>(sincPhases);
    const auto row = std::min(static_cast<std::size_t>(fraction), static_cast<std::size_t>(sincPhases - 1));
    const float t = fraction - static_cast<float>(row);
    const float* k0 = &sincTable[row * sincTaps];
    const float* k1 = k0 + sincTaps;
    float y = 0;
    for (int k = 0; k < sincTaps; ++k) {
        const float c = k0[k] + t * (k1[k] - k0[k]);
        y += c * line.data[(index + std::size_t(k) - 3) & line.mask];   // taps at index-3 .. index+4
    }
    return y;
}

void DungeonSynth::prepare(double sampleRate) {
    sr = std::isfinite(sampleRate) && sampleRate > 1000 ? sampleRate : 48000;
    amount.setTime(0.08, sr);
    era.setTime(0.25, sr);
    chorusAmount.setTime(0.2, sr);
    send.setTime(0.05, sr);
    for (auto* p : { &placeLength, &placeTone, &placeSend }) p->setTime(0.3, sr);
    chorusLine.prepare(static_cast<std::size_t>(0.03 * sr) + 8);
    for (auto& w : wowLine) w.prepare(static_cast<std::size_t>(0.012 * sr) + 8);
    for (auto& d : dc) d.prepare(sr);
    for (int i = 0; i < lines; ++i) {
        hallLength[std::size_t(i)] = static_cast<float>(hallLengthsMs[i] * 0.001 * sr);
        hall[std::size_t(i)].prepare(static_cast<std::size_t>(hallLength[std::size_t(i)] + 0.0006 * sr) + 32);
    }
    for (int k = 0; k < diffusers; ++k) {
        diffuserLength[std::size_t(k)] = std::max(1.0f, std::round(static_cast<float>(diffuserMs[k] * 0.001 * sr)));
        diffuser[std::size_t(k)].prepare(static_cast<std::size_t>(diffuserLength[std::size_t(k)]) + 8);
    }
    preDelay.prepare(static_cast<std::size_t>(0.06 * sr) + 8);
    for (int k = 0; k < blooms; ++k) {
        bloomLength[std::size_t(k)] = std::max(1.0f, std::round(static_cast<float>(bloomMs[k] * 0.001 * sr)));
        bloomLine[std::size_t(k)].prepare(static_cast<std::size_t>(bloomLength[std::size_t(k)]) + 8);
    }
    modernRelease = dsp::timeCoefficient(0.25, sr);
    for (auto& v : vintage) v.prepare(sr);
    // The windowed-sinc kernels: for a fraction f the taps sit at -3 .. +4 around it (Kaiser,
    // beta 6, cutoff 0.46 of the host's rate), each kernel's sum 1.
    for (int p = 0; p <= sincPhases; ++p) {
        const double f = double(p) / sincPhases;
        double sum = 0, w[sincTaps];
        for (int k = 0; k < sincTaps; ++k) {
            const double x = double(k - 3) - f, u = x / 4.0;
            const double sinc = std::abs(x) < 1.0e-9 ? 1.0 : std::sin(dsp::pi * 0.92 * x) / (dsp::pi * 0.92 * x);
            double i0a = 1, ta = 1, i0b = 1, tb = 1;
            const double arg = 6.0 * std::sqrt(std::max(0.0, 1.0 - u * u));
            for (int m = 1; m < 30; ++m) { ta *= (arg / (2.0 * m)) * (arg / (2.0 * m)); i0a += ta; tb *= (6.0 / (2.0 * m)) * (6.0 / (2.0 * m)); i0b += tb; }
            w[k] = sinc * i0a / i0b;
            sum += w[k];
        }
        for (int k = 0; k < sincTaps; ++k) sincTable[std::size_t(p * sincTaps + k)] = static_cast<float>(w[k] / sum);
    }
    for (auto& f : satSplit) f.set(2500.0, sr, 1.414f);
    for (auto& f : satDc) f.prepare(sr);
    for (auto& al : alignLine) al.prepare(static_cast<std::size_t>(vintage[0].latency()) + 8);
    crushAttack = dsp::timeCoefficient(0.002, sr);
    crushRelease = dsp::timeCoefficient(1.5, sr);
    satCoefficient = dsp::timeCoefficient(0.5, sr);
    satAttack = dsp::timeCoefficient(0.005, sr);
    gateAttack = dsp::timeCoefficient(0.05, sr);
    trebleCoefficient = dsp::poleForHz(1000.0, sr);
    gateRelease = dsp::timeCoefficient(0.4, sr);
    driftCoefficient = dsp::timeCoefficient(0.5, sr);
    slowCoefficient = dsp::timeCoefficient(0.3, sr);
    fastCoefficient = dsp::timeCoefficient(0.05, sr);
    glideCoefficient = dsp::timeCoefficient(0.12, sr);
    holdAttack = dsp::timeCoefficient(0.01, sr);
    holdRelease = dsp::timeCoefficient(1.5, sr);
    powerCoefficient = dsp::timeCoefficient(0.2, sr);
    ctlDown = dsp::timeCoefficient(0.05, sr);
    ctlUp = dsp::timeCoefficient(0.5, sr);
    ceilingDown = dsp::timeCoefficient(0.015, sr);
    ceilingUp = dsp::timeCoefficient(0.4, sr);
    widthCoefficient = dsp::timeCoefficient(0.1, sr);
    for (auto& f : weightHigh) f.set(60.0, sr, 1.414f);
    for (auto& f : weightShelf) f.highShelf(sr, 1700.0, 4.0);
    noise.seed(0x44535945u);
    grainStep = static_cast<float>(std::min(1.0, 11025.0 / sr));
    grainBits = std::exp2(1.0f - 12.0f);
    reset();
}

void DungeonSynth::reset() noexcept {
    amount.snap(targetAmount);
    era.snap(targetEra);
    chorusAmount.snap(targetChorus);
    send.snap(targetSend);
    for (auto* p : { &placeLength, &placeTone, &placeSend }) p->snap(p->target == 0.0f ? 1.0f : p->target);
    for (auto& f : antiAlias) f.clear();
    for (auto& f : reconstruct) f.clear();
    held = previous = lastIn = { 0, 0 };
    holdPhase = 0;
    crushEnv = 0;
    for (auto& v : vintage) v.clear();
    for (auto& al : alignLine) al.clear();
    modernEnv = 0;
    for (auto& f : satSplit) f.clear();
    for (auto& f : satDc) f.clear();
    for (auto& b : bloomLine) b.clear();
    chorusLine.clear();
    satEnv = 0;
    for (auto& w : wowLine) w.clear();
    for (auto& d : dc) d.clear();
    hissLow = hissLow2 = hissHigh = { 0, 0 };
    gate = trebleLow = hissApplied = 0;
    for (auto* bank : { &lowCut, &lowCut2, &top, &top2, &top3 }) for (auto& f : *bank) f.clear();
    for (auto* bank : { &warmth, &honk, &shelf }) for (auto& f : *bank) { f.clear(); f.setNormalised(1, 0, 0, 1, 0, 0); }
    for (auto& h : hall) h.clear();
    for (auto& d : diffuser) d.clear();
    preDelay.clear();
    hallHigh.clear();
    hallLow.clear();
    for (auto& f : hallDark) f.clear();
    hallState = {};
    grainHeld = { 0, 0 };
    grainPhase = 1;
    hallInHold = hallInPower = hallPower = 0;
    midPower = sidePower = 0;
    widthGain = 1;
    hallCtl = 1;
    hallRinging = false;
    quiet = 0;
    refSlow = refFast = outSlow = outFast = refHold = 0;
    gain = gainTarget = ceiling = ceilingTarget = 1;
    for (auto& f : weightHigh) f.clear();
    for (auto& f : weightShelf) f.clear();
    now = derive(amount.value, era.value, chorusAmount.value);
    updateFilters(now, amount.value, era.value);
}

void DungeonSynth::setControls(float a, int e, float chorus, float hallSend) noexcept {
    targetAmount = std::isfinite(a) ? std::clamp(a, 0.0f, 1.0f) : 0.0f;
    targetEra = e == Modern ? 1.0f : 0.0f;
    targetChorus = std::isfinite(chorus) ? std::clamp(chorus, 0.0f, 1.0f) : 0.6f;
    targetSend = std::isfinite(hallSend) ? std::clamp(hallSend, 0.0f, 2.0f) : 0.0f;
    amount.set(targetAmount);
    era.set(targetEra);
    chorusAmount.set(targetChorus);
    send.set(targetSend);
    // Off and still: the Era and the chorus weight follow at once (nothing to glide).
    if (!shaping()) { era.snap(targetEra); chorusAmount.snap(targetChorus); }
}

void DungeonSynth::setPlace(float decaySeconds, float cutoffHz, float wet) noexcept {
    const float decay = std::isfinite(decaySeconds) ? std::clamp(decaySeconds, 0.3f, 30.0f) : 3.0f;
    const float cutoff = std::isfinite(cutoffHz) ? std::clamp(cutoffHz, 40.0f, 20000.0f) : 12000.0f;
    const float w = std::isfinite(wet) ? std::clamp(wet, 0.0f, 1.0f) : 0.0f;
    lastPlace = { decay, cutoff, w };
    placeLength.set(std::clamp(std::sqrt(decay / 3.0f), 0.6f, 1.6f));     // a long reverb: a longer hall; a small room: shorter
    placeTone.set(std::clamp(std::sqrt(cutoff / 8000.0f), 0.35f, 1.2f));   // a dark place: a darker hall (the Crypt: a third of the tone)
    placeSend.set(1.0f - 0.35f * w);                                       // an already wet place: a little less of it
    if (!shaping()) { placeLength.snap(placeLength.target); placeTone.snap(placeTone.target); placeSend.snap(placeSend.target); }
}

// Everything the amount (a), the Era (e: 0 Classic .. 1 Modern) and the instrument's chorus
// weight set. Every amount, mix and depth is 0 at a = 0 (the identity). The curves are
// shaped so that 25 % is subtle, 50 % clearly heard and 100 % the full genre sound.
DungeonSynth::Derived DungeonSynth::derive(float a, float e, float chorus) const noexcept {
    Derived d;
    const float c = 1.0f - e;
    // The sampler (Classic only). It takes over early (by 17 %: a partial blend with the
    // direct path would comb-filter the top), its rate and bits then fall with the amount:
    // 25 % about 29 kHz and 16 bits, 50 % about 21 kHz and 13 bits, 100 % 13 kHz and 8.5 bits.
    d.lofiMix = std::min(1.0f, 6.0f * a) * c;
    const float classicStep = static_cast<float>(std::pow(std::min(1.0, classicRate / sr), std::pow(static_cast<double>(a), 0.7)));
    d.rateStep = mix(classicStep, 1.0f, e);
    d.aliasOpen = a * a;   // towards 100 % the anti-alias filter opens: the aliasing is heard
    const float bits = 24.0f - std::sqrt(a) * (24.0f - classicBits) * c;
    d.bitStep = bits < 23.5f ? std::exp2(1.0f - bits) : 0.0f;
    // Session 18: Modern's sampler, the warm vintage one (its rate fixed: Vintage), blended in
    // with the amount (a fifth at 25 %, half at 50 %, all of it from 78 %), its bits falling gently: 25 % 18 bits, 50 % 15.5,
    // 100 % 12. Only where Modern weighs in (Classic exact).
    if (e > 0.0f) {
        d.modern = e;
        d.align = e * std::min(1.0f, 20.0f * a);
        d.lofiMix = mix(d.lofiMix, std::min(1.0f, a * (0.5f + a)), e);   // 25 % 0.19, 50 % 0.5, all of it from 78 %
        const float b = 24.0f - std::sqrt(a) * (24.0f - modernBits);
        d.modernBitStep = b < 23.5f ? std::exp2(1.0f - b) : 0.0f;
    }
    // The keyboard's chorus (Classic, light) or the vintage ensemble (Modern, deep and wide).
    d.chorusMix = a * chorus * mix(0.3f, 1.0f, e);
    d.chorusDepth = mix(0.0012f, 0.0042f, e);
    // Tape saturation: Classic a cheap keyboard's mild grit, Modern warm and gentle (Session 18:
    // 14 dB of drive made every chord a distorted blur).
    d.satMix = std::min(1.0f, 3.0f * a);
    d.satDrive = dsp::dbToGain(a * mix(4.0f, 5.0f, e));
    d.satAsym = a * mix(0.05f, 0.14f, e);
    // Wow (the delay's swing, in samples: 0.45 % pitch at 0.45 Hz Classic, 0.12 % Modern) and flutter.
    d.wowDepth = static_cast<float>(a * mix(1.6e-3f, 0.55e-3f, e) * sr);
    d.flutterDepth = static_cast<float>(a * mix(30.0e-6f, 3.0e-6f, e) * sr);
    // Hiss (against the music's treble, above about 1 kHz): about 25 dB under it at 100 % Classic
    // as measured after the roll-off (-45 dBFS on a test chord with -23 dBFS of treble), 12 dB
    // less at 50 %, 24 dB less at 25 %; Modern nearly none.
    d.hissGain = a * a * dsp::dbToGain(mix(-19.5f, -52.0f, e)) * hissNorm;   // before the place and the roll-off, which take some of it
    d.lowCutMix = std::min(1.0f, 4.0f * a);   // fully in by 25 %: its corner glides instead (a partial blend notches)
    d.warmthDb = a * mix(1.0f, 5.0f, e);
    d.honkDb = std::pow(a, 1.5f) * 10.0f * c;   // Classic: the cheap keyboard's boxy, honky middle
    d.topMix = std::min(1.0f, 4.0f * a);
    d.shelfDb = -a * mix(1.0f, 3.5f, e);
    // Classic: nearly mono (the side -20 dB at 100 %); Modern wider (+3.5 dB of side, the power kept).
    d.side = mix(1.0f - 0.9f * a, 1.0f + 0.6f * a, e);
    d.hallIn = std::pow(a, 1.5f);
    d.hallWidth = mix(0.6f, 1.0f, e);
    d.grainMix = c;
    if (e > 0.0f) d.bloom = e * std::sqrt(a);
    return d;
}

void DungeonSynth::updateFilters(const Derived& d, float a, float e) noexcept {
    const double rate = d.rateStep * sr;
    for (auto& f : antiAlias) f.set(rate * mix(0.45f, 0.9f, d.aliasOpen), sr, 1.414f);
    for (auto& f : reconstruct) f.set(0.45 * rate, sr, 1.414f);
    // The low cut's corner glides up with the amount from 20 Hz: Classic a thin bottom (fourth
    // order, 33 / 48 / 100 Hz at 25 / 50 / 100 %), Modern only the rumble (second order, 30 Hz).
    const double lowCorner = 20.0 * std::pow(mix(100.0f, 30.0f, e) / 20.0, std::pow(static_cast<double>(a), 0.8));
    for (auto& f : lowCut) f.set(lowCorner, sr, 1.414f);
    for (auto& f : lowCut2) f.set(mix(static_cast<float>(lowCorner), 10.0f, e), sr, 1.414f);
    // Front-loaded (a^0.6): 25 % about 11 kHz, 50 % about 8.5 kHz, 100 % the cassette's 5.5 kHz
    // (Classic) or Modern's softer 10 kHz.
    const double fc = 20000.0 * std::pow(mix(5500.0f, 14000.0f, e) / 20000.0, std::pow(static_cast<double>(a), 0.6));
    for (auto& f : top) f.set(fc, sr, 1.414f);
    for (auto& f : top2) f.set(std::min(0.45 * sr, fc * mix(1.2f, 1.4f, e)), sr, 1.3f);
    for (auto& f : top3) f.set(std::min(0.45 * sr, fc * mix(1.1f, 1.6f, e)), sr, 1.5f);   // sixth order: nothing of the tape's top survives
    warmth[0].peaking(sr, mix(250.0f, 200.0f, e), e > 0.0f ? static_cast<double>(mix(0.7f, 0.6f, e)) : 0.7, d.warmthDb);
    honk[0].peaking(sr, 1200.0, 0.8, d.honkDb);
    shelf[0].highShelf(sr, mix(4000.0f, 5500.0f, e), d.shelfDb);
    for (auto* bank : { &warmth, &honk, &shelf }) {
        auto& b = (*bank)[1];
        const auto& m = (*bank)[0];
        b.b0 = m.b0; b.b1 = m.b1; b.b2 = m.b2; b.a1 = m.a1; b.a2 = m.a2;
    }
    // The hall: longer and darker towards the full amount; Classic static and grainy (old
    // digital), Modern slowly modulated and very long (lush).
    const float t60 = mix(1.5f, mix(7.0f, 10.0f, e), a) * placeLength.value;   // measured broadband (an average place): about 4 s and 9.7 s
    for (int i = 0; i < lines; ++i)
        hallGain[std::size_t(i)] = std::pow(10.0f, -3.0f * hallLength[std::size_t(i)] / (t60 * static_cast<float>(sr)));
    hallDampCoefficient = dsp::poleForHz(mix(4500.0f, 5000.0f, e), sr);
    hallModDepth = static_cast<float>(mix(0.3f, 12.0f, e) * sr / 48000.0);
    hallLow.set(mix(3200.0f, 3000.0f, e) * placeTone.value, sr, 1.0f);
    hallHigh.set(mix(220.0f, 100.0f, e), sr, 1.414f);
    hallPreDelay = static_cast<float>(mix(0.025f, 0.03f, e) * sr);
    for (auto& f : hallDark) f.set(mix(3000.0f, 3200.0f, e) * placeTone.value, sr, 1.414f);
    hallOut = mix(1.23f, 0.32f, e);   // at 100 %: about -1 dB (Classic) and 0 dB (Modern) against the dry while it plays
    diffusion = mix(0.5f, 0.72f, e);
}

// Linear quantisation of x / scale to `step` (no dither: the grain of the old converters),
// back to its scale.
float DungeonSynth::quantise(float x, float step, float scale) noexcept {
    return std::round(x / (scale * step)) * (scale * step);
}

// One sample of the hall (mono in); returns the left, the right in outR.
float DungeonSynth::runHallMono(float in, float& outR, const Derived& d) noexcept {
    preDelay.push(in);
    float x = preDelay.read(1.0f + hallPreDelay);
    x = hallLow.low(hallHigh.highPass(x));
    for (int k = 0; k < diffusers; ++k) {
        auto& line = diffuser[std::size_t(k)];
        const float z = line.readInteger(static_cast<std::size_t>(diffuserLength[std::size_t(k)]));
        const float v = x + diffusion * z;
        x = z - diffusion * v;
        line.push(v);
    }
    // Session 18: Modern's bloom (two long diffusers: the hall builds up instead of starting at once).
    if (d.bloom > 0.0f) {
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
    // Width, then the old-digital grain (Classic): 11 kHz, 12 bits.
    const float m = 0.5f * (l + r), s = 0.5f * (l - r) * d.hallWidth;
    float outL = m + s;
    outR = m - s;
    if (d.grainMix > 0.0f) {
        grainPhase += grainStep;
        if (grainPhase >= 1.0f) {
            grainPhase -= 1.0f;
            grainHeld[0] = quantise(outL, grainBits, 1.0f);
            grainHeld[1] = quantise(outR, grainBits, 1.0f);
        }
        outL += d.grainMix * (grainHeld[0] - outL);
        outR += d.grainMix * (grainHeld[1] - outR);
    }
    outR = hallDark[3].low(hallDark[1].low(outR)) * hallOut;
    return hallDark[2].low(hallDark[0].low(outL)) * hallOut;
}

// The hiss, added to the input before the place (Session 16), riding about 25 dB under the music's
// own treble (above about 1 kHz), as a cassette's noise reduction lets it through only where the
// music masks it: about -45 dBFS on bright material, less on dark, none in silence. Before the
// place, its filters and rooms shape the hiss as they shape the music. (Added after the place, the
// same bright noise lay over every place: a dark Crypt then sounded like the brighter Gates.)
void DungeonSynth::processHiss(float* l, float* r, int n) noexcept {
    const float g0 = hissApplied, g1 = now.hissGain;
    hissApplied = g1;
    if (g0 <= 0.0f && g1 <= 0.0f) { gate = trebleLow = 0; return; }
    const float step = 1.0f / static_cast<float>(std::max(1, n));
    for (int i = 0; i < n; ++i) {
        const float mid = 0.5f * (l[i] + r[i]);
        trebleLow += trebleCoefficient * (mid - trebleLow);
        const float treble = mid - trebleLow, power = treble * treble;
        gate += (power > gate ? gateAttack : gateRelease) * (power - gate);
        const float gHiss = (g0 + (g1 - g0) * static_cast<float>(i + 1) * step) * std::sqrt(gate);
        for (int ch = 0; ch < 2; ++ch) {
            const auto k = std::size_t(ch);
            const float w = noise.bipolar();
            hissHigh[k] += 0.12f * (w - hissHigh[k]);
            hissLow[k] += 0.42f * ((w - hissHigh[k]) - hissLow[k]);
            hissLow2[k] += 0.42f * (hissLow[k] - hissLow2[k]);
            (ch == 0 ? l : r)[i] += hissLow2[k] * gHiss;
        }
    }
}

void DungeonSynth::process(float* l, float* r, int n) noexcept {
    float hallPeak = 0;
    int at = 0;
    while (at < n) {
        const int m = std::min(controlBlock, n - at);
        amount.skip(m);
        era.skip(m);
        chorusAmount.skip(m);
        send.skip(m);
        placeLength.skip(m);
        placeTone.skip(m);
        placeSend.skip(m);
        bool justStopped = false;
        if (targetAmount <= 0.0f && amount.value < 1.0e-5f && amount.value != 0.0f) { amount.snap(0.0f); justStopped = true; }
        if (std::abs(era.value - targetEra) < 1.0e-5f) era.snap(targetEra);
        const float a = amount.value, e = era.value;
        const Derived next = derive(a, e, chorusAmount.value);
        updateFilters(next, a, e);
        const float sendNow = send.value;
        if (!shaping()) {
            // Off: the signal passes exactly; only the hall rings out (added on top).
            if (justStopped) {
                for (auto& f : antiAlias) f.clear();
                for (auto& f : reconstruct) f.clear();
                for (auto& v : vintage) v.clear();
                for (auto& al : alignLine) al.clear();
                modernEnv = 0;
                for (auto& f : satSplit) f.clear();
                for (auto& f : satDc) f.clear();
                held = previous = lastIn = { 0, 0 };
                crushEnv = 0;
                chorusLine.clear();
                satEnv = 0;
                for (auto& w : wowLine) w.clear();
                for (auto& dcb : dc) dcb.clear();
                for (auto* bank : { &lowCut, &lowCut2, &top, &top2, &top3 }) for (auto& f : *bank) f.clear();
                for (auto* bank : { &warmth, &honk, &shelf }) for (auto& f : *bank) f.clear();
                midPower = sidePower = 0;
                widthGain = 1;
                refSlow = refFast = outSlow = outFast = refHold = 0;
                gain = gainTarget = ceiling = ceilingTarget = 1;
                for (auto& f : weightHigh) f.clear();
                for (auto& f : weightShelf) f.clear();
            }
            if (hallRinging)
                for (int i = at; i < at + m; ++i) {
                    float hr = 0;
                    const float hl = runHallMono(0.0f, hr, next);
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
        const float matchWeight = std::min(1.0f, 4.0f * a);
        for (int j = 0; j < m; ++j) {
            const int i = at + j;
            const float t = static_cast<float>(j + 1) * step;
            // The derived values glide across the control block (no steps).
            Derived d;
            d.lofiMix = mix(now.lofiMix, next.lofiMix, t);
            d.rateStep = mix(now.rateStep, next.rateStep, t);
            d.bitStep = mix(now.bitStep, next.bitStep, t);
            d.chorusMix = mix(now.chorusMix, next.chorusMix, t);
            d.chorusDepth = mix(now.chorusDepth, next.chorusDepth, t);
            d.satMix = mix(now.satMix, next.satMix, t);
            d.satDrive = mix(now.satDrive, next.satDrive, t);
            d.satAsym = mix(now.satAsym, next.satAsym, t);
            d.wowDepth = mix(now.wowDepth, next.wowDepth, t);
            d.flutterDepth = mix(now.flutterDepth, next.flutterDepth, t);
            d.hissGain = mix(now.hissGain, next.hissGain, t);
            d.lowCutMix = mix(now.lowCutMix, next.lowCutMix, t);
            d.topMix = mix(now.topMix, next.topMix, t);
            d.side = mix(now.side, next.side, t);
            d.hallIn = mix(now.hallIn, next.hallIn, t);
            d.hallWidth = mix(now.hallWidth, next.hallWidth, t);
            d.grainMix = mix(now.grainMix, next.grainMix, t);
            d.modern = mix(now.modern, next.modern, t);
            d.bloom = mix(now.bloom, next.bloom, t);
            d.align = mix(now.align, next.align, t);
            d.modernBitStep = mix(now.modernBitStep, next.modernBitStep, t);
            const float xl = l[i], xr = r[i];
            const float level = std::max(std::abs(xl), std::abs(xr));
            float yl = xl, yr = xr;
            // The sampler: anti-alias (opening towards 100 %: the aliasing is let through),
            // the sampling instant interpolated between two host samples, held, fewer bits
            // (scaled to the music's envelope, as a sample is normalised: the grain stays
            // whatever the level, and silence stays silent), the output filter.
            // Session 18, Modern: its sampler runs a fixed latency behind (its filters are linear
            // phase); the direct path is delayed to match, so a partial blend never comb-filters.
            if (d.modern > 0.0f) {
                alignLine[0].push(xl);
                alignLine[1].push(xr);
                const auto back = static_cast<std::size_t>(vintage[0].latency() + 1);
                yl += d.align * (alignLine[0].readInteger(back) - yl);
                yr += d.align * (alignLine[1].readInteger(back) - yr);
            }
            if (d.lofiMix > 0.0f) {
                crushEnv += (level > crushEnv ? crushAttack : crushRelease) * (level - crushEnv);
                const float al = antiAlias[2].low(antiAlias[0].low(xl)), ar = antiAlias[3].low(antiAlias[1].low(xr));
                holdPhase += d.rateStep;
                if (holdPhase >= 1.0f) {
                    holdPhase -= 1.0f;
                    const float back = std::min(1.0f, holdPhase / std::max(1.0e-3f, d.rateStep));   // how far past the instant
                    previous = held;
                    held[0] = al - back * (al - lastIn[0]);
                    held[1] = ar - back * (ar - lastIn[1]);
                    if (d.bitStep > 0.0f) {
                        const float scale = std::max(crushEnv, 1.0e-3f);
                        held[0] = quantise(held[0], d.bitStep, scale);
                        held[1] = quantise(held[1], d.bitStep, scale);
                    }
                }
                lastIn[0] = al;
                lastIn[1] = ar;
                // Between its samples the sampler's output is drawn as a line (a hard step at a
                // rate the host's is no whole multiple of would fold its images back down as
                // a hiss of its own); its output filter then takes what is above its band.
                const float hl0 = previous[0] + (held[0] - previous[0]) * holdPhase, hr0 = previous[1] + (held[1] - previous[1]) * holdPhase;
                float ql = reconstruct[2].low(reconstruct[0].low(hl0));
                float qr = reconstruct[3].low(reconstruct[1].low(hr0));
                // Session 18, Modern: the warm vintage sampler (a whole-number ratio of the
                // host's rate, linear-phase polyphase filters: nothing folds back or images up
                // but a whisper at the top of its band), its bits following the music's
                // envelope closely (released over 0.25 s: a decay keeps its bits, no grain).
                if (d.modern > 0.0f) {
                    modernEnv += (level > modernEnv ? crushAttack : modernRelease) * (level - modernEnv);
                    const float scale = std::max(modernEnv, 1.0e-3f), bitStep = d.modernBitStep;
                    const auto crush = [scale, bitStep](float v) { return bitStep > 0.0f ? quantise(v, bitStep, scale) : v; };
                    ql += d.modern * (vintage[0].process(xl, crush) - ql);
                    qr += d.modern * (vintage[1].process(xr, crush) - qr);
                }
                yl += d.lofiMix * (ql - yl);
                yr += d.lofiMix * (qr - yr);
            }
            // The chorus / ensemble: three voices from the mid, slow sweeps and a little vibrato.
            chorusLine.push(0.5f * (yl + yr));
            if (d.chorusMix > 0.0f) {
                vibratoPhase += 5.8 / sr;
                if (vibratoPhase >= 1.0) vibratoPhase -= 1.0;
                float v[3];
                static constexpr float base[3] { 0.008f, 0.0115f, 0.015f }, rate[3] { 0.53f, 0.71f, 0.89f };
                // Session 18: Modern's ensemble sweeps slower (0.55x: deep and slow).
                const float rateScale = d.modern > 0.0f ? mix(1.0f, 0.55f, d.modern) : 1.0f;
                for (int k = 0; k < 3; ++k) {
                    auto& ph = chorusPhase[std::size_t(k)];
                    ph += (d.modern > 0.0f ? rate[k] * rateScale : rate[k]) / sr;
                    if (ph >= 1.0) ph -= 1.0;
                    const float delay = base[k] + d.chorusDepth * std::sin(dsp::twoPi * static_cast<float>(ph))
                                      + 0.00015f * std::sin(dsp::twoPi * static_cast<float>(vibratoPhase) + 2.1f * static_cast<float>(k));
                    v[k] = chorusLine.read(1.0f + delay * static_cast<float>(sr));
                    if (d.modern > 0.0f) v[k] += d.modern * (readSinc(chorusLine, 1.0f + delay * static_cast<float>(sr)) - v[k]);   // Session 18: clean sweeps
                }
                const float c = d.chorusMix;
                yl = yl * (1.0f - 0.35f * c) + c * (0.8f * v[0] - 0.35f * v[2] + 0.4f * v[1]);
                yr = yr * (1.0f - 0.35f * c) + c * (0.8f * v[2] - 0.35f * v[0] + 0.4f * v[1]);
            }
            // The hall: a send from here, its return joins before the tape.
            {
                float hr = 0;
                const float hl = runHallMono(0.5f * (yl + yr) * d.hallIn * placeSend.value, hr, d);
                hallPeak = std::max(hallPeak, std::abs(hl) + std::abs(hr));
                const float pin = 0.5f * (xl * xl + xr * xr), ph = 0.5f * (hl * hl + hr * hr);
                hallInPower += powerCoefficient * (pin - hallInPower);   // the music's level (0.2 s), then held
                hallInHold += (hallInPower > hallInHold ? holdAttack : holdRelease) * (hallInPower - hallInHold);
                hallPower += powerCoefficient * (ph - hallPower);
                const float limit = 0.63f * hallInHold;   // -2 dB
                const float wanted = hallPower > limit && hallPower > 1.0e-12f ? std::sqrt(limit / hallPower) : 1.0f;
                hallCtl += (wanted < hallCtl ? ctlDown : ctlUp) * (wanted - hallCtl);
                const float g = hallOnly ? sendNow : sendNow * hallCtl;
                if (hallOnly) { yl = yr = 0.0f; }
                yl += hl * g;
                yr += hr * g;
            }
            // Tape saturation. Its drive follows the music's level (the same colour loud or
            // soft); small-signal gain 1; the asymmetry adds even harmonics (warmth).
            if (d.satMix > 0.0f && !hallOnly) {
                // The level it follows rises at once (a note after silence is not clipped by a
                // drive still set for the silence) and falls slowly.
                const float p = 0.5f * (yl * yl + yr * yr);
                satEnv += (p > satEnv ? satAttack : satCoefficient) * (p - satEnv);
                const float norm = std::clamp(0.31f / std::sqrt(std::max(satEnv, 1.0e-10f)), 0.5f, 4.0f);
                const float g = d.satDrive, inv = 1.0f / (g * norm);
                const float ul = yl * norm, ur = yr * norm;
                float sl = dc[0].process(std::tanh(g * (ul + d.satAsym * ul * ul)) * inv);
                float sr2 = dc[1].process(std::tanh(g * (ur + d.satAsym * ur * ur)) * inv);
                // Session 18, Modern: the tape warms the body (below about 2.5 kHz) and leaves the
                // top clean: saturating the treble made harmonics above the host's Nyquist,
                // folded back between the notes (a fizz). The bands sum back to the input exactly.
                if (d.modern > 0.0f) {
                    const float bl = satSplit[0].low(yl), br = satSplit[1].low(yr);
                    const float vl = bl * norm, vr = br * norm;
                    const float ml = satDc[0].process(std::tanh(g * (vl + d.satAsym * vl * vl)) * inv) + (yl - bl);
                    const float mr = satDc[1].process(std::tanh(g * (vr + d.satAsym * vr * vr)) * inv) + (yr - br);
                    sl += d.modern * (ml - sl);
                    sr2 += d.modern * (mr - sr2);
                }
                yl += d.satMix * (sl - yl);
                yr += d.satMix * (sr2 - yr);
            }
            // Wow and flutter: one modulated delay for both sides (the tape moves for both).
            {
                wowPhase += 0.45 / sr;
                if (wowPhase >= 1.0) wowPhase -= 1.0;
                flutterPhase += 6.8 / sr;
                if (flutterPhase >= 1.0) flutterPhase -= 1.0;
                flutter2Phase += 11.3 / sr;
                if (flutter2Phase >= 1.0) flutter2Phase -= 1.0;
                if (--driftCounter <= 0) {
                    driftCounter = static_cast<int>(0.7 * sr);
                    driftTarget = noise.bipolar();
                }
                drift += driftCoefficient * (driftTarget - drift);
                const float delay = d.wowDepth * (1.4f + std::sin(dsp::twoPi * static_cast<float>(wowPhase)) + 0.35f * drift)
                                  + d.flutterDepth * (1.0f + 0.7f * std::sin(dsp::twoPi * static_cast<float>(flutterPhase))
                                                     + 0.3f * std::sin(dsp::twoPi * static_cast<float>(flutter2Phase)));
                wowLine[0].push(yl);
                wowLine[1].push(yr);
                if (d.modern > 0.0f) {
                    // Session 18, Modern: the moving read through eight samples (a linear one
                    // swept the treble's level with its fraction: a fizz around every high note).
                    const float ll = wowLine[0].read(1.0f + std::max(0.0f, delay)), lr = wowLine[1].read(1.0f + std::max(0.0f, delay));
                    yl = ll + d.modern * (readSinc(wowLine[0], 5.0f + std::max(0.0f, delay)) - ll);
                    yr = lr + d.modern * (readSinc(wowLine[1], 5.0f + std::max(0.0f, delay)) - lr);
                } else {
                    yl = wowLine[0].read(1.0f + std::max(0.0f, delay));
                    yr = wowLine[1].read(1.0f + std::max(0.0f, delay));
                }
            }
            // Low cut, low-mid warmth, the low-pass and the softer shelf.
            yl += d.lowCutMix * (lowCut2[0].highPass(lowCut[0].highPass(yl)) - yl);
            yr += d.lowCutMix * (lowCut2[1].highPass(lowCut[1].highPass(yr)) - yr);
            yl = honk[0].process(warmth[0].process(yl));
            yr = honk[1].process(warmth[1].process(yr));
            const float tl = top3[0].low(top2[0].low(top[0].low(yl))), tr = top3[1].low(top2[1].low(top[1].low(yr)));
            yl += d.topMix * (tl - yl);
            yr += d.topMix * (tr - yr);
            yl = shelf[0].process(yl);
            yr = shelf[1].process(yr);
            // The image: Classic nearly mono, Modern wider. The power the narrowing takes
            // away (or the widening adds) is given back (or taken) evenly, slowly tracked, so the
            // level matcher below has little to make up.
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
            // The level matcher: the stage's output at its input's loudness.
            const float wl = weightShelf[0].process(weightHigh[0].highPass(xl)), wr = weightShelf[1].process(weightHigh[1].highPass(xr));
            const float ref = 0.5f * (wl * wl + wr * wr);
            const float pl = weightShelf[2].process(weightHigh[2].highPass(yl));
            const float pr = weightShelf[3].process(weightHigh[3].highPass(yr));
            const float out = 0.5f * (pl * pl + pr * pr);
            refSlow += slowCoefficient * (ref - refSlow);
            refFast += fastCoefficient * (ref - refFast);
            outSlow += slowCoefficient * (out - outSlow);
            outFast += fastCoefficient * (out - outFast);
            refHold += (refSlow > refHold ? holdAttack : holdRelease) * (refSlow - refHold);
            // While it plays (the input near its recent level): the slow matcher learns, and a
            // fast ceiling keeps the output within 0.5 dB of the input (the hall's onset, the
            // first notes). In the gaps both hold, so the tails ring at the level they had.
            const bool playing = refSlow > 1.0e-7f && refFast > 0.3f * refSlow && refSlow > 0.2f * refHold;
            if (--matchCounter <= 0) {
                matchCounter = 16;
                if (playing && outSlow > 1.0e-12f) gainTarget = std::clamp(std::sqrt(refSlow / outSlow), 0.5f, 2.0f);
                if (playing && outFast > 1.0e-12f) ceilingTarget = std::clamp(std::sqrt(1.122f * refFast / outFast), 0.5f, 2.0f);
            }
            gain += glideCoefficient * (gainTarget - gain);
            ceiling += (ceilingTarget < ceiling ? ceilingDown : ceilingUp) * (ceilingTarget - ceiling);
            const float g = 1.0f + (std::min(gain, ceiling) - 1.0f) * matchWeight;
            l[i] = yl * g;
            r[i] = yr * g;
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
            for (auto& b : bloomLine) b.clear();
            hallHigh.clear();
            hallLow.clear();
            for (auto& f : hallDark) f.clear();
            hallState = {};
            grainHeld = { 0, 0 };
        }
    }
}
}
