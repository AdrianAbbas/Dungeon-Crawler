// Warmth pass: the warm character chain and the Grit (included by DungeonCrawlerTests.cpp,
// "warmth" group; `--warmth-renders dir`). The suite's older groups run with the warm chain
// unavailable (the engine exactly as before it); this group turns it on.

namespace warmtests {
using dstests::Ds;
using dstests::setDs;

// Makes the warm chain available (or not) for every processor made while it lives.
struct Available {
    bool was;
    explicit Available(bool on) : was(DungeonCrawlerProcessor::warmthAvailableForTests()) { DungeonCrawlerProcessor::setWarmthAvailableForTests(on); }
    ~Available() { DungeonCrawlerProcessor::setWarmthAvailableForTests(was); }
};

// The four instruments of the measured renders (pads and chords on the test phrase).
const std::vector<int>& instruments() {
    static const std::vector<int> v { dc::inst::designFor(dc::Castle, 2), dc::inst::designFor(dc::Necropolis, 0), dc::inst::designFor(dc::Castle, 3),
                                      dc::inst::designFor(dc::Castle, 0) };
    return v;   // Royal Strings, Dark Choir Pad, Chamber Organ, Lute
}

// The dark fantasy test phrase (the test MIDI: the A minor phrase twice over held Am-F-G-E chords,
// 9.6 s) on an instrument in its place, the tail after it 3.5 s (as `--listen-dungeonsynth`).
// The bus compressor's gain reduction, block by block, of the last phrase rendered (dB).
std::vector<float>& lastReduction() { static std::vector<float> v; return v; }
effectstests::Take phrase(int instrument, Ds d, bool warm = true, double tail = 3.5, float air = 0.15f) {
    Available available(warm);
    InstHost h;
    h.play(instrument);
    setDs(h, d);
    h.set(dc::ids::air, air);
    h.render(0.05);
    dstests::darkFantasyPhrase(h, 0.1);
    lastReduction().clear();
    for (double t = 0; t < 0.05 + dstests::phraseSeconds + tail; t += 512.0 / 48000.0) {
        h.render(512.0 / 48000.0);
        lastReduction().push_back(h.p->warmChainForTest().compressionDb());
    }
    return { h.outL, h.outR };
}
// The gain reduction's median and 95th percentile while it compresses (dB).
std::pair<double, double> reductionOf(std::vector<float> v) {
    v.erase(std::remove_if(v.begin(), v.end(), [](float x) { return x < 0.3f; }), v.end());
    if (v.empty()) return { 0.0, 0.0 };
    std::sort(v.begin(), v.end());
    return { v[v.size() / 2], v[std::min(v.size() - 1, std::size_t(0.95 * double(v.size())))] };
}

double correlation(const effectstests::Take& t) {
    double lr = 0, ll = 0, rr = 0;
    for (std::size_t i = 0; i < t.l.size(); ++i) {
        lr += double(t.l[i]) * t.r[i];
        ll += double(t.l[i]) * t.l[i];
        rr += double(t.r[i]) * t.r[i];
    }
    return ll > 0.0 && rr > 0.0 ? lr / std::sqrt(ll * rr) : 1.0;
}

// The averaged power spectrum (Welch: 8192-point Hann, half overlap), both sides summed.
struct Spectrum {
    std::vector<double> p;
    double bin = 48000.0 / 8192.0;
    double share(double lo, double hi) const {
        double band = 0, all = 0;
        for (std::size_t k = 0; k < p.size(); ++k) {
            const double hz = double(k) * bin;
            if (hz >= 10.0) all += p[k];
            if (hz >= lo && hz < hi) band += p[k];
        }
        return 10.0 * std::log10(std::max(band, 1.0e-30) / std::max(all, 1.0e-30));
    }
    double flatness(double lo, double hi) const {
        double logSum = 0, sum = 0;
        int count = 0;
        for (std::size_t k = 0; k < p.size(); ++k) {
            const double hz = double(k) * bin;
            if (hz < lo || hz > hi) continue;
            const double v = std::max(p[k], 1.0e-30);
            logSum += std::log(v);
            sum += v;
            ++count;
        }
        return count > 0 ? std::exp(logSum / count) / (sum / count) : 0.0;
    }
};
Spectrum spectrum(const effectstests::Take& t) {
    constexpr int order = 13, size = 1 << order;
    juce::dsp::FFT fft(order);
    Spectrum s;
    s.p.assign(size / 2 + 1, 0.0);
    std::vector<float> buf(2 * size);
    for (std::size_t at = 0; at + size <= t.l.size(); at += size / 2)
        for (const auto* x : { &t.l, &t.r }) {
            std::fill(buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < size; ++i) buf[std::size_t(i)] = (*x)[at + std::size_t(i)] * float(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * i / size));
            fft.performFrequencyOnlyForwardTransform(buf.data());
            for (int k = 0; k <= size / 2; ++k) s.p[std::size_t(k)] += double(buf[std::size_t(k)]) * buf[std::size_t(k)];
        }
    return s;
}

