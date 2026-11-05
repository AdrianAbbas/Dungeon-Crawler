// Session 11: the MIX section, the reverb helpers, the brass, Punch (included by
// DungeonCrawlerTests.cpp, "mix" group).
//   groups     each MIX slider (Reverb / Space, Echoes, Modulation, Character) moves its own
//              group of module amounts and nothing else; in sound, its effect at 0 / 100 /
//              150 % grows in places where that group carries the place; the level stays
//   helpers    Reverb Low Cut thins the tails' lows; Pre-delay moves the reverbs later;
//              Duck dips the tails under playing and they bloom back; Atmosphere scales it
//   smooth     sweeping every MIX control on a held tone is click-free
//   sessions   all new parameters are automatable, saved, identity at their defaults; a
//              session saved before Session 11 sounds as it did (also the instrument)
//   instrument the mix defaults (less reverb on brass, strings, drums, plucks; still
//              available at 150 %), the brass's harder, brighter attack at the same
//              sustain level, Punch (overshoot and settle as set, velocity, no clicks, never
//              clips)

namespace mixtests {
using effectstests::Take;
using effectstests::effectShare;
using effectstests::differenceShare;
using effectstests::levelDb;

const std::array<const char*, 4> groupNames { "Reverb / Space", "Echoes", "Modulation", "Character" };

Take fxMix(int location, int sub, const std::function<void(Host&)>& setup, double seconds, const Signal& signal) {
    Host h;
    h.go(location, sub);
    setup(h);
    h.render(seconds, signal);
    return { h.outL, h.outR };
}
// Energy in a band of a stretch (a 2-pole filter; lows below `hz`, or highs above it).
double bandDb(const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to, double hz, bool lows) {
    dc::dsp::Svf a, b;
    a.set(hz, 48000, 1.4142f);
    b.set(hz, 48000, 1.4142f);
    double sum = 0;
    to = std::min(to, l.size());
    for (std::size_t i = 0; i < to; ++i) {
        const float x = lows ? a.low(l[i]) : a.highPass(l[i]);
        const float y = lows ? b.low(r[i]) : b.highPass(r[i]);
        if (i >= from) sum += 0.5 * (double(x) * x + double(y) * y);
    }
    return toDb(std::sqrt(sum / double(std::max<std::size_t>(1, to - from))));
}
// When the cumulative energy after `from` reaches `share` of the stretch's (seconds after from).
double energyTime(const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to, double share) {
    double total = 0;
    for (std::size_t i = from; i < to; ++i) total += double(l[i]) * l[i] + double(r[i]) * r[i];
    double run = 0;
    for (std::size_t i = from; i < to; ++i) {
        run += double(l[i]) * l[i] + double(r[i]) * r[i];
        if (run >= share * total) return double(i - from) / 48000.0;
    }
    return double(to - from) / 48000.0;
}
double maxStep(const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to) {
    double m = 0;
    to = std::min(to, l.size());
    for (std::size_t i = std::max<std::size_t>(from, 2); i < to; ++i) {
        m = std::max(m, std::abs(double(l[i]) - 2.0 * l[i - 1] + l[i - 2]));
        m = std::max(m, std::abs(double(r[i]) - 2.0 * r[i - 1] + r[i - 2]));
    }
    return m;
}
// An impulse (a short click) at `at` seconds, then silence.
Signal click(double at) {
    return [at](int, long long n, double sr) { const long long k = n - static_cast<long long>(at * sr); return k >= 0 && k < 48 ? 0.5f * float(1.0 - k / 48.0) : 0.0f; };
}
// The material for `seconds`, then silence (tails).
Signal thenSilence(const Signal& s, double seconds) {
    return [s, seconds](int ch, long long n, double sr) { return double(n) < seconds * sr ? s(ch, n, sr) : 0.0f; };
}

// The output's dry component: the dry render's gain in it (least squares) over a stretch.
double dryGain(const Take& out, const Take& dry, std::size_t from, std::size_t to) {
    double xy = 0, yy = 0;
    to = std::min({ to, out.l.size(), dry.l.size() });
    for (std::size_t i = from; i < to; ++i) {
        xy += double(out.l[i]) * dry.l[i] + double(out.r[i]) * dry.r[i];
        yy += double(dry.l[i]) * dry.l[i] + double(dry.r[i]) * dry.r[i];
    }
    return yy > 0.0 ? xy / yy : 0.0;
}
// What the place adds against the dry it keeps (dB): the output less its dry component.
double wetVsDry(const Take& out, const Take& dry, std::size_t from, std::size_t to) {
    const double g = dryGain(out, dry, from, to);
    double wet = 0, kept = 0;
    to = std::min({ to, out.l.size(), dry.l.size() });
    for (std::size_t i = from; i < to; ++i) {
        const double a = out.l[i] - g * dry.l[i], b = out.r[i] - g * dry.r[i];
        wet += a * a + b * b;
        kept += g * g * (double(dry.l[i]) * dry.l[i] + double(dry.r[i]) * dry.r[i]);
    }
    return 10.0 * std::log10(std::max(wet, 1.0e-20) / std::max(kept, 1.0e-20));
}
// A tail (after the playing) against the dry component while playing (dB).
double tailVsDry(const Take& out, const Take& dry, std::size_t p0, std::size_t p1, std::size_t t0, std::size_t t1) {
    const double g = std::abs(dryGain(out, dry, p0, p1));
    return levelDb(out.l, out.r, t0, t1) - (levelDb(dry.l, dry.r, p0, p1) + toDb(std::max(g, 1.0e-9)));
}

// The level matcher's mean gain (dB) over a render, sampled after every block from `from` s.
double matcherDb(int location, int sub, const std::function<void(Host&)>& setup, double seconds, const Signal& signal, double from, double to) {
    Host h;
    h.go(location, sub);
    setup(h);
    double sum = 0;
    int count = 0;
    for (double t = 0; t < seconds; t += 512.0 / 48000.0) {
        h.render(512.0 / 48000.0, signal);
        if (t >= from && t < to) { sum += toDb(h.p->engine().matcherGain()); ++count; }
    }
    return count > 0 ? sum / count : 0.0;
}

// The instrument: a note (or chord), dry or in its place, classic or the new sound.
struct Inst {
    InstHost h;
    explicit Inst(int instrument, bool classic, bool inPlace = true) {
        h.p->setClassicSound(classic);
        h.play(instrument, true);
        if (!inPlace) h.dry();
        h.render(0.05);
    }
};
// The loudest 10 ms window between from and to.
double hitDb(const InstHost& h, double from, double to) {
    double best = 0;
    for (double t = from; t + 0.01 <= to; t += 0.005) best = std::max(best, h.rms(t, t + 0.01));
    return toDb(best);
}
double presenceShare(const InstHost& h, double from, double to) {
    // 2-4 kHz against everything (dB).
    dc::dsp::Biquad bl, br;
    bl.bandPass(48000, 2900, 0.9);
    br.bandPass(48000, 2900, 0.9);
    double band = 0, all = 0;
    const auto a = h.idx(from), b = h.idx(to);
    for (std::size_t i = 0; i < b; ++i) {
        const float x = bl.process(h.outL[i]), y = br.process(h.outR[i]);
        if (i >= a) { band += double(x) * x + double(y) * y; all += double(h.outL[i]) * h.outL[i] + double(h.outR[i]) * h.outR[i]; }
    }
    return 10.0 * std::log10(std::max(band, 1.0e-20) / std::max(all, 1.0e-20));
}
} // namespace mixtests

