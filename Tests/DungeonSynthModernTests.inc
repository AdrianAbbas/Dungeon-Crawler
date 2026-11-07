// Session 18: DUNGEON SYNTH's Modern rebuilt as a lush, warm vintage sampler (included by
// DungeonCrawlerTests.cpp after DungeonSynthTests.inc; group "dsmodern", CI shard 16-dsmodern;
// `--ds-modern` the numbers, `--listen-dsmodern dir` the before / after render).
//
// Classic is unchanged, bit for bit: the stage is compared sample for sample with the stage as
// it was at the end of Session 17 (Tests/legacy, verbatim), which also renders Modern "before".
#include "legacy/DungeonSynthS17.cpp.inc"

namespace dsmodern {
using namespace dstests;

// One copy of the stage (now or Session 17's) on a signal, 64-sample blocks at `rate`, the
// controls (and optionally a sweep of the amount, an Era switch, the place) set every block as
// the processor does, the hiss before (as the processor adds it, before its place).
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
// a sampler meets. Its harmonics lie on a 1531 Hz grid that no sampling rate shares (a round
// 1500 Hz would put a 30 or 36 kHz sampler's folds back on its own harmonics), so whatever the
// stage folds back or images up (and its noise) lands between them.
constexpr double f0 = 1531.0;
Signal brightSaw(float amplitude = 0.25f) {
    return [amplitude](int, long long n, double sr) {
        double v = 0;
        for (int k = 1; f0 * k < 23000.0; ++k) v += std::sin(2.0 * juce::MathConstants<double>::pi * f0 * k * double(n) / sr) / k;
        return float(amplitude * 0.55 * v);
    };
}
// Above `from` (8 kHz), what is not the saw's harmonics (+/-0.6 %: Modern's slow wow moves them
// a little) against the whole output (dB): the aliasing, the images, the noise, the fizz.
double harshnessDb(const std::vector<float>& x, double from = 8000.0) {
    const auto p = spectrumOf(x, std::size_t(2 * 48000));
    const double all = bandPower(p, 20.0, 24000.0);
    double harmonic = 0;
    for (int k = 1; f0 * k < 24000.0; ++k)
        if (f0 * k * 1.006 > from) harmonic += bandPower(p, std::max(from, f0 * k * 0.994), f0 * k * 1.006);
    const double above = bandPower(p, from, 24000.0) - harmonic;
    return 10.0 * std::log10(std::max(above, 1.0e-30) / std::max(all, 1.0e-30));
}
// The low-mid's warmth: the 160-500 Hz bands against the 1-5 kHz bands (dB, third-octave
// bands): fuller below, as much as the level matcher and the music allow.
double lowMidDb(const std::vector<double>& bands) {
    double low = 0, upper = 0;
    for (std::size_t b = 0; b < bands.size(); ++b) {
        const double fc = 63.0 * std::pow(2.0, double(b) / 3.0);
        if (fc >= 158.0 && fc <= 505.0) low += std::pow(10.0, bands[b] / 10.0);
        if (fc >= 990.0 && fc <= 5100.0) upper += std::pow(10.0, bands[b] / 10.0);
    }
    return 10.0 * std::log10(std::max(low, 1.0e-30) / std::max(upper, 1.0e-30));
}

// Everything the Session 18 checks and `--ds-modern` measure.
struct Modern {
    double harsh[4] {}, harshClassic = 0;   // 0 / 25 / 50 / 100 % Modern; Classic 100 %
    struct Take { std::string name; double top8[4] {}, lowMid[4] {}, width[4] {}, tail[4] {}, distance[4] {}, loud[4] {}; bool tails = true; };
    std::vector<Take> takes;
    double t60[4] {};
};
Modern measure() {
    Modern m;
    const float amounts[4] { 0.0f, 0.25f, 0.5f, 1.0f };
    for (int k = 0; k < 4; ++k) {
        Plan p;
        p.amount = amounts[k];
        p.era = 1;
        p.chorus = 0.0f;
        p.send = 0.0f;
        m.harsh[k] = harshnessDb(runStage<dc::DungeonSynth>(p, brightSaw(), 4.0).l);
        m.t60[k] = k == 0 ? 0.0 : hallT60(amounts[k], 1);
    }
    {
        Plan p;
        p.amount = 1;
        p.era = 0;
        p.chorus = 0.0f;
        p.send = 0.0f;
        m.harshClassic = harshnessDb(runStage<dc::DungeonSynth>(p, brightSaw(), 4.0).l);
    }
    const auto take = [&](const std::string& name, const std::function<effectstests::Take(Ds)>& render, bool tails) {
        Modern::Take t;
        t.name = name;
        t.tails = tails;
        const auto base = render({ 0, 1 });
        const auto b0 = bandsDb(base.l, base.r);
        const double l0 = loudness(base.l, base.r, 48000);
        for (int k = 0; k < 4; ++k) {
            const auto r = k == 0 ? base : render({ amounts[k], 1 });
            const auto b = bandsDb(r.l, r.r);
            t.top8[k] = topDb(b, 8000) - topDb(b0, 8000);
            t.lowMid[k] = lowMidDb(b) - lowMidDb(b0);
            t.width[k] = widthDb(r) - widthDb(base);
            t.tail[k] = tails ? tailDb(r) - tailDb(base) : 0.0;
            t.distance[k] = spectralDistance(b0, b);
            t.loud[k] = loudness(r.l, r.r, 48000) - l0;
        }
        m.takes.push_back(t);
    };
    for (int instrument : dsInstruments()) take(instName(instrument) + " in its place", [instrument](Ds d) { return instMelody(instrument, d); }, true);
    // Bright material dry (the Wooden Flute's synthesis, as the Session 16 checks: the recorded
    // flute is dark by design), and the FX in two places and on its dry path.
    for (int instrument : brightInstruments())
        take(instName(instrument) + " dry", [instrument](Ds d) {
            return instMelody(instrument, d, 3.0, false, instrument == dc::inst::designFor(dc::HauntedForest, 0) ? int(dc::inst::SoundSeated) : int(dc::inst::SoundLatest));
        }, false);
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Cathedral, 2 }, { dc::Necropolis, 0 } })
        take("FX " + placeName(l, k), [l = l, k = k](Ds d) { return fxMaterial(l, k, d); }, false);
    take("FX dry path", [](Ds d) { return fxDry(d); }, false);
    return m;
}

