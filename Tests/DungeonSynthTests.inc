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
effectstests::Take instMelody(int instrument, Ds d, double tail = 3.0, bool inPlace = true) {
    InstHost h;
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
    ds.setControls(amount, era, 0.6f, 1.0f, 0.0f);
    ds.prepare(48000);
    std::array<float, 64> l {}, r {}, hl {}, hr {};
    std::vector<double> energy;
    for (int b = 0; b < 48000 * 10 / 64; ++b) {
        l.fill(0.0f);
        r.fill(0.0f);
        if (b == 0) l[0] = r[0] = 0.5f;
        ds.process(l.data(), r.data(), hl.data(), hr.data(), 64);
        for (int i = 0; i < 64; ++i) energy.push_back(double(hl[i]) * hl[i] + double(hr[i]) * hr[i]);
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
} // namespace dstests

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
    // What each Era does at 50 and 100 %, measured on the dry path (Intensity 0: only the stage):
    // the top rolls off, the tape saturates (Modern more), the pitch wanders (Classic more), the
    // image narrows (Classic), the hall's tail is long.
    const std::vector<Ds> halfAndFull { { 0.5f, 0 }, { 1, 0 }, { 0.5f, 1 }, { 1, 1 } };
    for (int instrument : dsInstruments()) {
        const auto base = instMelody(instrument, { 0, 0 }, 1.0, false);
        const double h0 = highShare(base.l, base.r, 6000);
        std::array<double, 4> drop {};
        for (std::size_t k = 0; k < halfAndFull.size(); ++k) {
            const auto t = instMelody(instrument, halfAndFull[k], 1.0, false);
            drop[k] = highShare(t.l, t.r, 6000) - h0;
        }
        for (int era : { 0, 1 }) {
            const double half = drop[std::size_t(era * 2)], full = drop[std::size_t(era * 2 + 1)];
            s.check(half <= -2.0 && full <= -4.0 && full < half - 0.5, instName(instrument) + ": " + (era == 0 ? "Classic" : "Modern") + " rolls off the top (above 6 kHz, 50 / 100 %)",
                    fmt(half) + " / " + fmt(full) + " dB");
        }
    }
    {
        std::array<double, 5> harmonics {}, wander {};
        const std::vector<Ds> sweep { { 0, 0 }, { 0.5f, 0 }, { 1, 0 }, { 0.5f, 1 }, { 1, 1 } };
        for (std::size_t k = 0; k < sweep.size(); ++k) {
            auto h = fxSine(sweep[k], 220.0, 0.3f, 4.0);
            harmonics[k] = harmonicsDb(h.outL, 220.0, h.at(1.0), h.at(4.0));
            auto w = fxSine(sweep[k], 1000.0, 0.3f, 6.0);
            wander[k] = wanderCents(w.outL, w.at(1.0), w.at(6.0));
        }
        const auto list = [](const std::array<double, 5>& v, int decimals) { return fmt(v[0], decimals) + " | C " + fmt(v[1], decimals) + " / " + fmt(v[2], decimals) + " | M " + fmt(v[3], decimals) + " / " + fmt(v[4], decimals); };
        s.check(harmonics[0] < -100.0 && harmonics[1] > -55.0 && harmonics[2] > -55.0 && harmonics[3] > -40.0 && harmonics[4] > -36.0 && harmonics[4] > harmonics[2] + 6.0,
                "saturation harmonics on a sine (-10 dBFS): none at 0 %, gentle in Classic, the tape's in Modern", list(harmonics, 1) + " dB");
        s.check(wander[0] < 0.05 && wander[1] > 1.0 && wander[2] > 2.5 && wander[3] > 0.3 && wander[4] > 0.8 && wander[4] < wander[2],
                "wow and flutter on a held 1 kHz tone: none at 0 %, the cassette's in Classic, a little in Modern", list(wander, 2) + " cents RMS");
        s.metric("harmonics 0 | Classic 50/100 | Modern 50/100 (dB)", harmonics[2]);
        s.metric("pitch wander, Classic 100 % (cents RMS)", wander[2]);
    }
    {
        const double c50 = hallT60(0.5f, 0), c100 = hallT60(1.0f, 0), m50 = hallT60(0.5f, 1), m100 = hallT60(1.0f, 1);
        s.metric("the hall's T60, Classic 100 %", c100, " s");
        s.metric("the hall's T60, Modern 100 %", m100, " s");
        s.check(c50 > 2.0 && c100 > 3.5 && c100 > c50 && m50 > 2.0 && m100 > 4.0 && m100 > m50, "the hall: a long tail, longer at 100 % (T60)",
                "Classic " + fmt(c50, 2) + " / " + fmt(c100, 2) + " s, Modern " + fmt(m50, 2) + " / " + fmt(m100, 2) + " s");
        // In a place: the tail after the melody grows (a plucked lute and a flute in their places).
        for (int instrument : { dc::inst::designFor(dc::Castle, 0), dc::inst::designFor(dc::HauntedForest, 0) }) {
            const double t0 = tailDb(instMelody(instrument, { 0, 0 })), tc = tailDb(instMelody(instrument, { 1, 0 })), tm = tailDb(instMelody(instrument, { 1, 1 }));
            s.check(tc > t0 + 2.0 && tm > t0 + 2.0, instName(instrument) + " in its place: the tail grows at 100 % (Classic, Modern)",
                    fmt(tc - t0) + " / " + fmt(tm - t0) + " dB");
        }
    }
    {
        // The image: a wide pad (independent sides) on the FX's dry path.
        const auto wide = [](Ds d) {
            Host h;
            h.set(dc::ids::intensity, 0.0f);
            setDs(h, d);
            h.render(3.0, [](int ch, long long n, double sr) { return 0.2f * float(std::sin(2.0 * juce::MathConstants<double>::pi * (ch == 0 ? 220.0 : 330.0) * double(n) / sr)); });
            return widthDb({ h.outL, h.outR }, h.at(1.0), h.end());
        };
        const double w0 = wide({ 0, 0 }), wc = wide({ 1, 0 }), wm = wide({ 1, 1 });
        s.check(wc < w0 - 3.0 && wm > w0 - 1.0, "Classic narrows the image (nearly mono), Modern keeps it", fmt(wc - w0) + " / " + fmt(wm - w0) + " dB of side");
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
    // Level-matched: the loudest 400 ms (K-weighted) within 1.5 dB of 0 % at 50 and 100 %.
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
        s.metric("worst loudness deviation at 50 / 100 % (both Eras)", worst, " dB");
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
                s.check(p.dungeonSynthAmount() > 0.2f && p.presetDisplayName() == juce::String(presets[std::size_t(k)].name),
                        std::string(instrument ? "instrument" : "FX") + " preset " + presets[std::size_t(k)].name + ": dungeon synth on, named, unmodified");
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
// at 0 %, Classic 50 / 100 % and Modern 50 / 100 % (one file each), the FX's guitar-and-synth material through the Crypt and the Gates
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
        for (const auto& [d, label] : settings) {
            InstHost h;
            h.play(instrument);
            setDs(h, d);
            h.render(0.05);
            darkFantasyPhrase(h, 0.1);
            h.render(0.05 + phraseSeconds + 3.5);
            brasstests::writeWav(dir.getChildFile(slug(instName(instrument)) + "-" + label + ".wav"), h.outL, h.outR);
        }
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