void testMixGroups(Suite& s) {
    using namespace mixtests;
    s.section("mix: the MIX groups (frames)");
    // Each group moves its own amounts and nothing else; at 100 % the frame is untouched.
    std::string onlyFail, identityFail, movesFail;
    for (const auto& [l, k] : allPlaces(true)) {
        dc::Controls c;
        c.location = l;
        c.sub = k;
        const auto base = dc::Engine::targetFrame(c);
        dc::Controls same = c;
        same.mix = { 1, 1, 1, 1 };
        if (std::memcmp(base.data(), dc::Engine::targetFrame(same).data(), sizeof(dc::Frame)) != 0) identityFail += placeName(l, k) + "; ";
        for (int g = 0; g < 4; ++g)
            for (float t : { 0.0f, 1.5f }) {
                dc::Controls m = c;
                m.mix[std::size_t(g)] = t;
                const auto f = dc::Engine::targetFrame(m);
                bool moved = false;
                for (std::size_t i = 0; i < dc::Count; ++i) {
                    const int group = dc::Engine::mixGroupOf(static_cast<dc::P>(i), base);
                    const bool tail = (g == dc::MixReverb && (i == dc::Decay || i == dc::Space)) || (g == dc::MixEchoes && (i == dc::Feedback || i == dc::TapeFeedback));
                    if (group != g && !(tail && t > 1.0f) && f[i] != base[i]) { onlyFail += placeName(l, k) + " " + groupNames[std::size_t(g)] + " moved " + dc::moduleSpecs[i].id + "; "; break; }
                    if (group == g && f[i] != base[i]) moved = true;
                    if (group == g && t == 0.0f && std::abs(f[i] - dc::neutralFrame()[i]) > 1.0e-3f * std::max(1.0f, std::abs(dc::neutralFrame()[i])))
                        { onlyFail += placeName(l, k) + " " + groupNames[std::size_t(g)] + " at 0 % leaves " + dc::moduleSpecs[i].id + "; "; break; }
                }
                if (!moved && t == 0.0f && g == dc::MixReverb && (base[dc::ReverbMix] > 0.01f || base[dc::VaultMix] > 0.01f)) movesFail += placeName(l, k) + "; ";
            }
    }
    s.check(identityFail.empty(), "every place: the MIX section at 100 % leaves the frame exactly as designed", identityFail);
    s.check(onlyFail.empty(), "every place: each MIX slider moves its own group and nothing else (0 % = none of it)", onlyFail.substr(0, 600));
    s.check(movesFail.empty(), "every place with a reverb: Reverb / Space at 0 % removes it", movesFail);

    s.section("mix: the MIX groups (sound, FX)");
    // In places where the group carries the place: the effect grows 0 -> 100 -> 150 %
    // (measured against the dry render), the level within 3 dB.
    struct Case { int group, location, sub; };
    const std::vector<Case> cases {
        { dc::MixReverb, dc::Cathedral, 1 }, { dc::MixReverb, dc::Castle, 1 }, { dc::MixReverb, dc::Caverns, 3 },
        { dc::MixEchoes, dc::Mountains, 1 }, { dc::MixEchoes, dc::Castle, 4 }, { dc::MixEchoes, dc::Mountains, 0 },
        { dc::MixModulation, dc::Dungeon, 3 }, { dc::MixModulation, dc::HauntedForest, 2 }, { dc::MixModulation, dc::Village, 0 },
        { dc::MixCharacter, dc::Village, 3 }, { dc::MixCharacter, dc::Dungeon, 2 }, { dc::MixCharacter, dc::Swamp, 1 },
    };
    const double seconds = 6.0;
    const auto from = std::size_t(0.5 * 48000), to = std::size_t(seconds * 48000);
    std::string growFail, levelFail;
    for (const auto& c : cases) {
        const auto signal = effectstests::material();
        const auto dry = fxMix(c.location, c.sub, [](Host& h) { h.set(dc::ids::intensity, 0.0f); }, seconds, signal);
        std::array<Take, 3> t;
        const std::array<float, 3> amounts { 0.0f, 1.0f, 1.5f };
        for (std::size_t k = 0; k < 3; ++k)
            t[k] = fxMix(c.location, c.sub, [&](Host& h) { h.set(dc::ids::mixGroups[std::size_t(c.group)], amounts[k]); }, seconds, signal);
        std::array<double, 3> share {}, wet {};
        for (std::size_t k = 0; k < 3; ++k) {
            share[k] = effectShare(t[k].l, t[k].r, dry.l, dry.r, from, to);
            wet[k] = wetVsDry(t[k], dry, from, to);
        }
        const std::string name = placeName(c.location, c.sub) + " " + groupNames[std::size_t(c.group)];
        // On the place's addition against the dry it keeps (or the spectral distance).
        const bool grows = (wet[0] < wet[1] - 0.5 || share[0] < share[1] - 0.2) && (wet[2] > wet[1] + 0.3 || share[2] > share[1] + 0.1);
        if (!grows) growFail += name + " (added against the dry " + fmt(wet[0]) + " / " + fmt(wet[1]) + " / " + fmt(wet[2]) + " dB, spectral " + fmt(share[0]) + " / "
                                + fmt(share[1]) + " / " + fmt(share[2]) + "); ";
        const double l0 = levelDb(t[0].l, t[0].r, from, to), l1 = levelDb(t[1].l, t[1].r, from, to), l2 = levelDb(t[2].l, t[2].r, from, to);
        if (std::abs(l0 - l1) > 3.0 || std::abs(l2 - l1) > 3.0) levelFail += name + " (" + fmt(l0 - l1) + " / " + fmt(l2 - l1) + " dB); ";
    }
    s.check(growFail.empty(), "each group's effect grows 0 -> 100 -> 150 % where it carries the place (12 places)", growFail);
    s.check(levelFail.empty(), "and the level stays within 3 dB of 100 %", levelFail);
    {
        // Reverb / Space at 0 %: the tail after the input stops nearly gone; at 150 % longer.
        const auto sig = thenSilence(plucks(0.35f), 2.0);
        const auto t0 = fxMix(dc::Cathedral, 1, [](Host& h) { h.set(dc::ids::mixGroups[0], 0.0f); }, 5.0, sig);
        const auto t1 = fxMix(dc::Cathedral, 1, [](Host&) {}, 5.0, sig);
        const auto t2 = fxMix(dc::Cathedral, 1, [](Host& h) { h.set(dc::ids::mixGroups[0], 1.5f); }, 5.0, sig);
        // The tail against the playing (the level matcher keeps the playing's level).
        const auto a = std::size_t(2.6 * 48000), b = std::size_t(4.5 * 48000), p0 = std::size_t(0.5 * 48000), p1 = std::size_t(2.0 * 48000);
        const auto dry = fxMix(dc::Cathedral, 1, [](Host& h) { h.set(dc::ids::intensity, 0.0f); }, 5.0, sig);
        const auto rel = [&](const Take& t) { return tailVsDry(t, dry, p0, p1, a, b); };
        const double d0 = rel(t0), d1 = rel(t1), d2 = rel(t2);
        s.check(d0 < d1 - 12.0 && d2 > d1 + 1.0, "the Nave's tail against the dry: Reverb / Space 0 % removes it, 150 % makes more of it",
                fmt(d0) + " / " + fmt(d1) + " / " + fmt(d2) + " dB");
    }
}

