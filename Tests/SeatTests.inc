// Session 14: the brass and the War Drums sit in the world (included by
// DungeonCrawlerTests.cpp, group "seat", CI shard "13-seat"). Sound 13 against 14:
//   level      the brass and the War Drums about 2.5 dB lower (dry melody, the instruments'
//              level measure), every instrument still within the level-match target (3 dB of
//              the median), every other instrument unchanged sample for sample
//   hit        the brass's attack still clearly above its sustain (the punch kept), its Punch
//              a little lower with the same shape
//   tone       less high-frequency energy (above 7 kHz), more so played softly; the 2-4 kHz
//              presence kept; a little wider; the hit rounded, the held note not
//   space      more of the place's reverb on the held note, none more on the first 50 ms; the
//              phrase in its place changes as it does dry
//   clean      never above full scale, Punch 200 % + Level +12 dB never clips
//   sessions   a sound-13 session opens at 14: the defaults become the new defaults, a set
//              Level and Punch are kept as heard; 10-12 unchanged
//
//   --seat-report             the numbers behind the checks
//   --seat-calibrate          the level offsets and make-ups by bisection (prints them)
//   --listen-seat dir         the brass and the War Drums before (13) and after (14), alone
//                             (dry and in its place) and in a short mix with choir and strings

namespace seat {
using namespace brasstests;

struct Member { int index; int key; const char* file; bool whole; };
// `whole`: the instrument's whole level is lowered (else only its brass layer).
const std::array<Member, 6> members { {
    { 9, 60, "heroic-horn", true },             // Tower Top
    { 6, 60, "brass-fanfare", true },           // Great Hall
    { 4, 48, "doom-brass-and-choir", true },    // Final Boss Lair
    { 32, 60, "dwarven-chant", false },         // Dwarven Hall (its low brass)
    { 21, 45, "hoard-gong", false },            // Dragon's Hoard (its growling brass)
    { 30, 40, "war-drums", true },              // Mountain Pass
} };
bool brassy(int index) { return index != 30; }
constexpr int before = dc::inst::SoundRealBrass, after = dc::inst::SoundSeated;

// A 4th-order Butterworth high-pass (two RBJ biquads) at `hz`, 48 kHz: the energy above it
// against the whole, in dB.
double bandAbove(const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to, double hz) {
    to = std::min(to, l.size());
    const double w = 2.0 * 3.14159265358979 * hz / 48000.0, cw = std::cos(w), sw = std::sin(w);
    double total = 0, high = 0;
    for (int ch = 0; ch < 2; ++ch) {
        const auto& x = ch == 0 ? l : r;
        double z[2][4] {};
        for (std::size_t i = 0; i < to; ++i) {
            double y = x[i];
            for (int st = 0; st < 2; ++st) {
                const double q = st == 0 ? 0.5412 : 1.3066, a = sw / (2.0 * q);
                const double b0 = (1 + cw) / 2, b1 = -(1 + cw), b2 = (1 + cw) / 2, a0 = 1 + a, a1 = -2 * cw, a2 = 1 - a;
                const double out = (b0 * y + b1 * z[st][0] + b2 * z[st][1] - a1 * z[st][2] - a2 * z[st][3]) / a0;
                z[st][1] = z[st][0]; z[st][0] = y; z[st][3] = z[st][2]; z[st][2] = out;
                y = out;
            }
            if (i >= from) { high += y * y; total += double(x[i]) * x[i]; }
        }
    }
    return 10.0 * std::log10(std::max(high, 1.0e-30) / std::max(total, 1.0e-30));
}
double sideToMid(const Played& p, double from, double to) {
    const std::size_t a = p.onset + std::size_t(from * 48000), b = std::min(p.l.size(), p.onset + std::size_t(to * 48000));
    double m = 0, sd = 0;
    for (std::size_t i = a; i < b; ++i) {
        const double mid = 0.5 * (p.l[i] + p.r[i]), side = 0.5 * (p.l[i] - p.r[i]);
        m += mid * mid;
        sd += side * side;
    }
    return 10.0 * std::log10(std::max(sd, 1.0e-30) / std::max(m, 1.0e-30));
}
double phraseLevel(int k, int sound, bool dry) {
    const auto p = perform(k, sound, dry, brassmetrics::hornPhrase(), brassmetrics::hornPhraseSeconds());
    double sum = 0;
    for (std::size_t i = p.onset; i < p.l.size(); ++i) sum += 0.5 * (double(p.l[i]) * p.l[i] + double(p.r[i]) * p.r[i]);
    return 10.0 * std::log10(std::max(sum, 1.0e-20) / double(p.l.size() - p.onset));
}
// The reverb on a held note in its place: the place against the same with MIX Reverb 0 %
// (Session 12's measure), in a window from the onset (seconds): what the reverb adds there,
// against the note itself, in dB.
double reverbShare(int k, int key, int sound, double from, double to) {
    const auto take = [&](float reverb) {
        InstHost h;
        h.p->setSoundGeneration(sound);
        h.play(k);
        h.set(dc::ids::mixGroups[0], reverb);
        h.render(0.4);
        h.note(0.4, 1.2, key, 0.9f);
        h.render(1.8);
        return std::make_pair(h.outL, h.outR);
    };
    const auto placed = take(1.0f), none = take(0.0f);
    const std::size_t a = 19200 + std::size_t(from * 48000), b = 19200 + std::size_t(to * 48000);
    double xy = 0, yy = 0, oo = 0;
    for (std::size_t i = a; i < b; ++i) {
        xy += double(placed.first[i]) * none.first[i] + double(placed.second[i]) * none.second[i];
        yy += double(none.first[i]) * none.first[i] + double(none.second[i]) * none.second[i];
        oo += double(placed.first[i]) * placed.first[i] + double(placed.second[i]) * placed.second[i];
    }
    const double g = xy / std::max(yy, 1.0e-20), note = g * g * yy, rest = std::max(oo - note, 1.0e-20);
    return 10.0 * std::log10(rest / std::max(note, 1.0e-20));
}
// The drums' hit: a low stroke, its loudest 5 ms against 150-250 ms after the onset.
double drumHit(int sound) {
    const auto p = perform(30, sound, true, { { 0.0, 0.2, 40, 0.95f } }, 0.6);
    const auto env = brassmetrics::envelope({ p.l, p.r, 48000.0, p.onset }, 5.0, 0.3);
    double peak = 0, tail = 0;
    for (std::size_t i = 0; i < env.size(); ++i) {
        if (i < 20) peak = std::max(peak, env[i]);
        if (i >= 30 && i < 50) tail = std::max(tail, env[i]);
    }
    return brassmetrics::toDb(peak) - brassmetrics::toDb(tail);
}
} // namespace seat

