// DUNGEON SYNTH Modern (Session 19: as obvious as Classic, warm instead of grimy; Session 18's
// file rewritten). Included by DungeonCrawlerTests.cpp after DungeonSynthTests.inc; group
// "dsmodern", CI shard 16-dsmodern; `--ds-modern` the numbers, `--listen-dsmodern dir` the render.
//
// Classic is unchanged, bit for bit: the stage is compared sample for sample with the stage as
// it was at the end of Session 17 (Tests/legacy, verbatim; Sessions 18 and 19 left Classic alone).
#include "legacy/DungeonSynthS17.cpp.inc"

namespace dsmodern {
using namespace dstests;

// One copy of the stage (now or Session 17's) on a signal, 64-sample blocks at `rate`, the
// controls (and optionally a sweep of the amount, the place) set every block as the processor
// does, the hiss before (as the processor adds it, before its place).
struct Plan {
    float amount = 1;
    int era = 0;
    float chorus = 0.6f, send = 1.0f;
    float decay = 3.0f, cutoff = 12000.0f, wet = 0.0f;
    bool sweep = false;   // the amount 0 -> 100 -> 0 % over the render, then held at `amount`
    bool hallOnly = false;
};
template <class Stage>
effectstests::Take runStage(const Plan& p, const Signal& signal, double seconds, double rate = 48000.0) {
    Stage ds;
    ds.setControls(p.sweep ? 0.0f : p.amount, p.era, p.chorus, p.send);
    ds.setPlace(p.decay, p.cutoff, p.wet);
    ds.prepare(rate);
    ds.setHallOnlyForTest(p.hallOnly);
    const auto n = std::size_t(seconds * rate);
    effectstests::Take t { std::vector<float>(n), std::vector<float>(n) };
    for (std::size_t i = 0; i < n; ++i) { t.l[i] = signal(0, (long long) i, rate); t.r[i] = signal(1, (long long) i, rate); }
    for (std::size_t i = 0; i < n; i += 64) {
        const int m = int(std::min<std::size_t>(64, n - i));
        float a = p.amount;
        if (p.sweep) {
            const double x = double(i) / double(n);
            a = x < 0.6 ? float(1.0 - std::abs(x / 0.3 - 1.0)) : p.amount;   // 0 -> 1 -> 0, then held
        }
        ds.setControls(a, p.era, p.chorus, p.send);
        ds.setPlace(p.decay, p.cutoff, p.wet);
        if (!p.hallOnly) ds.processHiss(t.l.data() + i, t.r.data() + i, m);
        ds.process(t.l.data() + i, t.r.data() + i, m);
    }
    return t;
}
bool identical(const effectstests::Take& a, const effectstests::Take& b) {
    return a.l.size() == b.l.size() && std::memcmp(a.l.data(), b.l.data(), a.l.size() * sizeof(float)) == 0
           && std::memcmp(a.r.data(), b.r.data(), a.r.size() * sizeof(float)) == 0;
}

// A bright, band-limited saw (1531 Hz, every harmonic up to 23 kHz, 1/k): the harshest thing
// a sampler meets. Its harmonics lie on a 1531 Hz grid that no sampling rate shares, so whatever
// the stage folds back or images up (and its noise) lands between them.
constexpr double f0 = 1531.0;
Signal brightSaw(float amplitude = 0.25f) {
    return [amplitude](int, long long n, double sr) {
        double v = 0;
        for (int k = 1; f0 * k < 23000.0; ++k) v += std::sin(2.0 * juce::MathConstants<double>::pi * f0 * k * double(n) / sr) / k;
        return float(amplitude * 0.55 * v);
    };
}
// Above `from` (8 kHz), what is not the saw's harmonics (+/-0.6 %: the slow wow moves them a
// little) against the whole output (dB): the aliasing, the images, the noise, the fizz.
double harshnessDb(const std::vector<float>& x, double from = 8000.0) {
    const auto p = spectrumOf(x, std::size_t(2 * 48000));
    const double all = bandPower(p, 20.0, 24000.0);
    double harmonic = 0;
    for (int k = 1; f0 * k < 24000.0; ++k)
        if (f0 * k * 1.006 > from) harmonic += bandPower(p, std::max(from, f0 * k * 0.994), f0 * k * 1.006);
    const double above = bandPower(p, from, 24000.0) - harmonic;
    return 10.0 * std::log10(std::max(above, 1.0e-30) / std::max(all, 1.0e-30));
}
// The low-mid's warmth: the 160-500 Hz bands against the 1-5 kHz bands (dB, third-octave bands).
double lowMidDb(const std::vector<double>& bands) {
    double low = 0, upper = 0;
    for (std::size_t b = 0; b < bands.size(); ++b) {
        const double fc = 63.0 * std::pow(2.0, double(b) / 3.0);
        if (fc >= 158.0 && fc <= 505.0) low += std::pow(10.0, bands[b] / 10.0);
        if (fc >= 990.0 && fc <= 5100.0) upper += std::pow(10.0, bands[b] / 10.0);
    }
    return 10.0 * std::log10(std::max(low, 1.0e-30) / std::max(upper, 1.0e-30));
}

// Session 19: how far two renders are apart, in spectrum AND in time (dB). Both are cut into short
// frames, each frame into bands from 63 Hz to 10 kHz; every frame-band's energy is taken against
// its render's whole energy (so a plain level change moves nothing), floored 50 dB under the
// loudest frame-band of the two. The distance is the RMS of the differences over the frame-bands
// either render fills. A different timbre counts (every frame's bands move), and so does what
// happens in time: a hall filling the gaps and lengthening the tails, the wet level, an ensemble or
// a wow moving the energy from frame to frame. `spectrogram(t, true)` (the check): 43 ms frames,
// twelfth-octave bands, each ear on its own (what each ear hears: Classic's image folding to mono
// counts, and so does Modern's ensemble, whose moving voices go into one ear and out of the other
// so that the level does not pump); fine enough to see pitch moving, the wow and the ensemble's
// detune, half of what each Era does. Reported beside it: the same with both ears' energy summed
// (`spectrogram(t)`), and `frameBands` (50 ms, third-octave bands, summed: smooths pitch away).
std::vector<std::vector<double>> frameBands(const effectstests::Take& t) {
    constexpr int bands = 23;
    const auto frame = std::size_t(0.05 * 48000);
    const std::size_t frames = t.l.size() / frame;
    std::vector<std::vector<double>> e(frames, std::vector<double>(bands, 0.0));
    double total = 0;
    for (int b = 0; b < bands; ++b) {
        const double fc = 63.0 * std::pow(2.0, b / 3.0);
        juce::IIRFilter f[4];
        for (auto& x : f) x.setCoefficients(juce::IIRCoefficients::makeBandPass(48000.0, fc, 4.32));
        for (std::size_t i = 0; i < frames * frame; ++i) {
            const double a = f[1].processSingleSampleRaw(f[0].processSingleSampleRaw(t.l[i]));
            const double c = f[3].processSingleSampleRaw(f[2].processSingleSampleRaw(t.r[i]));
            e[i / frame][std::size_t(b)] += a * a + c * c;
            total += a * a + c * c;
        }
    }
    for (auto& row : e)
        for (auto& v : row) v = 10.0 * std::log10(std::max(v, 1.0e-30) / std::max(total, 1.0e-30));
    return e;
}
// The same at a finer grain: a spectrogram (2048-point Hann FFT, 2048-sample hops, about 43 ms),
// its bins grouped into twelfth-octave bands from 63 Hz to 10 kHz (88 bands, L + R).
std::vector<std::vector<double>> spectrogram(const effectstests::Take& t, bool perEar = false) {
    constexpr int order = 11, size = 1 << order, bands = 88;
    juce::dsp::FFT fft(order);
    std::vector<float> buf(2 * size);
    std::vector<std::vector<double>> e;
    double total = 0;
    for (std::size_t at = 0; at + size <= t.l.size(); at += size) {
        std::vector<double> row(perEar ? 2 * bands : bands, 0.0);
        int ear = 0;
        for (const auto* x : { &t.l, &t.r }) {
            const std::size_t offset = perEar && ear++ == 1 ? std::size_t(bands) : 0;
            std::fill(buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < size; ++i) buf[std::size_t(i)] = (*x)[at + std::size_t(i)] * float(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * i / size));
            fft.performFrequencyOnlyForwardTransform(buf.data());
            for (int k = 1; k < size / 2; ++k) {
                const double hz = k * 48000.0 / size;
                const int b = int(std::floor(12.0 * std::log2(hz / 63.0)));
                if (b < 0 || b >= bands) continue;
                const double p = double(buf[std::size_t(k)]) * buf[std::size_t(k)];
                row[offset + std::size_t(b)] += p;
                total += p;
            }
        }
        e.push_back(row);
    }
    for (auto& row : e)
        for (auto& v : row) v = 10.0 * std::log10(std::max(v, 1.0e-30) / std::max(total, 1.0e-30));
    return e;
}
double stDistance(const std::vector<std::vector<double>>& a, const std::vector<std::vector<double>>& b) {
    const std::size_t frames = std::min(a.size(), b.size());
    double top = -300;
    for (std::size_t f = 0; f < frames; ++f)
        for (std::size_t k = 0; k < a[f].size(); ++k) top = std::max({ top, a[f][k], b[f][k] });
    const double floor = top - 50.0;
    double sum = 0;
    std::size_t n = 0;
    for (std::size_t f = 0; f < frames; ++f)
        for (std::size_t k = 0; k < a[f].size(); ++k) {
            if (std::max(a[f][k], b[f][k]) < floor) continue;
            const double d = std::max(a[f][k], floor) - std::max(b[f][k], floor);
            sum += d * d;
            ++n;
        }
    return n > 0 ? std::sqrt(sum / double(n)) : 0.0;
}