void testMixHelpers(Suite& s) {
    using namespace mixtests;
    s.section("mix: reverb low cut, pre-delay, duck, atmosphere");
    {
        // Low Cut 400 Hz: the tail's lows (< 150 Hz) much lower, its highs (> 2 kHz) kept.
        const auto sig = thenSilence(effectstests::material(), 2.0);
        const auto off = fxMix(dc::Caverns, 3, [](Host&) {}, 4.5, sig);
        const auto cut = fxMix(dc::Caverns, 3, [](Host& h) { h.set(dc::ids::mixLowCut, 400.0f); }, 4.5, sig);
        const auto a = std::size_t(2.3 * 48000), b = std::size_t(4.4 * 48000);
        const double lowOff = bandDb(off.l, off.r, a, b, 150, true), lowCut = bandDb(cut.l, cut.r, a, b, 150, true);
        const double highOff = bandDb(off.l, off.r, a, b, 2000, false), highCut = bandDb(cut.l, cut.r, a, b, 2000, false);
        const auto p0 = std::size_t(0.5 * 48000), p1 = std::size_t(2.0 * 48000);
        const double playOff = matcherDb(dc::Caverns, 3, [](Host&) {}, 2.0, sig, 0.5, 2.0),
                     playCut = matcherDb(dc::Caverns, 3, [](Host& h) { h.set(dc::ids::mixLowCut, 400.0f); }, 2.0, sig, 0.5, 2.0);
        s.check(lowCut < lowOff - 8.0 && std::abs(highCut - highOff) < 2.0, "Reverb Low Cut 400 Hz: the Dragon's Hoard tail loses its lows, keeps its highs",
                "lows " + fmt(lowOff) + " -> " + fmt(lowCut) + " dB, highs " + fmt(highOff) + " -> " + fmt(highCut) + " dB");
        s.metric("low cut: the playing's highs (> 2 kHz), cut - off", bandDb(cut.l, cut.r, p0, p1, 2000, false) - bandDb(off.l, off.r, p0, p1, 2000, false), " dB");
        s.check(std::abs(playCut - playOff) < 0.2, "and the dry keeps its level (the level matcher's gain does not move)", fmt(playCut - playOff, 2) + " dB");
    }
    {
        // Pre-delay +80 ms: the reverb's energy arrives later after a click.
        const auto sig = click(0.5);
        const auto a0 = fxMix(dc::Castle, 1, [](Host&) {}, 2.5, sig);
        const auto a1 = fxMix(dc::Castle, 1, [](Host& h) { h.set(dc::ids::mixPreDelay, 80.0f); }, 2.5, sig);
        const auto from = std::size_t(0.5 * 48000) + 480, to = std::size_t(2.4 * 48000);
        const double t0 = energyTime(a0.l, a0.r, from, to, 0.1), t1 = energyTime(a1.l, a1.r, from, to, 0.1);
        s.check(t1 - t0 > 0.04 && t1 - t0 < 0.13, "Reverb Pre-delay +80 ms: the Great Hall's reverb arrives about 80 ms later",
                fmt((t1 - t0) * 1000.0, 0) + " ms later (10 % of the energy)");
    }
    {
        // Duck 100 %: under the playing the place adds less (against the dry), the tail after
        // the playing blooms back to the unducked tail.
        const auto sig = thenSilence(effectstests::material(), 3.0);
        const auto dry = fxMix(dc::Castle, 1, [](Host& h) { h.set(dc::ids::intensity, 0.0f); }, 5.5, sig);
        const auto plain = fxMix(dc::Castle, 1, [](Host&) {}, 5.5, sig);
        const auto ducked = fxMix(dc::Castle, 1, [](Host& h) { h.set(dc::ids::mixDuck, 1.0f); }, 5.5, sig);
        const auto a = std::size_t(1.0 * 48000), b = std::size_t(2.9 * 48000);
        const double under = wetVsDry(plain, dry, a, b), underDucked = wetVsDry(ducked, dry, a, b);
        const double gPlain = matcherDb(dc::Castle, 1, [](Host&) {}, 3.0, sig, 1.0, 2.9), gDucked = matcherDb(dc::Castle, 1, [](Host& h) { h.set(dc::ids::mixDuck, 1.0f); }, 3.0, sig, 1.0, 2.9);
        s.check(std::abs(gDucked - gPlain) < 0.2, "Reverb Duck: the dry keeps its level (the level matcher's gain does not move)", fmt(gDucked - gPlain, 2) + " dB");
        const auto c = std::size_t(4.0 * 48000), d = std::size_t(5.4 * 48000);
        const double tail = levelDb(plain.l, plain.r, c, d), tailDucked = levelDb(ducked.l, ducked.r, c, d);
        s.check(underDucked < under - 3.0, "Reverb Duck 100 %: the reverb dips while playing", fmt(under) + " -> " + fmt(underDucked) + " dB against the dry");
        s.check(std::abs(tailDucked - tail) < 2.5, "and blooms back in the gap after it", fmt(tail) + " / " + fmt(tailDucked) + " dB");
    }
    {
        // The Atmosphere: on with a level, MIX Atmosphere 0 % = off, 150 % louder than 100 %.
        const auto sig = plucks(0.3f);
        const auto setup = [](float mixAtmos, bool on) {
            return [mixAtmos, on](Host& h) {
                h.set(dc::ids::atmosLevel, 0.6f);
                h.set(dc::ids::atmosDensity, 1.0f);
                h.set(dc::ids::atmosOn, on ? 1.0f : 0.0f);
                h.set(dc::ids::mixGroups[4], mixAtmos);
            };
        };
        const auto none = fxMix(dc::Dungeon, 0, setup(1.0f, false), 8.0, sig);
        const auto zero = fxMix(dc::Dungeon, 0, setup(0.0f, true), 8.0, sig);
        const auto full = fxMix(dc::Dungeon, 0, setup(1.0f, true), 8.0, sig);
        const auto more = fxMix(dc::Dungeon, 0, setup(1.5f, true), 8.0, sig);
        const auto a = std::size_t(0.2 * 48000), b = std::size_t(8.0 * 48000);
        const auto energy = [&](const Take& t) {
            double e = 0;
            for (std::size_t i = a; i < b && i < t.l.size(); ++i) { const double x = double(t.l[i]) - none.l[i], y = double(t.r[i]) - none.r[i]; e += x * x + y * y; }
            return 10.0 * std::log10(std::max(e, 1.0e-20));
        };
        const double e0 = energy(zero), e1 = energy(full), e2 = energy(more);
        s.check(e0 < -150.0 && e1 > -60.0 && e2 > e1 + 2.0, "Atmosphere: Off and MIX 0 % silence it; 150 % is louder than 100 %",
                fmt(e0) + " / " + fmt(e1) + " / " + fmt(e2) + " dB (energy added)");
    }
}