// Modern before (Session 17's stage) and after (now), of the dark fantasy phrase in its place:
// the processor renders it at 0 % (the stage passes, the place as always), then each stage runs
// on that output with the controls the processor gave it (the instrument's chorus weight, the
// hall's send, the place). The "after" is also rendered through the processor itself.
struct BeforeAfter { effectstests::Take off, before, after, afterOffline, quarter, half; };
BeforeAfter beforeAfter(int instrument, double tail = 3.5) {
    BeforeAfter out;
    Plan plan;
    plan.era = 1;
    const auto phrase = [&](Ds d, bool capture) {
        InstHost h;
        h.play(instrument);
        setDs(h, d);
        h.render(0.05);
        darkFantasyPhrase(h, 0.1);
        h.render(0.05 + phraseSeconds + tail);
        if (capture) {
            const auto& st = h.p->dungeonSynthStage();
            plan.chorus = st.chorusForTest();
            plan.send = st.sendForTest();
            const auto place = st.placeForTest();
            plan.decay = place[0];
            plan.cutoff = place[1];
            plan.wet = place[2];
        }
        return effectstests::Take { h.outL, h.outR };
    };
    out.off = phrase({ 0, 1 }, true);
    const auto offline = [&](auto stage) {
        using Stage = decltype(stage);
        effectstests::Take t = out.off;
        Stage ds;
        ds.setControls(1.0f, 1, plan.chorus, plan.send);
        ds.setPlace(plan.decay, plan.cutoff, plan.wet);
        ds.prepare(48000);
        for (std::size_t i = 0; i < t.l.size(); i += 64) {
            const int m = int(std::min<std::size_t>(64, t.l.size() - i));
            ds.setControls(1.0f, 1, plan.chorus, plan.send);
            ds.setPlace(plan.decay, plan.cutoff, plan.wet);
            ds.process(t.l.data() + i, t.r.data() + i, m);
        }
        return t;
    };
    out.before = offline(dc::s17::DungeonSynth {});
    out.afterOffline = offline(dc::DungeonSynth {});
    out.after = phrase({ 1, 1 }, false);
    out.quarter = phrase({ 0.25f, 1 }, false);
    out.half = phrase({ 0.5f, 1 }, false);
    return out;
}
} // namespace dsmodern