// Everything the Session 19 checks and `--ds-modern` measure.
constexpr int steps = 4;   // 25 / 50 / 75 / 100 %
constexpr float amounts[steps] { 0.25f, 0.5f, 0.75f, 1.0f };
struct Modern {
    double harsh[steps] {}, harshClassic = 0;          // the bright saw, Modern; Classic 100 %
    double harmonics[steps] {}, harmonicsClassic = 0;  // harmonics 2-5 of a -10 dBFS 200 Hz sine
    double even = 0, odd = 0;                          // Modern 100 %: harmonics 2 + 4 and 3 + 5 (dB against the fundamental)
    double rate44 = 0, rate48 = 0, rate96 = 0;
    struct Take {
        std::string name;
        double ears[steps] {}, earsClassic = 0;        // spectro-temporal distance to 0 % (spectrogram, each ear: the check)
        double fine[steps] {}, fineClassic = 0;        // the same, both ears summed
        double st[steps] {}, stClassic = 0;            // the same with third-octave bands
        double spec[steps] {}, specClassic = 0;        // the third-octave (whole-render) distance
        double top8[steps] {}, lowMid[steps] {}, width[steps] {}, tail[steps] {}, loud[steps] {}, loudClassic = 0;
        bool inPlace = true;
    };
    std::vector<Take> takes;
};
Modern measure(bool quick = false) {
    Modern m;
    for (int k = 0; k < steps; ++k) {
        Plan p;
        p.amount = amounts[k];
        p.era = 1;
        p.chorus = 0.0f;
        p.send = 0.0f;
        m.harsh[k] = harshnessDb(runStage<dc::DungeonSynth>(p, brightSaw(), 4.0).l);
        const auto tone = runStage<dc::DungeonSynth>(p, sine(200.0, 0.316f), 4.0);
        m.harmonics[k] = harmonicsFft(tone.l, 200.0, 48000);
        if (k == steps - 1) {
            const auto sp = spectrumOf(tone.l, 48000);
            const double f1 = std::max(bandPower(sp, 194.0, 206.0), 1.0e-30);
            m.even = 10.0 * std::log10(std::max(bandPower(sp, 388.0, 412.0) + bandPower(sp, 776.0, 824.0), 1.0e-30) / f1);
            m.odd = 10.0 * std::log10(std::max(bandPower(sp, 582.0, 618.0) + bandPower(sp, 970.0, 1030.0), 1.0e-30) / f1);
        }
    }
    {
        Plan p;
        p.amount = 1;
        p.era = 0;
        p.chorus = 0.0f;
        p.send = 0.0f;
        m.harshClassic = harshnessDb(runStage<dc::DungeonSynth>(p, brightSaw(), 4.0).l);
        m.harmonicsClassic = harmonicsFft(runStage<dc::DungeonSynth>(p, sine(200.0, 0.316f), 4.0).l, 200.0, 48000);
    }
    for (double rate : { 44100.0, 48000.0, 96000.0 }) {
        dc::DungeonSynth ds;
        ds.setControls(1.0f, 1, 0.0f, 0.0f);
        ds.prepare(rate);
        (rate < 46000.0 ? m.rate44 : rate < 50000.0 ? m.rate48 : m.rate96) = ds.samplerRate();
    }
    const auto take = [&](const std::string& name, const std::function<effectstests::Take(Ds)>& render, bool inPlace) {
        Modern::Take t;
        t.name = name;
        t.inPlace = inPlace;
        const auto base = render({ 0, 1 });
        const auto b0 = bandsDb(base.l, base.r);
        const auto frames0 = frameBands(base);
        const auto gram0 = spectrogram(base), ears0 = spectrogram(base, true);
        const double l0 = loudness(base.l, base.r, 48000);
        for (int k = 0; k < steps; ++k) {
            const auto r = render({ amounts[k], 1 });
            const auto b = bandsDb(r.l, r.r);
            t.st[k] = stDistance(frames0, frameBands(r));
            t.fine[k] = stDistance(gram0, spectrogram(r));
            t.ears[k] = stDistance(ears0, spectrogram(r, true));
            t.spec[k] = spectralDistance(b0, b);
            t.top8[k] = topDb(b, 8000) - topDb(b0, 8000);
            t.lowMid[k] = lowMidDb(b) - lowMidDb(b0);
            t.width[k] = widthDb(r) - widthDb(base);
            t.tail[k] = inPlace ? tailDb(r) - tailDb(base) : 0.0;
            t.loud[k] = loudness(r.l, r.r, 48000) - l0;
        }
        const auto c = render({ 1, 0 });
        t.stClassic = stDistance(frames0, frameBands(c));
        t.fineClassic = stDistance(gram0, spectrogram(c));
        t.earsClassic = stDistance(ears0, spectrogram(c, true));
        t.specClassic = spectralDistance(b0, bandsDb(c.l, c.r));
        t.loudClassic = loudness(c.l, c.r, 48000) - l0;
        m.takes.push_back(t);
    };
    for (int instrument : dsInstruments()) take(instName(instrument) + " in its place", [instrument](Ds d) { return instMelody(instrument, d); }, true);
    if (quick) return m;
    // Bright material dry (the Wooden Flute's synthesis, as the Session 16 checks), the FX in two
    // places and on its dry path.
    for (int instrument : brightInstruments())
        take(instName(instrument) + " dry", [instrument](Ds d) {
            return instMelody(instrument, d, 3.0, false, instrument == dc::inst::designFor(dc::HauntedForest, 0) ? int(dc::inst::SoundSeated) : int(dc::inst::SoundLatest));
        }, false);
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Cathedral, 2 }, { dc::Necropolis, 0 } })
        take("FX " + placeName(l, k), [l = l, k = k](Ds d) { return fxMaterial(l, k, d); }, false);
    take("FX dry path", [](Ds d) { return fxDry(d); }, false);
    return m;
}
std::string list4(const double (&v)[steps], int decimals = 1) {
    return fmt(v[0], decimals) + " / " + fmt(v[1], decimals) + " / " + fmt(v[2], decimals) + " / " + fmt(v[3], decimals);
}
} // namespace dsmodern