void testMixSmooth(Suite& s) {
    using namespace mixtests;
    s.section("mix: click-free while moving");
    // A held sine in three places; each control swept 100 -> 0 -> 150 -> 100 % (or across
    // its range) over 3 s, one block at a time. Its largest step against the same place held
    // still at either end.
    const std::vector<std::pair<int, int>> places { { dc::Cathedral, 1 }, { dc::Mountains, 1 }, { dc::Village, 3 } };
    struct Control { const char* id; float a, b; };
    const std::vector<Control> controls {
        { dc::ids::mixGroups[0], 0.0f, 1.5f }, { dc::ids::mixGroups[1], 0.0f, 1.5f }, { dc::ids::mixGroups[2], 0.0f, 1.5f }, { dc::ids::mixGroups[3], 0.0f, 1.5f },
        { dc::ids::mixLowCut, dc::mixLowCutOff, 600.0f }, { dc::ids::mixPreDelay, -40.0f, 120.0f }, { dc::ids::mixDuck, 0.0f, 1.0f },
    };
    std::string fail;
    for (const auto& [l, k] : places)
        for (const auto& c : controls) {
            const auto sig = sine(220.0, 0.25f);
            const auto run = [&](bool sweep, float hold) {
                Host h;
                h.go(l, k);
                h.set(c.id, hold);
                h.render(1.0, sig);
                const int blocks = int(3.0 * 48000 / 512);
                for (int b = 0; b < blocks; ++b) {
                    if (sweep) {
                        // a -> b over the first half, back to a over the second.
                        const float u = float(b) / float(blocks);
                        h.set(c.id, u < 0.5f ? c.a + (c.b - c.a) * 2.0f * u : c.b + (c.a - c.b) * 2.0f * (u - 0.5f));
                    }
                    h.render(512.0 / 48000.0, sig);
                }
                return Take { h.outL, h.outR };
            };
            const auto swept = run(true, c.a);
            const auto stillA = run(false, c.a), stillB = run(false, c.b);
            const auto a = std::size_t(1.0 * 48000), b = swept.l.size();
            const double steady = std::max(maxStep(stillA.l, stillA.r, a, b), maxStep(stillB.l, stillB.r, a, b));
            const double moving = maxStep(swept.l, swept.r, a, b);
            if (moving > steady * 1.6 + 2.0e-3) fail += placeName(l, k) + " " + c.id + " (" + fmt(moving, 4) + " vs " + fmt(steady, 4) + "); ";
        }
    s.check(fail.empty(), "every MIX control swept on a held tone: no click (3 places)", fail);
}

