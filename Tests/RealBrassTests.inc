// Session 13: the recorded brass (VSCO 2 Community Edition, CC0; included by
// DungeonCrawlerTests.cpp, group "realbrass", CI shard "12-realbrass").
//   licence    the licence recorded (docs/LICENSES, the DEVLOG) and shipped with the notes
//   sets       the eight prepared sets loaded (horn, trumpet, trombone, tuba; sustain and
//              staccato), every sustain looped
//   tuning     every set in tune across its range and dynamics; every brass instrument in tune
//   velocity   the level rises smoothly with velocity (no step at a layer change)
//   hit        attack-to-sustain and presence clearly up against sound 12 (the synthesis),
//              the overall level and the held level not up
//   levels     level-matched with the other instruments (dry), and in its place
//   clean      no clicks (loops, layer changes, releases), never above full scale
//   stand-in   without the samples (and in sound 12) the synthesis plays exactly as before
// Without the samples in the build the checks that need them are skipped, unless
// DC_REQUIRE_BRASS is set (CI sets it once the library is fetched).
//
//   --realbrass-report         the numbers behind the checks (and the trims' calibration)
//   --listen-realbrass dir     each brass instrument before (sound 12) and after (13),
//                              dry and in its place, as WAV

namespace realbrass {
using namespace brasstests;

// The brass instruments: design index, a key for single notes, the velocity that wakes
// the brass (the Hoard Gong growls only when struck hard).
struct Brass { int index; int key; const char* file; };
const std::array<Brass, 5> brass { {
    { 9, 60, "heroic-horn" },            // Tower Top
    { 6, 60, "brass-fanfare" },          // Great Hall
    { 4, 48, "doom-brass-and-choir" },   // Final Boss Lair
    { 32, 60, "dwarven-chant" },         // Dwarven Hall (its low brass)
    { 21, 45, "hoard-gong" },            // Dragon's Hoard (its growling brass)
} };
const char* const sets[8] { "brass-horn-sus", "brass-horn-stac", "brass-trumpet-sus", "brass-trumpet-stac",
                            "brass-trombone-sus", "brass-trombone-stac", "brass-tuba-sus", "brass-tuba-stac" };

bool haveBrass() {
    auto& bank = wb::samples::Bank::get();
    bank.load();
    bank.waitUntilLoaded(60000);
    return bank.instrument("brass-horn-sus") != nullptr;
}
bool required() { return juce::SystemStats::getEnvironmentVariable("DC_REQUIRE_BRASS", {}).isNotEmpty(); }

// The pitch of a mono stretch near an expected frequency: the normalised autocorrelation's
// peak at about k periods (k so that the lag is near 1000 samples: a high note's single
// period is too few samples to resolve cents), searched within half a period's worth of
// the expected lag (at most 3 %), refined by a parabola. Cents from `hz`.
double centsFrom(const std::vector<float>& x, double sr, double hz) {
    const double period = sr / hz;
    const int k = std::max(1, int(std::lround(1000.0 / period)));
    const double expected = period * double(k), spread = std::min(0.03, 0.45 / double(k));
    const int lo = int(std::floor(expected / (1.0 + spread))), hi = int(std::ceil(expected * (1.0 + spread)));
    const int n = int(x.size()) - hi - 1;
    if (n < 2048 || lo < 2) return 1.0e9;
    std::vector<double> nsdf(std::size_t(hi + 2), 0.0);
    for (int tau = lo - 1; tau <= hi + 1; ++tau) {
        double acf = 0, m = 0;
        for (int i = 0; i < n; ++i) {
            acf += double(x[std::size_t(i)]) * x[std::size_t(i + tau)];
            m += double(x[std::size_t(i)]) * x[std::size_t(i)] + double(x[std::size_t(i + tau)]) * x[std::size_t(i + tau)];
        }
        nsdf[std::size_t(tau)] = m > 0 ? 2.0 * acf / m : 0.0;
    }
    int best = lo;
    for (int tau = lo; tau <= hi; ++tau)
        if (nsdf[std::size_t(tau)] > nsdf[std::size_t(best)]) best = tau;
    const double a = nsdf[std::size_t(best - 1)], b = nsdf[std::size_t(best)], c = nsdf[std::size_t(best + 1)];
    const double shift = (a - 2 * b + c) != 0.0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0.0;
    const double found = sr * double(k) / (double(best) + shift);
    return 1200.0 * std::log2(found / hz);
}

std::vector<float> monoBetween(const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to) {
    to = std::min(to, l.size());
    std::vector<float> m;
    for (std::size_t i = from; i < to; ++i) m.push_back(0.5f * (l[i] + r[i]));
    return m;
}

// One prepared set through the brass voice alone (one player, no staccato, no bite): a note
// held `seconds`, released, at 48 kHz.
struct Voiced { std::vector<float> l, r; };
Voiced voiceSet(const char* id, int key, float velocity, double seconds, double tail = 0.6, bool asStaccato = false, float presence = 0, float hitPresence = 0, float stacWith = 0) {
    const auto* inst = wb::samples::Bank::get().instrument(id);
    Voiced out;
    if (inst == nullptr) return out;
    auto voice = std::make_unique<dc::inst::BrassVoice>();
    voice->reset();
    const auto* stacSet = stacWith > 0.0f ? wb::samples::Bank::get().instrument(juce::String(id).replace("-sus", "-stac").toRawUTF8()) : nullptr;
    voice->addPlayer(asStaccato ? nullptr : inst, asStaccato ? inst : stacSet, 0, 0, 1, 0);
    voice->stacLevel = asStaccato ? 1.0f : stacWith;
    voice->presence = presence;
    voice->hitPresence = hitPresence;
    if (!voice->start(float(key), velocity, 0.3f, 48000.0f, 0.001f)) return out;
    const int total = int((seconds + tail) * 48000.0), hold = int(seconds * 48000.0);
    for (int i = 0; i < total; ++i) {
        if (i % wb::dsp::chunk == 0) voice->update(0.0f);
        if (i == hold) voice->release(0.3f);
        float l = 0, r = 0;
        voice->tick(l, r);
        out.l.push_back(l);
        out.r.push_back(r);
    }
    return out;
}

double loudnessOf(const Voiced& v) { return loudness(v.l, v.r, 48000.0); }

// The largest sample-to-sample jump (second difference) against the signal's own: a click
// stands out of the hold by far more than a note's own movement does.
double clickRatio(const std::vector<float>& x, std::size_t from, std::size_t to, double* where = nullptr) {
    to = std::min(to, x.size());
    if (to < from + 4800) return 0.0;
    double peak = 0, sum = 0;
    std::size_t count = 0;
    for (std::size_t i = from + 2; i < to; ++i) {
        const double d2 = std::abs(double(x[i]) - 2.0 * x[i - 1] + x[i - 2]);
        if (d2 > peak && where != nullptr) *where = double(i) / 48000.0;
        peak = std::max(peak, d2);
        sum += d2 * d2;
        ++count;
    }
    const double rms = std::sqrt(sum / double(std::max<std::size_t>(count, 1)));
    return rms > 0 ? peak / rms : 0.0;
}

// The steepest front in the recordings a brass instrument plays (every take of its sets):
// a loud trombone's waveform steepens into near shock fronts (what makes it brassy), which
// the second difference sees like a click. A click is a jump the playback adds beyond those.
double recordingsSteepest(int index) {
    std::vector<const char*> ids;
    switch (index) {
        case 9: ids = { "brass-horn-sus", "brass-horn-stac" }; break;
        case 6: ids = { "brass-trumpet-sus", "brass-trumpet-stac", "brass-trombone-sus", "brass-trombone-stac" }; break;
        case 4: ids = { "brass-horn-sus", "brass-horn-stac", "brass-trombone-sus", "brass-trombone-stac", "brass-tuba-sus", "brass-tuba-stac" }; break;
        default: ids = { "brass-trombone-sus", "brass-trombone-stac", "brass-tuba-sus", "brass-tuba-stac" }; break;
    }
    double most = 0;
    for (const char* id : ids)
        if (const auto* inst = wb::samples::Bank::get().instrument(id))
            for (const auto& z : inst->zones) {
                std::vector<float> x(std::size_t(z.length));
                for (int i = 0; i < z.length; ++i) x[std::size_t(i)] = float(z.data[i]) / 32768.0f;
                most = std::max(most, clickRatio(x, 96, x.size()));
            }
    return most;
}

// The melody level (InstrumentTests' measure): dry or in its place, at a sound generation.
double melodyLevel(int k, int sound, bool inPlace, float punch = -1.0f) {
    InstHost h;
    h.p->setSoundGeneration(sound);
    h.play(k);
    if (punch >= 0.0f) h.set(dc::ids::instPunch, punch);
    if (!inPlace) h.dry();
    h.render(0.05);
    melody(h, 0.1);
    h.render(0.05 + melodySeconds + (inPlace ? 3.0 : 1.5));
    return loudness(h.outL, h.outR, h.sr);
}

// A held note (Punch 0: the pitch, not the hit) through the whole instrument, dry.
Played heldNote(int k, int key, float velocity, double seconds, int sound = dc::inst::SoundRealBrass, float punch = 0.0f) {
    return perform(k, sound, true, { { 0.0, seconds, key, velocity } }, seconds + 0.8, punch);
}

// The phrase's hold (the sustained chord at the end) level, dry: what "not louder overall"
// means for a held note.
double heldChordLevel(int k, int sound) {
    const auto p = perform(k, sound, true, held({ 60, 64, 67 }, 0.8f), 3.0);
    return brassmetrics::toDb(brassmetrics::rmsBetween({ p.l, p.r, 48000.0, p.onset }, 1.2, 2.2));
}

} // namespace realbrass