void testSeat(Suite& s) {
    using namespace seat;
    using namespace dc::inst;
    const bool recorded = realbrass::haveBrass();
    if (!recorded && realbrass::required()) s.check(false, "the recorded brass is in the build (DC_REQUIRE_BRASS)");
    std::cout << "    (" << (recorded ? "the recorded brass" : "no recorded brass: the synthesis") << ")\n";

    s.section("seat: the brass and the War Drums about 2.5 dB lower, level-matched");
    std::vector<double> levels(static_cast<std::size_t>(instrumentCount));
    for (int k = 0; k < instrumentCount; ++k) levels[std::size_t(k)] = realbrass::melodyLevel(k, after, false);
    std::vector<double> sorted = levels;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[sorted.size() / 2];
    for (const auto& m : members) {
        const std::string name = design(m.index).name;
        const double was = realbrass::melodyLevel(m.index, before, false), now = levels[std::size_t(m.index)];
        s.metric(name + ": dry melody, sound 13 -> 14", now - was, " dB");
        if (m.whole)
            s.check(now - was <= -1.5 && now - was >= -3.5, name + ": about 2.5 dB lower (1.5-3.5 dB, dry melody)", fmt(was) + " -> " + fmt(now) + " dB");
        else
            s.check(now - was <= 0.1 && now - was >= -3.0, name + ": its brass lower, the instrument not louder", fmt(was) + " -> " + fmt(now) + " dB");
        s.check(std::abs(now - median) < 3.0, name + ": within the level-match target (3 dB of the median)", fmt(now - median) + " dB");
    }
    {
        // Nothing else moves: every other instrument plays sound 14 exactly as 13.
        std::string moved;
        for (int k = 0; k < instrumentCount; ++k) {
            bool member = false;
            for (const auto& m : members) member |= m.index == k;
            if (member) continue;
            const auto a = perform(k, before, false, held({ 48, 60, 67 }, 0.8f), 1.2), b = perform(k, after, false, held({ 48, 60, 67 }, 0.8f), 1.2);
            if (a.l != b.l || a.r != b.r) moved += std::string(design(k).name) + "; ";
        }
        s.check(moved.empty(), "every other instrument plays sound 14 sample for sample as 13", moved);
    }

    s.section("seat: the hit still lands (the punch kept)");
    for (const auto& m : members) {
        if (!brassy(m.index) || m.index == 21) continue;
        const std::string name = design(m.index).name;
        const auto b = chord(m.index, before).m(), a = chord(m.index, after).m();
        std::cout << "    " << name << " 13: " << line(b) << "\n    " << name << " 14: " << line(a) << "\n";
        const double floor = m.index == 32 ? 6.0 : 10.0;
        s.check(a.attackToSustainDb >= floor, name + ": the attack clearly above its sustain (" + fmt(floor, 0) + " dB or more)", fmt(a.attackToSustainDb) + " dB");
        s.check(a.attackToSustainDb >= b.attackToSustainDb - 3.0, name + ": the punch kept (attack-to-sustain within 3 dB of sound 13's)",
                fmt(b.attackToSustainDb) + " -> " + fmt(a.attackToSustainDb) + " dB");
        // As quickly: within 3 dB of its peak by 12 ms or as soon as at 13 (+3 ms), its rise
        // no slower than LIGHT SEEKER's Heroic Horns' (12 ms) + 2. A lower hit is less
        // flattened by the hit clipper, so its peak frame can come a frame or two later.
        s.check((a.hitMs <= b.hitMs + 3.0 || a.hitMs <= 12.0) && a.riseMs <= std::max(b.riseMs + 2.0, 14.0), name + ": the hit arrives as quickly",
                "within 3 dB " + fmt(b.hitMs, 0) + " -> " + fmt(a.hitMs, 0) + " ms, rise " + fmt(b.riseMs, 0) + " -> " + fmt(a.riseMs, 0) + " ms");
        s.check(a.presenceDb >= b.presenceDb - 1.5, name + ": its 2-4 kHz presence kept (within 1.5 dB)", fmt(b.presenceDb) + " -> " + fmt(a.presenceDb) + " dB");
        s.check(a.peakDb <= -0.1, name + ": never above full scale (dry chord, velocity 0.95)", fmt(a.peakDb) + " dBFS");
    }
    {
        const double b = drumHit(before), a = drumHit(after);
        s.metric("War Drums: the stroke over its ring, 13 -> 14", a - b, " dB");
        s.check(a >= b - 1.5, "War Drums: the stroke still lands (its hit over its ring within 1.5 dB of sound 13's)", fmt(b) + " -> " + fmt(a) + " dB");
    }
    {
        bool lower = true, sameShape = true, sameMost = true;
        std::string detail;
        for (int k : { 9, 6, 4, 32, 30 }) {
            lower &= designPunchDb(k, after) < designPunchDb(k, before) && designPunchDb(k, after) >= 0.75f * designPunchDb(k, before);
            sameMost &= punchMaxDb(k, after) == punchMaxDb(k, before);
            sameShape &= punchVelocityScale(k, 0.7f, after) == punchVelocityScale(k, 0.7f, before) && brassPresence(k, after) == brassPresence(k, before);
            detail += std::string(design(k).name) + " " + fmt(designPunchDb(k, before)) + " -> " + fmt(designPunchDb(k, after)) + " dB; ";
        }
        s.check(lower, "Punch 100 %: a little lower on the brass and the War Drums (by at most a quarter)", detail);
        s.check(sameShape && sameMost, "the Punch's shape, velocity response and most (200 %) unchanged");
    }

    s.section("seat: a softer top, warmer, a little wider");
    for (const auto& m : members) {
        if (!brassy(m.index)) continue;
        const std::string name = design(m.index).name;
        const int key = m.index == 21 ? 45 : 60;
        const auto loud13 = perform(m.index, before, true, held({ key, key + 4, key + 7 }, 0.95f), 1.8), loud14 = perform(m.index, after, true, held({ key, key + 4, key + 7 }, 0.95f), 1.8);
        const auto soft13 = perform(m.index, before, true, held({ key, key + 4, key + 7 }, 0.45f), 1.8), soft14 = perform(m.index, after, true, held({ key, key + 4, key + 7 }, 0.45f), 1.8);
        const auto hf = [](const Played& p) { return bandAbove(p.l, p.r, p.onset, p.onset + 72000, 7000.0); };
        // Played softly: on the held part (0.5-1.3 s; the loud hit's own clipping at 13 is
        // not in it).
        const auto hfHeld = [](const Played& p) { return bandAbove(p.l, p.r, p.onset + 24000, p.onset + 62400, 7000.0); };
        const double loudDrop = hf(loud14) - hf(loud13), softDrop = hf(soft14) - hf(soft13);
        const double loudHeldDrop = hfHeld(loud14) - hfHeld(loud13), softHeldDrop = hfHeld(soft14) - hfHeld(soft13);
        s.metric(name + ": energy above 7 kHz (share), loud 13 -> 14", loudDrop, " dB");
        s.metric(name + ": energy above 7 kHz (share), soft 13 -> 14", softDrop, " dB");
        const double needed = m.index == 32 ? 0.75 : m.index == 21 ? 0.5 : 1.5;
        s.check(loudDrop <= -needed, name + ": less high-frequency energy (above 7 kHz, " + fmt(needed, 2) + " dB or more)", fmt(loudDrop) + " dB");
        if (m.index != 21)
            s.check(softHeldDrop <= loudHeldDrop - 0.3, name + ": darker still played softly (the held part above 7 kHz, velocity 0.45 against 0.95)",
                    "soft " + fmt(softHeldDrop) + ", loud " + fmt(loudHeldDrop) + " dB");
        const double w13 = sideToMid(loud13, 0.2, 1.3), w14 = sideToMid(loud14, 0.2, 1.3);
        s.check(w14 >= w13 + 0.5, name + ": a little wider (side against mid, +0.5 dB or more)", fmt(w13) + " -> " + fmt(w14) + " dB");
        // The held note's level unchanged by the warmth (it is the trim that moves it).
        const double held13 = brassmetrics::toDb(brassmetrics::rmsBetween({ loud13.l, loud13.r, 48000.0, loud13.onset }, 0.8, 1.3));
        const double held14 = brassmetrics::toDb(brassmetrics::rmsBetween({ loud14.l, loud14.r, 48000.0, loud14.onset }, 0.8, 1.3));
        s.metric(name + ": held level, 13 -> 14", held14 - held13, " dB");
    }
    {
        // The warmth: identity at zero, under 0.1 dB at -15 dBFS, a dB or two off a hit at -3.
        // As applied: half of the curve's difference (render, the seat amount 1).
        const auto heard = [](float x) { return (x + 0.5f * (seatWarmth(x) - x)) / x; };
        const float quiet = heard(0.178f), hot = heard(0.708f);
        s.check(std::abs(20.0 * std::log10(quiet)) < 0.1 && 20.0 * std::log10(hot) < -0.4 && 20.0 * std::log10(hot) > -2.0 && seatWarmth(0.0f) == 0.0f,
                "the tape warmth is gentle (the held level untouched, the hit rounded)", fmt(20.0 * std::log10(quiet), 2) + " / " + fmt(20.0 * std::log10(hot), 2) + " dB");
    }

    s.section("seat: the place's space on the sustain, not on the attack");
    for (const auto& m : members) {
        if (!brassy(m.index) || m.index == 21) continue;
        const std::string name = design(m.index).name;
        const int key = m.index == 4 ? 48 : 60;
        const double hit13 = reverbShare(m.index, key, before, 0.0, 0.05), hit14 = reverbShare(m.index, key, after, 0.0, 0.05);
        const double sus13 = reverbShare(m.index, key, before, 0.35, 1.1), sus14 = reverbShare(m.index, key, after, 0.35, 1.1);
        s.metric(name + ": the reverb on the first 50 ms, 13 -> 14", hit14 - hit13, " dB");
        s.check(hit14 <= -25.0, name + ": the first 50 ms stay dry (what the reverb adds there 25 dB or more under the note)", fmt(hit13) + " -> " + fmt(hit14) + " dB");
        if (m.index == 32) {   // its brass is a layer under the voices, which carry most of its held note: its space as at 13
            const auto a = mixFit(32, before), b = mixFit(32, after);
            s.check(a.reverb == b.reverb && a.echoes == b.echoes && a.duck == b.duck && a.preDelayMs == b.preDelayMs && a.makeupDb == b.makeupDb,
                    name + ": its place as at sound 13 (its voices carry the held note)", fmt(sus13) + " -> " + fmt(sus14) + " dB");
        } else
            s.check(sus14 >= sus13 + 0.75, name + ": more of the place on the held note (+0.75 dB or more)", fmt(sus13) + " -> " + fmt(sus14) + " dB");
        if (m.index != 32) s.check(mixFit(m.index, after).preDelayMs >= 50.0f, name + ": the reverb starts 50 ms or more after the hit");
    }
    for (const auto& m : members) {
        if (!brassy(m.index) || m.index == 21) continue;
        const std::string name = design(m.index).name;
        const double dry = phraseLevel(m.index, after, true) - phraseLevel(m.index, before, true);
        const double placed = phraseLevel(m.index, after, false) - phraseLevel(m.index, before, false);
        s.check(std::abs(placed - dry) < 0.75, name + ": the phrase in its place changes as it does dry (within 0.75 dB)", "dry " + fmt(dry) + ", in its place " + fmt(placed) + " dB");
    }

    s.section("seat: clean");
    for (const auto& m : members) {
        const std::string name = design(m.index).name;
        for (bool dry : { true, false }) {
            const auto p = m.index == 30 ? perform(30, after, dry, { { 0.0, 0.2, 40, 1.0f }, { 0.25, 0.2, 58, 0.8f }, { 0.5, 0.2, 40, 1.0f }, { 0.75, 0.2, 62, 0.9f } }, 2.0)
                                         : perform(m.index, after, dry, brassmetrics::hornPhrase(), brassmetrics::hornPhraseSeconds());
            bool finite = true;
            for (std::size_t i = 0; i < p.l.size(); ++i) finite &= std::isfinite(p.l[i]) && std::isfinite(p.r[i]);
            const auto r = p.m();
            s.check(finite && r.peakDb <= -0.1, name + (dry ? ": dry" : ": in its place") + ", finite and never above -0.1 dBFS", fmt(r.peakDb) + " dBFS");
        }
    }
    {
        std::string fail;
        for (int i : { 9, 6, 4, 32 }) {
            std::vector<brassmetrics::PhraseNote> notes;
            for (int k : { 48, 55, 60, 64, 67, 72 }) notes.push_back({ 0.0, 1.0, k, 1.0f });
            const auto p = perform(i, after, false, notes, 2.0, 2.0f, 12.0f);
            if (p.m().peakDb > 0.0) fail += std::string(design(i).name) + " (" + fmt(p.m().peakDb) + " dBFS); ";
        }
        s.check(fail.empty(), "brass at Punch 200 %, Level +12 dB, velocity 1: never clips", fail);
    }

    s.section("seat: sessions");
    const auto saved = [](int sound, int instrument, float level, float punch) {
        DungeonCrawlerProcessor p(PluginKind::Instrument);
        p.setSoundGeneration(sound);
        const auto set = [&p](const char* id, float v) { auto* q = p.parameters.getParameter(id); q->setValueNotifyingHost(q->convertTo0to1(v)); };
        set(dc::ids::instLock, 1);
        set(dc::ids::instrument, float(instrument));
        set(dc::ids::instLevel, level);
        set(dc::ids::instPunch, punch);
        juce::MemoryBlock block;
        p.getStateInformation(block);
        return block;
    };
    const auto value = [](DungeonCrawlerProcessor& p, const char* id) { auto* q = p.parameters.getParameter(id); return q->convertFrom0to1(q->getValue()); };
    {
        DungeonCrawlerProcessor fresh(PluginKind::Instrument);
        s.check(fresh.soundGenerationUsed() == SoundSeated && SoundLatest == SoundSeated, "a new instance plays sound 14");
    }
    {
        const auto block = saved(before, 9, 0.0f, 1.0f);
        DungeonCrawlerProcessor r(PluginKind::Instrument);
        r.setStateInformation(block.getData(), int(block.getSize()));
        s.check(r.soundGenerationUsed() == after && value(r, dc::ids::instLevel) == 0.0f && std::abs(value(r, dc::ids::instPunch) - 1.0f) < 1.0e-4f,
                "a sound-13 session at the defaults opens at 14 with the new defaults (Level 0 dB, Punch 100 %)");
    }
    {
        const auto block = saved(before, 9, -3.0f, 1.37f);
        DungeonCrawlerProcessor r(PluginKind::Instrument);
        r.setStateInformation(block.getData(), int(block.getSize()));
        const float level = value(r, dc::ids::instLevel), punch = value(r, dc::ids::instPunch);
        const float heardBefore = punchDb(9, 1.37f, before), heardNow = punchDb(9, punch, after);
        s.check(r.soundGenerationUsed() == after && std::abs(level - (-3.0f - seatLevelDb(9, after))) < 0.05f && std::abs(heardNow - heardBefore) < 0.05f,
                "a sound-13 session with its Level and Punch set keeps them as heard (the Level +2.5 dB, the same dB on the hit)",
                "Level " + fmt(level, 2) + " dB, Punch " + fmt(punch, 3) + " (" + fmt(heardBefore, 2) + " -> " + fmt(heardNow, 2) + " dB)");
        // The horn as heard: the dry melody at the kept Level within 1 dB of sound 13's.
        InstHost a, b;
        a.p->setStateInformation(block.getData(), int(block.getSize()));
        b.p->setSoundGeneration(before);
        b.play(9);
        b.set(dc::ids::instLevel, -3.0f);
        b.set(dc::ids::instPunch, 1.37f);
        for (auto* h : { &a, &b }) { h->dry(); h->render(0.4); melody(*h, 0.45); h->render(0.05 + melodySeconds + 1.5); }
        const double gap = loudness(a.outL, a.outR, a.sr) - loudness(b.outL, b.outR, b.sr);
        s.check(std::abs(gap) < 1.0, "and it sounds as loud as it did (within 1 dB)", fmt(gap) + " dB");
    }
    {
        const auto block = saved(before, 10, -3.0f, 0.5f);
        DungeonCrawlerProcessor r(PluginKind::Instrument);
        r.setStateInformation(block.getData(), int(block.getSize()));
        s.check(r.soundGenerationUsed() == after && std::abs(value(r, dc::ids::instLevel) + 3.0f) < 1.0e-3f && std::abs(value(r, dc::ids::instPunch) - 0.5f) < 1.0e-3f,
                "a sound-13 session on another instrument keeps its Level and Punch as they were");
    }
    {
        const auto block = saved(before, 30, -6.0f, 1.0f);
        DungeonCrawlerProcessor r(PluginKind::Instrument);
        r.setStateInformation(block.getData(), int(block.getSize()));
        s.check(std::abs(value(r, dc::ids::instLevel) - (-6.0f - dc::inst::seatLevelDb(30, after))) < 0.05f && std::abs(value(r, dc::ids::instPunch) - 1.0f) < 1.0e-4f,
                "the War Drums: a set Level kept as heard, Punch at its default gets the new default", fmt(value(r, dc::ids::instLevel), 2) + " dB");
    }
    for (int sound : { int(SoundMix), int(SoundHorns) }) {
        const auto block = saved(sound, 9, -3.0f, 1.37f);
        DungeonCrawlerProcessor r(PluginKind::Instrument);
        r.setStateInformation(block.getData(), int(block.getSize()));
        s.check(r.soundGenerationUsed() == sound && std::abs(value(r, dc::ids::instLevel) + 3.0f) < 1.0e-3f && std::abs(value(r, dc::ids::instPunch) - 1.37f) < 1.0e-3f,
                "a session saved with sound " + std::to_string(sound) + " keeps it, its Level and its Punch");
    }
    {
        const auto block = saved(after, 9, -3.0f, 1.37f);
        DungeonCrawlerProcessor r(PluginKind::Instrument);
        r.setStateInformation(block.getData(), int(block.getSize()));
        s.check(r.soundGenerationUsed() == after && std::abs(value(r, dc::ids::instLevel) + 3.0f) < 1.0e-3f && std::abs(value(r, dc::ids::instPunch) - 1.37f) < 1.0e-3f,
                "a sound-14 session reopens exactly (no second move)");
    }
}