void testMixSessions(Suite& s) {
    using namespace mixtests;
    s.section("mix: parameters, presets, sessions");
    for (bool instrument : { false, true }) {
        DungeonCrawlerProcessor p(instrument ? PluginKind::Instrument : PluginKind::Effect);
        std::string missing;
        std::vector<const char*> ids { dc::ids::mixGroups[0], dc::ids::mixGroups[1], dc::ids::mixGroups[2], dc::ids::mixGroups[3], dc::ids::mixGroups[4],
                                       dc::ids::mixLowCut, dc::ids::mixPreDelay, dc::ids::mixDuck, dc::ids::atmosOn };
        if (instrument) { ids.push_back(dc::ids::instPunch); ids.push_back(dc::ids::instPunchTime); }
        for (const auto* id : ids) {
            auto* q = p.parameters.getParameter(id);
            if (q == nullptr || !q->isAutomatable()) missing += std::string(id) + "; ";
        }
        const std::string kind = instrument ? "instrument" : "FX";
        s.check(missing.empty(), kind + ": the MIX, reverb, Atmosphere" + (instrument ? " and Punch" : "") + " parameters exist and are automatable", missing);
        s.check(p.parameters.getParameter(dc::ids::instPunch) == nullptr || instrument, kind + ": Punch only in the instrument");
        // Saved and restored.
        auto set = [&](const char* id, float plain) { auto* q = p.parameters.getParameter(id); q->setValueNotifyingHost(q->convertTo0to1(plain)); };
        set(dc::ids::mixGroups[0], 0.4f);
        set(dc::ids::mixLowCut, 250.0f);
        set(dc::ids::mixDuck, 0.6f);
        if (instrument) set(dc::ids::instPunch, 1.6f);
        juce::MemoryBlock state;
        p.getStateInformation(state);
        DungeonCrawlerProcessor q(instrument ? PluginKind::Instrument : PluginKind::Effect);
        q.setStateInformation(state.getData(), int(state.getSize()));
        const auto plain = [](DungeonCrawlerProcessor& x, const char* id) { return x.parameters.getRawParameterValue(id)->load(); };
        s.check(std::abs(plain(q, dc::ids::mixGroups[0]) - 0.4f) < 1.0e-3f && std::abs(plain(q, dc::ids::mixLowCut) - 250.0f) < 0.5f && std::abs(plain(q, dc::ids::mixDuck) - 0.6f) < 1.0e-3f
                    && (!instrument || std::abs(plain(q, dc::ids::instPunch) - 1.6f) < 1.0e-3f) && !q.usesClassicSound(),
                kind + ": saved with the session and restored (the new sound kept)");
        // A session saved before Session 11: no new parameters, no "sound": every new one at
        // its default (identity), the instrument with its old sound.
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
        if (xml != nullptr) {
            for (const auto* id : ids)
                for (auto* child = xml->getFirstChildElement(); child != nullptr;)
                    if (child->getStringAttribute("id") == id) { auto* next = child->getNextElement(); xml->removeChildElement(child, true); child = next; }
                    else child = child->getNextElement();
            xml->removeAttribute("sound");
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary(*xml, old);
            DungeonCrawlerProcessor r(instrument ? PluginKind::Instrument : PluginKind::Effect);
            r.setStateInformation(old.getData(), int(old.getSize()));
            bool defaults = true;
            for (const auto* id : ids) {
                auto* param = r.parameters.getParameter(id);
                defaults = defaults && std::abs(param->getValue() - param->getDefaultValue()) < 1.0e-6f;
            }
            s.check(defaults && r.usesClassicSound() == instrument, kind + ": a session saved before Session 11 opens with every new control at its default"
                                                                         + (instrument ? " and the instrument's old sound" : ""));
            if (instrument) {
                r.applyFactoryPreset(0);
                s.check(!r.usesClassicSound(), "instrument: a factory preset brings the new sound");
            }
        }
    }
    {
        // FX: the defaults are exactly the sound before (every new control at its default
        // against all of them set explicitly to it: the same samples, and a session without them).
        Host a, b;
        a.go(dc::Castle, 1);
        b.go(dc::Castle, 1);
        b.set(dc::ids::mixGroups[0], 1.0f);
        b.set(dc::ids::mixLowCut, dc::mixLowCutOff);
        b.set(dc::ids::mixPreDelay, 0.0f);
        b.set(dc::ids::mixDuck, 0.0f);
        a.render(3.0, effectstests::material());
        b.render(3.0, effectstests::material());
        bool same = a.outL.size() == b.outL.size();
        for (std::size_t i = 0; same && i < a.outL.size(); ++i) same = a.outL[i] == b.outL[i] && a.outR[i] == b.outR[i];
        s.check(same, "FX: at their defaults the new controls change nothing (bit-identical)");
    }
    {
        // An old instrument session sounds exactly as the classic sound (bit-identical).
        const auto render = [](bool classic) {
            InstHost h;
            h.p->setClassicSound(classic);
            h.play(6);   // the Brass Fanfare, in the Great Hall
            h.render(0.05);
            melody(h, 0.1);
            h.render(5.0);
            return Take { h.outL, h.outR };
        };
        const auto classic = render(true), again = render(true), modern = render(false);
        bool same = classic.l == again.l && classic.r == again.r, differs = classic.l != modern.l;
        s.check(same && differs, "instrument: the old sound is reproduced exactly (and differs from the new one)");
    }
}