// `--ds-modern`: the numbers behind the Session 19 checks.
int dsModern() {
    using namespace dsmodern;
    if (std::getenv("DS_REED")) {   // TEMP
        const auto take = [&](float amount) {
            InstHost h;
            h.play(27);
            h.set(dc::ids::dungeonSynth, amount);
            h.set(dc::ids::dungeonSynthEra, 1.0f);
            h.render(0.05);
            melody(h, 0.1, 45);
            h.render(0.05 + melodySeconds + 1.5);
            return effectstests::Take { h.outL, h.outR };
        };
        const auto b = take(0);
        for (float a : { 0.5f, 1.0f }) {
            const auto t = take(a);
            std::cout << a << ": loudness " << fmt(loudness(t.l, t.r, 48000) - loudness(b.l, b.r, 48000), 2) << " per 0.25 s:";
            for (std::size_t i = 0; i + 12000 <= t.l.size(); i += 12000) std::cout << " " << fmt(effectstests::levelDb(t.l, t.r, i, i + 12000) - effectstests::levelDb(b.l, b.r, i, i + 12000), 1);
            std::cout << "\n";
        }
        return 0;
    }
    const auto m = measure();
    std::cout << "Modern 100 %: the sampler at " << fmt(m.rate44, 0) << " / " << fmt(m.rate48, 0) << " / " << fmt(m.rate96, 0) << " Hz (44.1 / 48 / 96 kHz hosts), "
              << fmt(dc::modern::samplerBits, 1) << " bits; tape " << fmt(dc::modern::tapeDriveDb, 1) << " dB; low-pass " << fmt(dc::modern::lowPassHz, 0) << " Hz\n";
    std::cout << "bright saw, non-harmonic energy above 8 kHz (dB of the output): Modern 25 / 50 / 75 / 100 % " << list4(m.harsh) << "; Classic 100 % " << fmt(m.harshClassic) << "\n";
    std::cout << "harmonics 2-5 of a -10 dBFS 200 Hz sine: Modern " << list4(m.harmonics) << " dB (100 %: even " << fmt(m.even) << ", odd " << fmt(m.odd) << " dB); Classic 100 % " << fmt(m.harmonicsClassic) << " dB\n";
    std::cout << "Modern hall T60 25 / 50 / 100 %: " << fmt(hallT60(0.25f, 1), 2) << " / " << fmt(hallT60(0.5f, 1), 2) << " / " << fmt(hallT60(1.0f, 1), 2) << " s (Classic 100 %: "
              << fmt(hallT60(1.0f, 0), 2) << " s)\n";
    for (int e : { 0, 1 }) {
        const auto w = wetOf(1.0f, e), w50 = wetOf(0.5f, e), w25 = wetOf(0.25f, e);
        std::cout << (e == 0 ? "Classic" : "Modern") << " wet level (a chord 1 s on / 1 s off): gaps against the playing " << fmt(w25.gaps) << " / " << fmt(w50.gaps) << " / "
                  << fmt(w.gaps) << " dB at 25 / 50 / 100 %, the hall against the input " << fmt(w.hall) << " dB at 100 %\n";
    }
    for (int e : { 0, 1 }) {
        const auto held = stageRun(1.0f, e, 0.0f, 0.0f, sine(1000.0, 0.25f), 12.0);
        const auto w = wowFlutterOf(held.l, 48000, held.l.size());
        std::cout << (e == 0 ? "Classic" : "Modern") << " 100 % wow " << fmt(w.wow, 2) << " cents RMS (peak " << fmt(w.wowPeakPercent, 3) << " %), flutter " << fmt(w.flutter, 2) << "\n";
    }
    std::cout << "(25 / 50 / 75 / 100 % Modern against 0 %; Classic 100 % against 0 % in brackets)\n";
    for (const auto& t : m.takes)
        std::cout << t.name << ": spectro-temporal distance " << list4(t.ears, 2) << " dB [" << fmt(t.earsClassic, 2) << "], ears summed " << list4(t.fine, 2) << " dB ["
                  << fmt(t.fineClassic, 2) << "], third-octave " << list4(t.st, 2) << " dB [" << fmt(t.stClassic, 2) << "], spectral " << list4(t.spec, 2) << " dB ["
                  << fmt(t.specClassic, 2) << "], above 8 kHz " << list4(t.top8) << " dB, low-mid " << list4(t.lowMid) << " dB, width " << list4(t.width)
                  << " dB, tail " << list4(t.tail) << " dB, loudness " << list4(t.loud, 2) << " dB [" << fmt(t.loudClassic, 2) << "]\n";
    return 0;
}