void testRealBrass(Suite& s) {
    using namespace realbrass;
    // DC_REALBRASS_PART=tuning,clean...: only those parts (development).
    const auto parts = juce::StringArray::fromTokens(juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_PART", {}), ",", "");
    const auto part = [&](const char* name) { return parts.isEmpty() || parts.contains(name); };
    s.section("realbrass: the licence");
    {
        const juce::File root(DC_SOURCE_DIR);
        const auto text = root.getChildFile("docs/LICENSES/VSCO-2-CE.txt").loadFileAsString();
        s.check(text.contains("CC0 1.0 Universal") && text.contains("https://github.com/sgossner/VSCO-2-CE") && text.contains("440300901dfe9275fd84e0b7763af1f8443ae62e")
                    && text.contains("Statement of Purpose") && text.contains("Version:"),
                "docs/LICENSES/VSCO-2-CE.txt: the CC0 1.0 text, the source URL, the version and the pinned commit");
        const auto devlog = root.getChildFile("docs/DEVLOG.md").loadFileAsString();
        s.check(devlog.contains("Session 13") && devlog.contains("CC0 1.0") && devlog.contains("LICENSES/VSCO-2-CE.txt"), "the DEVLOG records the licence (Session 13)");
        const auto fetch = root.getChildFile("scripts/ci-fetch-brass.sh").loadFileAsString();
        s.check(fetch.contains("440300901dfe9275fd84e0b7763af1f8443ae62e") && fetch.contains("CC0"), "CI fetches the library at the pinned commit and checks its licence");
        const bool loaded = haveBrass();
        const auto folder = wb::samples::Bank::get().folder();
        if (loaded)
            s.check(folder.getChildFile("licenses/brass-samples-CC0.txt").loadFileAsString() == text, "the licence ships with the prepared notes (Samples/licenses)");
    }
    if (!haveBrass()) {
        if (required()) s.check(false, "the recorded brass is in the build (DC_REQUIRE_BRASS)", wb::samples::Bank::get().message().toStdString());
        else std::cout << "  [skipped] no recorded brass in this build (scripts/ci-fetch-brass.sh): the brass plays its synthesis\n";
    }
    s.section("realbrass: without the samples, and in sound 12, the synthesis exactly as before");
    if (part("standin")) {
        // Sound 12 never reads the recordings (a Session 12 session plays as it did); the
        // same with the bank bypassed (a build without the samples) in sound 13.
        for (const auto& b : brass) {
            const auto twelve = perform(b.index, dc::inst::SoundHorns, true, brassmetrics::hornPhrase(), 3.0);
            wb::samples::Bank::bypassForTests(true);
            const auto standIn = perform(b.index, dc::inst::SoundRealBrass, true, brassmetrics::hornPhrase(), 3.0);
            const auto standIn12 = perform(b.index, dc::inst::SoundHorns, true, brassmetrics::hornPhrase(), 3.0);
            wb::samples::Bank::bypassForTests(false);
            double diff = 0, diff12 = 0;
            for (std::size_t i = 0; i < twelve.l.size() && i < standIn.l.size(); ++i) diff = std::max(diff, double(std::abs(twelve.l[i] - standIn.l[i])));
            for (std::size_t i = 0; i < standIn12.l.size() && i < standIn.l.size(); ++i) diff12 = std::max(diff12, double(std::abs(standIn12.l[i] - standIn.l[i])));
            const bool voice = dc::inst::usesRecordings(b.index);   // the developer's voice changes with the bypass
            s.check(diff12 == 0.0 && (voice || diff == 0.0), std::string(dc::inst::design(b.index).name) + ": without the samples sound 13 plays sound 12's synthesis, sample for sample",
                    "max difference " + fmt(voice ? diff12 : std::max(diff, diff12), 9));
            s.check(!dc::inst::designUsesRealBrass(b.index, dc::inst::SoundHorns), std::string(dc::inst::design(b.index).name) + ": sound 12 does not use the recordings");
        }
    }
    if (!haveBrass()) return;

    s.section("realbrass: the prepared sets");
    {
        int loops = 0, sustains = 0;
        for (const char* id : sets) {
            const auto* inst = wb::samples::Bank::get().instrument(id);
            s.check(inst != nullptr && inst->zones.size() >= 20 && inst->layers >= 2, std::string(id) + ": loaded, at least 20 notes and 2 dynamics",
                    inst ? std::to_string(inst->zones.size()) + " notes, " + std::to_string(inst->layers) + " dynamics, keys " + std::to_string(inst->lowKey) + "-" + std::to_string(inst->highKey) : "missing");
            if (inst != nullptr && juce::String(id).endsWith("-sus"))
                for (const auto& z : inst->zones) loops += z.loops() ? 1 : 0, ++sustains;
        }
        s.check(sustains > 0 && loops == sustains, "every sustained note loops (and has its recorded release)", std::to_string(loops) + " of " + std::to_string(sustains));
    }

    s.section("realbrass: in tune across the range");
    if (part("tuning")) {
        // Every set, every other semitone of its recorded range, soft and loud: the held part
        // (sustains, 0.4-1.2 s) or the body (staccatos, 20-170 ms: what sounds of them in the
        // instrument, where the preparation tunes them).
        for (const char* id : sets) {
            const auto* inst = wb::samples::Bank::get().instrument(id);
            if (inst == nullptr) continue;
            const bool stac = juce::String(id).endsWith("-stac");
            double worst = 0;
            int worstKey = 0, measured = 0;
            for (int key = int(std::ceil(inst->lowRoot)); key <= int(std::floor(inst->highRoot)); key += 2)
                for (float vel : { 0.2f, 0.9f }) {
                    const auto v = voiceSet(id, key, vel, stac ? 0.5 : 1.4, 0.2, stac);
                    const auto from = std::size_t((stac ? 0.02 : 0.4) * 48000), to = std::size_t((stac ? 0.17 : 1.2) * 48000);
                    const double c = centsFrom(monoBetween(v.l, v.r, from, to), 48000.0, 440.0 * std::exp2((key - 69) / 12.0));
                    if (std::abs(c) > 1.0e6) continue;
                    ++measured;
                    if (std::abs(c) > std::abs(worst)) worst = c, worstKey = key;
                }
            // A staccato's pitch moves through its short life (the lips settle): measured
            // over its body, it is held to a looser limit than a held note.
            const double limit = stac ? 15.0 : 8.0;
            s.check(measured > 6 && std::abs(worst) <= limit, std::string(id) + ": in tune (every other key, soft and loud) within " + fmt(limit, 0) + " cents",
                    "worst " + fmt(worst, 1) + " cents at key " + std::to_string(worstKey) + ", " + std::to_string(measured) + " measured");
        }
        // The instruments themselves (all their players, Punch 0): low, middle and high.
        for (const auto& b : brass) {
            if (b.index == 21) continue;   // a gong: its brass growls under inharmonic modes
            double worst = 0;
            int worstKey = 0;
            for (int d : { -12, -5, 0, 7 }) {
                const int key = b.key + d;
                const auto p = heldNote(b.index, key, 0.85f, 1.6);
                const float transpose = dc::inst::design(b.index).transpose;
                // The section's lowest voice sounds the fundamental of the note (the Dwarven
                // Chant's brass an octave under its chant, the doom tuba an octave under).
                const double hz = 440.0 * std::exp2((key + transpose - 69) / 12.0);
                const double c = centsFrom(monoBetween(p.l, p.r, p.onset + 24000, p.onset + 62000), 48000.0, hz);
                if (std::abs(c) > std::abs(worst)) worst = c, worstKey = key;
            }
            s.check(std::abs(worst) <= 10.0, std::string(dc::inst::design(b.index).name) + ": in tune, low to high (within 10 cents; its players a few cents apart)",
                    "worst " + fmt(worst, 1) + " cents at key " + std::to_string(worstKey));
        }
    }

    s.section("realbrass: velocity layers smooth");
    if (part("velocity")) {
        for (const char* id : sets) {
            if (!juce::String(id).endsWith("-sus")) continue;
            const auto* inst = wb::samples::Bank::get().instrument(id);
            const int key = int(std::lround(0.5f * (inst->lowRoot + inst->highRoot)));
            double prev = -1000, worstStep = 0, worstDrop = 0;
            for (int k = 1; k <= 20; ++k) {
                const double l = loudnessOf(voiceSet(id, key, float(k) / 20.0f, 0.9, 0.1));
                if (prev > -999) worstStep = std::max(worstStep, l - prev), worstDrop = std::max(worstDrop, prev - l);
                prev = l;
            }
            s.check(worstStep <= 1.5 && worstDrop <= 0.3, std::string(id) + ": its level rises smoothly with velocity (steps of 0.05: under 1.5 dB, never falling)",
                    "largest rise " + fmt(worstStep, 2) + " dB, largest fall " + fmt(worstDrop, 2) + " dB");
        }
        for (const auto& b : brass) {
            if (b.index == 21) continue;   // the gong's brass enters with velocity (Growl)
            double prev = -1000, worstStep = 0, worstDrop = 0, stepAt = 0, dropAt = 0;
            for (int k = 4; k <= 20; ++k) {
                const auto p = perform(b.index, dc::inst::SoundRealBrass, true, { { 0.0, 0.9, b.key, float(k) / 20.0f } }, 1.2);
                const double l = loudness(p.l, p.r, 48000.0);
                if (prev > -999) {
                    if (l - prev > worstStep) worstStep = l - prev, stepAt = k / 20.0;
                    if (prev - l > worstDrop) worstDrop = prev - l, dropAt = k / 20.0;
                }
                prev = l;
            }
            s.check(worstStep <= 2.5 && worstDrop <= 0.5, std::string(dc::inst::design(b.index).name) + ": smooth from velocity 0.2 to 1 (steps of 0.05: under 2.5 dB, never falling)",
                    "largest rise " + fmt(worstStep, 2) + " dB (to velocity " + fmt(stepAt, 2) + "), largest fall " + fmt(worstDrop, 2) + " dB (to " + fmt(dropAt, 2) + ")");
        }
    }

    s.section("realbrass: the hit against sound 12 (the synthesis), the level not up");
    if (part("hit")) {
        for (const auto& b : brass) {
            if (b.index == 21) continue;
            const std::string name = dc::inst::design(b.index).name;
            const auto before = chord(b.index, dc::inst::SoundHorns).m(), after = chord(b.index, dc::inst::SoundRealBrass).m();
            std::cout << "  " << name << " before: " << line(before) << "\n  " << name << " after:  " << line(after) << "\n";
            s.check(after.attackToSustainDb >= before.attackToSustainDb + 1.5, name + ": attack-to-sustain clearly up (+1.5 dB or more)",
                    fmt(before.attackToSustainDb) + " -> " + fmt(after.attackToSustainDb) + " dB");
            s.check(after.presenceDb >= before.presenceDb + 1.5, name + ": presence (2-4 kHz share of the hit) clearly up (+1.5 dB or more)",
                    fmt(before.presenceDb) + " -> " + fmt(after.presenceDb) + " dB");
            s.check(after.sustainDb <= before.sustainDb + 1.0, name + ": the held chord not louder (within +1 dB)", fmt(before.sustainDb) + " -> " + fmt(after.sustainDb) + " dBFS");
            s.check(after.peakDb <= -0.1, name + ": the hit stays under full scale", fmt(after.peakDb) + " dBFS");
            const double was = melodyLevel(b.index, dc::inst::SoundHorns, false), now = melodyLevel(b.index, dc::inst::SoundRealBrass, false);
            s.check(now <= was + 1.0 && now >= was - 2.5, name + ": overall not louder (the melody's loudest 400 ms at most 1 dB over sound 12's, at most 2.5 dB under)", fmt(was) + " -> " + fmt(now) + " dB");
        }
    }

    s.section("realbrass: level-matched with the other instruments");
    if (part("levels")) {
        std::vector<double> all;
        for (int k = 0; k < dc::inst::instrumentCount; ++k) all.push_back(melodyLevel(k, dc::inst::SoundRealBrass, false));
        auto sorted = all;
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];
        for (const auto& b : brass) {
            const double dry = all[std::size_t(b.index)], placed = melodyLevel(b.index, dc::inst::SoundRealBrass, true);
            s.check(std::abs(dry - median) < 3.0, std::string(dc::inst::design(b.index).name) + ": dry level within 3 dB of the instruments' median", fmt(dry - median) + " dB");
            const double wasPlaced = melodyLevel(b.index, dc::inst::SoundHorns, true) - melodyLevel(b.index, dc::inst::SoundHorns, false);
            s.check(std::abs((placed - dry) - wasPlaced) < 1.5, std::string(dc::inst::design(b.index).name) + ": in its place as loud against its dry self as sound 12 was (within 1.5 dB)",
                    fmt(placed - dry) + " dB (sound 12: " + fmt(wasPlaced) + " dB)");
        }
    }

    s.section("realbrass: no clicks, no clipping");
    if (part("clean")) {
        // Held notes over their loops' turns, and the release into the recorded tail.
        for (const char* id : sets) {
            if (!juce::String(id).endsWith("-sus")) continue;
            const auto* inst = wb::samples::Bank::get().instrument(id);
            // A click is what the playback adds (a loop's turn, the crossing into the release):
            // the render's largest jump against the recordings' own (some takes carry a lip
            // tick or a sharp transient of their own; those are the performance).
            double worst = 0, worstAt = 0, worstOwn = 0;
            int worstKey = 0;
            for (int key = int(std::ceil(inst->lowRoot)); key <= int(std::floor(inst->highRoot)); key += 3) {
                const auto v = voiceSet(id, key, 0.6f, 6.0, 0.8);
                const auto m = monoBetween(v.l, v.r, 0, v.l.size());
                double at = 0, own = 0;
                for (float layerVel : { 0.4f, 0.75f }) {
                    if (const auto* z = inst->find(std::clamp(key, inst->lowKey, inst->highKey), layerVel, 0.3f)) {
                        std::vector<float> x(std::size_t(z->length));
                        for (int i = 0; i < z->length; ++i) x[std::size_t(i)] = float(z->data[i]) / 32768.0f;
                        own = std::max(own, clickRatio(x, std::size_t(std::min(0.5 * 48000.0, 0.5 * z->length)), x.size()));
                    }
                }
                const double r = clickRatio(m, std::size_t(0.5 * 48000), m.size(), &at);
                if (r / std::max(own, 12.0) > worst / std::max(worstOwn, 12.0)) worst = r, worstKey = key, worstAt = at, worstOwn = own;
            }
            s.check(worst < 1.25 * std::max(worstOwn, 12.0), std::string(id) + ": six-second holds (the loop turning several times) and the release add no click (largest jump under 12x, or 1.25x the recording's own)",
                    "largest jump " + fmt(worst, 1) + "x (recording " + fmt(worstOwn, 1) + "x; key " + std::to_string(worstKey) + ", at " + fmt(worstAt, 3) + " s)");
        }
        for (const auto& b : brass) {
            const std::string name = dc::inst::design(b.index).name;
            for (bool dry : { true, false }) {
                const auto p = perform(b.index, dc::inst::SoundRealBrass, dry, brassmetrics::hornPhrase(), brassmetrics::hornPhraseSeconds());
                double peak = 0;
                bool finite = true;
                for (std::size_t i = 0; i < p.l.size(); ++i) {
                    peak = std::max({ peak, double(std::abs(p.l[i])), double(std::abs(p.r[i])) });
                    finite &= std::isfinite(p.l[i]) && std::isfinite(p.r[i]);
                }
                s.check(finite && brassmetrics::toDb(peak) <= -0.1, name + (dry ? ": the horn phrase dry" : ": the horn phrase in its place") + " never above -0.1 dBFS",
                        fmt(brassmetrics::toDb(peak)) + " dBFS");
                if (dry) {
                    const auto m = monoBetween(p.l, p.r, p.onset, p.l.size());
                    // Against the synthesis's own render (its hits are the reference for what
                    // a brass attack does to the second difference).
                    const auto ref = perform(b.index, dc::inst::SoundHorns, true, brassmetrics::hornPhrase(), brassmetrics::hornPhraseSeconds());
                    double at = 0;
                    const double mine = clickRatio(m, 0, m.size(), &at), theirs = clickRatio(monoBetween(ref.l, ref.r, ref.onset, ref.l.size()), 0, m.size());
                    const double own = recordingsSteepest(b.index);
                    s.check(mine < std::max({ 20.0, 1.5 * theirs, 1.25 * own }), name + ": the phrase without clicks (largest jump under 20x its own, 1.5x the synthesis's, or 1.25x the recordings' steepest front)",
                            fmt(mine, 1) + "x at " + fmt(at, 3) + " s (synthesis " + fmt(theirs, 1) + "x, recordings " + fmt(own, 1) + "x)");
                }
            }
            // The hardest it can be played: Punch 200 %, Level +12 dB, velocity 1, chords.
            const auto p = perform(b.index, dc::inst::SoundRealBrass, true, held({ 48, 55, 60, 64, 67 }, 1.0f), 1.8, 2.0f, 12.0f);
            double peak = 0;
            for (std::size_t i = 0; i < p.l.size(); ++i) peak = std::max({ peak, double(std::abs(p.l[i])), double(std::abs(p.r[i])) });
            s.check(brassmetrics::toDb(peak) <= -0.1, name + ": Punch 200 %, Level +12 dB, velocity 1 chords never clip", fmt(brassmetrics::toDb(peak)) + " dBFS");
        }
    }
}