// `--ds-modern`: the numbers behind the Session 18 checks, and Modern before / after.
int dsModern() {
    using namespace dsmodern;
    if (std::getenv("DS_HARSH")) { for (float a : {0.25f, 0.5f, 1.0f}) { Plan p; p.amount = a; p.era = 1; p.chorus = 0; p.send = 0; std::cout << a << ": " << harshnessDb(runStage<dc::DungeonSynth>(p, brightSaw(), 4.0).l) << " / all " << harshnessDb(runStage<dc::DungeonSynth>(p, brightSaw(), 4.0).l, 20.0) << "\n"; } return 0; }
    const auto m = measure();
    const auto list4 = [](const double (&v)[4], int decimals = 1) { return fmt(v[1], decimals) + " / " + fmt(v[2], decimals) + " / " + fmt(v[3], decimals); };
    std::cout << "bright saw, non-harmonic energy above 8 kHz (dB of the output): Modern 0 % " << fmt(m.harsh[0]) << ", 25 / 50 / 100 % " << list4(m.harsh)
              << "; Classic 100 % " << fmt(m.harshClassic) << "\n";
    std::cout << "Modern hall T60 25 / 50 / 100 %: " << list4(m.t60, 2) << " s\n";
    for (const auto& t : m.takes)
        std::cout << t.name << " (Modern 25 / 50 / 100 % against 0 %): above 8 kHz " << list4(t.top8) << " dB, low-mid " << list4(t.lowMid) << " dB, width "
                  << list4(t.width) << " dB, tail " << list4(t.tail) << " dB, distance " << list4(t.distance, 2) << " dB, loudness " << list4(t.loud, 2) << " dB\n";
    for (int instrument : dsInstruments()) {
        const auto r = beforeAfter(instrument);
        const auto b0 = bandsDb(r.off.l, r.off.r), bb = bandsDb(r.before.l, r.before.r), ba = bandsDb(r.after.l, r.after.r);
        std::cout << instName(instrument) << " Modern 100 % before / after: above 8 kHz " << fmt(topDb(bb, 8000) - topDb(b0, 8000)) << " / " << fmt(topDb(ba, 8000) - topDb(b0, 8000))
                  << " dB, low-mid " << fmt(lowMidDb(bb) - lowMidDb(b0)) << " / " << fmt(lowMidDb(ba) - lowMidDb(b0)) << " dB, width " << fmt(widthDb(r.before) - widthDb(r.off))
                  << " / " << fmt(widthDb(r.after) - widthDb(r.off)) << " dB, loudness " << fmt(loudness(r.before.l, r.before.r, 48000) - loudness(r.off.l, r.off.r, 48000), 2)
                  << " / " << fmt(loudness(r.after.l, r.after.r, 48000) - loudness(r.off.l, r.off.r, 48000), 2) << " dB\n";
    }
    for (int k = 0; k < 2; ++k) {
        Plan p;
        p.amount = 1;
        p.era = 1;
        p.chorus = 0;
        p.send = 0;
        const auto t = k == 0 ? runStage<dc::s17::DungeonSynth>(p, sine(200.0, 0.316f), 4.0) : runStage<dc::DungeonSynth>(p, sine(200.0, 0.316f), 4.0);
        std::cout << "Modern 100 % " << (k == 0 ? "before" : "after") << ": harmonics of a -10 dBFS 200 Hz sine " << fmt(harmonicsFft(t.l, 200.0, 48000)) << " dB, bright saw harshness "
                  << fmt(harshnessDb((k == 0 ? runStage<dc::s17::DungeonSynth>(p, brightSaw(), 4.0) : runStage<dc::DungeonSynth>(p, brightSaw(), 4.0)).l)) << " dB\n";
    }
    return 0;
}

// `--listen-dsmodern dir`: the dark fantasy melody on five instruments in their places, Modern
// 100 % before (Session 17) and after (Session 18): one A/B file each (0 %, before, after, back
// to back), each alone, and after at 25 and 50 %.
int listenDsModern(const juce::String& directory) {
    using namespace dsmodern;
    juce::File dir(directory);
    dir.createDirectory();
    const auto slug = [](std::string name) {
        for (auto& c : name) c = c == ' ' || c == '\'' ? '-' : char(std::tolower(static_cast<unsigned char>(c)));
        return name;
    };
    const auto append = [](std::vector<float>& all, const std::vector<float>& part) {
        all.insert(all.end(), part.begin(), part.end());
        all.insert(all.end(), std::size_t(0.6 * 48000), 0.0f);
    };
    for (int instrument : dsInstruments()) {
        const auto r = beforeAfter(instrument);
        const auto name = slug(instName(instrument));
        std::vector<float> al, ar;
        for (const auto* t : { &r.off, &r.before, &r.after }) { append(al, t->l); append(ar, t->r); }
        brasstests::writeWav(dir.getChildFile(name + "-ab-0-modern-before-modern-after.wav"), al, ar);
        brasstests::writeWav(dir.getChildFile(name + "-modern-100-1-before.wav"), r.before.l, r.before.r);
        brasstests::writeWav(dir.getChildFile(name + "-modern-100-2-after.wav"), r.after.l, r.after.r);
        brasstests::writeWav(dir.getChildFile(name + "-modern-25-after.wav"), r.quarter.l, r.quarter.r);
        brasstests::writeWav(dir.getChildFile(name + "-modern-50-after.wav"), r.half.l, r.half.r);
    }
    return 0;
}