// ITU-R BS.1770 K-weighting (the shelf and the high-pass), at 48 kHz.
struct KWeight {
    juce::IIRFilter shelf, high;
    KWeight() {
        shelf.setCoefficients(juce::IIRCoefficients(1.53512485958697, -2.69169618940638, 1.19839281085285, 1.0, -1.69065929318241, 0.73248077421585));
        high.setCoefficients(juce::IIRCoefficients(1.0, -2.0, 1.0, 1.0, -1.99004745483398, 0.99007225036621));
    }
    float operator()(float x) { return high.processSingleSampleRaw(shelf.processSingleSampleRaw(x)); }
};
// Short-term (3 s, 0.1 s hop) or momentary (0.4 s) loudness blocks.
std::vector<double> blocks(const effectstests::Take& t, double seconds) {
    KWeight kl, kr;
    std::vector<double> wl(t.l.size()), wr(t.r.size());
    for (std::size_t i = 0; i < t.l.size(); ++i) { wl[i] = kl(t.l[i]); wr[i] = kr(t.r[i]); }
    const auto n = std::size_t(seconds * 48000), hop = std::size_t(0.1 * 48000);
    std::vector<double> out;
    for (std::size_t at = 0; at + n <= wl.size(); at += hop) {
        double z = 0;
        for (std::size_t i = at; i < at + n; ++i) z += wl[i] * wl[i] + wr[i] * wr[i];
        out.push_back(-0.691 + 10.0 * std::log10(std::max(z / double(n), 1.0e-30)));
    }
    return out;
}
double gatedMean(const std::vector<double>& v) {
    double sum = 0;
    int n = 0;
    for (double x : v) if (x > -70.0) { sum += std::pow(10.0, x / 10.0); ++n; }
    return n > 0 ? 10.0 * std::log10(sum / n) : -99.0;
}
double integrated(const effectstests::Take& t) {
    const auto b = blocks(t, 0.4);
    const double relative = gatedMean(b) - 10.0;
    double sum = 0;
    int n = 0;
    for (double x : b) if (x > -70.0 && x > relative) { sum += std::pow(10.0, x / 10.0); ++n; }
    return n > 0 ? 10.0 * std::log10(sum / n) : -99.0;
}
// EBU Tech 3342 loudness range (LU) and the short-term 10th percentile, over the first `seconds`.
std::pair<double, double> loudnessRange(const effectstests::Take& t, double seconds = 1.0e9) {
    const auto n = std::min(t.l.size(), std::size_t(seconds * 48000));
    const auto b = blocks({ std::vector<float>(t.l.begin(), t.l.begin() + long(n)), std::vector<float>(t.r.begin(), t.r.begin() + long(n)) }, 3.0);
    const double relative = gatedMean(b) - 20.0;
    std::vector<double> kept;
    for (double x : b) if (x > -70.0 && x > relative) kept.push_back(x);
    if (kept.empty()) return { 0.0, -99.0 };
    std::sort(kept.begin(), kept.end());
    const auto at = [&](double q) { return kept[std::min(kept.size() - 1, std::size_t(q * double(kept.size() - 1) + 0.5))]; };
    return { at(0.95) - at(0.10), at(0.10) };
}
// The true peak (dBTP): 4x oversampled through the plug-in's own linear-phase oversampler.
double truePeakDb(const effectstests::Take& t) {
    double peak = 0;
    for (const auto* x : { &t.l, &t.r }) {
        dc::Oversampler os;
        os.prepare(4);
        float hi[4];
        for (float v : *x) {
            os.up(v, hi);
            for (float h : hi) peak = std::max(peak, double(std::abs(h)));
        }
    }
    return 20.0 * std::log10(std::max(peak, 1.0e-12));
}
// Side against mid below 100 Hz (eighth-order low-pass; dB): how mono the bass is.
double bassSideDb(const effectstests::Take& t) {
    std::array<juce::IIRFilter, 4> mf, sf;
    for (auto* bank : { &mf, &sf }) for (auto& f : *bank) f.setCoefficients(juce::IIRCoefficients::makeLowPass(48000.0, 100.0));
    const auto run = [](std::array<juce::IIRFilter, 4>& bank, float x) { for (auto& f : bank) x = f.processSingleSampleRaw(x); return double(x); };
    double mm = 0, ss = 0;
    for (std::size_t i = 0; i < t.l.size(); ++i) {
        const double m = run(mf, 0.5f * (t.l[i] + t.r[i]));
        const double s = run(sf, 0.5f * (t.l[i] - t.r[i]));
        mm += m * m;
        ss += s * s;
    }
    return 10.0 * std::log10(std::max(ss, 1.0e-30) / std::max(mm, 1.0e-30));
}

struct Metrics { double corr = 1, b20 = 0, b200 = 0, b8 = 0, b12 = 0, b820 = 0, flat = 0, lufs = 0, lra = 0, lraFile = 0, p10 = 0, tp = 0, bass = 0; };
Metrics measure(const effectstests::Take& t) {
    Metrics m;
    m.corr = correlation(t);
    const auto sp = spectrum(t);
    m.b20 = sp.share(20, 80);
    m.b200 = sp.share(200, 500);
    m.b8 = sp.share(8000, 12000);
    m.b12 = sp.share(12000, 20000);
    m.b820 = sp.share(8000, 20000);
    m.flat = sp.flatness(2000, 8000);
    m.lufs = integrated(t);
    // Over the music: the phrase and 1.5 s of its ring-out (tools/warmth/measure.py reads it so too).
    std::tie(m.lra, m.p10) = loudnessRange(t, 0.1 + dstests::phraseSeconds + 1.5);
    m.lraFile = loudnessRange(t).first;
    m.tp = truePeakDb(t);
    m.bass = bassSideDb(t);
    return m;
}
std::string describe(const Metrics& m) {
    return "corr " + fmt(m.corr, 2) + ", 20-80 " + fmt(m.b20) + ", 200-500 " + fmt(m.b200) + ", 8-12k " + fmt(m.b8) + ", 12-20k " + fmt(m.b12) + " dB, flat "
         + fmt(m.flat, 3) + ", " + fmt(m.lufs) + " LUFS, LRA " + fmt(m.lra) + " (file " + fmt(m.lraFile) + "), p10 " + fmt(m.p10) + ", TP " + fmt(m.tp, 2) + " dBTP, bass side " + fmt(m.bass) + " dB";
}
std::string slug(std::string name) {
    for (auto& c : name) c = c == ' ' || c == '\'' ? '-' : char(std::tolower(static_cast<unsigned char>(c)));
    return name;
}
const char* eraName(int era) { return era == 0 ? "classic" : "modern"; }

