// Session 15: the DUNGEON SYNTH roller (included by DungeonCrawlerTests.cpp, "dungeonsynth"
// group; `--listen-dungeonsynth dir`, `--ds-hashes`).

namespace dstests {
// FNV-1a over the bits of every output sample.
std::uint64_t hashOf(const std::vector<float>& l, const std::vector<float>& r) {
    std::uint64_t h = 1469598103934665603ull;
    const auto add = [&h](float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        for (int k = 0; k < 4; ++k) { h ^= (bits >> (8 * k)) & 0xffu; h *= 1099511628211ull; }
    };
    for (std::size_t i = 0; i < l.size(); ++i) { add(l[i]); add(r[i]); }
    return h;
}
// The renders the null test compares (the FX on its material in four places, the instrument
// playing the melody on five instruments in their places, one locked elsewhere, one dry).
struct NullRender { std::string name; std::vector<float> l, r; };
using Configure = std::function<void(DungeonCrawlerProcessor&)>;
std::vector<NullRender> nullRenders(const Configure& configure = {}) {
    std::vector<NullRender> out;
    const auto material = [] {
        const auto p = plucks(0.35f), c = sawChord(0.12f);
        return Signal([p, c](int ch, long long n, double sr) { return p(ch, n, sr) + c(ch, n, sr); });
    }();
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Dungeon, 0 }, { dc::Cathedral, 2 }, { dc::Necropolis, 0 }, { dc::Mountains, 3 } }) {
        Host h;
        if (configure) configure(*h.p);
        h.go(l, k);
        h.render(3.0, material);
        out.push_back({ "FX " + placeName(l, k), h.outL, h.outR });
    }
    for (int instrument : { 7, 8, 14, 33, 39 }) {
        InstHost h;
        if (configure) configure(*h.p);
        h.play(instrument);
        h.render(0.05);
        melody(h, 0.1);
        h.render(0.05 + melodySeconds + 1.0);
        out.push_back({ "instrument " + instName(instrument), h.outL, h.outR });
    }
    {
        InstHost h;
        if (configure) configure(*h.p);
        h.play(22, false);
        h.go(dc::Cathedral, 2);
        h.render(0.05);
        melody(h, 0.1);
        h.render(0.05 + melodySeconds + 1.0);
        out.push_back({ "instrument Tin Whistle locked in the Crypt", h.outL, h.outR });
    }
    {
        InstHost h;
        if (configure) configure(*h.p);
        h.play(10);
        h.dry();
        h.render(0.05);
        melody(h, 0.1);
        h.render(0.05 + melodySeconds + 0.5);
        out.push_back({ "instrument Soft Choir dry", h.outL, h.outR });
    }
    return out;
}

// Settings for one render: the DUNGEON SYNTH amount and Era (Classic 0, Modern 1).
struct Ds { float amount = 0; int era = 0; };
std::string dsName(Ds d) { return std::to_string(juce::roundToInt(d.amount * 100)) + " % " + (d.era == 0 ? "Classic" : "Modern"); }
void setDs(InstHost& h, Ds d) { h.set(dc::ids::dungeonSynth, d.amount); h.set(dc::ids::dungeonSynthEra, float(d.era)); }
void setDs(Host& h, Ds d) { h.set(dc::ids::dungeonSynth, d.amount); h.set(dc::ids::dungeonSynthEra, float(d.era)); }
// The melody on an instrument in its place (or dry), with DUNGEON SYNTH set; `tail` after it.
effectstests::Take instMelody(int instrument, Ds d, double tail = 3.0, bool inPlace = true, int sound = dc::inst::SoundLatest) {
    InstHost h;
    h.p->setSoundGeneration(sound);
    h.play(instrument);
    if (!inPlace) h.dry();
    setDs(h, d);
    h.render(0.05);
    melody(h, 0.1);
    h.render(0.05 + melodySeconds + tail);
    return { h.outL, h.outR };
}
// The FX on its material in a place.
effectstests::Take fxMaterial(int location, int sub, Ds d, double seconds = 6.0) {
    Host h;
    h.go(location, sub);
    setDs(h, d);
    h.render(seconds, effectstests::material());
    return { h.outL, h.outR };
}
// Energy above `hz` against the whole (dB), from `from` to `to` (samples), both channels.
double highShare(const std::vector<float>& l, const std::vector<float>& r, double hz, std::size_t from = 0, std::size_t to = SIZE_MAX) {
    juce::IIRFilter a1, a2, b1, b2;
    a1.setCoefficients(juce::IIRCoefficients::makeHighPass(48000.0, hz));
    a2.setCoefficients(juce::IIRCoefficients::makeHighPass(48000.0, hz));
    b1.setCoefficients(juce::IIRCoefficients::makeHighPass(48000.0, hz));
    b2.setCoefficients(juce::IIRCoefficients::makeHighPass(48000.0, hz));
    double high = 0, all = 0;
    to = std::min(to, l.size());
    for (std::size_t i = 0; i < to; ++i) {
        const double hl = a2.processSingleSampleRaw(a1.processSingleSampleRaw(l[i])), hr = b2.processSingleSampleRaw(b1.processSingleSampleRaw(r[i]));
        if (i < from) continue;
        high += hl * hl + hr * hr;
        all += double(l[i]) * l[i] + double(r[i]) * r[i];
    }
    return 10.0 * std::log10(std::max(high, 1.0e-24) / std::max(all, 1.0e-24));
}
// Goertzel power of one frequency over a window (Hann).
double tonePower(const std::vector<float>& x, double hz, std::size_t from, std::size_t to) {
    const double w = 2.0 * juce::MathConstants<double>::pi * hz / 48000.0, coeff = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (std::size_t i = from; i < to; ++i) {
        const double win = 0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * double(i - from) / double(to - from));
        const double s0 = x[i] * win + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::max(1.0e-30, s1 * s1 + s2 * s2 - coeff * s1 * s2);
}
// The FX's dry path (Intensity 0: only DUNGEON SYNTH acts) on a sine.
Host fxSine(Ds d, double hz, float amplitude, double seconds, int location = dc::Dungeon) {
    Host h;
    h.go(location, 0);
    h.set(dc::ids::intensity, 0.0f);
    setDs(h, d);
    h.render(seconds, sine(hz, amplitude));
    return h;
}
// Harmonics 2..5 against the fundamental (dB).
double harmonicsDb(const std::vector<float>& x, double hz, std::size_t from, std::size_t to) {
    const double f = tonePower(x, hz, from, to);
    double h = 0;
    for (int k = 2; k <= 5; ++k) h += tonePower(x, hz * k, from, to);
    return 10.0 * std::log10(h / f);
}
// Pitch wander of a held sine: the instantaneous period from zero crossings (interpolated),
// its standard deviation in cents and the strongest modulation rate's share.
double wanderCents(const std::vector<float>& input, std::size_t from, std::size_t to) {
    // Low-passed first (2 kHz, fourth order): the crossings of the tone, not of the sampler's steps.
    std::vector<float> x(input);
    juce::IIRFilter f1, f2;
    f1.setCoefficients(juce::IIRCoefficients::makeLowPass(48000.0, 2000.0));
    f2.setCoefficients(juce::IIRCoefficients::makeLowPass(48000.0, 2000.0));
    for (auto& v : x) v = f2.processSingleSampleRaw(f1.processSingleSampleRaw(v));
    std::vector<double> crossings;
    for (std::size_t i = from + 1; i < to; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) crossings.push_back(double(i - 1) + double(-x[i - 1]) / double(x[i] - x[i - 1]));
    if (crossings.size() < 10) return 0.0;
    std::vector<double> cents;
    double mean = 0;
    for (std::size_t k = 1; k < crossings.size(); ++k) mean += crossings[k] - crossings[k - 1];
    mean /= double(crossings.size() - 1);
    double sum = 0;
    for (std::size_t k = 1; k < crossings.size(); ++k) {
        const double c = 1200.0 * std::log2(mean / (crossings[k] - crossings[k - 1]));
        sum += c * c;
    }
    return std::sqrt(sum / double(crossings.size() - 1));
}
// The tail after the melody (from 0.3 to 2.5 s after its last note ends) against the melody (dB).
double tailDb(const effectstests::Take& t) {
    const auto a = std::size_t(0.3 * 48000), b = std::size_t((0.1 + melodySeconds) * 48000);
    const auto c = std::size_t((0.1 + melodySeconds + 0.6) * 48000), e = std::min(t.l.size(), std::size_t((0.1 + melodySeconds + 2.8) * 48000));
    return effectstests::levelDb(t.l, t.r, c, e) - effectstests::levelDb(t.l, t.r, a, b);
}
// Side against mid (dB): how wide.
double widthDb(const effectstests::Take& t, std::size_t from = 0, std::size_t to = SIZE_MAX) {
    double m = 0, sd = 0;
    to = std::min(to, t.l.size());
    for (std::size_t i = from; i < to; ++i) {
        const double a = 0.5 * (double(t.l[i]) + t.r[i]), b = 0.5 * (double(t.l[i]) - t.r[i]);
        m += a * a;
        sd += b * b;
    }
    return 10.0 * std::log10(std::max(sd, 1.0e-24) / std::max(m, 1.0e-24));
}
// The five instruments of the listening render and the level checks.
const std::vector<int>& dsInstruments() {
    static const std::vector<int> v { dc::inst::designFor(dc::Castle, 2), dc::inst::designFor(dc::Necropolis, 0), dc::inst::designFor(dc::Castle, 3),
                                      dc::inst::designFor(dc::HauntedForest, 0), dc::inst::designFor(dc::Castle, 0) };
    return v;   // Royal Strings, Dark Choir Pad, Chamber Organ, Wooden Flute, Lute
}
// Session 16: the spectrum in third-octave bands (63 Hz .. 16 kHz, fourth-order band-passes),
// each band's share of the whole in dB (so a plain level change moves nothing).
std::vector<double> bandsDb(const std::vector<float>& l, const std::vector<float>& r, std::size_t from = 0, std::size_t to = SIZE_MAX) {
    std::vector<double> out;
    to = std::min(to, l.size());
    std::vector<double> energy;
    double total = 0;
    for (int b = 0; b < 25; ++b) {
        const double fc = 63.0 * std::pow(2.0, b / 3.0);
        juce::IIRFilter f[4];
        for (auto& x : f) x.setCoefficients(juce::IIRCoefficients::makeBandPass(48000.0, fc, 4.32));
        double e = 0;
        for (std::size_t i = 0; i < to; ++i) {
            const double a = f[1].processSingleSampleRaw(f[0].processSingleSampleRaw(l[i]));
            const double c = f[3].processSingleSampleRaw(f[2].processSingleSampleRaw(r[i]));
            if (i >= from) e += a * a + c * c;
        }
        energy.push_back(e);
        total += e;
    }
    for (double e : energy) out.push_back(10.0 * std::log10(std::max(e, 1.0e-30) / std::max(total, 1.0e-30)));
    return out;
}
// How far two spectra are apart (dB, RMS over the bands from 63 Hz to 10 kHz that carry
// anything in either: within 50 dB of the loudest band), level-independent. (Bands far down,
// or above 10 kHz where the places are nearly silent, would let a whisper of hiss or of
// filtering count as much as the music.)
double spectralDistance(const std::vector<double>& a, const std::vector<double>& b) {
    const std::size_t bands = std::min<std::size_t>(a.size(), 23);
    const double top = std::max(*std::max_element(a.begin(), a.begin() + std::ptrdiff_t(bands)), *std::max_element(b.begin(), b.begin() + std::ptrdiff_t(bands)));
    double sum = 0;
    int n = 0;
    for (std::size_t k = 0; k < bands; ++k) {
        if (std::max(a[k], b[k]) < top - 50.0) continue;
        const double d = std::max(a[k], top - 50.0) - std::max(b[k], top - 50.0);
        sum += d * d;
        ++n;
    }
    return n > 0 ? std::sqrt(sum / n) : 0.0;
}
// The share above `hz` (dB), from the third-octave bands (steep: what lies below does not leak in).
double topDb(const std::vector<double>& bands, double hz) {
    double sum = 0;
    for (std::size_t b = 0; b < bands.size(); ++b)
        if (63.0 * std::pow(2.0, double(b) / 3.0) >= hz * 0.99) sum += std::pow(10.0, bands[b] / 10.0);
    return 10.0 * std::log10(std::max(sum, 1.0e-30));
}
double topDb(const effectstests::Take& t, double hz) { return topDb(bandsDb(t.l, t.r), hz); }
double spectralDistance(const effectstests::Take& a, const effectstests::Take& b) {
    return spectralDistance(bandsDb(a.l, a.r), bandsDb(b.l, b.r));
}
} // namespace dstests