void testDungeonSynthModern(Suite& s) {
    using namespace dsmodern;
    s.section("dungeon synth: Classic bit-exact with Session 17 (Session 18)");
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
    s.section("dungeon synth: Modern, a lush warm vintage sampler (Session 18)");
    const auto m = measure();
    const auto list4 = [](const double (&v)[4], int decimals = 1) { return fmt(v[1], decimals) + " / " + fmt(v[2], decimals) + " / " + fmt(v[3], decimals); };
    // No fizz: above 8 kHz, what is not music (the aliasing, the images, the noise) stays far down
    // even on the harshest material, at every amount.
    s.check(m.harsh[1] < -60.0 && m.harsh[2] < -60.0 && m.harsh[3] < -60.0, "Modern: on a bright saw, the aliasing and noise above 8 kHz stay under -60 dB of the output (25 / 50 / 100 %)",
            list4(m.harsh) + " dB (Classic 100 %: " + fmt(m.harshClassic) + " dB)");
    s.check(m.t60[3] >= 7.0 && m.t60[2] < m.t60[3] && m.t60[1] < m.t60[2], "Modern's hall: long at 100 % (T60), shorter towards 0 %", list4(m.t60, 2) + " s");
    double meanTop = 0, meanWarm = 0;
    for (const auto& t : m.takes) {
        meanTop += t.top8[3] / double(m.takes.size());
        meanWarm += t.lowMid[3] / double(m.takes.size());
        s.check(t.top8[3] <= -1.5 && t.top8[3] >= -14.0 && t.top8[1] <= 0.3, t.name + ": Modern 100 % smooths the top (above 8 kHz lower), not muffled",
                "above 8 kHz " + list4(t.top8) + " dB at 25 / 50 / 100 %");
        // (A pad already all low-mid, in a dark place, has the least to gain: at least not thinner.)
        s.check(t.lowMid[3] >= 0.25 && t.lowMid[2] >= 0.0, t.name + ": Modern adds low-mid warmth (160-500 Hz against 1-5 kHz)", list4(t.lowMid) + " dB at 25 / 50 / 100 %");
        s.check(t.width[3] >= 1.5 && t.width[2] > 0.3, t.name + ": Modern widens the image", list4(t.width) + " dB of side at 25 / 50 / 100 %");
        if (t.tails && t.name.rfind(instName(dc::inst::designFor(dc::Necropolis, 0)), 0) != 0)   // the pad's place rings longer than any hall
            s.check(t.tail[3] >= 1.5, t.name + ": Modern 100 % has a longer tail", fmt(t.tail[3]) + " dB");
        s.check(t.distance[3] >= 1.2 && t.distance[1] > 0.05 && t.distance[1] <= 0.5 * t.distance[3] && t.distance[2] <= t.distance[3],
                t.name + ": 25 / 50 % subtle and clearly there, 100 % the full sound (third-octave distance to 0 %)", list4(t.distance, 2) + " dB");
        s.check(std::abs(t.loud[1]) <= 1.5 && std::abs(t.loud[2]) <= 1.5 && std::abs(t.loud[3]) <= 1.5, t.name + ": Modern level-matched (within 1.5 dB of 0 %)", list4(t.loud, 2) + " dB");
    }
    s.check(meanTop <= -4.0 && meanWarm >= 1.5, "Modern 100 %: on average a clearly smoother top and a fuller low-mid",
            "above 8 kHz " + fmt(meanTop) + " dB, low-mid against 1-5 kHz +" + fmt(meanWarm) + " dB");
    s.metric("Modern 100 %: mean change above 8 kHz", meanTop, " dB");
    s.metric("Modern 100 %: mean low-mid warmth", meanWarm, " dB");
    s.metric("Modern 100 %: bright saw harshness above 8 kHz", m.harsh[3], " dB");
    // Before / after (Session 17's Modern): cleaner at the top, gentler saturation.
    {
        Plan p;
        p.era = 1;
        p.chorus = 0;
        p.send = 0;
        const double before = harshnessDb(runStage<dc::s17::DungeonSynth>(p, brightSaw(), 4.0).l), after = m.harsh[3];
        const double hb = harmonicsFft(runStage<dc::s17::DungeonSynth>(p, sine(200.0, 0.316f), 4.0).l, 200.0, 48000);
        const double ha = harmonicsFft(runStage<dc::DungeonSynth>(p, sine(200.0, 0.316f), 4.0).l, 200.0, 48000);
        s.check(ha < hb - 6.0 && ha > -50.0, "Modern 100 %: warm, gentle saturation (harmonics 2-5 of a -10 dBFS sine: present, far less than Session 17's)",
                fmt(hb) + " -> " + fmt(ha) + " dB");
        s.check(after < before - 15.0, "Modern 100 %: on a bright saw far cleaner above 8 kHz than Session 17's Modern", fmt(before) + " -> " + fmt(after) + " dB");
    }
}