// Hiss fix (build 3). The silence test: a chord held 5 s (from 0.3 s), released, then 6 s of
// silence; Grit 0 %, the instrument in its place, AIR as given.
constexpr double holdStart = 0.3, holdSeconds = 5.0, silenceSeconds = 6.0, releaseAt = holdStart + holdSeconds;
effectstests::Take silenceTest(int instrument, int era, float air, std::vector<float>* gate = nullptr) {
    InstHost h;
    h.play(instrument);
    setDs(h, { 0, era });
    h.set(dc::ids::air, air);
    for (int key : { 45, 52, 57, 60 }) h.note(holdStart, holdSeconds, key, 0.6f);
    if (gate != nullptr) gate->clear();
    for (double t = 0; t < releaseAt + silenceSeconds; t += 512.0 / 48000.0) {
        h.render(512.0 / 48000.0);
        if (gate != nullptr) gate->push_back(h.p->warmChainForTest().airGain());
    }
    return { h.outL, h.outR };
}
// The difference of two takes (the hiss alone: AIR changes nothing else in the chain).
effectstests::Take difference(const effectstests::Take& a, const effectstests::Take& b) {
    effectstests::Take d;
    const auto n = std::min(a.l.size(), b.l.size());
    for (std::size_t i = 0; i < n; ++i) { d.l.push_back(a.l[i] - b.l[i]); d.r.push_back(a.r[i] - b.r[i]); }
    return d;
}
// RMS (both sides) over [from, to) seconds, dBFS.
double rmsDb(const effectstests::Take& t, double from, double to) {
    const auto a = std::min(t.l.size(), std::size_t(from * 48000)), b = std::min(t.l.size(), std::size_t(to * 48000));
    double sum = 0;
    for (std::size_t i = a; i < b; ++i) sum += 0.5 * (double(t.l[i]) * t.l[i] + double(t.r[i]) * t.r[i]);
    return 10.0 * std::log10(std::max(sum / double(std::max<std::size_t>(1, b - a)), 1.0e-30));
}
// The level above 6 kHz (eighth-order high-pass) over [from, to) seconds, RMS dBFS.
double above6kDb(const effectstests::Take& t, double from, double to) {
    effectstests::Take hp;
    for (const auto* x : { &t.l, &t.r }) {
        std::array<juce::IIRFilter, 4> f;
        for (auto& b : f) b.setCoefficients(juce::IIRCoefficients::makeHighPass(48000.0, 6000.0));
        auto& out = x == &t.l ? hp.l : hp.r;
        for (float v : *x) { for (auto& b : f) v = b.processSingleSampleRaw(v); out.push_back(v); }
    }
    return rmsDb(hp, from, to);
}
// The last sample (s) that is not exactly 0, or -1.
double lastNonZero(const effectstests::Take& t) {
    for (std::size_t i = t.l.size(); i-- > 0;)
        if (t.l[i] != 0.0f || t.r[i] != 0.0f) return double(i) / 48000.0;
    return -1.0;
}
double firstNonZero(const effectstests::Take& t) {
    for (std::size_t i = 0; i < t.l.size(); ++i)
        if (t.l[i] != 0.0f || t.r[i] != 0.0f) return double(i) / 48000.0;
    return -1.0;
}
// The silence test's numbers: the floor above 6 kHz in the last 4 s, the hiss under the held pad
// (before and after it has been held 2 s), when the hiss starts and ends, its share of the whole.
struct HissMetrics { double floor6k = 0, hiss = 0, hissEarly = 0, hissLate = 0, first = -1, last = -1, b820 = 0; };
HissMetrics hissOf(const effectstests::Take& withAir, const effectstests::Take& noAir) {
    HissMetrics m;
    const auto hiss = difference(withAir, noAir);
    m.floor6k = above6kDb(withAir, releaseAt + silenceSeconds - 4.0, releaseAt + silenceSeconds);
    m.hiss = rmsDb(hiss, holdStart + 0.5, releaseAt);
    m.hissEarly = rmsDb(hiss, holdStart + 0.3, holdStart + 1.8);
    m.hissLate = rmsDb(hiss, holdStart + 3.2, releaseAt - 0.1);
    m.first = firstNonZero(hiss);
    m.last = lastNonZero(hiss);
    m.b820 = spectrum(withAir).share(8000, 20000);
    return m;
}
std::string describe(const HissMetrics& m) {
    return "floor above 6 kHz (last 4 s) " + fmt(m.floor6k) + " dBFS, hiss under the pad " + fmt(m.hiss) + " dBFS (" + fmt(m.hissEarly) + " -> " + fmt(m.hissLate)
         + " after 2 s held), hiss from " + fmt(m.first, 2) + " s to " + fmt(m.last, 2) + " s (release at " + fmt(releaseAt, 2) + " s), 8-20 kHz " + fmt(m.b820) + " dB";
}
// The hiss's share of the whole over the test phrase (AIR as given against AIR 0, Grit 0 %), dB.
double phraseHissShare(int instrument, int era, float air = 0.15f) {
    const auto with = phrase(instrument, { 0, era }, true, 3.5, air), without = phrase(instrument, { 0, era }, true, 3.5, 0.0f);
    const auto hiss = difference(with, without);
    const double end = 0.1 + dstests::phraseSeconds + 0.5;
    return rmsDb(hiss, 0.1, end) - rmsDb(with, 0.1, end);
}
} // namespace warmtests