int dsHashes() {
    for (const auto& r : dstests::nullRenders())
        std::cout << std::hex << dstests::hashOf(r.l, r.r) << std::dec << "  " << r.name << "\n";
    return 0;
}

// `--ds-report`: the numbers behind the checks, for every Era at 0 / 25 / 50 / 100 %.
int dsReport() {
    using namespace dstests;

    const std::vector<Ds> settings { { 0, 0 }, { 0.25f, 0 }, { 0.5f, 0 }, { 1, 0 }, { 0.25f, 1 }, { 0.5f, 1 }, { 1, 1 } };
    for (int instrument : dsInstruments()) {
        const auto base = instMelody(instrument, { 0, 0 });
        const double l0 = loudness(base.l, base.r, 48000), h0 = highShare(base.l, base.r, 6000), t0 = tailDb(base), w0 = widthDb(base);
        for (const auto& d : settings) {
            const auto t = d.amount == 0 ? base : instMelody(instrument, d);
            std::cout << instName(instrument) << " " << dsName(d) << ": loudness " << fmt(loudness(t.l, t.r, 48000) - l0, 2) << " dB, above 6 kHz "
                      << fmt(highShare(t.l, t.r, 6000) - h0, 2) << " dB, tail " << fmt(tailDb(t) - t0, 2) << " dB (" << fmt(tailDb(t)) << "), width "
                      << fmt(widthDb(t) - w0, 2) << " dB, peak " << fmt(toDb(std::max(*std::max_element(t.l.begin(), t.l.end()), -*std::min_element(t.l.begin(), t.l.end()))), 1) << " dBFS\n";
        }
    }
    for (int instrument : dsInstruments()) {
        const auto base = instMelody(instrument, { 0, 0 }, 1.0, false);
        const double h0 = highShare(base.l, base.r, 6000), l0 = loudness(base.l, base.r, 48000);
        for (const auto& d : settings) {
            const auto t = d.amount == 0 ? base : instMelody(instrument, d, 1.0, false);
            std::cout << instName(instrument) << " dry " << dsName(d) << ": above 6 kHz " << fmt(highShare(t.l, t.r, 6000) - h0, 2) << " dB, loudness "
                      << fmt(loudness(t.l, t.r, 48000) - l0, 2) << " dB\n";
        }
    }
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Dungeon, 0 }, { dc::Cathedral, 2 }, { dc::Necropolis, 0 }, { dc::Mountains, 3 } }) {
        const auto base = fxMaterial(l, k, { 0, 0 });
        const auto from = std::size_t(2.0 * 48000);
        const double l0 = effectstests::levelDb(base.l, base.r, from, base.l.size()), h0 = highShare(base.l, base.r, 6000, from);
        for (const auto& d : settings) {
            const auto t = d.amount == 0 ? base : fxMaterial(l, k, d);
            std::cout << "FX " << placeName(l, k) << " " << dsName(d) << ": level " << fmt(effectstests::levelDb(t.l, t.r, from, t.l.size()) - l0, 2)
                      << " dB, loudness " << fmt(loudness(t.l, t.r, 48000) - loudness(base.l, base.r, 48000), 2) << " dB, above 6 kHz "
                      << fmt(highShare(t.l, t.r, 6000, from) - h0, 2) << " dB\n";
        }
    }
    for (const auto& d : settings) {
        auto h = fxSine(d, 220.0, 0.3f, 4.0);
        const auto a = h.at(1.0), b = h.at(4.0);
        auto w = fxSine(d, 1000.0, 0.3f, 6.0);
        std::cout << "sine " << dsName(d) << ": harmonics " << fmt(harmonicsDb(h.outL, 220.0, a, b)) << " dB, wander " << fmt(wanderCents(w.outL, w.at(1.0), w.at(6.0)), 2)
                  << " cents, level " << fmt(toDb(h.rms(a, b)) - toDb(0.3 / std::sqrt(2.0)), 2) << " dB\n";
    }
    return 0;
}