void testMixInstrument(Suite& s) {
    using namespace mixtests;
    s.section("mix: the instrument (mix defaults, brass, Punch)");
    {
        // Less reverb on the instruments played in a mix (the tail after the melody against
        // the melody), still available at Reverb / Space 150 %.
        std::string fail, availFail;
        for (int i : { 6, 7, 30, 5, 9 }) {
            const auto render = [&](bool classic, float reverb, bool dry) {
                InstHost h;
                h.p->setClassicSound(classic);
                h.play(i);
                h.set(dc::ids::mixGroups[0], reverb);
                if (dry) h.dry();
                h.render(0.05);
                melody(h, 0.1);
                h.render(0.05 + melodySeconds + 2.5);
                return Take { h.outL, h.outR };
            };
            const auto p0 = std::size_t(0.1 * 48000), p1 = std::size_t((0.1 + melodySeconds) * 48000);
            const auto t0 = std::size_t((0.1 + melodySeconds + 0.6) * 48000), t1 = std::size_t((0.1 + melodySeconds + 2.4) * 48000);
            const auto take = [&](bool classic, float reverb) { return tailVsDry(render(classic, reverb, false), render(classic, reverb, true), p0, p1, t0, t1); };
            const double classic = take(true, 1.0f), modern = take(false, 1.0f), more = take(false, 1.5f);
            const std::string name = dc::inst::design(i).name;
            s.metric(name + ": tail against the dry, after - before", modern - classic, " dB");
            if (modern > classic - 1.0) fail += name + " (" + fmt(classic) + " -> " + fmt(modern) + " dB); ";
            if (more < classic - 2.0) availFail += name + " (" + fmt(more) + " vs " + fmt(classic) + " dB); ";
        }
        s.check(fail.empty(), "brass, strings, drums, plucks: less of the place's reverb by default (the tail against the dry)", fail);
        s.check(availFail.empty(), "and their place's full space is still there at Reverb / Space 150 %", availFail);
    }
    {
        // The brass: a louder, brighter hit, the sustain level the same.
        std::string hitFail, sustainFail, presenceFail, velocityFail;
        for (int i : { 6, 9, 4, 32 }) {
            const auto take = [&](bool classic, float velocity) {
                auto in = std::make_unique<Inst>(i, classic, false);
                in->h.note(0.1, 1.3, 60, velocity);
                in->h.render(1.5);
                return in;
            };
            const auto oldNote = take(true, 0.9f), newNote = take(false, 0.9f);
            const double oldHit = hitDb(oldNote->h, 0.1, 0.26), newHit = hitDb(newNote->h, 0.1, 0.26);
            const double oldSus = toDb(oldNote->h.rms(0.8, 1.3)), newSus = toDb(newNote->h.rms(0.8, 1.3));
            const double gain = (newHit - newSus) - (oldHit - oldSus);
            const std::string name = dc::inst::design(i).name;
            s.metric(name + ": hit against sustain, before -> after", gain, " dB");
            s.metric(name + ": sustain level, before -> after", newSus - oldSus, " dB");
            if (gain < (i == 32 ? 1.5 : 3.0)) hitFail += name + " (+" + fmt(gain) + " dB); ";
            if (std::abs(newSus - oldSus) > 1.5) sustainFail += name + " (" + fmt(newSus - oldSus) + " dB); ";
            if (i != 32) {
                const double p0 = presenceShare(oldNote->h, 0.1, 0.25), p1 = presenceShare(newNote->h, 0.1, 0.25);
                s.metric(name + ": presence (2-4 kHz) in the first 150 ms, before -> after", p1 - p0, " dB");
                if (p1 < p0 + 1.0) presenceFail += name + " (" + fmt(p0) + " -> " + fmt(p1) + " dB); ";
                const auto softOld = take(true, 0.4f), softNew = take(false, 0.4f), hardOld = take(true, 1.0f), hardNew = take(false, 1.0f);
                const double spanOld = toDb(hardOld->h.rms(0.8, 1.3)) - toDb(softOld->h.rms(0.8, 1.3)), spanNew = toDb(hardNew->h.rms(0.8, 1.3)) - toDb(softNew->h.rms(0.8, 1.3));
                if (spanNew < spanOld + 2.0) velocityFail += name + " (" + fmt(spanOld) + " -> " + fmt(spanNew) + " dB); ";
            }
        }
        s.check(hitFail.empty(), "the brass hits harder: the first 150 ms against the sustain, +3 dB or more (Dwarven Chant +1.5)", hitFail);
        s.check(sustainFail.empty(), "at the same sustain level (within 1.5 dB)", sustainFail);
        s.check(presenceFail.empty(), "with more presence (2-4 kHz) on the attack", presenceFail);
        s.check(velocityFail.empty(), "and a stronger velocity response (soft to hard, +2 dB more span)", velocityFail);
        s.check(dc::inst::mixFit(6).preDelayMs >= 20.0f && dc::inst::mixFit(4).preDelayMs >= 20.0f, "the brass's reverb starts later (the dry hit first)");
    }
    {
        // Punch on an organ (no Punch of its own): +12 dB at 200 %, settled after Punch Time.
        const auto take = [](float punch, float timeMs, float velocity) {
            InstHost h;
            h.play(8);   // the Chamber Organ, steady
            h.dry();
            h.set(dc::ids::instLevel, -18.0f);
            h.set(dc::ids::instPunch, punch);
            h.set(dc::ids::instPunchTime, timeMs);
            h.render(0.05);
            h.note(0.1, 1.0, 60, velocity);
            h.render(1.3);
            return h;
        };
        const auto ratio = [](const InstHost& a, const InstHost& b, double t) { return toDb(a.rms(0.1 + t, 0.1 + t + 0.004)) - toDb(b.rms(0.1 + t, 0.1 + t + 0.004)); };
        const auto none = take(0.0f, 100.0f, 1.0f), full = take(2.0f, 100.0f, 1.0f);
        const double start = ratio(full, none, 0.004), settled = ratio(full, none, 0.115), late = ratio(full, none, 0.6);
        s.check(start > 10.5 && start < 12.6, "Punch 200 %: the hit overshoots by +12 dB", fmt(start) + " dB");
        s.check(std::abs(settled) < 0.6 && std::abs(late) < 0.1, "and has settled after Punch Time (100 ms)", fmt(settled) + " / " + fmt(late) + " dB");
        const auto slow = take(2.0f, 300.0f, 1.0f);
        const double mid = ratio(slow, none, 0.148), after = ratio(slow, none, 0.31);
        s.check(mid > 4.5 && mid < 7.5 && std::abs(after) < 0.6, "Punch Time 300 ms: half-way at 150 ms, settled at 300 ms", fmt(mid) + " / " + fmt(after) + " dB");
        const auto softNone = take(0.0f, 100.0f, 0.5f), soft = take(2.0f, 100.0f, 0.5f);
        const double softStart = ratio(soft, softNone, 0.004);
        s.check(softStart > 6.5 && softStart < 9.0, "velocity-sensitive: at half velocity about +8 dB", fmt(softStart) + " dB");
        const double stepNone = maxStep(none.outL, none.outR, none.idx(0.1), none.idx(0.4)) / std::max(1.0e-9, none.peak(0.3, 0.6));
        const double stepFull = maxStep(full.outL, full.outR, full.idx(0.1), full.idx(0.4)) / std::max(1.0e-9, full.peak(0.3, 0.6));
        s.check(stepFull < stepNone * 4.5 + 1.0e-4, "no click: the boost glides (its steps no larger than the louder hit's own)", fmt(stepNone, 4) + " / " + fmt(stepFull, 4));
        s.check(dc::inst::punchDb(6, 1.0f) >= 8.0f && dc::inst::punchDb(30, 1.0f) >= 1.5f && dc::inst::punchDb(10, 1.0f) == 0.0f && dc::inst::punchDb(10, 2.0f) == 12.0f
                    && dc::inst::punchDb(6, 0.0f) == 0.0f,
                "Punch 100 %: high on the brass, some on the drums, none on the choir; 0 % none; 200 % +12 dB anywhere");
    }
    {
        // Never clips: the loudest brass chord, Punch 200 %, Level +12 dB.
        InstHost h;
        h.play(4);
        h.set(dc::ids::instPunch, 2.0f);
        h.set(dc::ids::instLevel, 12.0f);
        h.render(0.05);
        for (int key : { 48, 55, 60, 64, 67, 72 }) h.note(0.1, 1.0, key, 1.0f);
        h.render(2.0);
        s.check(h.finite() && h.peak(0.0, 2.0) <= 1.0, "Punch 200 % on a loud brass chord at +12 dB never clips (the safety limiter)", fmt(h.peak(0.0, 2.0), 3));
    }
}