int seatReport() {
    using namespace seat;
    using namespace dc::inst;
    if (auto lv = juce::SystemStats::getEnvironmentVariable("DC_SEAT_LEVEL", {}); lv.isNotEmpty())
        for (const auto& m : members) overrideSeatDb(m.index, lv.getFloatValue(), std::numeric_limits<float>::quiet_NaN());
    std::cout << "Session 14 (sound 13 -> 14), " << (realbrass::haveBrass() ? "the recorded brass" : "no recorded brass") << "\n";
    for (const auto& m : members) {
        const std::string name = design(m.index).name;
        std::cout << name << "\n  dry melody " << fmt(realbrass::melodyLevel(m.index, before, false)) << " -> " << fmt(realbrass::melodyLevel(m.index, after, false))
                  << " dB; in its place " << fmt(realbrass::melodyLevel(m.index, before, true)) << " -> " << fmt(realbrass::melodyLevel(m.index, after, true)) << " dB\n";
        std::cout << "  Punch " << fmt(designPunchDb(m.index, before)) << " -> " << fmt(designPunchDb(m.index, after)) << " dB\n";
        if (m.index == 30) {
            std::cout << "  stroke over ring " << fmt(drumHit(before)) << " -> " << fmt(drumHit(after)) << " dB\n";
            continue;
        }
        const int key = m.index == 21 ? 45 : 60;
        const auto b = chord(m.index, before).m(), a = chord(m.index, after).m();
        std::cout << "  chord 13: " << line(b) << "\n  chord 14: " << line(a) << "\n";
        const auto l13 = perform(m.index, before, true, held({ key, key + 4, key + 7 }, 0.95f), 1.8), l14 = perform(m.index, after, true, held({ key, key + 4, key + 7 }, 0.95f), 1.8);
        std::cout << "  above 7 kHz " << fmt(bandAbove(l13.l, l13.r, l13.onset, l13.onset + 72000, 7000.0)) << " -> " << fmt(bandAbove(l14.l, l14.r, l14.onset, l14.onset + 72000, 7000.0))
                  << " dB; side/mid " << fmt(sideToMid(l13, 0.2, 1.3)) << " -> " << fmt(sideToMid(l14, 0.2, 1.3)) << " dB\n";
        if (m.index != 21) {
            const int k = m.index == 4 ? 48 : 60;
            std::cout << "  reverb on the first 50 ms " << fmt(reverbShare(m.index, k, before, 0.0, 0.05)) << " -> " << fmt(reverbShare(m.index, k, after, 0.0, 0.05))
                      << " dB, on the held note " << fmt(reverbShare(m.index, k, before, 0.35, 1.1)) << " -> " << fmt(reverbShare(m.index, k, after, 0.35, 1.1)) << " dB\n";
            const double dry = phraseLevel(m.index, after, true) - phraseLevel(m.index, before, true), placed = phraseLevel(m.index, after, false) - phraseLevel(m.index, before, false);
            std::cout << "  horn phrase 13 -> 14: dry " << fmt(dry) << " dB, in its place " << fmt(placed) << " dB\n";
        }
    }
    return 0;
}