namespace dstests {
// The hall's decay (s): its impulse response on the stage alone, T30 from the Schroeder curve.
double hallT60(float amount, int era) {
    dc::DungeonSynth ds;
    ds.setControls(amount, era, 0.6f, 1.0f);
    ds.prepare(48000);
    ds.setHallOnlyForTest(true);
    std::array<float, 64> l {}, r {};
    std::vector<double> energy;
    for (int b = 0; b < 48000 * 14 / 64; ++b) {
        l.fill(0.0f);
        r.fill(0.0f);
        if (b == 0) l[0] = r[0] = 0.5f;
        ds.process(l.data(), r.data(), 64);
        for (int i = 0; i < 64; ++i) energy.push_back(double(l[i]) * l[i] + double(r[i]) * r[i]);
    }
    std::vector<double> curve(energy.size());
    double sum = 0;
    for (std::size_t i = energy.size(); i-- > 0;) { sum += energy[i]; curve[i] = sum; }
    if (sum <= 0.0) return 0.0;
    double t5 = -1, t35 = -1;
    for (std::size_t i = 0; i < curve.size(); ++i) {
        const double db = 10.0 * std::log10(std::max(curve[i], 1.0e-30) / curve[0]);
        if (t5 < 0 && db <= -5.0) t5 = double(i) / 48000.0;
        if (t35 < 0 && db <= -35.0) { t35 = double(i) / 48000.0; break; }
    }
    return t5 >= 0 && t35 > t5 ? 2.0 * (t35 - t5) : 0.0;
}
// The largest second difference over a range (both channels).
double maxStepOf(const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to) {
    double m = 0;
    for (std::size_t i = std::max<std::size_t>(from, 2); i < std::min(to, l.size()); ++i) {
        m = std::max(m, std::abs(double(l[i]) - 2.0 * l[i - 1] + l[i - 2]));
        m = std::max(m, std::abs(double(r[i]) - 2.0 * r[i - 1] + r[i - 2]));
    }
    return m;
}

// Session 16: measuring the stage's strength.
// The power spectrum (Hann, 65536 points: 0.73 Hz a bin) of x from `from`.
std::vector<double> spectrumOf(const std::vector<float>& x, std::size_t from) {
    constexpr int order = 16, size = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<float> buf(2 * size, 0.0f);
    for (int i = 0; i < size; ++i) {
        const auto k = from + std::size_t(i);
        buf[std::size_t(i)] = k < x.size() ? x[k] * float(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * i / size)) : 0.0f;
    }
    fft.performFrequencyOnlyForwardTransform(buf.data());
    std::vector<double> p(size / 2);
    for (int k = 0; k < size / 2; ++k) p[std::size_t(k)] = double(buf[std::size_t(k)]) * buf[std::size_t(k)];
    return p;
}
double bandPower(const std::vector<double>& p, double lo, double hi) {
    const double bin = 48000.0 / double(2 * p.size());
    double sum = 0;
    for (auto k = std::size_t(std::max(1.0, lo / bin)); k < p.size() && double(k) * bin <= hi; ++k) sum += p[k];
    return sum;
}
// Harmonics 2..5 against the fundamental (dB), each a +/-3 % band (the wow moves them).
double harmonicsFft(const std::vector<float>& x, double hz, std::size_t from) {
    const auto p = spectrumOf(x, from);
    double h = 0;
    for (int k = 2; k <= 5; ++k) h += bandPower(p, hz * k * 0.97, hz * k * 1.03);
    return 10.0 * std::log10(std::max(h, 1.0e-30) / std::max(bandPower(p, hz * 0.97, hz * 1.03), 1.0e-30));
}
// What is neither the tones nor their harmonics (the hiss, the grain), in dBFS (one channel).
double noiseFloorDbfs(const std::vector<float>& x, std::initializer_list<double> tones, std::size_t from) {
    const auto p = spectrumOf(x, from);
    double total = bandPower(p, 20.0, 24000.0), tonal = 0;
    // The tones' harmonics, +/-3 % around each (the wow moves them).
    std::vector<std::pair<double, double>> spans;
    for (double hz : tones)
        for (int k = 1; k * hz < 23000.0; ++k) spans.push_back({ hz * k * 0.97, hz * k * 1.03 });
    std::sort(spans.begin(), spans.end());
    double reach = 0;
    for (const auto& [lo, hi] : spans) {
        const double from = std::max(lo, reach);
        if (hi > from) { tonal += bandPower(p, from, hi); reach = hi; }
    }
    double ms = 0;
    for (std::size_t i = from; i < std::min(x.size(), from + 65536); ++i) ms += double(x[i]) * x[i];
    ms /= 65536.0;
    return 10.0 * std::log10(std::max(1.0e-30, (total - tonal) / std::max(total, 1.0e-30) * ms));
}
// The stage alone (no place, no processor) on a signal, 64-sample blocks at 48 kHz.
effectstests::Take stageRun(float amount, int era, float chorus, float send, const Signal& signal, double seconds, bool hallOnly = false) {
    dc::DungeonSynth ds;
    ds.setControls(amount, era, chorus, send);
    ds.prepare(48000);
    ds.setHallOnlyForTest(hallOnly);
    const auto n = std::size_t(seconds * 48000.0);
    effectstests::Take t { std::vector<float>(n), std::vector<float>(n) };
    for (std::size_t i = 0; i < n; ++i) { t.l[i] = signal(0, (long long) i, 48000.0); t.r[i] = signal(1, (long long) i, 48000.0); }
    for (std::size_t i = 0; i < n; i += 64) {
        const int m = int(std::min<std::size_t>(64, n - i));
        ds.setControls(amount, era, chorus, send);
        if (!hallOnly) ds.processHiss(t.l.data() + i, t.r.data() + i, m);   // as the processor does (before its place)
        ds.process(t.l.data() + i, t.r.data() + i, m);
    }
    return t;
}
// Wow and flutter of a held tone (cents RMS): the pitch per period (zero crossings), below
// 2 Hz (wow) and 2..25 Hz (flutter); and the wow's peak (% of speed).
struct WowFlutter { double wow = 0, flutter = 0, wowPeakPercent = 0; };
WowFlutter wowFlutterOf(const std::vector<float>& input, std::size_t from, std::size_t to) {
    std::vector<float> x(input);
    juce::IIRFilter f1, f2;
    f1.setCoefficients(juce::IIRCoefficients::makeLowPass(48000.0, 2000.0));
    f2.setCoefficients(juce::IIRCoefficients::makeLowPass(48000.0, 2000.0));
    for (auto& v : x) v = f2.processSingleSampleRaw(f1.processSingleSampleRaw(v));
    std::vector<double> crossings;
    for (std::size_t i = from + 1; i < std::min(to, x.size()); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) crossings.push_back(double(i - 1) + double(-x[i - 1]) / double(x[i] - x[i - 1]));
    WowFlutter w;
    if (crossings.size() < 1000) return w;
    double mean = (crossings.back() - crossings.front()) / double(crossings.size() - 1);
    const double rate = 48000.0 / mean;   // periods a second
    const double aw = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 2.0 / rate), af = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 25.0 / rate);
    double lw = 0, lf = 0, sw = 0, sf = 0, peak = 0;
    std::size_t count = 0;
    for (std::size_t k = 1; k < crossings.size(); ++k) {
        const double c = 1200.0 * std::log2(mean / (crossings[k] - crossings[k - 1]));
        lw += aw * (c - lw);
        lf += af * (c - lf);
        if (k < std::size_t(rate)) continue;   // the filters settle (a second)
        sw += lw * lw;
        sf += (lf - lw) * (lf - lw);
        peak = std::max(peak, std::abs(lw));
        ++count;
    }
    w.wow = std::sqrt(sw / double(std::max<std::size_t>(count, 1)));
    w.flutter = std::sqrt(sf / double(std::max<std::size_t>(count, 1)));
    w.wowPeakPercent = (std::pow(2.0, peak / 1200.0) - 1.0) * 100.0;
    return w;
}
// A saw chord, 1 s on and 1 s off.
Signal pulsedChord() {
    return [](int, long long n, double sr) {
        if ((n / (long long) sr) % 2 != 0) return 0.0f;
        double v = 0;
        for (double f : { 110.0, 164.8, 220.0 }) v += 2.0 * (std::fmod(f * double(n) / sr, 1.0) - 0.5);
        return 0.08f * float(v);
    };
}
// The stage's tails: the energy in the gaps against the energy while it plays (dB), and the
// hall alone while it plays against the input (dB).
struct Wet { double gaps = -200, hall = -200; };
Wet wetOf(float amount, int era) {
    const auto chord = pulsedChord();
    const auto out = stageRun(amount, era, 0.6f, 1.0f, chord, 10.0), hall = stageRun(amount, era, 0.6f, 1.0f, chord, 10.0, true);
    double on = 0, off = 0, in = 0, h = 0;
    for (std::size_t i = 2 * 48000; i < out.l.size(); ++i) {
        const bool playing = (i / 48000) % 2 == 0;
        const double y = double(out.l[i]) * out.l[i] + double(out.r[i]) * out.r[i];
        if (playing) { on += y; const double x = chord(0, (long long) i, 48000.0); in += 2.0 * x * x; h += double(hall.l[i]) * hall.l[i] + double(hall.r[i]) * hall.r[i]; }
        else off += y;
    }
    return { 10.0 * std::log10(std::max(off, 1.0e-30) / std::max(on, 1.0e-30)), 10.0 * std::log10(std::max(h, 1.0e-30) / std::max(in, 1.0e-30)) };
}
// The FX's dry path (Intensity 0: the place is out, the stage on the input) on its material.
effectstests::Take fxDry(Ds d, double seconds = 6.0) {
    Host h;
    h.set(dc::ids::intensity, 0.0f);
    setDs(h, d);
    h.render(seconds, effectstests::material());
    return { h.outL, h.outR };
}
// The bright material the top's checks use: three instruments dry and the FX's dry path.
const std::vector<int>& brightInstruments() {
    static const std::vector<int> v { dc::inst::designFor(dc::Castle, 2), dc::inst::designFor(dc::HauntedForest, 0), dc::inst::designFor(dc::Castle, 0) };
    return v;   // Royal Strings, Wooden Flute, Lute
}

// Everything the strength checks and `--ds-strength` measure.
struct Strength {
    // [era][0 25 %, 1 50 %, 2 100 %]
    double harmonics[2][3] {}, alias[2] {}, noise[2][3] {}, t60[2][3] {}, rate44 = 0, rate48 = 0;
    WowFlutter wow[2][3] {};
    Wet wet[2][3] {};
    struct Place { std::string name; double distance[2][3] {}, cm = 0, width[2][3] {}, tail[2] {}, top8[2] {}, top12[2] {}; };
    std::vector<Place> places;   // in its place (five instruments) and the FX in two places
    struct Bright { std::string name; double top8[2][3] {}, top12[2][3] {}; };
    std::vector<Bright> bright;  // dry
};
Strength measureStrength() {
    Strength m;
    const float amounts[3] { 0.25f, 0.5f, 1.0f };
    for (int era : { 0, 1 })
        for (int k = 0; k < 3; ++k) {
            const float a = amounts[k];
            const auto e = std::size_t(era);
            const auto ku = std::size_t(k);
            const auto tone = stageRun(a, era, 0.0f, 0.0f, sine(200.0, 0.316f), 4.0);
            m.harmonics[e][ku] = harmonicsFft(tone.l, 200.0, 48000);
            // A tone with some treble (220 Hz at -15 dBFS RMS and its 12th harmonic, 2.64 kHz, at
            // -23 dBFS RMS, so whatever the tape makes of them stays on 220's harmonics): the hiss
            // rides under the music's treble.
            const auto typical = stageRun(a, era, 0.0f, 0.0f, [](int ch, long long n, double sr) { return sine(220.0, 0.25f)(ch, n, sr) + sine(2640.0, 0.1f)(ch, n, sr); }, 4.0);
            m.noise[e][ku] = noiseFloorDbfs(typical.l, { 220.0 }, 48000);
            const auto held = stageRun(a, era, 0.0f, 0.0f, sine(1000.0, 0.25f), 12.0);
            m.wow[e][ku] = wowFlutterOf(held.l, 48000, held.l.size());
            m.t60[e][ku] = hallT60(a, era);
            m.wet[e][ku] = wetOf(a, era);
        }
    for (int era : { 0, 1 }) {
        // Aliasing: a 9 kHz tone at 100 % folds to 13 - 9 = 4 kHz in Classic (the sampler at 13 kHz).
        const auto t = stageRun(1.0f, era, 0.0f, 0.0f, sine(9000.0, 0.25f), 3.0);
        const auto p = spectrumOf(t.l, 48000);
        m.alias[std::size_t(era)] = 10.0 * std::log10(std::max(bandPower(p, 3850.0, 4150.0), 1.0e-30) / std::max(bandPower(p, 20.0, 24000.0), 1.0e-30));
    }
    for (double rate : { 44100.0, 48000.0 }) {
        dc::DungeonSynth ds;
        ds.setControls(1.0f, 0, 0.0f, 0.0f);
        ds.prepare(rate);
        (rate < 46000.0 ? m.rate44 : m.rate48) = ds.samplerRate();
    }
    const auto place = [&](const std::string& name, const std::function<effectstests::Take(Ds)>& render, bool tails) {
        Strength::Place p;
        p.name = name;
        const auto base = render({ 0, 0 });
        const auto b0 = bandsDb(base.l, base.r);
        effectstests::Take full[2];
        for (int era : { 0, 1 })
            for (int k = 0; k < 3; ++k) {
                const auto t = render({ amounts[k], era });
                const auto e = std::size_t(era), ku = std::size_t(k);
                const auto b = bandsDb(t.l, t.r);
                p.distance[e][ku] = spectralDistance(b0, b);
                p.width[e][ku] = widthDb(t) - widthDb(base);
                if (k == 2) {
                    full[e] = t;
                    p.top8[e] = topDb(b, 8000) - topDb(b0, 8000);
                    p.top12[e] = topDb(b, 12000) - topDb(b0, 12000);
                    p.tail[e] = tails ? tailDb(t) - tailDb(base) : 0.0;
                }
            }
        p.cm = spectralDistance(full[0], full[1]);
        m.places.push_back(p);
    };
    for (int instrument : dsInstruments()) place(instName(instrument) + " in its place", [instrument](Ds d) { return instMelody(instrument, d); }, true);
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Cathedral, 2 }, { dc::Necropolis, 0 } })
        place("FX " + placeName(l, k), [l = l, k = k](Ds d) { return fxMaterial(l, k, d); }, false);
    const auto bright = [&](const std::string& name, const std::function<effectstests::Take(Ds)>& render) {
        Strength::Bright b;
        b.name = name;
        const auto b0 = bandsDb(render({ 0, 0 }).l, render({ 0, 0 }).r);
        for (int era : { 0, 1 })
            for (int k = 0; k < 3; ++k) {
                const auto t = render({ amounts[k], era });
                const auto bt = bandsDb(t.l, t.r);
                b.top8[era][k] = topDb(bt, 8000) - topDb(b0, 8000);
                b.top12[era][k] = topDb(bt, 12000) - topDb(b0, 12000);
            }
        m.bright.push_back(b);
    };
    // Session 17: the Wooden Flute's bright material is its synthesis (sound 14), as these checks
    // were written for: the recorded flute (sound 15) is dark by design (it has little above
    // 8 kHz for the stage to roll off; the realwinds group checks the roller on it).
    for (int instrument : brightInstruments())
        bright(instName(instrument) + " dry", [instrument](Ds d) {
            return instMelody(instrument, d, 1.0, false, instrument == dc::inst::designFor(dc::HauntedForest, 0) ? int(dc::inst::SoundSeated) : int(dc::inst::SoundLatest));
        });
    bright("FX dry path", [](Ds d) { return fxDry(d); });
    return m;
}
} // namespace dstests