// `--listen-dsmodern dir`: the dark fantasy melody on five instruments in their places, ONE file
// each: 4 s at Modern 0 %, 25 %, 50 %, 75 %, 100 %, then 4 s of Classic 100 % to compare (the same
// first 4 s of the phrase every time, each a fresh render through the processor; 15 ms fades at
// the joins, no gaps).
int listenDsModern(const juce::String& directory) {
    using namespace dsmodern;
    juce::File dir(directory);
    dir.createDirectory();
    const auto slug = [](std::string name) {
        for (auto& c : name) c = c == ' ' || c == '\'' ? '-' : char(std::tolower(static_cast<unsigned char>(c)));
        return name;
    };
    const std::vector<Ds> settings { { 0, 1 }, { 0.25f, 1 }, { 0.5f, 1 }, { 0.75f, 1 }, { 1, 1 }, { 1, 0 } };
    const auto segment = std::size_t(4.0 * 48000), fade = std::size_t(0.015 * 48000);
    for (int instrument : dsInstruments()) {
        std::vector<float> al, ar;
        for (const auto& d : settings) {
            InstHost h;
            h.play(instrument);
            setDs(h, d);
            h.render(0.05);
            darkFantasyPhrase(h, 0.1);
            h.render(0.05 + 4.0);
            for (std::size_t i = 0; i < segment; ++i) {
                const float g = float(std::min({ 1.0, double(i) / double(fade), double(segment - 1 - i) / double(fade) }));
                al.push_back(i < h.outL.size() ? h.outL[i] * g : 0.0f);
                ar.push_back(i < h.outR.size() ? h.outR[i] * g : 0.0f);
            }
        }
        brasstests::writeWav(dir.getChildFile(slug(instName(instrument)) + "-modern-0-25-50-75-100-then-classic-100.wav"), al, ar);
    }
    return 0;
}