void testMixWindow(Suite& s) {
    s.section("mix: the panel in the window");
    for (bool instrument : { false, true }) {
        const std::string kind = instrument ? "instrument" : "FX";
        windowtests::Window w(instrument);
        auto& v = w.view();
        v.settleForTest();
        s.check(v.mixButton().isVisible() && !v.mixOpen(), kind + ": the Mix button, the panel closed");
        if (instrument) nav::click(w.editor->instrumentPanel()->mixButton());
        else nav::click(v.mixButton());
        s.check(v.mixOpen() && (!instrument || w.editor->instrumentPanel()->mixButton().getToggleState()),
                kind + ": the Mix button opens the MIX panel" + (instrument ? " (from the instrument panel, its button lit)" : ""));
        nav::click(v.journeyButton());
        s.check(!v.mixOpen() && v.journeyOpen(), kind + ": one panel at a time (Journey closes it)");
        v.toggleMix();
        s.check(v.mixOpen() && !v.journeyOpen(), kind + ": and Mix closes Journey");
        v.pressEscapeForTest();
        s.check(!v.mixOpen(), kind + ": Esc closes it");
        v.toggleMix();
        // Closed and reopened with the panel open (the host deletes the editor): it comes
        // back open. (The first version crashed here in a real host: the view, destroyed
        // after the instrument panel, told it the panel's state.)
        w.close();
        w.open();
        s.check(w.view().mixOpen() && (!instrument || w.editor->instrumentPanel()->mixButton().getToggleState()),
                kind + ": a closed and reopened window comes back with the MIX panel open");
        w.close();
        w.open();
        w.view().toggleMix();
        w.close();
        w.open();
        s.check(!w.view().mixOpen(), kind + ": and closed when it was closed");
        auto& panel = w.view().mixPanelForTest();
        bool labels = panel.labelForTest(0) == "Reverb / Space" && panel.labelForTest(7) == "Duck";
        s.check(labels, kind + ": its eight rows (Reverb / Space ... Duck)");
    }
}

void testMix(Suite& s) {
    testMixWindow(s);
    testMixGroups(s);
    testMixHelpers(s);
    testMixSmooth(s);
    testMixSessions(s);
    testMixInstrument(s);
}