// `--ds-strength` (Session 16): the numbers behind the strength checks.
int dsStrength() {
    using namespace dstests;
    const auto m = measureStrength();
    const char* eras[2] { "Classic", "Modern" };
    const char* at[3] { "25", "50", "100" };
    std::cout << "sampler rate at 100 % Classic: " << fmt(m.rate44, 0) << " Hz at 44.1 kHz, " << fmt(m.rate48, 0) << " Hz at 48 kHz\n";
    for (int e : { 0, 1 }) {
        std::cout << eras[e] << ": alias of a 9 kHz tone at 4 kHz " << fmt(m.alias[e], 1) << " dB\n";
        for (int k = 0; k < 3; ++k)
            std::cout << "  " << eras[e] << " " << at[k] << " %: harmonics " << fmt(m.harmonics[e][k], 1) << " dB, noise floor " << fmt(m.noise[e][k], 1)
                      << " dBFS, wow " << fmt(m.wow[e][k].wow, 2) << " cents RMS (peak " << fmt(m.wow[e][k].wowPeakPercent, 3) << " %), flutter "
                      << fmt(m.wow[e][k].flutter, 2) << " cents RMS, hall T60 " << fmt(m.t60[e][k], 2) << " s, gaps/playing " << fmt(m.wet[e][k].gaps, 1)
                      << " dB, hall/input " << fmt(m.wet[e][k].hall, 1) << " dB\n";
    }
    for (const auto& p : m.places) {
        std::cout << p.name << ":";
        for (int e : { 0, 1 })
            std::cout << " " << eras[e] << " distance " << fmt(p.distance[e][0], 2) << " / " << fmt(p.distance[e][1], 2) << " / " << fmt(p.distance[e][2], 2)
                      << " dB, width " << fmt(p.width[e][0], 1) << " / " << fmt(p.width[e][1], 1) << " / " << fmt(p.width[e][2], 1) << " dB, tail " << fmt(p.tail[e], 1)
                      << " dB, top8 " << fmt(p.top8[e], 1) << ", top12 " << fmt(p.top12[e], 1) << ";";
        std::cout << " Classic-Modern " << fmt(p.cm, 2) << " dB\n";
    }
    for (const auto& b : m.bright) {
        std::cout << b.name << ":";
        for (int e : { 0, 1 }) std::cout << " " << eras[e] << " above 8 kHz " << fmt(b.top8[e][0], 1) << " / " << fmt(b.top8[e][1], 1) << " / " << fmt(b.top8[e][2], 1)
                                         << ", above 12 kHz " << fmt(b.top12[e][0], 1) << " / " << fmt(b.top12[e][1], 1) << " / " << fmt(b.top12[e][2], 1) << ";";
        std::cout << "\n";
    }
    return 0;
}

// `--ds-audit` (Session 16): what reaches the output. For each instrument, the stage alone (the
// instrument dry) and in its place, and the FX in two places: 0 % against Classic and Modern at
// 100 %, and Classic against Modern: spectral distance, above 6 kHz, width (side against mid),
// the tail after the melody.
int dsAudit() {
    using namespace dstests;
    const auto line = [](const std::string& name, const effectstests::Take& base, const effectstests::Take& c, const effectstests::Take& m, bool tails) {
        std::cout << name << ": distance 0-C " << fmt(spectralDistance(base, c), 2) << " / 0-M " << fmt(spectralDistance(base, m), 2) << " / C-M "
                  << fmt(spectralDistance(c, m), 2) << " dB; above 8 kHz C " << fmt(topDb(c, 8000) - topDb(base, 8000), 1)
                  << " M " << fmt(topDb(m, 8000) - topDb(base, 8000), 1) << ", above 12 kHz M " << fmt(topDb(m, 12000) - topDb(base, 12000), 1) << " dB (0 %: " << fmt(topDb(base, 8000), 1) << "); width C " << fmt(widthDb(c) - widthDb(base), 1)
                  << " M " << fmt(widthDb(m) - widthDb(base), 1) << " dB (0 %: " << fmt(widthDb(base), 1) << ")";
        if (tails) std::cout << "; tail C " << fmt(tailDb(c) - tailDb(base), 1) << " M " << fmt(tailDb(m) - tailDb(base), 1) << " dB";
        std::cout << "\n";
    };
    for (int instrument : dsInstruments()) {
        for (bool inPlace : { false, true }) {
            const auto base = instMelody(instrument, { 0, 0 }, 3.0, inPlace), c = instMelody(instrument, { 1, 0 }, 3.0, inPlace),
                       m = instMelody(instrument, { 1, 1 }, 3.0, inPlace);
            line(instName(instrument) + (inPlace ? " in its place" : " dry"), base, c, m, true);
        }
    }
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Cathedral, 2 }, { dc::Necropolis, 0 } }) {
        const auto base = fxMaterial(l, k, { 0, 0 }), c = fxMaterial(l, k, { 1, 0 }), m = fxMaterial(l, k, { 1, 1 });
        line("FX " + placeName(l, k), base, c, m, false);
    }
    return 0;
}

// `--ds-places` (Session 16): the place still matters. The Soft Choir (locked) in the Crypt, the
// Gates and the Dwarven Hall: how different each pair is at 0 % and at 100 % (both Eras).
namespace dstests {
struct PlacePair { std::string a, b; double d0 = 0, d1 = 0; };
std::vector<PlacePair> placePairs(int era) {
    const int choir = dc::inst::designFor(dc::Cathedral, 0);
    const std::vector<std::pair<int, int>> places { { dc::Cathedral, 2 }, { dc::Necropolis, 0 }, { dc::Mountains, 3 } };
    std::vector<effectstests::Take> at0, at100;
    for (const auto& [l, k] : places)
        for (float a : { 0.0f, 1.0f }) {
            InstHost h;
            h.play(choir, false);
            h.go(l, k);
            setDs(h, { a, era });
            h.render(0.05);
            melody(h, 0.1);
            h.render(0.05 + melodySeconds + 2.0);
            (a == 0.0f ? at0 : at100).push_back({ h.outL, h.outR });
        }
    std::vector<PlacePair> out;
    for (std::size_t i = 0; i < places.size(); ++i)
        for (std::size_t j = i + 1; j < places.size(); ++j)
            out.push_back({ placeName(places[i].first, places[i].second), placeName(places[j].first, places[j].second),
                            effectstests::effectShare(at0[i].l, at0[i].r, at0[j].l, at0[j].r, 0, at0[i].l.size()),
                            effectstests::effectShare(at100[i].l, at100[i].r, at100[j].l, at100[j].r, 0, at100[i].l.size()) });
    return out;
}
}
int dsPlaces() {
    if (std::getenv("DS_FRAMES") != nullptr)
        for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Cathedral, 2 }, { dc::Necropolis, 0 }, { dc::Mountains, 3 } }) {
            Host h;
            h.go(l, k);
            h.render(1.0, silence());
            const auto& f = h.p->engine().frame();
            std::cout << placeName(l, k) << ": decay " << f[dc::Decay] << " cutoff " << f[dc::Cutoff] << " reverbMix " << f[dc::ReverbMix] << " vaultMix " << f[dc::VaultMix]
                      << " space " << f[dc::Space] << " delayMix " << f[dc::DelayMix] << " darkness " << f[dc::Darkness] << "\n";
        }
    for (int era : { 0, 1 })
        for (const auto& p : dstests::placePairs(era))
            std::cout << (era == 0 ? "Classic" : "Modern") << " " << p.a << " / " << p.b << ": " << fmt(p.d0) << " dB at 0 %, " << fmt(p.d1) << " dB at 100 % ("
                      << fmt(100.0 * p.d1 / std::max(p.d0, 1.0e-9), 0) << " %)\n";
    return 0;
}