void testDungeonSynthModern(Suite& s) {
    using namespace dsmodern;
    s.section("dungeon synth: Classic bit-exact with Session 17 (Sessions 18, 19)");
    {
        const auto material = effectstests::material();
        const std::vector<std::pair<std::string, Signal>> signals { { "the FX's material", material }, { "a 220 Hz sine", sine(220.0, 0.25f) },
                                                                    { "a pulsed saw chord", pulsedChord() }, { "a bright saw", brightSaw() } };
        std::vector<std::pair<std::string, Plan>> plans;
        for (float a : { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f }) {
            Plan p;
            p.amount = a;
            plans.push_back({ fmt(a * 100.0, 0) + " %", p });
        }
        {
            Plan p;
            p.sweep = true;
            p.amount = 0.8f;
            plans.push_back({ "swept 0 -> 100 -> 0 %, then 80 %", p });
        }
        {
            Plan p;
            p.chorus = 1.0f;
            p.send = 1.6f;
            p.decay = 9.0f;
            p.cutoff = 2500.0f;
            p.wet = 0.7f;
            plans.push_back({ "100 %, a long dark wet place, the chorus and the send up", p });
        }
        {
            Plan p;
            p.hallOnly = true;
            plans.push_back({ "100 %, the hall alone", p });
        }
        for (const auto& [plan, p] : plans) {
            bool same = true;
            std::string differs;
            for (const auto& [name, signal] : signals) {
                const bool ok = identical(runStage<dc::DungeonSynth>(p, signal, 5.0), runStage<dc::s17::DungeonSynth>(p, signal, 5.0));
                if (!ok) differs += name + "; ";
                same = same && ok;
            }
            s.check(same, "Classic " + plan + ": the stage is bit-identical to Session 17's", differs.empty() ? "4 signals, 5 s each" : "differs: " + differs);
        }
        Plan p44;
        p44.amount = 1.0f;
        s.check(identical(runStage<dc::DungeonSynth>(p44, material, 4.0, 44100.0), runStage<dc::s17::DungeonSynth>(p44, material, 4.0, 44100.0))
                    && identical(runStage<dc::DungeonSynth>(p44, material, 3.0, 96000.0), runStage<dc::s17::DungeonSynth>(p44, material, 3.0, 96000.0)),
                "Classic 100 % at 44.1 and 96 kHz: bit-identical to Session 17's");
    }
    s.section("dungeon synth: Modern as obvious as Classic, warm instead of grimy (Session 19)");
    const auto m = measure();
    // The sampler as asked: 22-26 kHz at 100 % whatever the host's rate, 10-11 bits.
    s.check(m.rate44 >= 22000.0 && m.rate44 <= 26000.0 && m.rate48 >= 22000.0 && m.rate48 <= 26000.0 && m.rate96 >= 22000.0 && m.rate96 <= 26000.0
                && dc::modern::samplerBits >= 10.0f && dc::modern::samplerBits <= 11.0f,
            "Modern 100 %: the vintage sampler at 22-26 kHz (44.1 / 48 / 96 kHz hosts) and 10-11 bits",
            fmt(m.rate44, 0) + " / " + fmt(m.rate48, 0) + " / " + fmt(m.rate96, 0) + " Hz, " + fmt(dc::modern::samplerBits, 1) + " bits");
    // No fizz, at any setting: above 8 kHz, what is not music stays far down on the harshest material.
    s.check(m.harsh[0] < -60.0 && m.harsh[1] < -60.0 && m.harsh[2] < -60.0 && m.harsh[3] < -60.0,
            "Modern: no fizz (on a bright saw, the non-harmonic energy above 8 kHz under -60 dB at 25 / 50 / 75 / 100 %)",
            list4(m.harsh) + " dB (Classic 100 %: " + fmt(m.harshClassic) + " dB)");
    // The tape clearly heard at 100 % (about Classic's amount of harmonics, but even), growing
    // with the roller, subtle at 25 %; even harmonics as strong as the odd ones (warm).
    s.check(m.harmonics[3] >= -32.0 && m.harmonics[3] <= -18.0 && m.harmonics[0] < m.harmonics[3] - 12.0 && m.harmonics[1] < m.harmonics[3] - 4.0,
            "Modern: warm tape saturation clearly there at 100 % (harmonics 2-5 of a -10 dBFS 200 Hz sine), less at 25 / 50 %",
            list4(m.harmonics) + " dB (Classic 100 %: " + fmt(m.harmonicsClassic) + " dB)");
    s.check(m.even >= m.odd - 3.0, "Modern 100 %: the tape's harmonics are warm (2nd and 4th about as strong as 3rd and 5th, or stronger)",
            "even " + fmt(m.even) + " dB, odd " + fmt(m.odd) + " dB");
    double meanTop = 0, meanWarm = 0, meanEars = 0, meanEarsClassic = 0, meanSum = 0, meanSumClassic = 0;
    int bright = 0;
    for (const auto& t : m.takes) {
        if (t.inPlace) {
            // At 100 % each instrument at least most of Classic's distance (the mean over the five,
            // below, at least Classic's), and the roller scales it: 25 % subtle (a third of 100 % at
            // most), 50 % part of the way, growing at every step.
            s.check(t.ears[3] >= 0.75 * t.earsClassic, t.name + ": Modern 100 % far from 0 %, near Classic 100 %'s distance or beyond (spectro-temporal, each ear)",
                    fmt(t.ears[3], 2) + " dB (Classic " + fmt(t.earsClassic, 2) + " dB, x" + fmt(t.ears[3] / std::max(t.earsClassic, 1.0e-9), 2) + "); ears summed " + fmt(t.fine[3], 2)
                        + " / " + fmt(t.fineClassic, 2) + ", third-octave " + fmt(t.st[3], 2) + " / " + fmt(t.stClassic, 2) + " dB");
            s.check(t.ears[0] < t.ears[1] && t.ears[1] < t.ears[2] && t.ears[2] < t.ears[3] && t.ears[0] <= 0.36 * t.ears[3] && t.ears[1] >= 0.3 * t.ears[3] && t.ears[1] <= 0.75 * t.ears[3],
                    t.name + ": one roller curve: 25 % subtle, 50 % part of the way, growing to 100 %", list4(t.ears, 2) + " dB at 25 / 50 / 75 / 100 %");
            meanEars += t.ears[3] / 5.0;
            meanEarsClassic += t.earsClassic / 5.0;
            meanSum += t.fine[3] / 5.0;
            meanSumClassic += t.fineClassic / 5.0;
            if (t.name.rfind(instName(dc::inst::designFor(dc::Necropolis, 0)), 0) != 0)   // the pad's place rings longer than any hall
                s.check(t.tail[3] >= 3.0, t.name + ": Modern 100 %: the hall's tail after the melody", fmt(t.tail[3]) + " dB");
        } else if (t.name.rfind("FX " + placeName(dc::Cathedral, 2), 0) != 0 && t.name.rfind("FX " + placeName(dc::Necropolis, 0), 0) != 0) {
            // (Bright material: three instruments dry and the FX's dry path; the places are dark.)
            meanTop += t.top8[3];
            ++bright;
            s.check(t.top8[3] <= -5.0 && t.top8[3] >= -30.0 && t.top8[0] > t.top8[3] + 3.0, t.name + ": Modern's smooth low-pass (above 8 kHz clearly lower at 100 %, gently at 25 %)",
                    "above 8 kHz " + list4(t.top8) + " dB");
        }
        meanWarm += t.lowMid[3] / double(m.takes.size());
        // (A pad that is already all low-mid, in a dark wet place, has the least to gain: at least
        // not thinner. Session 16's heavy tape made it 6 dB thinner.)
        s.check(t.lowMid[3] >= -0.5 && t.lowMid[2] >= -0.5, t.name + ": Modern adds low-mid warmth, never thinner (160-500 Hz against 1-5 kHz)", list4(t.lowMid) + " dB");
        s.check(t.width[3] >= 2.0 && t.width[1] > 0.3, t.name + ": Modern widens the image (the ensemble)", list4(t.width) + " dB of side");
        bool level = true;
        for (double v : t.loud) level = level && std::abs(v) <= 1.5;
        s.check(level, t.name + ": Modern level-matched (within 1.5 dB of 0 % at 25 / 50 / 75 / 100 %)", list4(t.loud, 2) + " dB");
    }
    meanTop /= std::max(1, bright);
    s.check(meanWarm >= 1.5, "Modern 100 %: on average a fuller low-mid", "+" + fmt(meanWarm) + " dB");
    // THE check: on the five instruments in their places, Modern 100 % is at least as far from 0 %
    // as Classic 100 % (the spectro-temporal distance, each ear, their mean).
    s.check(meanEars >= meanEarsClassic, "Modern 100 % is at least as obvious as Classic 100 % on the five instruments in their places (mean spectro-temporal distance from 0 %)",
            fmt(meanEars, 2) + " dB against Classic's " + fmt(meanEarsClassic, 2) + " dB (x" + fmt(meanEars / std::max(meanEarsClassic, 1.0e-9), 2) + "); both ears summed "
                + fmt(meanSum, 2) + " / " + fmt(meanSumClassic, 2) + " dB");
    s.metric("Modern 100 % against Classic 100 %: mean distance ratio (five instruments in their places)", meanEars / std::max(meanEarsClassic, 1.0e-9), "");
    s.metric("Modern 100 %: bright saw, non-harmonic energy above 8 kHz", m.harsh[3], " dB");
    s.metric("Modern 100 %: harmonics of a -10 dBFS sine", m.harmonics[3], " dB");
    s.metric("Modern 100 %: mean change above 8 kHz (bright material)", meanTop, " dB");
    // The hall at a wet level comparable to Classic's: the gaps against the playing (what is heard
    // as its wetness) within 1.5 dB of Classic's, and the hall alone at least Classic's (its longer
    // decay builds up more while the chord plays).
    {
        const auto wc = wetOf(1.0f, 0), wm = wetOf(1.0f, 1);
        s.check(std::abs(wm.gaps - wc.gaps) <= 1.5 && wm.hall >= wc.hall - 1.0,
                "Modern 100 %: the hall as wet as Classic's (a chord 1 s on / 1 s off: the gaps against the playing, the hall against the input)",
                "gaps " + fmt(wm.gaps) + " dB (Classic " + fmt(wc.gaps) + "), hall " + fmt(wm.hall) + " dB (Classic " + fmt(wc.hall) + ")");
    }
}