void testWarmth(Suite& s) {
    using namespace warmtests;
    Available available(true);
    s.section("warmth: the parameters (warmth pass)");
    {
        DungeonCrawlerProcessor fx, inst(PluginKind::Instrument);
        for (auto* p : { &fx, &inst }) {
            const std::string kind = p->isInstrument() ? "instrument" : "FX";
            auto* warmth = dynamic_cast<juce::RangedAudioParameter*>(p->parameters.getParameter(dc::ids::warmth));
            auto* mono = dynamic_cast<juce::AudioParameterBool*>(p->parameters.getParameter(dc::ids::stereoMono));
            s.check(warmth != nullptr && warmth->isAutomatable() && warmth->getName(40) == "Warmth"
                        && warmth->getDefaultValue() == (p->isInstrument() ? 1.0f : 0.0f),
                    kind + ": Warmth is an automatable parameter, " + (p->isInstrument() ? "100 %" : "0 %") + " by default");
            s.check(mono != nullptr && !mono->get() && mono->getName(40) == "Mono", kind + ": Mono is a switch, off by default");
            auto* air = dynamic_cast<juce::RangedAudioParameter*>(p->parameters.getParameter(dc::ids::air));
            s.check(air != nullptr && air->isAutomatable() && air->getName(40) == "Air" && std::abs(air->convertFrom0to1(air->getDefaultValue()) - 0.15f) < 1.0e-6f
                        && p->getParameters().getLast() == air,
                    kind + ": Air (hiss fix) is an automatable parameter, 15 % by default, appended last");
            // A session saved before the warmth pass (no Warmth in it) opens with the new sound on
            // (the instrument), the parameters it has unchanged.
            p->parameters.getParameter(dc::ids::dungeonSynth)->setValueNotifyingHost(0.4f);
            juce::MemoryBlock block;
            p->getStateInformation(block);
            auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), int(block.getSize()));
            for (auto* child = xml->getFirstChildElement(); child != nullptr;) {
                auto* next = child->getNextElement();
                const auto id = child->getStringAttribute("id");
                if (id == dc::ids::warmth || id == dc::ids::stereoMono || id == dc::ids::air) xml->removeChildElement(child, true);
                child = next;
            }
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary(*xml, old);
            DungeonCrawlerProcessor reopened(p->kind());
            reopened.parameters.getParameter(dc::ids::warmth)->setValueNotifyingHost(0.3f);
            reopened.parameters.getParameter(dc::ids::air)->setValueNotifyingHost(0.9f);
            reopened.setStateInformation(old.getData(), int(old.getSize()));
            s.check(std::abs(reopened.warmthAmount() - (p->isInstrument() ? 1.0f : 0.0f)) < 1.0e-4f && !reopened.monoOn()
                        && std::abs(reopened.dungeonSynthAmount() - 0.4f) < 1.0e-3f && std::abs(reopened.airAmount() - 0.15f) < 1.0e-4f,
                    kind + ": an older session opens with Warmth and Air at their defaults and its own Grit");
        }
        s.check(fx.getParameters().size() + 13 == inst.getParameters().size(), "both plug-ins got the same three parameters (the instrument still adds thirteen)");
    }
    // Warmth 0 % is the engine exactly as before the warmth pass (bit for bit).
    {
        const auto run = [](bool warm, float amount) {
            Available a(warm);
            InstHost h;
            h.play(dc::inst::designFor(dc::Castle, 2));
            h.set(dc::ids::warmth, amount);
            h.render(2.0);   // (prepared at the default, the chain fades out in silence)
            melody(h, 2.1);
            h.render(0.05 + melodySeconds + 1.0);
            return effectstests::Take { h.outL, h.outR };
        };
        const auto before = run(false, 1.0f), off = run(true, 0.0f);
        for (std::size_t i = 0; i < before.l.size(); ++i)
            if (before.l[i] != off.l[i] || before.r[i] != off.r[i]) { std::cout << "  first difference at " << i << " (" << double(i) / 48000.0 << " s)\n"; break; }
        s.check(before.l == off.l && before.r == off.r && DungeonCrawlerProcessor::warmthAvailableForTests(), "Warmth 0 %: bit-identical to the engine before the warmth pass");
    }

    s.section("warmth: the test phrase, 4 instruments x 2 Eras x Grit 0 / 50 / 100 %");
    std::array<std::array<double, 2>, 3> corrSum {}, airSum {};
    for (int instrument : instruments()) {
        const auto name = instName(instrument);
        for (int era : { 0, 1 }) {
            const auto cold = measure(phrase(instrument, { 0, era }, false));
            for (int g = 0; g < 3; ++g) {
                const float grit = 0.5f * float(g);
                const auto m = measure(phrase(instrument, { grit, era }));
                const std::string label = name + " " + (era == 0 ? "Classic" : "Modern") + " Grit " + std::to_string(50 * g) + " %";
                corrSum[std::size_t(g)][std::size_t(era)] += m.corr;
                airSum[std::size_t(g)][std::size_t(era)] += m.b820;
                const double lo = era == 0 ? 0.42 : 0.28, hi = era == 0 ? 0.70 : 0.55;
                s.check(m.corr <= 0.8 && m.corr >= lo && m.corr <= hi, label + ": stereo, never mono (L/R correlation " + fmt(lo, 2) + "-" + fmt(hi, 2) + ")", describe(m));
                // Hiss fix (build 3): the 8-20 kHz share is reported only (the hiss gate is below).
                s.metric(label + ": 8-20 kHz share of the whole", m.b820, " dB");
                s.check(m.lra <= 9.0, label + ": glued (loudness range over the music 9 LU or less)", fmt(m.lra) + " LU");
                s.check(m.tp <= -0.5, label + ": the true peak at -0.5 dBTP or under", fmt(m.tp, 2) + " dBTP");
                s.check(m.bass <= -20.0, label + ": the bass below 100 Hz mono (side 20 dB or more under mid)", fmt(m.bass) + " dB");
                if (g == 2) s.check(m.flat >= 0.12, label + ": the Grit's crushed texture (2-8 kHz flatness 0.12 or more)", fmt(m.flat, 3));
                if (g == 0) {
                    s.metric(label + ": 200-500 Hz share, with the warmth against without", m.b200 - cold.b200, " dB");
                    s.metric(label + ": 20-80 Hz share, with the warmth against without", m.b20 - cold.b20, " dB");
                }
            }
        }
    }
    for (int g = 0; g < 3; ++g) {
        const double n = double(instruments().size());
        s.check(corrSum[std::size_t(g)][1] / n <= corrSum[std::size_t(g)][0] / n - 0.05, "Grit " + std::to_string(50 * g) + " %: Modern wider than Classic (mean correlation)",
                fmt(corrSum[std::size_t(g)][0] / n, 2) + " / " + fmt(corrSum[std::size_t(g)][1] / n, 2));
        s.metric("Grit " + std::to_string(50 * g) + " %: 8-20 kHz share, Classic (mean)", airSum[std::size_t(g)][0] / n, " dB");
        s.metric("Grit " + std::to_string(50 * g) + " %: 8-20 kHz share, Modern (mean)", airSum[std::size_t(g)][1] / n, " dB");
    }

    // The chain's tone on noise (no hall): 40-80 Hz up 1-3.5 dB, 200-500 Hz down 2-3.5 dB, against 1-2 kHz.
    for (int era : { 0, 1 }) {
        dc::WarmChain chain;
        chain.setControls(1.0f, era, false, 1.0f, 0.0f, false);
        chain.prepare(48000);
        dc::dsp::Random noise;
        effectstests::Take in, out;
        std::array<float, 256> l {}, r {};
        for (int block = 0; block < int(8.0 * 48000 / 256); ++block) {
            for (std::size_t i = 0; i < l.size(); ++i) { l[i] = r[i] = 0.1f * noise.bipolar(); in.l.push_back(l[i]); in.r.push_back(r[i]); }
            chain.setControls(1.0f, era, false, 1.0f, 0.0f, false);
            chain.process(l.data(), r.data(), int(l.size()));
            for (std::size_t i = 0; i < l.size(); ++i) { const float mid = 0.5f * (l[i] + r[i]); out.l.push_back(mid); out.r.push_back(mid); }
        }
        const auto si = spectrum(in), so = spectrum(out);
        const auto gain = [&](double lo, double hi) { return so.share(lo, hi) - si.share(lo, hi); };
        const double ref = gain(1000, 2000), low = gain(40, 80) - ref, mud = gain(200, 500) - ref;
        s.check(low >= 1.0 && low <= 3.5 && mud <= -2.0 && mud >= -3.5, std::string(era == 0 ? "Classic" : "Modern") + ": the tone, 40-80 Hz warmer, 200-500 Hz tamed (against 1-2 kHz, on noise)",
                "40-80 Hz " + fmt(low) + " dB, 200-500 Hz " + fmt(mud) + " dB");
    }

    s.section("warmth: motion, Mono, silence, clicks, real time");
    // Tape wow and flutter on a held 1 kHz tone (the FX's dry path with Warmth on).
    {
        double cents[2] {};
        for (int era : { 0, 1 }) {
            Host h;
            h.go(dc::Dungeon, 0);
            h.set(dc::ids::intensity, 0.0f);
            h.set(dc::ids::warmth, 1.0f);
            h.set(dc::ids::dungeonSynthEra, float(era));
            h.render(8.0, sine(1000.0, 0.25f));
            cents[era] = dstests::wanderCents(h.outL, h.at(1.0), h.at(8.0));
        }
        s.check(cents[0] >= 2.5 && cents[1] >= 1.5 && cents[0] > cents[1], "tape wow and flutter: the pitch moves (Classic more than Modern, cents RMS)",
                fmt(cents[0], 2) + " / " + fmt(cents[1], 2) + " cents");
    }
    // The voices' own detune: two notes of the same pitch, one after the other, land apart.
    {
        const auto pitchOf = [](int round) {
            InstHost h;
            h.play(dc::inst::designFor(dc::Castle, 3));
            h.dry();
            h.set(dc::ids::warmth, 1.0f);
            for (int k = 0; k < round; ++k) { h.note(0.02 + 0.05 * k, 0.03, 30, 0.1f); }
            h.render(0.1 + 0.05 * round);
            h.note(double(h.position) / h.sr + 0.01, 1.5, 69, 0.8f);
            const double start = double(h.position) / h.sr;
            h.render(1.6);
            const auto a = h.idx(start + 0.4), b = h.idx(start + 1.4);
            std::vector<float> x(h.outL.begin() + long(a), h.outL.begin() + long(b));
            // Zero crossings of the low-passed tone: its mean period.
            juce::IIRFilter f1, f2;
            f1.setCoefficients(juce::IIRCoefficients::makeLowPass(48000.0, 900.0));
            f2.setCoefficients(juce::IIRCoefficients::makeLowPass(48000.0, 900.0));
            for (auto& v : x) v = f2.processSingleSampleRaw(f1.processSingleSampleRaw(v));
            double first = -1, last = -1;
            int count = 0;
            for (std::size_t i = 1; i < x.size(); ++i)
                if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
                    const double c = double(i - 1) + double(-x[i - 1]) / double(x[i] - x[i - 1]);
                    if (first < 0) first = c;
                    last = c;
                    ++count;
                }
            return count > 2 ? 48000.0 * (count - 1) / (last - first) : 0.0;
        };
        double lowest = 1e9, highest = 0;
        for (int round = 0; round < 4; ++round) {
            const double hz = pitchOf(round);
            if (hz > 0.0) { lowest = std::min(lowest, hz); highest = std::max(highest, hz); }
        }
        const double spread = 1200.0 * std::log2(highest / lowest);
        s.check(spread >= 3.0 && spread <= 20.0, "per-voice detune: the same key lands 3-20 cents apart from note to note", fmt(spread, 1) + " cents");
    }
    // Mono: deliberate, exact.
    {
        InstHost h;
        h.play(dc::inst::designFor(dc::Castle, 2));
        h.set(dc::ids::stereoMono, 1.0f);
        h.render(0.05);
        melody(h, 0.1);
        h.render(0.05 + melodySeconds + 1.0);
        bool same = true;
        for (std::size_t i = h.idx(0.1); i < h.outL.size(); ++i) same = same && h.outL[i] == h.outR[i];
        s.check(same && h.peak(0.2, 2.0) > 1.0e-3, "Mono on: left and right are the same, the sound plays");
    }
    // Silence stays silent (the hiss is gated), and the tail dies away.
    for (int era : { 0, 1 }) {
        InstHost h;
        h.play(dc::inst::designFor(dc::Necropolis, 0));
        setDs(h, { 1, era });
        h.render(2.0);
        const double silent = h.peak(0.0, 2.0);
        melody(h, 2.1);
        h.render(0.1 + melodySeconds + 25.0);
        const double last = h.peak(h.outL.size() / 48000.0 - 1.0, h.outL.size() / 48000.0);
        s.check(silent < 1.0e-6 && h.finite() && last < 3.0e-5, std::string(era == 0 ? "Classic" : "Modern") + ": silent without notes, and the tail dies away",
                fmt(toDb(std::max(silent, 1.0e-12))) + " dBFS before, " + fmt(toDb(std::max(last, 1.0e-12))) + " dBFS 25 s after");
    }
    s.section("warmth: the hiss gate (hiss fix, build 3)");
    // A held chord, its release and 6 s of silence: the floor above 6 kHz in the last 4 s under
    // -90 dBFS, the hiss under the held pad under -60 dBFS and ducking 6 dB or more once held 2 s,
    // nothing before the first note, gone within 400 ms of the release (the hall still ringing).
    {
        const double latency = double(InstHost().p->getLatencySamples()) / 48000.0;
        for (int instrument : { dc::inst::designFor(dc::Necropolis, 0), dc::inst::designFor(dc::Castle, 2) })
            for (int era : { 0, 1 }) {
                const std::string label = instName(instrument) + " " + (era == 0 ? "Classic" : "Modern");
                std::vector<float> gate;
                const auto with = silenceTest(instrument, era, 0.15f, &gate), without = silenceTest(instrument, era, 0.0f);
                const auto m = hissOf(with, without);
                s.check(m.floor6k < -90.0, label + ": the noise floor above 6 kHz in the last 4 s of silence under -90 dBFS", describe(m));
                s.check(m.hiss < -60.0, label + ": the hiss under a sustained pad under -60 dBFS", fmt(m.hiss) + " dBFS");
                s.check(m.hissLate <= m.hissEarly - 6.0, label + ": the hiss ducks 6 dB or more under a note held over 2 s", fmt(m.hissEarly) + " -> " + fmt(m.hissLate) + " dBFS");
                s.check(m.first >= holdStart && m.last >= 0.0 && m.last <= releaseAt + 0.4 + latency,
                        label + ": no hiss before the first note, none 400 ms after the release (while the hall rings)",
                        "hiss " + fmt(m.first, 3) + " .. " + fmt(m.last, 3) + " s, release " + fmt(releaseAt, 2) + " s");
                const bool tailRings = rmsDb(with, releaseAt + 0.5, releaseAt + 1.5) > -80.0;
                const bool shut = !gate.empty() && gate.back() == 0.0f;
                s.check(tailRings && shut, label + ": the hall still rings after the release while the hiss is exactly 0", "tail " + fmt(rmsDb(with, releaseAt + 0.5, releaseAt + 1.5)) + " dBFS");
                s.metric(label + ": silence test, 8-20 kHz share of the whole", m.b820, " dB");
            }
    }
    // AIR: 0 % no hiss at all, 100 % an obvious one (16.5 dB over the default).
    {
        const int pad = dc::inst::designFor(dc::Necropolis, 0);
        const auto off = silenceTest(pad, 0, 0.0f), def = silenceTest(pad, 0, 0.15f), full = silenceTest(pad, 0, 1.0f);
        std::vector<float> gate;
        silenceTest(pad, 0, 0.0f, &gate);
        const bool none = std::all_of(gate.begin(), gate.end(), [](float g) { return g == 0.0f; });
        const double d15 = rmsDb(difference(def, off), holdStart + 0.3, holdStart + 1.8), d100 = rmsDb(difference(full, off), holdStart + 0.3, holdStart + 1.8);
        s.check(none, "AIR 0 %: no hiss at all (the hiss bed's gain stays exactly 0 while notes are held)");
        s.check(d100 >= d15 + 14.0 && d100 > -60.0, "AIR 100 %: an obvious hiss (14 dB or more over the default)", fmt(d15) + " -> " + fmt(d100) + " dBFS");
    }
    // The share of the whole on the test phrase: about -46 dB (Classic) and -52 dB (Modern).
    for (int era : { 0, 1 }) {
        double sum = 0;
        for (int instrument : instruments()) sum += phraseHissShare(instrument, era);
        const double share = sum / double(instruments().size()), target = era == 0 ? -46.0 : -52.0;
        s.check(std::abs(share - target) <= 3.0, std::string(era == 0 ? "Classic" : "Modern") + ": the hiss at AIR 15 % is about " + fmt(target, 0) + " dB of the whole (the test phrase, mean of four)",
                fmt(share) + " dB");
    }
    // The FX: the hiss follows its input, gone within 400 ms of the input falling silent.
    {
        const auto run = [](float air) {
            Host h;
            h.go(dc::Cathedral, 2);
            h.set(dc::ids::warmth, 1.0f);
            h.set(dc::ids::air, air);
            h.render(0.5, silence());
            h.render(3.0, sine(440.0, 0.25f));
            h.render(4.0, silence());
            return effectstests::Take { h.outL, h.outR };
        };
        const auto hiss = difference(run(0.15f), run(0.0f));
        const double latency = double(Host().p->getLatencySamples()) / 48000.0;
        const double first = firstNonZero(hiss), last = lastNonZero(hiss);
        s.check(first >= 0.5 && last > 1.0 && last <= 3.5 + 0.05 + 0.4 + latency && rmsDb(hiss, 1.0, 3.0) < -60.0,
                "the FX: hiss only while the input sounds, gone within 400 ms of silence, under -60 dBFS",
                "hiss " + fmt(first, 3) + " .. " + fmt(last, 3) + " s (input 0.5 .. 3.5 s), " + fmt(rmsDb(hiss, 1.0, 3.0)) + " dBFS");
    }

    // Click-free: a held chord while Warmth sweeps 0 -> 100 -> 0 %, the Era flips and Mono switches.
    {
        const int strings = dc::inst::designFor(dc::Castle, 2);
        const auto run = [&](float fixed) {
            InstHost h;
            h.play(strings);
            h.set(dc::ids::warmth, std::max(0.0f, fixed));
            h.on(0.1, 57, 0.7f);
            h.on(0.1, 64, 0.6f);
            h.render(3.0);
            const int blocks = int(4.0 * 48000 / 512);
            for (int b = 0; b < blocks; ++b) {
                const float t = float(b) / float(blocks);
                if (fixed < 0.0f) {
                    h.set(dc::ids::warmth, t < 0.5f ? 2.0f * t : 2.0f - 2.0f * t);
                    if (b == blocks / 3) h.set(dc::ids::dungeonSynthEra, 1.0f);
                    if (b == blocks / 2) h.set(dc::ids::stereoMono, 1.0f);
                    if (b == 2 * blocks / 3) h.set(dc::ids::stereoMono, 0.0f);
                }
                h.render(512.0 / 48000.0);
            }
            return dstests::maxStepOf(h.outL, h.outR, h.idx(3.0), h.outL.size());
        };
        const double reference = std::max(run(0.0f), run(1.0f)), during = run(-1.0f);
        s.check(during < 2.0 * reference, "a held chord while Warmth sweeps, the Era and Mono switch: click-free", fmt(during / reference, 2) + "x the largest steady step");
    }
    // The warm hall follows the place's wet path: Effects 0 % and the MIX Reverb at 0 % take it away.
    {
        const int lute = dc::inst::designFor(dc::Castle, 0);
        const auto tailWith = [&](const juce::String& id, float value) {
            InstHost t;
            t.play(lute);
            setDs(t, { 1, 1 });
            if (id.isNotEmpty()) t.set(id, value);
            t.render(0.05);
            melody(t, 0.1);
            t.render(0.05 + melodySeconds + 3.0);
            return dstests::tailDb({ t.outL, t.outR });
        };
        const double full = tailWith({}, 0), noEffects = tailWith(dc::ids::effects, 0.0f), noReverb = tailWith(dc::ids::mixGroups[0], 0.0f);
        s.check(noEffects < full - 6.0, "Effects 0 % takes the place and the warm hall away", fmt(full) + " -> " + fmt(noEffects) + " dB");
        s.check(noReverb < full - 6.0, "the MIX section's Reverb turns the warm hall down with the place's reverbs", fmt(full) + " -> " + fmt(noReverb) + " dB");
    }
    // Bypass with the warmth on (the FX): the input, untouched.
    {
        Host b;
        b.go(dc::Cathedral, 2);
        b.set(dc::ids::warmth, 1.0f);
        b.set(dc::ids::bypass, 1.0f);
        b.render(2.0, effectstests::material());
        const auto latency = std::size_t(b.p->getLatencySamples());
        double diff = 0;
        for (std::size_t i = b.at(1.0); i < b.end(); ++i) diff = std::max(diff, std::abs(double(b.outL[i]) - b.inL[i - latency]));
        s.check(diff < 1.0e-6, "Bypass with Warmth on: the input (within float rounding)", fmt(toDb(std::max(diff, 1.0e-12))) + " dBFS");
    }
    // Real time: no allocation while it plays and every control moves.
    {
        InstHost rt;
        rt.play(dc::inst::designFor(dc::Castle, 2));
        rt.render(0.2);
        std::size_t allocations = 0;
        for (int k = 0; k < 240; ++k) {
            if (k % 20 == 0) rt.on(rt.position / rt.sr + 0.001, 50 + k % 20, 0.7f);
            rt.set(dc::ids::warmth, float(k % 60) / 59.0f);
            rt.set(dc::ids::dungeonSynth, float(k % 50) / 49.0f);
            if (k % 37 == 0) rt.set(dc::ids::dungeonSynthEra, float((k / 37) % 2));
            if (k % 53 == 0) rt.set(dc::ids::stereoMono, float((k / 53) % 2));
            if (k == 120) rt.set(dc::ids::quality, 0.0f);
            allocationProbe::calls = 0;
            allocationProbe::enabled = true;
            rt.processOne(256);
            allocationProbe::enabled = false;
            allocations += allocationProbe::calls;
        }
        s.check(allocations == 0 && rt.finite(), "no allocation in processBlock with the warmth on, every control moving", std::to_string(allocations));
    }
    // Mono outputs and other rates.
    {
        InstHost mono(48000, 512, 1);
        mono.play(dc::inst::designFor(dc::Castle, 2));
        mono.note(0.05, 0.5, 57);
        mono.render(1.0);
        bool rates = true;
        for (double sr : { 44100.0, 96000.0 }) {
            InstHost h(sr);
            h.play(dc::inst::designFor(dc::Castle, 2));
            setDs(h, { 1, 1 });
            h.note(0.05, 0.6, 57);
            h.render(1.2);
            rates = rates && h.finite() && h.peak(0.1, 0.7) > 1.0e-3 && h.peak(0.0, 1.2) < 0.95;
        }
        s.check(mono.finite() && mono.peak(0.1, 0.6) > 1.0e-3 && rates, "mono outputs, 44.1 and 96 kHz work with the warmth on");
    }
    // The AU harness's instrument figures (tools/au-harness), measured the same way: a held chord
    // at Grit 0 % and 100 % (Classic, Modern) keeps its level and never folds to mono.
    {
        const auto chord = [](Ds d) {
            InstHost h;
            setDs(h, d);
            h.render(0.3);
            for (int key : { 57, 64, 69 }) h.on(0.3, key, 100.0f / 127.0f);
            h.render(3.0);
            effectstests::Take t { h.outL, h.outR };
            return std::make_pair(dstests::widthDb(t, h.idx(1.3), h.outL.size()), effectstests::levelDb(t.l, t.r, h.idx(0.35), h.idx(0.9)));
        };
        const auto [w0, l0] = chord({ 0, 0 });
        const auto [wc, lc] = chord({ 1, 0 });
        const auto [wm, lm] = chord({ 1, 1 });
        s.check(w0 > -15.0 && wc > -15.0 && wm > -15.0 && std::abs(lc - l0) < 3.0 && std::abs(lm - l0) < 3.0,
                "the instrument harness's figures: the chord stays stereo (side within 15 dB of mid) and keeps its level",
                "side against mid " + fmt(w0) + " / " + fmt(wc) + " / " + fmt(wm) + " dB, level " + fmt(l0) + " / " + fmt(lc) + " / " + fmt(lm) + " dB");
    }
    // CPU: the test phrase with the warmth on against the engine before it (the best of three).
    {
        const auto timed = [](bool warm, float grit) {
            double best = 1e9;
            for (int k = 0; k < 3; ++k) {
                Available a(warm);
                InstHost h;
                h.play(dc::inst::designFor(dc::Castle, 2));
                setDs(h, { grit, 0 });
                dstests::darkFantasyPhrase(h, 0.1);
                const auto begin = std::chrono::steady_clock::now();
                h.render(0.15 + dstests::phraseSeconds + 1.0);
                best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count());
            }
            return best;
        };
        const double before = timed(false, 0.0f), after = timed(true, 0.0f), after100 = timed(true, 1.0f);
        s.metric("the test phrase, Grit 0 %: CPU with the warmth against without", after / before, "x");
        s.metric("the test phrase, Grit 100 %: CPU with the warmth against Grit 0 % without", after100 / before, "x");
        s.check(after <= 1.5 * before, "CPU: the test phrase with the warmth on within 1.5x of the engine before it (Grit 0 %)", fmt(after / before, 2) + "x");
    }
}