void testDungeonSynth(Suite& s) {
    using namespace dstests;
    s.section("dungeon synth: the roller and its Era (Session 15)");
    const auto kind = [](const DungeonCrawlerProcessor& p) { return std::string(p.isInstrument() ? "instrument" : "FX"); };
    // The parameters: both plug-ins, automatable, 0..100 % (0 % by default), Classic | Modern.
    {
        DungeonCrawlerProcessor fx, inst(PluginKind::Instrument);
        for (auto* p : { &fx, &inst }) {
            auto* amount = dynamic_cast<juce::RangedAudioParameter*>(p->parameters.getParameter(dc::ids::dungeonSynth));
            auto* era = dynamic_cast<juce::AudioParameterChoice*>(p->parameters.getParameter(dc::ids::dungeonSynthEra));
            s.check(amount != nullptr && amount->isAutomatable() && amount->getNormalisableRange().start == 0.0f && amount->getNormalisableRange().end == 1.0f
                        && amount->getDefaultValue() == 0.0f && amount->getCurrentValueAsText() == "0 %" && amount->getName(40) == "Dungeon Synth",
                    kind(*p) + ": Dungeon Synth is an automatable parameter, 0-100 %, 0 % by default");
            s.check(era != nullptr && era->isAutomatable() && era->choices == juce::StringArray { "Classic", "Modern" } && era->getIndex() == 0,
                    kind(*p) + ": its Era is Classic or Modern (Classic by default), automatable");
        }
        s.check(fx.getParameters().size() + 13 == inst.getParameters().size(), "both plug-ins got the same two parameters (the instrument still adds thirteen)");
    }
    // 0 %: the null test. Every render with the roller at 0 % (Classic and Modern) is bit for bit
    // the render with the stage never called (the code path as before Session 15).
    {
        const auto before = nullRenders([](DungeonCrawlerProcessor& p) { p.setDungeonSynthEnabledForTest(false); });
        const auto atZero = nullRenders();
        const auto atZeroModern = nullRenders([](DungeonCrawlerProcessor& p) {
            auto* era = p.parameters.getParameter(dc::ids::dungeonSynthEra);
            era->setValueNotifyingHost(era->convertTo0to1(1.0f));
        });
        for (std::size_t k = 0; k < before.size(); ++k) {
            const bool same = before[k].l == atZero[k].l && before[k].r == atZero[k].r && before[k].l == atZeroModern[k].l && before[k].r == atZeroModern[k].r;
            s.check(same, "0 %: " + before[k].name + " is bit-identical to the sound before (null test)", std::to_string(before[k].l.size()) + " samples");
        }
    }
    // Moved and brought back to 0 %: the stage rests once its hall has rung out, and the FX's dry
    // path is then the input exactly again.
    {
        Host h;
        h.go(dc::Dungeon, 0);
        h.set(dc::ids::intensity, 0.0f);
        h.set(dc::ids::dungeonSynth, 1.0f);
        h.render(2.0, effectstests::material());
        h.set(dc::ids::dungeonSynth, 0.0f);
        double waited = 0;
        while (h.p->dungeonSynthStage().active() && waited < 20.0) { h.render(0.25, effectstests::material()); waited += 0.25; }
        const auto from = h.end();
        h.render(1.0, effectstests::material());
        const auto latency = std::size_t(h.p->getLatencySamples());
        bool exact = !h.p->dungeonSynthStage().active();
        for (std::size_t i = from; i < h.end(); ++i) exact = exact && h.outL[i] == h.inL[i - latency];
        s.check(exact, "back at 0 %: the stage rests (" + fmt(waited, 2) + " s after) and the dry path is the input exactly");
    }
    // Sessions and presets.
    {
        DungeonCrawlerProcessor fx, inst(PluginKind::Instrument);
        for (auto* p : { &fx, &inst }) {
            auto* amount = p->parameters.getParameter(dc::ids::dungeonSynth);
            auto* era = p->parameters.getParameter(dc::ids::dungeonSynthEra);
            amount->setValueNotifyingHost(amount->convertTo0to1(0.6f));
            era->setValueNotifyingHost(era->convertTo0to1(1.0f));
            juce::MemoryBlock block;
            p->getStateInformation(block);
            DungeonCrawlerProcessor reopened(p->kind());
            reopened.setStateInformation(block.getData(), int(block.getSize()));
            s.check(std::abs(reopened.dungeonSynthAmount() - 0.6f) < 1.0e-3f && reopened.dungeonSynthEra() == dc::DungeonSynth::Modern,
                    kind(*p) + ": saved with the session (60 %, Modern)");
            // A session saved before Session 15 (neither parameter in it) opens at 0 %, Classic.
            auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), int(block.getSize()));
            for (auto* child = xml->getFirstChildElement(); child != nullptr;) {
                auto* next = child->getNextElement();
                const auto id = child->getStringAttribute("id");
                if (id == dc::ids::dungeonSynth || id == dc::ids::dungeonSynthEra) xml->removeChildElement(child, true);
                child = next;
            }
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary(*xml, old);
            reopened.setStateInformation(old.getData(), int(old.getSize()));
            s.check(reopened.dungeonSynthAmount() == 0.0f && reopened.dungeonSynthEra() == dc::DungeonSynth::Classic, kind(*p) + ": an older session opens at 0 % (as it sounded)");
            // A user preset brings it back; a factory preset without it plays at 0 %; Init: 0 %.
            juce::File preset = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("dc-ds-test.dcpreset");
            if (auto x = juce::AudioProcessor::getXmlFromBinary(block.getData(), int(block.getSize()))) x->writeTo(preset);
            p->applyFactoryPreset(0);
            s.check(p->dungeonSynthAmount() == 0.0f, kind(*p) + ": a factory preset without it plays at 0 %");
            p->loadUserPreset(preset);
            preset.deleteFile();
            s.check(std::abs(p->dungeonSynthAmount() - 0.6f) < 1.0e-3f && p->dungeonSynthEra() == dc::DungeonSynth::Modern, kind(*p) + ": a user preset brings it back");
            p->applyInit();
            s.check(p->dungeonSynthAmount() == 0.0f && p->dungeonSynthEra() == dc::DungeonSynth::Classic, kind(*p) + ": Init sets 0 %, Classic");
        }
    }
    // Session 16: strong enough. Each figure below fails the suite when the stage is too subtle
    // (the numbers: `--ds-strength`, DEVLOG Session 16). The stage alone first (no place, the
    // instrument's chorus weight 0 where the tape is measured), then in the places.
    s.section("dungeon synth: strong enough, Classic and Modern clearly apart (Session 16)");
    {
        const auto m = measureStrength();
        const auto list3 = [](const double (&v)[3], int decimals = 1) { return fmt(v[0], decimals) + " / " + fmt(v[1], decimals) + " / " + fmt(v[2], decimals); };
        s.check(m.rate44 >= 11000.0 && m.rate44 <= 16000.0 && m.rate48 >= 11000.0 && m.rate48 <= 16000.0, "Classic 100 %: the sampler runs at 11-16 kHz (44.1 and 48 kHz hosts)",
                fmt(m.rate44, 0) + " / " + fmt(m.rate48, 0) + " Hz");
        s.check(m.alias[0] > -15.0 && m.alias[1] < -40.0, "aliasing: a 9 kHz tone folds to 4 kHz in Classic (heard), not in Modern",
                "Classic " + fmt(m.alias[0]) + " dB, Modern " + fmt(m.alias[1]) + " dB of the output");
        s.check(m.harmonics[1][2] > -25.0 && m.harmonics[1][1] > -33.0 && m.harmonics[1][2] > m.harmonics[0][2] + 6.0 && m.harmonics[1][0] < m.harmonics[1][2] - 8.0,
                "saturation (a -10 dBFS sine, harmonics 2-5): Modern rich, far more than Classic, subtle at 25 %",
                "Classic " + list3(m.harmonics[0]) + " | Modern " + list3(m.harmonics[1]) + " dB at 25 / 50 / 100 %");
        s.check(m.noise[0][2] > -50.0 && m.noise[0][2] < -40.0 && m.noise[0][1] < m.noise[0][2] - 6.0 && m.noise[0][0] < -60.0 && m.noise[1][2] < -60.0,
                "tape hiss: about -45 dBFS at Classic 100 %, much less at 50 and 25 %, nearly none in Modern",
                "Classic " + list3(m.noise[0]) + " | Modern " + list3(m.noise[1]) + " dBFS");
        const auto& cw = m.wow[0][2];
        const auto& mw = m.wow[1][2];
        s.check(cw.wowPeakPercent >= 0.3 && cw.wowPeakPercent <= 0.8 && cw.wow >= 4.0 && cw.flutter >= 0.8,
                "Classic 100 %: cassette wow (0.3-0.8 % deep, slow) and flutter on a held 1 kHz tone",
                "wow " + fmt(cw.wow, 2) + " cents RMS (" + fmt(cw.wowPeakPercent, 3) + " % peak), flutter " + fmt(cw.flutter, 2) + " cents RMS");
        s.check(mw.wow >= 0.8 && mw.wow <= 3.0 && mw.wow < 0.5 * cw.wow && mw.flutter < 0.5 * cw.flutter && m.wow[0][0].wow < 0.35 * cw.wow,
                "Modern 100 %: a slight slow wow (far less than Classic); Classic 25 % only a hint",
                "Modern wow " + fmt(mw.wow, 2) + ", flutter " + fmt(mw.flutter, 2) + "; Classic 25 % wow " + fmt(m.wow[0][0].wow, 2) + " cents RMS");
        s.check(m.t60[0][2] >= 3.0 && m.t60[1][2] >= 7.0 && m.t60[1][2] >= m.t60[0][2] + 3.0 && m.t60[0][1] < m.t60[0][2] && m.t60[1][1] < m.t60[1][2],
                "the hall: big in Classic, very long in Modern (T60), shorter towards 0 %",
                "Classic " + list3(m.t60[0], 2) + " | Modern " + list3(m.t60[1], 2) + " s");
        const double g[2][3] { { m.wet[0][0].gaps, m.wet[0][1].gaps, m.wet[0][2].gaps }, { m.wet[1][0].gaps, m.wet[1][1].gaps, m.wet[1][2].gaps } };
        for (int e : { 0, 1 })
            s.check(g[e][2] > -12.0 && g[e][1] > -19.0 && g[e][1] < -9.0 && g[e][0] < -18.0 && m.wet[std::size_t(e)][2].hall > -3.0,
                    std::string(e == 0 ? "Classic" : "Modern") + ": the hall at a high wet level (a chord 1 s on / 1 s off: the gaps against the playing; the hall against the input)",
                    "gaps " + list3(g[e]) + " dB at 25 / 50 / 100 %; hall " + fmt(m.wet[std::size_t(e)][2].hall) + " dB at 100 %");
        s.metric("Classic 100 %: wow (cents RMS)", cw.wow);
        s.metric("Modern 100 %: harmonics (dB)", m.harmonics[1][2]);
        s.metric("Classic 100 %: hiss (dBFS)", m.noise[0][2]);
        // In the places (five instruments in theirs, the FX in two): the spectrum moves, the image
        // narrows (Classic) or widens (Modern), the tail grows; Classic and Modern far apart.
        double meanC = 0, meanM = 0;
        for (const auto& p : m.places) {
            meanC += p.distance[0][2] / double(m.places.size());
            meanM += p.distance[1][2] / double(m.places.size());
            for (int e : { 0, 1 }) {
                const auto& d = p.distance[e];
                s.check(d[2] >= (e == 0 ? 2.0 : 1.2) && d[1] >= d[0] && d[2] >= d[1] && d[0] <= 0.5 * d[2],
                        p.name + ": " + (e == 0 ? "Classic" : "Modern") + " moves the spectrum, clearly at 100 %, subtly at 25 % (third-octave distance to 0 %)",
                        list3(d, 2) + " dB at 25 / 50 / 100 %");
            }
            const auto& wc = p.width[0];
            s.check(wc[2] <= -15.0 && wc[1] <= -3.0 && wc[1] >= -8.0 && wc[0] <= -1.0 && wc[0] >= -4.0, p.name + ": Classic narrows the image (nearly mono at 100 %)",
                    list3(wc) + " dB of side at 25 / 50 / 100 %");
            s.check(p.width[1][2] >= 1.5 && p.width[1][2] >= wc[2] + 18.0, p.name + ": Modern widens it (the ensemble), far from Classic's",
                    fmt(p.width[1][2]) + " dB (Classic " + fmt(wc[2]) + ")");
            s.check(p.cm >= 3.0, p.name + ": Classic and Modern at 100 % sound clearly different (third-octave distance)", fmt(p.cm, 2) + " dB");
        }
        s.check(meanC >= 5.0 && meanM >= 2.5, "on average the spectrum moves strongly at 100 % (Classic, Modern)", fmt(meanC, 2) + " / " + fmt(meanM, 2) + " dB");
        for (const auto& p : m.places) {
            if (p.name.rfind("FX", 0) == 0 || p.name.rfind(instName(dc::inst::designFor(dc::Necropolis, 0)), 0) == 0) continue;   // the pad's place rings longer than any hall
            s.check(p.tail[0] >= 2.5 && p.tail[1] >= 1.5, p.name + ": the hall's tail after the melody (Classic, Modern 100 %)", fmt(p.tail[0]) + " / " + fmt(p.tail[1]) + " dB");
        }
        // The top: bright material dry (three instruments, the FX's dry path).
        for (const auto& b : m.bright) {
            s.check(b.top12[0][2] <= -10.0 && b.top12[0][1] <= -7.0 && b.top8[0][2] <= -12.0 && b.top8[0][1] <= -6.0,
                    b.name + ": Classic rolls off the top (the cassette's 5.5 kHz)",
                    "above 8 kHz " + list3(b.top8[0]) + ", above 12 kHz " + list3(b.top12[0]) + " dB at 25 / 50 / 100 %");
            s.check(b.top12[1][2] <= -5.5 && b.top12[1][1] <= -3.5, b.name + ": Modern softens the top (10 kHz)", "above 12 kHz " + list3(b.top12[1]) + " dB at 25 / 50 / 100 %");
        }
    }
    {
        // Modern's ensemble: a mono pad gains width, more on strings than on the war drums.
        const auto chorus = [](int instrument, Ds d) {
            InstHost h(48000, 512, 2);
            h.play(instrument);
            h.dry();
            setDs(h, d);
            h.on(0.05, 57, 0.7f);
            h.render(3.0);
            return widthDb({ h.outL, h.outR }, h.idx(1.0), h.outL.size());
        };
        const int strings = dc::inst::designFor(dc::Castle, 2);
        const double sw0 = chorus(strings, { 0, 1 }), sw1 = chorus(strings, { 1, 1 });
        s.check(sw1 > sw0 + 1.0, "Modern: the ensemble chorus widens the strings", fmt(sw0) + " -> " + fmt(sw1) + " dB of side");
        s.check(dc::inst::dungeonSynthChorus(strings) > dc::inst::dungeonSynthChorus(dc::inst::designFor(dc::Mountains, 1)) + 0.5f,
                "the chorus belongs on pads and strings (the war drums take almost none)");
    }
    // The AU host harnesses' DUNGEON SYNTH checks (tools/au-harness, in and out of process on
    // macOS), measured the same way here so a change that would fail them fails first here.
    {
        const auto fxRun = [](Ds d) {
            Host h;
            h.set(dc::ids::intensity, 0.0f);
            setDs(h, d);
            h.render(4.0, sine(440.0, 0.25f));
            effectstests::Take t { h.outL, h.outR };
            return std::make_tuple(h.residualDb(h.at(2.0), h.end()), widthDb(t, h.at(2.0), h.end()), toDb(h.rms(h.at(2.0), h.end())));
        };
        const auto [r0, w0, l0] = fxRun({ 0, 0 });
        const auto [rc, wc, lc] = fxRun({ 1, 0 });
        const auto [rm, wm, lm] = fxRun({ 1, 1 });
        s.check(r0 < -40.0 && rc > -10.0 && rm > -14.0 && wm > wc + 10.0 && std::abs(lc - l0) < 3.0 && std::abs(lm - l0) < 3.0,
                "the FX harness's figures: 0 % passes, Classic and Modern change a sine strongly, Modern widens it, the level stays",
                "residual " + fmt(r0) + " / " + fmt(rc) + " / " + fmt(rm) + " dB, side against mid Classic " + fmt(wc) + ", Modern " + fmt(wm) + " dB, level "
                    + fmt(l0) + " / " + fmt(lc) + " / " + fmt(lm) + " dBFS");
        const auto chord = [](Ds d) {
            InstHost h;
            setDs(h, d);
            h.render(0.3);
            for (int key : { 57, 64, 69 }) h.on(0.3, key, 100.0f / 127.0f);
            h.render(3.0);
            effectstests::Take t { h.outL, h.outR };
            return std::make_pair(widthDb(t, h.idx(1.3), h.outL.size()), effectstests::levelDb(t.l, t.r, h.idx(0.35), h.idx(0.9)));
        };
        const auto [iw0, il0] = chord({ 0, 0 });
        const auto [iwc, ilc] = chord({ 1, 0 });
        const auto [iwm, ilm] = chord({ 1, 1 });
        s.check(iwc < iw0 - 8.0 && iwm > iwc + 8.0 && std::abs(ilc - il0) < 3.0 && std::abs(ilm - il0) < 3.0,
                "the instrument harness's figures: a chord's image folds to mono in Classic, stays wide in Modern, its attack's level stays",
                "side against mid " + fmt(iw0) + " / " + fmt(iwc) + " / " + fmt(iwm) + " dB, level " + fmt(il0) + " / " + fmt(ilc) + " / " + fmt(ilm) + " dB");
    }
    const std::vector<Ds> halfAndFull { { 0.25f, 0 }, { 0.5f, 0 }, { 1, 0 }, { 0.25f, 1 }, { 0.5f, 1 }, { 1, 1 } };
    // Level-matched: the loudest 400 ms (K-weighted) within 1.5 dB of 0 % at 25, 50 and 100 %.
    {
        double worst = 0;
        for (int instrument : dsInstruments()) {
            const auto base = instMelody(instrument, { 0, 0 });
            const double l0 = loudness(base.l, base.r, 48000);
            for (const auto& d : halfAndFull) {
                const auto t = instMelody(instrument, d);
                const double dev = loudness(t.l, t.r, 48000) - l0;
                worst = std::max(worst, std::abs(dev));
                s.check(std::abs(dev) <= 1.5, instName(instrument) + " in its place: " + dsName(d) + " level-matched (within 1.5 dB of 0 %)", fmt(dev, 2) + " dB");
            }
        }
        for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Dungeon, 0 }, { dc::Cathedral, 2 }, { dc::Necropolis, 0 }, { dc::Mountains, 3 } }) {
            const auto base = fxMaterial(l, k, { 0, 0 });
            const double l0 = loudness(base.l, base.r, 48000);
            for (const auto& d : halfAndFull) {
                const auto t = fxMaterial(l, k, d);
                const double dev = loudness(t.l, t.r, 48000) - l0;
                worst = std::max(worst, std::abs(dev));
                s.check(std::abs(dev) <= 1.5, "FX " + placeName(l, k) + ": " + dsName(d) + " level-matched (within 1.5 dB of 0 %)", fmt(dev, 2) + " dB");
            }
        }
        s.metric("worst loudness deviation at 25 / 50 / 100 % (both Eras)", worst, " dB");
    }
    // Click-free: a held sine in the FX while the roller sweeps 0 -> 100 -> 0 % (a step every block),
    // jumps 0 -> 100 %, and the Era flips back and forth at 100 %.
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Cathedral, 2 }, { dc::Dungeon, 0 } }) {
        const auto steady = [&](Ds d) {
            Host h;
            h.go(l, k);
            setDs(h, d);
            h.render(3.0, sine(220.0, 0.25f));
            return h.maxStep(h.at(2.0), h.end());
        };
        const double reference = std::max({ steady({ 0, 0 }), steady({ 1, 0 }), steady({ 1, 1 }) });
        Host h;
        h.go(l, k);
        h.render(2.0, sine(220.0, 0.25f));
        const auto from = h.end();
        const int blocks = int(3.0 * 48000 / 512);
        for (int b = 0; b < blocks; ++b) {
            const float t = float(b) / float(blocks);
            h.set(dc::ids::dungeonSynth, t < 0.5f ? 2.0f * t : 2.0f - 2.0f * t);
            h.render(512.0 / 48000.0, sine(220.0, 0.25f));
        }
        h.set(dc::ids::dungeonSynth, 1.0f);   // a jump
        h.render(1.0, sine(220.0, 0.25f));
        for (int flip = 0; flip < 4; ++flip) {
            h.set(dc::ids::dungeonSynthEra, float((flip + 1) % 2));
            h.render(0.5, sine(220.0, 0.25f));
        }
        h.set(dc::ids::dungeonSynth, 0.0f);   // and back
        h.render(1.0, sine(220.0, 0.25f));
        const double during = h.maxStep(from, h.end());
        s.check(h.finite() && during < 2.0 * reference, "FX " + placeName(l, k) + ": sweeping, jumping and switching the Era are click-free",
                fmt(during / reference, 2) + "x the largest steady step at 0 / 100 %");
    }
    {
        // A held string chord in the instrument while the roller sweeps and the Era flips.
        const int strings = dc::inst::designFor(dc::Castle, 2);
        const auto run = [&](float fixed, int era) {
            InstHost h;
            h.play(strings);
            setDs(h, { std::max(0.0f, fixed), era });
            h.on(0.1, 57, 0.7f);
            h.on(0.1, 64, 0.6f);
            h.render(3.0);
            const int blocks = int(4.0 * 48000 / 512);
            for (int b = 0; b < blocks; ++b) {
                const float t = float(b) / float(blocks);
                if (fixed < 0.0f) {
                    h.set(dc::ids::dungeonSynth, t < 0.5f ? 2.0f * t : 2.0f - 2.0f * t);
                    if (b == blocks / 2) h.set(dc::ids::dungeonSynthEra, 1.0f);
                }
                h.render(512.0 / 48000.0);
            }
            return maxStepOf(h.outL, h.outR, h.idx(3.0), h.outL.size());
        };
        const double reference = std::max({ run(0.0f, 0), run(1.0f, 0), run(1.0f, 1) }), during = run(-1.0f, 0);
        s.check(during < 1.5 * reference, "instrument: a held string chord while the roller sweeps and the Era switches is click-free",
                fmt(during / reference, 2) + "x the largest steady step");
    }
    // The place still matters: one instrument (locked) at 100 % in the Crypt, the Necropolis and
    // the Dwarven Hall stays as different from place to place as most of what it was at 0 %.
    {
        const int choir = dc::inst::designFor(dc::Cathedral, 0);
        const std::vector<std::pair<int, int>> places { { dc::Cathedral, 2 }, { dc::Necropolis, 0 }, { dc::Mountains, 3 } };
        for (int era : { 0, 1 }) {
            std::vector<effectstests::Take> at0, at100;
            for (const auto& [l, k] : places)
                for (float a : { 0.0f, 1.0f }) {
                    InstHost h;
                    h.play(choir, false);
                    h.go(l, k);
                    setDs(h, { a, era });
                    h.render(0.05);
                    melody(h, 0.1);
                    h.render(0.05 + melodySeconds + 2.0);
                    (a == 0.0f ? at0 : at100).push_back({ h.outL, h.outR });
                }
            for (std::size_t i = 0; i < places.size(); ++i)
                for (std::size_t j = i + 1; j < places.size(); ++j) {
                    const double d0 = effectstests::effectShare(at0[i].l, at0[i].r, at0[j].l, at0[j].r, 0, at0[i].l.size());
                    const double d1 = effectstests::effectShare(at100[i].l, at100[i].r, at100[j].l, at100[j].r, 0, at100[i].l.size());
                    s.check(d1 > 1.0 && d1 > 0.5 * d0, std::string(era == 0 ? "Classic" : "Modern") + " 100 %: " + placeName(places[i].first, places[i].second) + " and "
                                + placeName(places[j].first, places[j].second) + " still sound different", fmt(d0) + " dB at 0 %, " + fmt(d1) + " dB at 100 %");
                }
        }
    }
    // It works with the rest: Lock instrument, Effects, the MIX section's Reverb, Punch, Bypass.
    {
        const int lute = dc::inst::designFor(dc::Castle, 0);
        InstHost h;
        h.play(lute, false);
        setDs(h, { 1, 0 });
        h.go(dc::Necropolis, 0);
        h.render(0.5);
        h.go(dc::Cathedral, 2);
        h.render(0.5);
        s.check(h.p->playingInstrument.load() == lute, "with it on, Lock instrument keeps the lute while travelling");
        const auto tailWith = [&](const juce::String& id, float value) {
            InstHost t;
            t.play(lute);
            setDs(t, { 1, 1 });
            if (id.isNotEmpty()) t.set(id, value);
            t.render(0.05);
            melody(t, 0.1);
            t.render(0.05 + melodySeconds + 3.0);
            return tailDb({ t.outL, t.outR });
        };
        const double full = tailWith({}, 0), noEffects = tailWith(dc::ids::effects, 0.0f), noReverb = tailWith(dc::ids::mixGroups[0], 0.0f);
        s.check(noEffects < full - 6.0, "Effects 0 % takes the place and the hall away (the stage stays on the dry instrument)", fmt(full) + " -> " + fmt(noEffects) + " dB");
        s.check(noReverb < full - 6.0, "the MIX section's Reverb turns the hall down with the place's reverbs", fmt(full) + " -> " + fmt(noReverb) + " dB");
        // Punch still shapes the hit (the brass, dry, with the stage on).
        const int horn = dc::inst::designFor(dc::Castle, 4);
        const auto hit = [&](float punch) {
            InstHost t;
            t.play(horn);
            t.dry();
            setDs(t, { 1, 0 });
            t.set(dc::ids::instPunch, punch);
            t.note(0.1, 1.2, 60, 0.95f);
            t.render(1.6);
            return toDb(t.peak(0.1, 0.2)) - toDb(t.rms(0.8, 1.2));
        };
        const double p0 = hit(0.0f), p2 = hit(2.0f);
        s.check(p2 > p0 + 3.0, "Punch still lands through it (0 % / 200 %: the hit over the held note)", fmt(p0) + " / " + fmt(p2) + " dB");
        // Bypass: the input, untouched, even at 100 %.
        Host b;
        b.go(dc::Cathedral, 2);
        setDs(b, { 1, 0 });
        b.set(dc::ids::bypass, 1.0f);
        b.render(2.0, effectstests::material());
        const auto latency = std::size_t(b.p->getLatencySamples());
        double diff = 0;
        for (std::size_t i = b.at(1.0); i < b.end(); ++i) diff = std::max(diff, std::abs(double(b.outL[i]) - b.inL[i - latency]));
        s.check(diff < 1.0e-6, "Bypass at 100 %: the input (within float rounding)", fmt(toDb(std::max(diff, 1.0e-12))) + " dBFS");
    }
    // No harsh digital noise: silence stays silence (no hiss without music), and after the melody
    // everything decays away.
    for (int era : { 0, 1 }) {
        InstHost h;
        h.play(dc::inst::designFor(dc::Necropolis, 0));
        setDs(h, { 1, era });
        h.render(2.0);
        const double silent = h.peak(0.0, 2.0);
        melody(h, 2.1);
        h.render(0.1 + melodySeconds + 25.0);
        const double last = h.peak(h.outL.size() / 48000.0 - 1.0, h.outL.size() / 48000.0);
        s.check(silent < 1.0e-6 && h.finite() && last < 3.0e-5, std::string(era == 0 ? "Classic" : "Modern") + " 100 %: silent without notes, and the tail dies away",
                fmt(toDb(std::max(silent, 1.0e-12))) + " dBFS before, " + fmt(toDb(std::max(last, 1.0e-12))) + " dBFS 25 s after");
    }
    // Real time: no allocation while it plays, sweeps and switches.
    {
        InstHost rt;
        rt.play(dc::inst::designFor(dc::Castle, 2));
        rt.render(0.2);
        std::size_t allocations = 0;
        for (int k = 0; k < 200; ++k) {
            if (k % 20 == 0) rt.on(rt.position / rt.sr + 0.001, 50 + k % 20, 0.7f);
            rt.set(dc::ids::dungeonSynth, float(k % 50) / 49.0f);
            if (k % 37 == 0) rt.set(dc::ids::dungeonSynthEra, float((k / 37) % 2));
            allocationProbe::calls = 0;
            allocationProbe::enabled = true;
            rt.processOne(256);
            allocationProbe::enabled = false;
            allocations += allocationProbe::calls;
        }
        s.check(allocations == 0, "no allocation in processBlock with it on, moving and switching", std::to_string(allocations));
    }
    // Mono layouts.
    {
        InstHost mono(48000, 512, 1);
        mono.play(dc::inst::designFor(dc::Castle, 2));
        setDs(mono, { 1, 1 });
        mono.note(0.05, 0.5, 57);
        mono.render(1.0);
        Host fxMono(48000, 512, 1, 1);
        setDs(fxMono, { 1, 0 });
        fxMono.render(1.0, effectstests::material());
        s.check(mono.finite() && mono.peak(0.1, 0.6) > 1.0e-3 && fxMono.finite() && fxMono.peak(0, fxMono.end()) > 1.0e-3, "mono outputs work with it on");
    }
    // The factory presets built around it.
    {
        for (bool instrument : { true, false }) {
            DungeonCrawlerProcessor p(instrument ? PluginKind::Instrument : PluginKind::Effect);
            const auto& presets = p.factoryPresetList();
            int found = 0;
            for (int k = 0; k < int(presets.size()); ++k) {
                if (juce::String(presets[std::size_t(k)].category) != "Dungeon synth") continue;
                ++found;
                p.applyFactoryPreset(k);
                s.check(p.dungeonSynthAmount() >= 0.85f && p.presetDisplayName() == juce::String(presets[std::size_t(k)].name),
                        std::string(instrument ? "instrument" : "FX") + " preset " + presets[std::size_t(k)].name + ": dungeon synth shown off (85 % or more), named, unmodified",
                        fmt(p.dungeonSynthAmount() * 100.0, 0) + " %");
            }
            s.check(found >= (instrument ? 8 : 3), std::string(instrument ? "instrument: 8 or more" : "FX: 3 or more") + " dungeon synth presets", std::to_string(found));
            const auto& index = dc::search::index(instrument);
            for (const char* query : { "dungeon synth", "lo-fi", "retro", "cassette", "lofi" }) {
                int presetsFound = 0;
                for (const auto& r : index.search(query))
                    if (index.entries()[std::size_t(r.entry)].kind == dc::search::Kind::Preset
                        && index.entries()[std::size_t(r.entry)].family == "Dungeon synth") ++presetsFound;
                s.check(presetsFound >= (instrument ? 3 : 1), std::string(instrument ? "instrument" : "FX") + ": searching \"" + query + "\" finds the dungeon synth presets",
                        std::to_string(presetsFound));
            }
        }
        // The presets play (their instruments sound, level within the instrument's own 0 % level).
        InstHost h;
        const auto& presets = h.p->factoryPresetList();
        for (int k = 0; k < int(presets.size()); ++k) {
            if (juce::String(presets[std::size_t(k)].category) != "Dungeon synth") continue;
            h.p->applyFactoryPreset(k);
            h.render(0.05);
            const double t = double(h.position) / h.sr;
            h.note(t + 0.01, 0.5, 60, 0.8f);
            h.render(0.8);
            s.check(h.finite() && h.peak(t, t + 0.8) > 3.0e-3 && h.peak(t, t + 0.8) < 0.98, std::string("preset ") + presets[std::size_t(k)].name + ": plays, clean");
            h.untilSilent(15.0);
        }
    }
}