// Listening renders (CI test-logs): a four-instrument mini-song (brass, strings, choir,
// drums) before (the sound before Session 11) and after (the new mix defaults, the brass,
// Punch), and with Session 12's horns; and Session 11's brass with Punch 0 against its own.
int listenMix(const juce::String& directory) {
    using mixtests::Take;
    juce::File dir(directory);
    dir.createDirectory();
    constexpr double bpm = 100, beat = 60.0 / bpm, songSeconds = 8 * 4 * beat + 3.0;
    struct Part { int instrument; std::function<void(InstHost&)> notes; float level; };
    const std::vector<Part> parts {
        // Brass Fanfare stabs: a chord on beats 1 and 3 (short), i-VI-III-VII in C minor.
        { 6, [beat](InstHost& h) {
              const int chords[4][3] { { 60, 63, 67 }, { 56, 60, 63 }, { 63, 67, 70 }, { 58, 62, 65 } };
              for (int bar = 0; bar < 8; ++bar)
                  for (int hit : { 0, 2 })
                      for (int k : chords[bar % 4]) h.note(0.2 + (bar * 4 + hit) * beat, 0.35 * beat, k, hit == 0 ? 0.95f : 0.75f);
          }, -4.0f },
        // Royal Strings: the chords held, an octave lower.
        { 7, [beat](InstHost& h) {
              const int chords[4][3] { { 48, 55, 63 }, { 44, 51, 60 }, { 51, 58, 67 }, { 46, 53, 62 } };
              for (int bar = 0; bar < 8; ++bar)
                  for (int k : chords[bar % 4]) h.note(0.2 + bar * 4 * beat, 4 * beat * 0.97, k, 0.7f);
          }, -6.0f },
        // Soft Choir: the top line, two bars a note.
        { 10, [beat](InstHost& h) {
              const int line[4] { 67, 68, 70, 67 };
              for (int k = 0; k < 4; ++k) h.note(0.2 + k * 8 * beat, 8 * beat * 0.96, line[k], 0.6f);
          }, -8.0f },
        // War Drums: low on 1 and 3, higher on 2 and 4, a fill every fourth bar.
        { 30, [beat](InstHost& h) {
              for (int bar = 0; bar < 8; ++bar) {
                  for (int b = 0; b < 4; ++b) h.note(0.2 + (bar * 4 + b) * beat, 0.2, b % 2 == 0 ? 40 : 58, b % 2 == 0 ? 0.95f : 0.7f);
                  if (bar % 4 == 3) for (int e = 0; e < 4; ++e) h.note(0.2 + (bar * 4 + 3 + e * 0.25) * beat, 0.1, 62 + e, 0.6f + 0.1f * e);
              }
          }, -3.0f },
    };
    const auto renderSong = [&](int sound) {
        std::vector<float> l(std::size_t(songSeconds * 48000), 0.0f), r(l.size(), 0.0f);
        for (const auto& part : parts) {
            InstHost h;
            h.p->setSoundGeneration(sound);
            h.play(part.instrument);
            h.set(dc::ids::instLevel, part.level);
            h.render(0.05);
            part.notes(h);
            h.render(songSeconds - 0.05);
            for (std::size_t i = 0; i < l.size() && i < h.outL.size(); ++i) { l[i] += h.outL[i]; r[i] += h.outR[i]; }
        }
        return Take { l, r };
    };
    const auto write = [&](const Take& t, const char* name) {
        float peak = 0;
        for (std::size_t i = 0; i < t.l.size(); ++i) peak = std::max({ peak, std::abs(t.l[i]), std::abs(t.r[i]) });
        const float g = peak > 0.89f ? 0.89f / peak : 1.0f;
        juce::AudioBuffer<float> b(2, int(t.l.size()));
        for (int i = 0; i < b.getNumSamples(); ++i) { b.setSample(0, i, t.l[std::size_t(i)] * g); b.setSample(1, i, t.r[std::size_t(i)] * g); }
        auto file = dir.getChildFile(name);
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
        if (auto* writer = wav.createWriterFor(stream.get(), 48000, 2, 24, {}, 0)) {
            stream.release();
            std::unique_ptr<juce::AudioFormatWriter> w(writer);
            w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
        }
        std::cout << "wrote " << file.getFullPathName() << "\n";
    };
    // Session 12: "after" stays Session 11's sound (the same file as Session 11 rendered, byte
    // for byte); the third is Session 12's horns.
    write(renderSong(dc::inst::SoundClassic), "mini-song-1-before.wav");
    write(renderSong(dc::inst::SoundMix), "mini-song-2-after.wav");
    write(renderSong(dc::inst::SoundHorns), "mini-song-3-session12.wav");
    for (float punch : { 0.0f, 1.0f }) {
        InstHost h;
        h.p->setSoundGeneration(dc::inst::SoundMix);
        h.play(6);
        h.set(dc::ids::instPunch, punch);
        h.render(0.05);
        for (int k = 0; k < 8; ++k) for (int key : { 60, 64, 67 }) h.note(0.2 + k * 0.6, 0.4, key + (k % 4 == 3 ? 5 : 0), 0.9f);
        h.render(6.0);
        write({ h.outL, h.outR }, punch == 0.0f ? "brass-punch-0.wav" : "brass-punch-default.wav");
    }
    return 0;
}

// docs/snapshots/session11: the MIX panel (FX and instrument), the instrument panel with
// Punch, the Advanced drawer's Atmosphere group.
int mixSnapshots(const juce::String& directory) {
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
        windowtests::Window w(false);
        w.go(dc::Castle, 1);
        auto set = [&](const char* id, float v) { auto* q = w.p->parameters.getParameter(id); q->setValueNotifyingHost(q->convertTo0to1(v)); };
        set(dc::ids::mixGroups[0], 0.6f);
        set(dc::ids::mixLowCut, 220.0f);
        set(dc::ids::mixDuck, 0.4f);
        w.view().setAnimation(false);
        w.view().toggleMix();
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s11-fx-mix-panel.png");
        save(w.editor->createComponentSnapshot(w.view().mixPanelForTest().getBoundsInParent(), true, 2.0f), "s11-fx-mix-panel-2x.png");
        w.view().toggleMix();
        nav::click(w.view().advancedButton());
        if (auto* vp = w.view().drawerViewportForTest()) vp->setViewPosition(0, 100000);
        save(w.editor->createComponentSnapshot(w.view().drawerForTest().getBoundsInParent(), true, 1.0f), "s11-advanced-atmosphere.png");
    }
    {
        windowtests::Window w(true);
        w.go(dc::Castle, 1);   // the Brass Fanfare
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        for (int k = 0; k < 4; ++k) w.p->processBlock(buffer, midi);
        if (auto* panel = w.editor->instrumentPanel()) panel->pollForTest();
        w.view().setAnimation(false);
        if (auto* panel = w.editor->instrumentPanel()) nav::click(panel->mixButton());
        save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s11-instrument-mix.png");
        if (auto* panel = w.editor->instrumentPanel()) save(w.editor->createComponentSnapshot(panel->getBounds(), true, 2.0f), "s11-instrument-panel-punch.png");
    }
    return written == 5 ? 0 : 1;
}