// The level offsets (the dry melody 2.5 dB under sound 13's) and the make-ups (the phrase in
// its place changing as it does dry), by bisection; prints the values for Designs.cpp.
int seatCalibrate() {
    using namespace seat;
    using namespace dc::inst;
    for (const auto& m : members) {
        if (!m.whole) continue;
        const double target = realbrass::melodyLevel(m.index, before, false) - 2.5;
        double lo = -6.0, hi = 0.0;
        for (int it = 0; it < 12; ++it) {
            const double mid = 0.5 * (lo + hi);
            overrideSeatDb(m.index, float(mid), std::numeric_limits<float>::quiet_NaN());
            (realbrass::melodyLevel(m.index, after, false) > target ? hi : lo) = mid;
        }
        const float level = float(0.5 * (lo + hi));
        overrideSeatDb(m.index, level, std::numeric_limits<float>::quiet_NaN());
        std::cout << design(m.index).name << ": level " << fmt(level, 2) << " dB\n";
    }
    for (const auto& m : members) {
        if (!brassy(m.index) || m.index == 21) continue;
        const float level = seatLevelDb(m.index, after);
        const double dry = phraseLevel(m.index, after, true) - phraseLevel(m.index, before, true), wasPlaced = phraseLevel(m.index, before, false);
        double lo = -3.0, hi = 6.0;
        for (int it = 0; it < 12; ++it) {
            const double mid = 0.5 * (lo + hi);
            overrideSeatDb(m.index, level, float(mid));
            (phraseLevel(m.index, after, false) - wasPlaced > dry ? hi : lo) = mid;
        }
        std::cout << design(m.index).name << ": make-up " << fmt(0.5 * (lo + hi), 2) << " dB\n";
    }
    return 0;
}