// CPU (report-only, the cpu group): the stage at 100 % against 0 %.
void testDungeonSynthCpu(Suite& s) {
    using namespace dstests;
    for (float a : { 0.0f, 1.0f }) {
        Host h;
        h.go(dc::Cathedral, 2);
        setDs(h, { a, 1 });
        h.render(0.5, effectstests::material());
        const auto begin = std::chrono::steady_clock::now();
        h.render(10.0, effectstests::material());
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        s.metric(std::string("FX Crypt CPU with dungeon synth at ") + (a == 0.0f ? "0" : "100") + " % (% of real time)", seconds / 10.0 * 100.0, " %");
    }
}

// The control in both windows: attached, where it should be, overlapping nothing.
void testDungeonSynthUi(Suite& s) {
    using namespace windowtests;
    // Visible all the way up to the editor (the test windows are not on the desktop).
    const auto shown = [](juce::Component* c, juce::Component* top) {
        for (; c != nullptr && c != top; c = c->getParentComponent())
            if (!c->isVisible()) return false;
        return c == top;
    };
    s.section("dungeon synth: the control (Session 15)");
    for (bool instrument : { true, false }) {
        Window w(instrument);
        const std::string kind = instrument ? "instrument" : "FX";
        auto* control = instrument ? (w.editor->instrumentPanel() != nullptr ? &w.editor->instrumentPanel()->dungeonSynthControl() : nullptr) : &w.view().dungeonSynthForTest();
        if (!s.check(control != nullptr, kind + ": the DUNGEON SYNTH control exists")) continue;
        s.check(shown(control, w.editor), kind + ": it is shown");
        auto& roller = control->rollerForTest();
        roller.setValue(0.5, juce::sendNotificationSync);
        s.check(std::abs(w.p->dungeonSynthAmount() - 0.5f) < 1.0e-3f, kind + ": the roller sets the amount (50 %)");
        s.check(roller.readout() == "50 %", kind + ": the roller reads 50 %");
        control->modernButton().onClick();
        s.check(w.p->dungeonSynthEra() == dc::DungeonSynth::Modern && control->modernButton().getToggleState() && !control->classicButton().getToggleState(),
                kind + ": Modern clicked: the Era is Modern and shown");
        auto* era = w.p->parameters.getParameter(dc::ids::dungeonSynthEra);
        era->setValueNotifyingHost(era->convertTo0to1(0.0f));   // automation
        s.check(control->shownEra() == 0 && control->classicButton().getToggleState(), kind + ": automation of the Era moves the switch");
        auto* amount = w.p->parameters.getParameter(dc::ids::dungeonSynth);
        amount->setValueNotifyingHost(0.8f);
        s.check(std::abs(roller.getValue() - 0.8) < 1.0e-3, kind + ": automation of the amount moves the roller");
        // Inside the window, and clear of every other visible control.
        const auto bounds = w.editor->getLocalArea(control, control->getLocalBounds());
        bool clear = w.editor->getLocalBounds().contains(bounds);
        std::string overlapping;
        std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
            for (auto* child : c.getChildren()) {
                if (child == control || control->isParentOf(child) || child->isParentOf(control) || !shown(child, w.editor)) { if (child->isParentOf(control)) walk(*child); continue; }
                const bool leaf = dynamic_cast<juce::Button*>(child) != nullptr || dynamic_cast<juce::Slider*>(child) != nullptr || dynamic_cast<juce::ComboBox*>(child) != nullptr
                                  || dynamic_cast<juce::Label*>(child) != nullptr || child == (instrument ? &w.editor->instrumentPanel()->keyboardForTest() : nullptr);
                if (leaf) {
                    if (w.editor->getLocalArea(child, child->getLocalBounds()).intersects(bounds)) { clear = false; overlapping += child->getName().toStdString() + "/" + typeid(*child).name() + " "; }
                } else walk(*child);
            }
        };
        walk(*w.editor);
        s.check(clear, kind + ": it sits inside the window and overlaps no other control", overlapping);
    }
}