// `--hiss-renders dir` (hiss fix, build 3): the test phrase (Grit 0 %) and the silence test (a
// chord held 5 s, released, 6 s of silence) on the four instruments, both Eras, by build 2's warm
// chain ("before/", its air bed exactly as it was) and this build's ("after/", AIR 15 %; the
// silence test also at AIR 0 %, "-air0", for the hiss alone). tools/warmth/measure.py --hiss gates
// the after renders; the numbers are printed (hiss-report.txt in CI).
int hissRenders(const juce::String& directory) {
    using namespace warmtests;
    Available available(true);
    juce::File dir(directory);
    for (bool legacy : { true, false }) {
        dc::WarmChain::setLegacyAirForTests(legacy);
        auto sub = dir.getChildFile(legacy ? "before" : "after");
        sub.createDirectory();
        std::cout << (legacy ? "before (build 2's air bed)" : "after (build 3, AIR 15 %)") << "\n";
        for (int instrument : instruments())
            for (int era : { 0, 1 }) {
                const auto base = slug(instName(instrument)) + "-" + eraName(era);
                const auto p = phrase(instrument, { 0, era });
                brasstests::writeWav(sub.getChildFile(base + "-phrase.wav"), p.l, p.r);
                const auto t = silenceTest(instrument, era, 0.15f);
                brasstests::writeWav(sub.getChildFile(base + "-silence.wav"), t.l, t.r);
                std::cout << "  " << instName(instrument) << " " << eraName(era) << ": phrase 8-20 kHz " << fmt(spectrum(p).share(8000, 20000)) << " dB; silence test: ";
                if (legacy) {
                    std::cout << "floor above 6 kHz (last 4 s) " << fmt(above6kDb(t, releaseAt + silenceSeconds - 4.0, releaseAt + silenceSeconds)) << " dBFS, above 6 kHz under the pad "
                              << fmt(above6kDb(t, holdStart + 0.5, releaseAt)) << " dBFS, 8-20 kHz " << fmt(spectrum(t).share(8000, 20000)) << " dB\n";
                } else {
                    const auto quiet = silenceTest(instrument, era, 0.0f);
                    brasstests::writeWav(sub.getChildFile(base + "-silence-air0.wav"), quiet.l, quiet.r);
                    std::cout << describe(hissOf(t, quiet)) << ", above 6 kHz under the pad " << fmt(above6kDb(t, holdStart + 0.5, releaseAt)) << " dBFS, phrase hiss share "
                              << fmt(phraseHissShare(instrument, era)) << " dB\n";
                }
            }
    }
    dc::WarmChain::setLegacyAirForTests(false);
    return 0;
}