int listenSeat(const juce::String& directory) {
    using namespace seat;
    juce::File dir(directory);
    dir.createDirectory();
    const auto write = [&](const std::vector<float>& l, const std::vector<float>& r, const std::string& name) {
        brasstests::writeWav(dir.getChildFile(name), l, r);
        std::cout << "wrote " << dir.getChildFile(name).getFullPathName() << "\n";
    };
    // Alone: the horn phrase (the drums: a war beat), sound 13 then 14, dry and in its place.
    const std::vector<brassmetrics::PhraseNote> beat = [] {
        std::vector<brassmetrics::PhraseNote> n;
        for (int bar = 0; bar < 4; ++bar)
            for (int b = 0; b < 4; ++b) n.push_back({ (bar * 4 + b) * 0.6, 0.2, b % 2 == 0 ? 40 : 58, b % 2 == 0 ? 0.95f : 0.7f });
        for (int e = 0; e < 4; ++e) n.push_back({ (15 + e * 0.25) * 0.6, 0.1, 62 + e, 0.6f + 0.1f * float(e) });
        return n;
    }();
    for (const auto& m : members)
        for (bool dry : { true, false })
            for (int sound : { int(before), int(after) }) {
                std::vector<brassmetrics::PhraseNote> notes = m.index == 30 ? beat : brassmetrics::hornPhrase();
                if (m.index == 21) for (auto& n : notes) { n.key -= 12; n.velocity = 1.0f; }
                const auto p = perform(m.index, sound, dry, notes, m.index == 30 ? 12.0 : brassmetrics::hornPhraseSeconds());
                write(p.l, p.r, std::string(m.file) + (dry ? "-dry-" : "-in-its-place-") + (sound == before ? "1-before.wav" : "2-after.wav"));
            }
    // A short mix: choir, low strings, brass (fanfare stabs, a horn call), war drums, a drone,
    // each in its place at fixed levels, 8 bars at 90 bpm, sound 13 then 14.
    constexpr double bpm = 90, q = 60.0 / bpm, seconds = 8 * 4 * q + 3.0;
    struct Part { int instrument; float level; std::function<void(InstHost&)> notes; };
    const int chords[4][3] { { 50, 53, 57 }, { 46, 50, 53 }, { 48, 52, 55 }, { 45, 49, 52 } };   // D minor: i VI VII V
    const std::vector<Part> parts {
        { 10, -7.0f, [&](InstHost& h) { for (int bar = 0; bar < 8; ++bar) for (int k : chords[bar % 4]) h.note(0.2 + bar * 4 * q, 4 * q * 0.97, k + 12, 0.6f); } },     // Soft Choir
        { 7, -6.0f, [&](InstHost& h) { for (int bar = 0; bar < 8; ++bar) for (int k : chords[bar % 4]) h.note(0.2 + bar * 4 * q, 4 * q * 0.97, k - 12, 0.7f); } },       // Royal Strings
        { 6, -4.0f, [&](InstHost& h) { for (int bar = 0; bar < 8; ++bar) for (int hit : { 0, 3 }) for (int k : chords[bar % 4]) h.note(0.2 + (bar * 4 + hit) * q, 0.4 * q, k + 12, hit == 0 ? 0.95f : 0.75f); } },   // Brass Fanfare
        { 9, -5.0f, [&](InstHost& h) { const int call[4] { 62, 69, 67, 65 }; for (int bar = 1; bar < 8; bar += 2) h.note(0.2 + (bar * 4 + 1) * q, 2.5 * q, call[(bar / 2) % 4], 0.85f); } },   // Heroic Horn
        { 30, -3.0f, [&](InstHost& h) { for (int bar = 0; bar < 8; ++bar) { for (int b = 0; b < 4; ++b) h.note(0.2 + (bar * 4 + b) * q, 0.2, b % 2 == 0 ? 40 : 58, b % 2 == 0 ? 0.95f : 0.7f);
                                            if (bar % 4 == 3) for (int e = 0; e < 4; ++e) h.note(0.2 + (bar * 4 + 3 + e * 0.25) * q, 0.1, 62 + e, 0.6f + 0.1f * float(e)); } } },   // War Drums
        { 1, -10.0f, [&](InstHost& h) { h.note(0.2, 8 * 4 * q, 38, 0.6f); } },   // Cello Drone-Lead: the drone
    };
    for (int sound : { int(before), int(after) }) {
        std::vector<float> l(std::size_t(seconds * 48000), 0.0f), r(l.size(), 0.0f);
        for (const auto& part : parts) {
            InstHost h;
            h.p->setSoundGeneration(sound);
            h.play(part.instrument);
            h.set(dc::ids::instLevel, part.level);
            h.render(0.05);
            part.notes(h);
            h.render(seconds - 0.05);
            for (std::size_t i = 0; i < l.size() && i < h.outL.size(); ++i) { l[i] += h.outL[i]; r[i] += h.outR[i]; }
        }
        float peak = 0;
        for (std::size_t i = 0; i < l.size(); ++i) peak = std::max({ peak, std::abs(l[i]), std::abs(r[i]) });
        std::cout << "mix " << sound << " peak " << fmt(brassmetrics::toDb(peak)) << " dBFS\n";
        // The same gain for both (the loudest of the two at -1 dBFS would hide the change):
        // fixed at -6 dB.
        for (std::size_t i = 0; i < l.size(); ++i) { l[i] *= 0.5f; r[i] *= 0.5f; }
        write(l, r, sound == before ? "mix-choir-strings-brass-drums-1-before.wav" : "mix-choir-strings-brass-drums-2-after.wav");
    }
    return 0;
}