// `--ds-snapshots dir`: the control in the instrument window and panel, and the FX's controls.
int dsSnapshots(const juce::String& directory) {
    using namespace windowtests;
    juce::File dir(directory);
    dir.createDirectory();
    int written = 0;
    const auto save = [&](const juce::Image& image, const char* name) {
        auto file = dir.getChildFile(name);
        file.deleteFile();
        juce::FileOutputStream stream(file);
        if (stream.openedOk() && juce::PNGImageFormat().writeImageToStream(image, stream)) ++written;
    };
    {
        Window w(true);
        w.go(dc::Castle, 2);
        w.p->parameters.getParameter(dc::ids::dungeonSynth)->setValueNotifyingHost(0.65f);
        w.p->keyboardNote(57, 0.8f);
        w.p->keyboardNote(64, 0.8f);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        for (int k = 0; k < 8; ++k) w.p->processBlock(buffer, midi);
        if (auto* panel = w.editor->instrumentPanel()) panel->pollForTest();
        w.view().setAnimation(false);
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s15-instrument-window.png");
        if (auto* panel = w.editor->instrumentPanel()) {
            save(w.editor->createComponentSnapshot(panel->getBounds(), true, 2.0f), "s15-instrument-panel-classic-65.png");
            auto* era = w.p->parameters.getParameter(dc::ids::dungeonSynthEra);
            era->setValueNotifyingHost(era->convertTo0to1(1.0f));
            w.p->parameters.getParameter(dc::ids::dungeonSynth)->setValueNotifyingHost(1.0f);
            const auto& c = panel->dungeonSynthControl();
            save(w.editor->createComponentSnapshot(panel->getBounds().getX() + c.getX() - 120 > 0 ? juce::Rectangle<int>(panel->getX() + c.getX() - 120, panel->getY() + c.getY() - 6, c.getWidth() + 126, c.getHeight() + 10)
                                                                                                    : panel->getBounds(), true, 3.0f), "s15-instrument-control-modern-100.png");
        }
    }
    {
        Window w(false);
        w.go(dc::Necropolis, 0);
        w.p->parameters.getParameter(dc::ids::dungeonSynth)->setValueNotifyingHost(0.5f);
        auto* era = w.p->parameters.getParameter(dc::ids::dungeonSynthEra);
        era->setValueNotifyingHost(era->convertTo0to1(1.0f));
        w.view().setAnimation(false);
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s15-fx-window.png");
        save(w.editor->createComponentSnapshot(juce::Rectangle<int>(0, 560 - 150, 960, 150), true, 2.0f), "s15-fx-controls-modern-50.png");
    }
    std::cout << written << " snapshots written to " << dir.getFullPathName() << "\n";
    return written == 5 ? 0 : 1;
}