// `--warmth-renders dir`: the test phrase on the four instruments in their places, both Eras,
// Grit 0 / 50 / 100 %, with the warmth on (the shipped sound): "<instrument>-<era>-grit<g>.wav",
// 48 kHz stereo. tools/warmth/measure.py measures them (and the 1.0 build's renders) in CI.
int warmthRenders(const juce::String& directory) {
    using namespace warmtests;
    juce::File dir(directory);
    dir.createDirectory();
    for (int instrument : instruments())
        for (int era : { 0, 1 })
            for (int g : { 0, 50, 100 }) {
                const auto t = phrase(instrument, { float(g) / 100.0f, era });
                brasstests::writeWav(dir.getChildFile(slug(instName(instrument)) + "-" + eraName(era) + "-grit" + std::to_string(g) + ".wav"), t.l, t.r);
                const auto [median, high] = reductionOf(lastReduction());
                std::cout << instName(instrument) << " " << eraName(era) << " grit " << g << ": " << describe(measure(t)) << ", gain reduction " << fmt(median)
                          << " dB (median), " << fmt(high) << " dB (95 %)\n";
            }
    return 0;
}

// `--hiss-snapshots dir` (hiss fix, build 3): the instrument and FX windows with GRIT, WARMTH, AIR
// and MONO, their controls up close (2x), and the Advanced panel's header with the build number.
int hissSnapshots(const juce::String& directory) {
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
        w.p->parameters.getParameter(dc::ids::dungeonSynth)->setValueNotifyingHost(0.4f);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        for (int k = 0; k < 8; ++k) w.p->processBlock(buffer, midi);
        if (auto* panel = w.editor->instrumentPanel()) panel->pollForTest();
        w.view().setAnimation(false);
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "hiss-instrument-window.png");
        if (auto* panel = w.editor->instrumentPanel()) {
            const auto& c = panel->warmthControl();
            save(w.editor->createComponentSnapshot(juce::Rectangle<int>(panel->getX() + c.getX() - 8, panel->getY() + c.getY() - 6, panel->getWidth() - c.getX() + 8, c.getHeight() + 10),
                                                   true, 2.0f), "hiss-instrument-controls-2x.png");
        }
        w.view().advancedButton().setToggleState(true, juce::dontSendNotification);
        w.view().advancedButton().onClick();
        const auto d = w.view().advancedBoundsForTest();
        save(w.editor->createComponentSnapshot(juce::Rectangle<int>(d.getX(), d.getY(), d.getWidth(), 60), true, 2.0f), "hiss-advanced-header-2x.png");
    }
    {
        Window w(false);
        w.go(dc::Necropolis, 0);
        w.view().setAnimation(false);
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "hiss-fx-window.png");
        save(w.editor->createComponentSnapshot(juce::Rectangle<int>(530, 560 - 150, 430, 150), true, 2.0f), "hiss-fx-controls-2x.png");
    }
    std::cout << written << " snapshots written to " << dir.getFullPathName() << "\n";
    return written == 5 ? 0 : 1;
}