// `--realbrass-report`: the numbers behind the checks, and the calibration of the trims.
int realBrassReport() {
    using namespace realbrass;
    std::cout << wb::samples::Bank::get().message() << "\n";
    if (!haveBrass()) {
        std::cout << "no recorded brass in this build\n";
        return 1;
    }
    for (const char* id : sets) {
        const auto* inst = wb::samples::Bank::get().instrument(id);
        std::cout << id << ": " << inst->zones.size() << " notes, layer medians";
        for (int l = 0; l < inst->layers; ++l) std::cout << " " << fmt(inst->medianDb(l));
        std::cout << " dB; voiced at key 60, velocity 0.2 / 0.5 / 0.95:";
        for (float v : { 0.2f, 0.5f, 0.95f }) std::cout << " " << fmt(loudnessOf(voiceSet(id, 60, v, 1.0, 0.1, juce::String(id).endsWith("-stac"))));
        std::cout << " dB\n";
    }
    for (const char* id : { "brass-horn-sus", "brass-trumpet-sus", "brass-trombone-sus", "brass-tuba-sus" }) {
        for (float pr : { 0.0f, 0.8f, 2.0f })
            for (float stac : { 0.0f, 1.0f }) {
                const auto v = voiceSet(id, juce::String(id).contains("tuba") ? 41 : 60, 0.95f, 1.4, 0.3, false, pr, pr * 2.0f, stac);
                std::cout << id << " presence " << fmt(pr, 1) << " stac " << fmt(stac, 1) << ": " << line(brassmetrics::measure({ v.l, v.r, 48000.0, 0 })) << "\n";
            }
    }
    {
        // The Heroic Horn's section alone (its design's settings), stage by stage.
        for (int stage = 0; stage < 4; ++stage) {
            Voiced out;
            out.l.assign(std::size_t(1.6 * 48000), 0.0f);
            out.r = out.l;
            const auto inst = juce::SystemStats::getEnvironmentVariable("DC_H_INST", "horn");
            const auto keys = juce::SystemStats::getEnvironmentVariable("DC_H_KEYS", "60 64 67");
            for (const auto& keyText : juce::StringArray::fromTokens(keys, " ", "")) {
                const int key = keyText.getIntValue();
                auto voice = std::make_unique<dc::inst::BrassVoice>();
                voice->reset();
                voice->addPlayer(wb::samples::Bank::get().instrument(("brass-" + inst + "-sus").toRawUTF8()), wb::samples::Bank::get().instrument(("brass-" + inst + "-stac").toRawUTF8()), 0, 0, 1, 0);
                voice->stacLevel = 1.0f;
                voice->stacLiftDb = juce::SystemStats::getEnvironmentVariable("DC_H_LIFT", "2").getFloatValue();
                voice->susDuck = 0.45f;
                const auto env = [](const char* name, float fallback) { return juce::SystemStats::getEnvironmentVariable(name, juce::String(fallback)).getFloatValue(); };
                voice->thinHz = env("DC_H_THINHZ", 800);
                if (stage >= 1) voice->hitThin = env("DC_H_THIN", 0.9f) * (0.3f + 0.7f * 0.9f);
                if (stage >= 2) voice->bite = env("DC_H_BITE", 1.7f) * (0.4f + 2.6f * 0.9f);
                if (stage >= 3) voice->presence = env("DC_H_PRES", 1.5f), voice->hitPresence = env("DC_H_HIT", 6.0f) * (0.3f + 0.7f * 0.9f);
                voice->presenceHz = env("DC_H_PRESHZ", 2900);
                if (stage >= 3) voice->excite = env("DC_H_EX", 0.0f), voice->hitExcite = env("DC_H_HITEX", 0.0f);
                voice->biteSeconds = 0.045f;
                voice->start(float(key), 0.95f, 0.3f, 48000.0f, juce::SystemStats::getEnvironmentVariable("DC_H_ATTACK", "0.001").getFloatValue());
                for (std::size_t i = 0; i < out.l.size(); ++i) {
                    if (i % wb::dsp::chunk == 0) voice->update(0.0f);
                    float l = 0, r = 0;
                    voice->tick(l, r);
                    out.l[i] += l;
                    out.r[i] += r;
                }
            }
            const char* names[] { "as recorded", "+ thin", "+ bite", "+ presence" };
            double at = 0;
            const double click = clickRatio(monoBetween(out.l, out.r, 0, out.l.size()), 0, out.l.size(), &at);
            std::cout << "horn section " << names[stage] << ": " << line(brassmetrics::measure({ out.l, out.r, 48000.0, 0 })) << ", click " << fmt(click, 1) << "x at " << fmt(at, 4) << " s\n";
        }
    }
    if (const auto look = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_LOOKUP", {}); look.isNotEmpty()) {
        const auto* inst = wb::samples::Bank::get().instrument(look.toRawUTF8());
        for (int key : { 70, 72, 76 })
            for (float vel : { 0.25f, 0.75f })
                for (float pick : { 0.0f, 0.3f, 0.6f, 0.99f }) {
                    const auto* z = inst->find(key, vel, pick);
                    std::cout << look << " key " << key << " vel " << vel << " pick " << pick << ": root " << fmt(z->root, 2) << " layer " << z->layer << " keys " << z->loKey << "-" << z->hiKey << "\n";
                }
        return 0;
    }
    if (const auto curve = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_CURVE", {}); curve.isNotEmpty()) {
        const auto* inst = wb::samples::Bank::get().instrument(curve.toRawUTF8());
        const int key = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_KEY", juce::String(int(std::lround(0.5f * (inst->lowRoot + inst->highRoot))))).getIntValue();
        for (int k = 1; k <= 20; ++k) {
            const float v = float(k) / 20.0f;
            const float stac = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_STAC", "0").getFloatValue();
            std::cout << curve << " key " << key << " velocity " << fmt(v, 2) << ": " << fmt(loudnessOf(voiceSet(curve.toRawUTF8(), key, v, 0.9, 0.1, false, 0, 0, stac))) << " dB";
            for (int l = 0; l < inst->layers; ++l)
                if (const auto* z = inst->find(key, (float(l) + 0.5f) / float(inst->layers), 0.3f)) std::cout << "  L" << l << " root " << fmt(z->root, 2) << " loud " << fmt(z->loudnessDb);
            std::cout << "\n";
        }
        return 0;
    }
    if (const auto ve = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_VENV", "-1").getIntValue(); ve >= 0) {
        for (float vel : { 0.45f, 0.5f, 0.9f }) {
            const auto p = perform(ve, dc::inst::SoundRealBrass, true, { { 0.0, 1.3, juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_KEY", "60").getIntValue(), vel } }, 1.5, 0.0f);
            std::cout << "velocity " << vel << ":";
            for (int f = 0; f < 70; ++f) {
                double e = 0;
                for (int i = 0; i < 960; ++i) { const auto k = p.onset + std::size_t(f * 960 + i); e += double(p.l[k]) * p.l[k] + double(p.r[k]) * p.r[k]; }
                std::cout << " " << fmt(10.0 * std::log10(e / 1920.0 + 1e-20), 0);
            }
            std::cout << "\n";
        }
        return 0;
    }
    if (const auto vc = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_VCURVE", "-1").getIntValue(); vc >= 0) {
        for (int sound : { int(dc::inst::SoundHorns), int(dc::inst::SoundRealBrass) }) {
            std::cout << "sound " << sound << ":";
            for (int k = 4; k <= 20; ++k) {
                const float punch = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_PUNCH", "-1").getFloatValue();
                const auto p = perform(vc, sound, true, { { 0.0, 0.9, 60, float(k) / 20.0f } }, 1.2, punch);
                std::cout << " " << fmt(k / 20.0, 2) << ":" << fmt(loudness(p.l, p.r, 48000.0), 1);
            }
            std::cout << "\n";
        }
        return 0;
    }
    if (const auto tune = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_TUNE", {}); tune.isNotEmpty()) {
        const auto* inst = wb::samples::Bank::get().instrument(tune.toRawUTF8());
        const bool stac = tune.endsWith("-stac");
        for (int key = int(std::ceil(inst->lowRoot)); key <= int(std::floor(inst->highRoot)); ++key)
            for (float vel : { 0.2f, 0.9f }) {
                const auto v = voiceSet(tune.toRawUTF8(), key, vel, stac ? 0.5 : 1.4, 0.2, stac);
                const auto from = std::size_t((stac ? 0.02 : 0.4) * 48000), to = std::size_t((stac ? 0.17 : 1.2) * 48000);
                const auto* z = inst->find(key, vel, 0.3f);
                std::cout << tune << " key " << key << " vel " << vel << ": " << fmt(centsFrom(monoBetween(v.l, v.r, from, to), 48000.0, 440.0 * std::exp2((key - 69) / 12.0)), 1)
                          << " cents (zone root " << fmt(z->root, 3) << ")\n";
            }
        return 0;
    }
    if (juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_SETS_ONLY", {}).isNotEmpty()) return 0;
    // DC_REALBRASS_ONLY=index: one instrument, against the last known median (quick).
    const int onlyOne = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_ONLY", "-1").getIntValue();
    std::vector<double> all(std::size_t(dc::inst::instrumentCount), -20.0);
    if (onlyOne < 0)
        for (int k = 0; k < dc::inst::instrumentCount; ++k) all[std::size_t(k)] = melodyLevel(k, dc::inst::SoundRealBrass, false);
    else
        all[std::size_t(onlyOne)] = melodyLevel(onlyOne, dc::inst::SoundRealBrass, false);
    auto sorted = all;
    std::sort(sorted.begin(), sorted.end());
    const double median = onlyOne < 0 ? sorted[sorted.size() / 2] : -20.0;
    for (const auto& b : brass) {
        if (onlyOne >= 0 && b.index != onlyOne) continue;
        const std::string name = dc::inst::design(b.index).name;
        const auto before = chord(b.index, dc::inst::SoundHorns).m(), after = chord(b.index, dc::inst::SoundRealBrass).m();
        const auto sb = perform(b.index, dc::inst::SoundHorns, true, { { 0.0, 1.4, b.key, 0.95f } }, 1.8).m();
        const auto sa = perform(b.index, dc::inst::SoundRealBrass, true, { { 0.0, 1.4, b.key, 0.95f } }, 1.8).m();
        const double was = melodyLevel(b.index, dc::inst::SoundHorns, false), now = all[std::size_t(b.index)];
        const double wasPlaced = melodyLevel(b.index, dc::inst::SoundHorns, true), nowPlaced = melodyLevel(b.index, dc::inst::SoundRealBrass, true);
        const auto b0 = chord(b.index, dc::inst::SoundHorns, 0.95f, true, 0.0f).m(), a0 = chord(b.index, dc::inst::SoundRealBrass, 0.95f, true, 0.0f).m();
        std::cout << "\n" << name << "\n  chord before: " << line(before) << "\n  chord after:  " << line(after)
                  << "\n  Punch 0 before: " << line(b0) << "\n  Punch 0 after:  " << line(a0)
                  << "\n  note  before: " << line(sb) << "\n  note  after:  " << line(sa)
                  << "\n  melody level: sound 12 " << fmt(was) << " dB, 13 " << fmt(now) << " dB (" << fmt(now - was) << "), against the median " << fmt(now - median)
                  << " dB; in its place against dry: 12 " << fmt(wasPlaced - was) << ", 13 " << fmt(nowPlaced - now)
                  << "\n  held chord: 12 " << fmt(heldChordLevel(b.index, dc::inst::SoundHorns)) << ", 13 " << fmt(heldChordLevel(b.index, dc::inst::SoundRealBrass)) << " dBFS\n";
    }
    return 0;
}

// `--realbrass-dump index seconds`: the phrase's samples around a moment (development).
int realBrassDump(int index, double at) {
    using namespace realbrass;
    haveBrass();
    const auto p = perform(index, dc::inst::SoundRealBrass, true, brassmetrics::hornPhrase(), brassmetrics::hornPhraseSeconds());
    const auto i0 = p.onset + std::size_t(at * 48000.0);
    for (std::size_t i = i0 - 12; i < i0 + 12; ++i)
        std::cout << i - p.onset << " " << p.l[i] << " " << p.r[i] << " d2 " << (p.l[i] - 2 * p.l[i - 1] + p.l[i - 2]) << "\n";
    return 0;
}

// `--realbrass-calibrate`: the recorded section's trim for each brass instrument that makes
// its dry melody (the instruments' level measure, K-weighted, its loudest 400 ms) as loud as
// sound 12's minus `DC_REALBRASS_BELOW` dB (default 0.5: the hit harder, the whole not
// louder), found by bisection (the drive, the Punch and the hit clipper make it nonlinear).
int realBrassCalibrate() {
    using namespace realbrass;
    if (!haveBrass()) return 1;
    const double below = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_BELOW", "0.5").getDoubleValue();
    const int only = juce::SystemStats::getEnvironmentVariable("DC_REALBRASS_ONLY", "-1").getIntValue();
    for (const auto& b : brass) {
        if (only >= 0 && b.index != only) continue;
        const float now = dc::inst::realBrassTrimDb(b.index);
        const double target = melodyLevel(b.index, dc::inst::SoundHorns, false) - below;
        double lo = now - 12.0, hi = now + 12.0;
        for (int step = 0; step < 9; ++step) {
            const double mid = 0.5 * (lo + hi);
            dc::inst::overrideRealBrassTrimDb(b.index, float(mid));
            (melodyLevel(b.index, dc::inst::SoundRealBrass, false) > target ? hi : lo) = mid;
        }
        dc::inst::overrideRealBrassTrimDb(b.index, float(0.5 * (lo + hi)));
        const double got = melodyLevel(b.index, dc::inst::SoundRealBrass, false);
        dc::inst::overrideRealBrassTrimDb(b.index, std::numeric_limits<float>::quiet_NaN());
        std::cout << dc::inst::design(b.index).name << ": trim " << fmt(now) << " dB -> " << fmt(0.5 * (lo + hi)) << " dB (melody: target " << fmt(target) << ", got " << fmt(got) << " dB)\n";
    }
    return 0;
}

// `--listen-realbrass dir`: each brass instrument, the horn phrase, before (sound 12, the
// synthesis) and after (13, the recordings), dry and in its place.
int listenRealBrass(const juce::String& directory) {
    using namespace realbrass;
    juce::File dir(directory);
    dir.createDirectory();
    if (!haveBrass()) std::cout << "note: no recorded brass in this build: 'after' plays the synthesis\n";
    const auto phrase = brassmetrics::hornPhrase();
    const double seconds = brassmetrics::hornPhraseSeconds();
    for (const auto& b : brass) {
        // The gong's brass growls when the gong is struck hard: its phrase at full velocity.
        auto notes = phrase;
        if (b.index == 21)
            for (auto& n : notes) n.velocity = 1.0f, n.key -= 12;
        for (bool dry : { true, false })
            for (int sound : { int(dc::inst::SoundHorns), int(dc::inst::SoundRealBrass) }) {
                const auto p = perform(b.index, sound, dry, notes, seconds);
                std::vector<float> l(p.l.begin() + long(p.onset), p.l.end()), r(p.r.begin() + long(p.onset), p.r.end());
                writeWav(dir.getChildFile(juce::String(b.file) + (dry ? "-dry-" : "-in-its-place-") + (sound == dc::inst::SoundHorns ? "1-before" : "2-after") + ".wav"), l, r);
            }
    }
    return 0;
}