namespace dstests {
// The dark fantasy test melody for the listening render: the A minor phrase twice (the second
// time a little higher), over held chords on the same instrument (Am, F, G, E), 9.6 s.
void darkFantasyPhrase(InstHost& h, double t) {
    melody(h, t, 57, 0.8f);
    melody(h, t + 4.8, 60, 0.75f);
    const int chords[4][3] { { 45, 48, 52 }, { 41, 45, 48 }, { 43, 47, 50 }, { 40, 44, 47 } };
    for (int c = 0; c < 4; ++c)
        for (int k : chords[c]) h.note(t + c * 2.4, 2.3, k, 0.45f);
}
constexpr double phraseSeconds = 9.6;
}

// `--listen-dungeonsynth dir`: the dark fantasy test melody on five instruments in their places
// at 0 %, Classic 50 / 100 % and Modern 50 / 100 % (one file each, and an A/B file: 0 %, Classic
// 100 %, Modern 100 % back to back), the FX's guitar-and-synth material through the Crypt and the Gates
// the same way, and the numbers (dungeonsynth-report.txt is written by --ds-report).
int listenDungeonSynth(const juce::String& directory) {
    using namespace dstests;
    juce::File dir(directory);
    dir.createDirectory();
    const std::vector<std::pair<Ds, std::string>> settings { { { 0, 0 }, "0-off" }, { { 0.5f, 0 }, "classic-50" }, { { 1, 0 }, "classic-100" },
                                                             { { 0.5f, 1 }, "modern-50" }, { { 1, 1 }, "modern-100" } };
    const auto slug = [](std::string name) {
        for (auto& c : name) c = c == ' ' || c == '\'' ? '-' : char(std::tolower(static_cast<unsigned char>(c)));
        return name;
    };
    const auto append = [](std::vector<float>& all, const std::vector<float>& part) {
        all.insert(all.end(), part.begin(), part.end());
        all.insert(all.end(), std::size_t(0.6 * 48000), 0.0f);
    };
    for (int instrument : dsInstruments()) {
        // Session 16: also one A/B file each: 0 %, Classic 100 %, Modern 100 %, back to back.
        std::vector<float> al, ar;
        for (const auto& [d, label] : settings) {
            InstHost h;
            h.play(instrument);
            setDs(h, d);
            h.render(0.05);
            darkFantasyPhrase(h, 0.1);
            h.render(0.05 + phraseSeconds + 3.5);
            brasstests::writeWav(dir.getChildFile(slug(instName(instrument)) + "-" + label + ".wav"), h.outL, h.outR);
            if (d.amount == 0.0f || d.amount == 1.0f) { append(al, h.outL); append(ar, h.outR); }
        }
        brasstests::writeWav(dir.getChildFile(slug(instName(instrument)) + "-ab-0-classic100-modern100.wav"), al, ar);
    }
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Cathedral, 2 }, { dc::Necropolis, 0 } }) {
        std::vector<float> sl, sr;
        for (const auto& [d, label] : settings) {
            const auto t = fxMaterial(l, k, d, 8.0);
            append(sl, t.l);
            append(sr, t.r);
        }
        brasstests::writeWav(dir.getChildFile("fx-" + slug(dc::subLocationName(l, k)) + "-all-0-c50-c100-m50-m100.wav"), sl, sr);
    }
    return 0;
}
