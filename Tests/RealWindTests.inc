// Session 17: the recorded woodwinds (VSCO 2 Community Edition, CC0; included by
// DungeonCrawlerTests.cpp after RealBrassTests.inc, group "realwinds", CI shard "15-realwinds").
//   licence    the woodwinds recorded in the licence entry, fetched at the same pin, shipped
//   stand-in   without the samples (and in sound 14) the synthesis plays exactly as before
//   sets       the thirteen prepared sets loaded, every sustain looped
//   tuning     every set in tune across its range (cents report), every instrument low to high
//   dynamics   the level rises smoothly with velocity; the wheel (Expression) swells smoothly
//   levels     level-matched with the other instruments on the melody, dry and in its place
//   clean      no clicks at loops, note-offs or legato transitions; never above full scale
//   legato     an overlapping note slurs (no new tongued attack, no gap)
//   character  still distinct from its siblings; each its own (small, dark, distant, murky)
//   roller     the DUNGEON SYNTH roller still works on them
// Without the samples in the build the checks that need them are skipped, unless
// DC_REQUIRE_WOODWINDS is set (CI sets it once the library is fetched).
//
//   --realwinds-report         the numbers behind the checks (cents per set and key)
//   --realwinds-calibrate      the recorded winds' trims and make-ups, by bisection
//   --listen-realwinds dir     each woodwind before (sound 14) and after (15), dry and in its
//                              place, a legato line and an Expression swell, as WAV

namespace realwinds {
using namespace brasstests;
using realbrass::centsFrom;
using realbrass::clickRatio;
using realbrass::monoBetween;

constexpr int before = dc::inst::SoundSeated, after = dc::inst::SoundWoodwinds;

// The woodwind instruments: design index, the key their range centres on, the file name,
// and the sets they play.
struct Wind { int index; int key; const char* file; std::vector<const char*> sets; };
const std::vector<Wind>& winds() {
    static const std::vector<Wind> w {
        { 22, 62, "village-green-tin-whistle", { "ww-flute-sus", "ww-flute-vib", "ww-flute-stac", "ww-piccolo-sus", "ww-piccolo-stac" } },
        { 14, 62, "forest-edge-wooden-flute", { "ww-flute-sus", "ww-flute-vib", "ww-flute-stac" } },
        { 29, 62, "foothills-shepherds-flute", { "ww-flute-sus", "ww-flute-vib", "ww-flute-stac" } },
        { 27, 50, "bog-murky-reed", { "ww-clarinet-sus", "ww-clarinet-stac", "ww-bassoon-sus", "ww-bassoon-vib", "ww-bassoon-stac", "ww-oboe-sus", "ww-oboe-vib", "ww-oboe-stac" } },
    };
    return w;
}
const char* const sets[13] { "ww-flute-sus", "ww-flute-vib", "ww-flute-stac", "ww-piccolo-sus", "ww-piccolo-stac", "ww-oboe-sus", "ww-oboe-vib",
                             "ww-oboe-stac", "ww-clarinet-sus", "ww-clarinet-stac", "ww-bassoon-sus", "ww-bassoon-vib", "ww-bassoon-stac" };
bool isStac(const char* id) { return juce::String(id).endsWith("-stac"); }
bool isVib(const char* id) { return juce::String(id).endsWith("-vib"); }

bool haveWinds() {
    auto& bank = wb::samples::Bank::get();
    bank.load();
    bank.waitUntilLoaded(60000);
    return bank.instrument("ww-flute-sus") != nullptr;
}
bool required() { return juce::SystemStats::getEnvironmentVariable("DC_REQUIRE_WOODWINDS", {}).isNotEmpty(); }
std::string nameOf(int k) { return dc::inst::design(k).name; }

// One set through the woodwind voice alone (one player, no tongue, none of the character): a
// note held `seconds`, released, at 48 kHz.
struct Voiced { std::vector<float> l, r; };
Voiced voiceSet(const char* id, int key, float velocity, double seconds, double tail = 0.6) {
    const auto* inst = wb::samples::Bank::get().instrument(id);
    Voiced out;
    if (inst == nullptr) return out;
    auto voice = std::make_unique<dc::inst::WoodwindVoice>();
    voice->reset();
    dc::inst::WoodwindVoice::Player p;
    if (isStac(id)) p.stac = inst;
    else p.sus = inst;
    voice->addPlayer(p);
    voice->stacLevel = isStac(id) ? 1.0f : 0.0f;
    voice->breath = voice->puff = 0;
    if (!voice->start(float(key), velocity, 0.3f, 48000.0f, 0.001f, 7)) return out;
    const int total = int((seconds + tail) * 48000.0), hold = int(seconds * 48000.0);
    for (int i = 0; i < total; ++i) {
        if (i % wb::dsp::chunk == 0) voice->update(0.0f, 0.0f, float(wb::dsp::chunk) / 48000.0f);
        if (i == hold) voice->release(0.25f);
        float l = 0, r = 0;
        voice->tick(l, r);
        out.l.push_back(l);
        out.r.push_back(r);
    }
    return out;
}
double loudnessOf(const Voiced& v) { return loudness(v.l, v.r, 48000.0); }
// The mean K-weighted level of a stretch (loudness()'s filter, averaged over all of it).
double meanLoudness(const std::vector<float>& l, const std::vector<float>& r) {
    juce::IIRFilter hp1, hp2, shelf1, shelf2;
    hp1.setCoefficients(juce::IIRCoefficients::makeHighPass(48000.0, 60.0));
    hp2.setCoefficients(juce::IIRCoefficients::makeHighPass(48000.0, 60.0));
    shelf1.setCoefficients(juce::IIRCoefficients::makeHighShelf(48000.0, 1700.0, 0.7, 1.585f));
    shelf2.setCoefficients(juce::IIRCoefficients::makeHighShelf(48000.0, 1700.0, 0.7, 1.585f));
    double sum = 0;
    for (std::size_t i = 0; i < l.size(); ++i) {
        const double a = shelf1.processSingleSampleRaw(hp1.processSingleSampleRaw(l[i])), b = shelf2.processSingleSampleRaw(hp2.processSingleSampleRaw(r[i]));
        sum += 0.5 * (a * a + b * b);
    }
    return 10.0 * std::log10(sum / double(std::max<std::size_t>(1, l.size())) + 1.0e-20);
}

// The pitch of a set's note as the instrument would sound it: the held part of a sustain
// (0.4-1.2 s), the body of a staccato (20-120 ms).
double setCents(const char* id, int key, float velocity) {
    // A vibrato take's centre over several of its cycles (0.4-2.0 s).
    const bool stac = isStac(id), vib = isVib(id);
    const auto v = voiceSet(id, key, velocity, stac ? 0.4 : vib ? 2.2 : 1.4, 0.1);
    const auto from = std::size_t((stac ? 0.02 : 0.4) * 48000), to = std::size_t((stac ? 0.13 : vib ? 2.0 : 1.2) * 48000);
    if (v.l.size() < to) return 1.0e9;
    return centsFrom(monoBetween(v.l, v.r, from, to), 48000.0, 440.0 * std::exp2((key - 69) / 12.0));
}

// The steepest front in the recordings an instrument plays (every take of its sets): a click
// is a jump the playback adds beyond those.
double recordingsSteepest(const Wind& w) {
    double most = 0;
    for (const char* id : w.sets)
        if (const auto* inst = wb::samples::Bank::get().instrument(id))
            for (const auto& z : inst->zones) {
                std::vector<float> x(std::size_t(z.length));
                for (int i = 0; i < z.length; ++i) x[std::size_t(i)] = float(z.data[i]) / 32768.0f;
                most = std::max(most, clickRatio(x, 96, x.size()));
            }
    return most;
}

// A woodwind phrase (seconds, key, velocity): a folk line around `root` with tongued notes,
// a slurred run (each note overlapping the next by 40 ms), short staccatos and a held end.
std::vector<brassmetrics::PhraseNote> windPhrase(int root) {
    std::vector<brassmetrics::PhraseNote> notes;
    double t = 0;
    const int line[] { 0, 2, 3, 7, 5, 3 };
    for (int k = 0; k < 6; ++k, t += 0.5) notes.push_back({ t, 0.45, root + line[k], k == 0 ? 0.85f : 0.7f });
    const int run[] { 2, 3, 5, 7, 8, 7, 5, 3 };
    for (int k = 0; k < 8; ++k, t += 0.22) notes.push_back({ t, 0.26, root + run[k], 0.65f });   // overlapping: slurred
    t += 0.3;
    for (int k = 0; k < 4; ++k, t += 0.25) notes.push_back({ t, 0.09, root + (k % 2 == 0 ? 7 : 3), 0.8f });   // staccato
    notes.push_back({ t + 0.2, 1.8, root, 0.75f });
    return notes;
}
double windPhraseSeconds() { return 3.0 + 8 * 0.22 + 0.3 + 1.0 + 2.0 + 2.0; }

// The melody level (InstrumentTests' measure) at a sound generation, dry or in its place.
double melodyLevel(int k, int sound, bool inPlace) { return realbrass::melodyLevel(k, sound, inPlace); }

// A held note through the whole instrument, dry, the macros at their middle unless given.
Played heldNote(int k, int key, float velocity, double seconds, int sound = after, float sigMacro = -1.0f, float wheel = -1.0f) {
    InstHost h;
    h.p->setSoundGeneration(sound);
    h.play(k);
    h.dry();
    if (sigMacro >= 0.0f) h.set(dc::ids::instMacros[4], sigMacro);
    if (wheel >= 0.0f) h.cc(0.0, 1, int(std::lround(wheel * 127.0f)));
    h.render(0.4);
    h.note(0.4, seconds, key, velocity);
    h.render(seconds + 0.8);
    Played p;
    p.l = h.outL;
    p.r = h.outR;
    p.onset = h.idx(0.4);
    return p;
}

// The spectral centroid (Hz) of a stretch of a render: brightness.
double centroid(const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to) {
    const auto m = monoBetween(l, r, from, to);
    const std::size_t n = 8192;
    if (m.size() < n) return 0;
    double num = 0, den = 0;
    for (std::size_t at = 0; at + n <= m.size(); at += n / 2) {
        std::vector<float> buf(2 * n, 0.0f);
        for (std::size_t i = 0; i < n; ++i) buf[i] = m[at + i] * float(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * double(i) / double(n)));
        juce::dsp::FFT fft(13);
        fft.performFrequencyOnlyForwardTransform(buf.data());
        for (std::size_t b = 1; b < n / 2; ++b) {
            const double f = double(b) * 48000.0 / double(n), p = double(buf[b]) * buf[b];
            num += f * p;
            den += p;
        }
    }
    return den > 0 ? num / den : 0;
}
double rmsDb(const Played& p, double from, double to) { return brassmetrics::toDb(brassmetrics::rmsBetween({ p.l, p.r, 48000.0, p.onset }, from, to)); }

// The siblings' score (InstrumentTests: third-octave colour + half the envelope's motion).
double siblingScore(const MelodyRender& a, const MelodyRender& b) {
    const auto pa = thirdOctaves(a.l, a.sr), pb = thirdOctaves(b.l, b.sr);
    double colour = 0, motion = 0;
    for (std::size_t i = 0; i < pa.size(); ++i) colour += std::abs(pa[i] - pb[i]);
    colour /= double(pa.size());
    const auto ea = envelopeShape(a.l, a.sr), eb = envelopeShape(b.l, b.sr);
    const std::size_t n = std::min(ea.size(), eb.size());
    for (std::size_t i = 0; i < n; ++i) motion += std::abs(ea[i] - eb[i]);
    motion /= double(std::max<std::size_t>(1, n));
    return colour + 0.5 * motion;
}
MelodyRender melodyAt(int k, int sound, bool inPlace = false) {
    InstHost h;
    h.p->setSoundGeneration(sound);
    h.play(k);
    if (!inPlace) h.dry();
    h.render(0.05);
    melody(h, 0.1);
    h.render(0.05 + melodySeconds + 1.5);
    return { h.outL, h.outR, h.sr };
}

bool sameSamples(const Played& a, const Played& b, double* worst = nullptr) {
    double d = 0;
    const std::size_t n = std::min(a.l.size(), b.l.size());
    for (std::size_t i = 0; i < n; ++i) d = std::max({ d, double(std::abs(a.l[i] - b.l[i])), double(std::abs(a.r[i] - b.r[i])) });
    if (worst != nullptr) *worst = d;
    return d == 0.0 && a.l.size() == b.l.size();
}

} // namespace realwinds

void testRealWinds(Suite& s) {
    using namespace realwinds;
    const auto parts = juce::StringArray::fromTokens(juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_PART", {}), ",", "");
    const auto part = [&](const char* name) { return parts.isEmpty() || parts.contains(name); };

    s.section("realwinds: the licence");
    {
        const juce::File root(DC_SOURCE_DIR);
        const auto text = root.getChildFile("docs/LICENSES/VSCO-2-CE.txt").loadFileAsString();
        s.check(text.contains("CC0 1.0 Universal") && text.contains("Woodwinds/Flute") && text.contains("Woodwinds/Piccolo") && text.contains("Woodwinds/Oboe")
                    && text.contains("Woodwinds/Clarinet") && text.contains("Woodwinds/Bassoon") && text.contains("440300901dfe9275fd84e0b7763af1f8443ae62e"),
                "docs/LICENSES/VSCO-2-CE.txt: the CC0 1.0 text lists the woodwinds used, at the pinned commit");
        const auto fetch = root.getChildFile("scripts/ci-fetch-brass.sh").loadFileAsString();
        s.check(fetch.contains("/Woodwinds/Flute/") && fetch.contains("/Woodwinds/Bassoon/") && fetch.contains("woodwinds.py") && fetch.contains("440300901dfe9275fd84e0b7763af1f8443ae62e")
                    && fetch.contains("licence_sha256"),
                "CI fetches the woodwinds at the same pin, after the same licence check, and prepares them");
        const auto devlog = root.getChildFile("docs/DEVLOG.md").loadFileAsString();
        s.check(devlog.contains("Session 17") && devlog.contains("VSCO 2 Community Edition") && devlog.contains("ww-clarinet-sus"), "the DEVLOG records which instruments use which woodwinds");
        const auto workflow = root.getChildFile(".github/workflows/build.yml").loadFileAsString();
        s.check(workflow.contains("realwinds") && workflow.contains("vsco-2-ce-440300901dfe9275fd84e0b7763af1f8443ae62e-woodwinds"), "CI caches the checkout with the woodwinds and renders them");
        if (haveWinds()) {
            const auto folder = wb::samples::Bank::get().folder();
            s.check(folder.getChildFile("licenses/woodwind-samples-CC0.txt").loadFileAsString() == text, "the licence ships with the prepared woodwinds (Samples/licenses)");
        }
    }
    if (!haveWinds()) {
        if (required()) s.check(false, "the recorded woodwinds are in the build (DC_REQUIRE_WOODWINDS)", wb::samples::Bank::get().message().toStdString());
        else std::cout << "  [skipped] no recorded woodwinds in this build (scripts/ci-fetch-brass.sh): the woodwinds play their synthesis\n";
    }

    s.section("realwinds: without the samples, and in sound 14, the synthesis exactly as before");
    if (part("standin")) {
        for (const auto& w : winds()) {
            const auto phrase = windPhrase(w.key);
            const auto fourteen = perform(w.index, before, true, phrase, windPhraseSeconds());
            wb::samples::Bank::bypassForTests(true);
            const auto standIn = perform(w.index, after, true, phrase, windPhraseSeconds());
            const auto standInPlaced = perform(w.index, after, false, phrase, windPhraseSeconds());
            wb::samples::Bank::bypassForTests(false);
            const auto fourteenPlaced = perform(w.index, before, false, phrase, windPhraseSeconds());
            double d1 = 0, d2 = 0;
            const bool same = sameSamples(fourteen, standIn, &d1), samePlaced = sameSamples(fourteenPlaced, standInPlaced, &d2);
            s.check(same && samePlaced, nameOf(w.index) + ": without the samples sound 15 plays sound 14's synthesis, sample for sample (dry and in its place)",
                    "max difference " + fmt(std::max(d1, d2), 9));
            s.check(!dc::inst::designUsesRealWinds(w.index, before) && dc::inst::wheelOf(w.index, before) == dc::inst::design(w.index).wheel
                        && std::string(dc::inst::wheelLabel(w.index, before)) == dc::inst::design(w.index).modLabel,
                    nameOf(w.index) + ": sound 14 does not use the recordings; its wheel as before");
        }
        // Every other instrument plays sound 15 exactly as 14.
        std::string changed;
        for (int k = 0; k < dc::inst::instrumentCount; ++k) {
            if (k == 14 || k == 22 || k == 27 || k == 29) continue;
            const auto a = perform(k, before, true, held({ 57, 64 }, 0.8f), 1.6), b = perform(k, after, true, held({ 57, 64 }, 0.8f), 1.6);
            if (!sameSamples(a, b)) changed += nameOf(k) + "; ";
        }
        s.check(changed.empty(), "every other instrument plays sound 15 sample for sample as 14", changed);
    }
    if (!haveWinds()) return;

    s.section("realwinds: the prepared sets");
    {
        int loops = 0, sustains = 0;
        for (const char* id : sets) {
            const auto* inst = wb::samples::Bank::get().instrument(id);
            const bool small = juce::String(id).contains("piccolo");
            s.check(inst != nullptr && inst->zones.size() >= (small ? 5u : 12u), std::string(id) + ": loaded",
                    inst ? std::to_string(inst->zones.size()) + " notes, " + std::to_string(inst->layers) + " dynamics, roots " + fmt(inst->lowRoot) + "-" + fmt(inst->highRoot) : "missing");
            if (inst != nullptr && !isStac(id))
                for (const auto& z : inst->zones) loops += z.loops() ? 1 : 0, ++sustains;
        }
        s.check(sustains > 0 && loops == sustains, "every sustained note loops (and has its recorded release)", std::to_string(loops) + " of " + std::to_string(sustains));
        for (const char* id : { "ww-flute-sus", "ww-oboe-sus", "ww-clarinet-sus", "ww-bassoon-sus" }) {
            const auto* inst = wb::samples::Bank::get().instrument(id);
            s.check(inst != nullptr && inst->layers >= 2, std::string(id) + ": its dynamics as velocity layers (2 or more)", inst ? std::to_string(inst->layers) : "missing");
        }
    }

    s.section("realwinds: in tune across the range (cents)");
    if (part("tuning")) {
        for (const char* id : sets) {
            const auto* inst = wb::samples::Bank::get().instrument(id);
            if (inst == nullptr) continue;
            double worst = 0, sum = 0;
            int worstKey = 0, measured = 0;
            for (int key = int(std::ceil(inst->lowRoot)); key <= int(std::floor(inst->highRoot)); key += 2)
                for (float vel : { 0.25f, 0.9f }) {
                    const double c = setCents(id, key, vel);
                    if (std::abs(c) > 1.0e6) continue;
                    ++measured;
                    sum += std::abs(c);
                    if (std::abs(c) > std::abs(worst)) worst = c, worstKey = key;
                }
            // A vibrato take's pitch swings ±20-40 cents: its average over 0.8 s is held looser;
            // a staccato's moves through its short life.
            const double limit = isStac(id) ? 15.0 : isVib(id) ? 12.0 : 8.0;
            s.metric(std::string(id) + ": mean |cents|", measured > 0 ? sum / measured : 0.0, " cents");
            s.check(measured >= 4 && std::abs(worst) <= limit, std::string(id) + ": in tune (every other key, soft and loud) within " + fmt(limit, 0) + " cents",
                    "worst " + fmt(worst, 1) + " cents at key " + std::to_string(worstKey) + ", mean " + fmt(measured > 0 ? sum / measured : 0.0, 1) + ", " + std::to_string(measured) + " measured");
        }
        for (const auto& w : winds()) {
            double worst = 0;
            int worstKey = 0;
            std::string report;
            for (int d : { -7, -3, 0, 4, 9, 14 }) {
                const int key = w.key + d;
                // The Forest Edge's second flute at its least Detune (6 cents), so the pair has one pitch.
                const auto p = heldNote(w.index, key, 0.75f, 1.6, after, w.index == 14 ? 0.0f : -1.0f);
                const double hz = 440.0 * std::exp2((key + dc::inst::design(w.index).transpose - 69) / 12.0);
                const double c = centsFrom(monoBetween(p.l, p.r, p.onset + 24000, p.onset + 62000), 48000.0, hz);
                report += std::to_string(key) + ": " + fmt(c, 1) + "  ";
                if (std::abs(c) > std::abs(worst)) worst = c, worstKey = key;
            }
            std::cout << "  " << nameOf(w.index) << " cents by key: " << report << "\n";
            s.check(std::abs(worst) <= 10.0, nameOf(w.index) + ": in tune, low to high (within 10 cents; its players a few cents apart, wandering)",
                    "worst " + fmt(worst, 1) + " cents at key " + std::to_string(worstKey) + " (" + report + ")");
        }
    }

    s.section("realwinds: dynamics smooth (velocity, Expression)");
    if (part("dynamics")) {
        for (const char* id : sets) {
            if (isStac(id)) continue;
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
        for (const auto& w : winds()) {
            double prev = -1000, worstStep = 0, worstDrop = 0, stepAt = 0, first = 0, last = 0;
            for (int k = 4; k <= 20; ++k) {
                const auto p = perform(w.index, after, true, { { 0.0, 0.9, w.key, float(k) / 20.0f } }, 1.2);
                const double l = loudness(p.l, p.r, 48000.0);
                if (k == 4) first = l;
                last = l;
                if (prev > -999) {
                    if (l - prev > worstStep) worstStep = l - prev, stepAt = k / 20.0;
                    worstDrop = std::max(worstDrop, prev - l);
                }
                prev = l;
            }
            s.check(worstStep <= 2.0 && worstDrop <= 0.5 && last - first >= 6.0,
                    nameOf(w.index) + ": smooth from velocity 0.2 to 1 (steps of 0.05: under 2 dB, never falling more than 0.5; 6 dB or more of range)",
                    "largest rise " + fmt(worstStep, 2) + " dB (to " + fmt(stepAt, 2) + "), largest fall " + fmt(worstDrop, 2) + " dB, range " + fmt(last - first, 1) + " dB");
            // The wheel at rest, half and full on a held note: louder and brighter, step by step.
            const auto rest = heldNote(w.index, w.key, 0.7f, 1.6, after, -1.0f, 0.0f), half = heldNote(w.index, w.key, 0.7f, 1.6, after, -1.0f, 0.5f),
                       full = heldNote(w.index, w.key, 0.7f, 1.6, after, -1.0f, 1.0f);
            // Loudness K-weighted (as heard: a brighter take at the same power sounds louder), the
            // brightness as the share above 1.5 kHz, both over the held part (0.6-1.4 s).
            const auto heldPart = [](const Played& p, double& top) {
                const auto from = p.onset + std::size_t(0.6 * 48000), to = p.onset + std::size_t(1.4 * 48000);
                const std::vector<float> l(p.l.begin() + long(from), p.l.begin() + long(to)), r(p.r.begin() + long(from), p.r.begin() + long(to));
                top = dstests::topDb(dstests::bandsDb(l, r), 1500.0);
                return loudness(l, r, 48000.0);
            };
            double t0 = 0, t1 = 0, t2 = 0;
            const double l0 = heldPart(rest, t0), l1 = heldPart(half, t1), l2 = heldPart(full, t2);
            s.check(l1 > l0 + 0.5 && l2 > l1 + 0.5 && l2 - l0 <= 4.5 && t2 > t0 + 1.0, nameOf(w.index) + ": the wheel is its Expression (louder and brighter as it rises, at most 4.5 dB)",
                    "loudness " + fmt(l0) + " / " + fmt(l1) + " / " + fmt(l2) + " dB, above 1.5 kHz " + fmt(t0) + " -> " + fmt(t2) + " dB");
            // A quick wheel sweep while the note is held adds no click.
            InstHost h;
            h.p->setSoundGeneration(after);
            h.play(w.index);
            h.dry();
            h.render(0.4);
            h.note(0.4, 2.4, w.key, 0.7f);
            for (int i = 0; i <= 40; ++i) h.cc(1.0 + 0.01 * i, 1, (127 * i) / 40);
            for (int i = 0; i <= 40; ++i) h.cc(1.8 + 0.01 * i, 1, 127 - (127 * i) / 40);
            h.render(3.4);
            double at = 0;
            const double jump = clickRatio(monoBetween(h.outL, h.outR, h.idx(0.9), h.idx(2.6)), 0, h.idx(2.6) - h.idx(0.9), &at);
            const auto still = heldNote(w.index, w.key, 0.7f, 2.4);
            const double own = clickRatio(monoBetween(still.l, still.r, still.onset + 24000, still.onset + 105600), 0, 81600);
            s.check(jump < std::max(12.0, 1.5 * own), nameOf(w.index) + ": a quick wheel sweep while held adds no click", fmt(jump, 1) + "x (held without it " + fmt(own, 1) + "x)");
        }
    }

    s.section("realwinds: level-matched with the other instruments (melody)");
    if (part("levels")) {
        std::vector<double> all;
        for (int k = 0; k < dc::inst::instrumentCount; ++k) all.push_back(melodyLevel(k, after, false));
        auto sorted = all;
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];
        for (const auto& w : winds()) {
            const double dry = all[std::size_t(w.index)], was = melodyLevel(w.index, before, false);
            const double placed = melodyLevel(w.index, after, true), wasPlaced = melodyLevel(w.index, before, true);
            s.check(std::abs(dry - was) <= 1.0, nameOf(w.index) + ": the melody as loud as its synthesis was (within 1 dB)", fmt(was) + " -> " + fmt(dry) + " dB");
            s.check(std::abs(dry - median) < 3.0, nameOf(w.index) + ": dry level within 3 dB of the instruments' median", fmt(dry - median) + " dB");
            s.check(std::abs((placed - dry) - (wasPlaced - was)) < 1.5, nameOf(w.index) + ": in its place as loud against its dry self as sound 14 was (within 1.5 dB)",
                    fmt(placed - dry) + " dB (sound 14: " + fmt(wasPlaced - was) + " dB)");
        }
    }

    s.section("realwinds: no clicks (loops, note-offs, legato), no clipping");
    if (part("clean")) {
        for (const char* id : sets) {
            if (isStac(id)) continue;
            const auto* inst = wb::samples::Bank::get().instrument(id);
            double worst = 0, worstAt = 0, worstOwn = 0;
            int worstKey = 0;
            for (int key = int(std::ceil(inst->lowRoot)); key <= int(std::floor(inst->highRoot)); key += 3) {
                const auto v = voiceSet(id, key, 0.6f, 6.0, 0.8);
                const auto m = monoBetween(v.l, v.r, 0, v.l.size());
                double at = 0, own = 0;
                for (float layerVel : { 0.25f, 0.75f })
                    if (const auto* z = inst->find(std::clamp(key, inst->lowKey, inst->highKey), layerVel, 0.3f)) {
                        std::vector<float> x(std::size_t(z->length));
                        for (int i = 0; i < z->length; ++i) x[std::size_t(i)] = float(z->data[i]) / 32768.0f;
                        own = std::max(own, clickRatio(x, std::size_t(std::min(0.5 * 48000.0, 0.5 * z->length)), x.size()));
                    }
                const double r = clickRatio(m, std::size_t(0.5 * 48000), m.size(), &at);
                if (r / std::max(own, 12.0) > worst / std::max(worstOwn, 12.0)) worst = r, worstKey = key, worstAt = at, worstOwn = own;
            }
            s.check(worst < 1.25 * std::max(worstOwn, 12.0), std::string(id) + ": six-second holds (the loop turning) and the note-off add no click (under 12x, or 1.25x the recording's own)",
                    "largest jump " + fmt(worst, 1) + "x (recording " + fmt(worstOwn, 1) + "x; key " + std::to_string(worstKey) + ", at " + fmt(worstAt, 3) + " s)");
        }
        for (const auto& w : winds()) {
            const auto phrase = windPhrase(w.key);
            const double own = recordingsSteepest(w);
            for (bool dry : { true, false }) {
                const auto p = perform(w.index, after, dry, phrase, windPhraseSeconds());
                double peak = 0;
                bool finite = true;
                for (std::size_t i = 0; i < p.l.size(); ++i) {
                    peak = std::max({ peak, double(std::abs(p.l[i])), double(std::abs(p.r[i])) });
                    finite &= std::isfinite(p.l[i]) && std::isfinite(p.r[i]);
                }
                s.check(finite && brassmetrics::toDb(peak) <= -0.1, nameOf(w.index) + (dry ? ": the phrase dry" : ": the phrase in its place") + " never above -0.1 dBFS",
                        fmt(brassmetrics::toDb(peak)) + " dBFS");
                if (dry) {
                    // Tongued notes, the slurred run, staccatos, note-offs: against the stand-in's own
                    // render of the phrase and the recordings' steepest front.
                    wb::samples::Bank::bypassForTests(true);
                    const auto ref = perform(w.index, after, true, phrase, windPhraseSeconds());
                    wb::samples::Bank::bypassForTests(false);
                    double at = 0;
                    const auto m = monoBetween(p.l, p.r, p.onset, p.l.size());
                    const double mine = clickRatio(m, 0, m.size(), &at), theirs = clickRatio(monoBetween(ref.l, ref.r, ref.onset, ref.l.size()), 0, m.size());
                    s.check(mine < std::max({ 20.0, 1.5 * theirs, 1.25 * own }), nameOf(w.index) + ": the phrase (tongued, slurred, staccato, note-offs) without clicks",
                            fmt(mine, 1) + "x at " + fmt(at, 3) + " s (synthesis " + fmt(theirs, 1) + "x, recordings " + fmt(own, 1) + "x)");
                }
            }
            // Played as hard as it goes: velocity 1, a chord, Level +12 dB, the wheel full.
            InstHost h;
            h.p->setSoundGeneration(after);
            h.play(w.index);
            h.dry();
            h.set(dc::ids::instLevel, 12.0f);
            h.cc(0.0, 1, 127);
            h.render(0.4);
            for (int d : { 0, 4, 7, 12 }) h.note(0.4, 1.4, w.key + d, 1.0f);
            h.render(2.2);
            double peak = 0;
            for (std::size_t i = 0; i < h.outL.size(); ++i) peak = std::max({ peak, double(std::abs(h.outL[i])), double(std::abs(h.outR[i])) });
            s.check(brassmetrics::toDb(peak) <= -0.1, nameOf(w.index) + ": velocity 1 chords, Level +12 dB, the wheel full never clip", fmt(brassmetrics::toDb(peak)) + " dBFS");
        }
    }

    s.section("realwinds: legato (an overlapping note slurs)");
    if (part("legato")) {
        for (const auto& w : winds()) {
            // Two notes a tone apart: tongued (a gap) against slurred (the second starts 60 ms
            // before the first ends). Slurred: no new attack over the held level, no gap, no click.
            const auto tongued = perform(w.index, after, true, { { 0.0, 0.8, w.key, 0.75f }, { 0.9, 0.9, w.key + 2, 0.75f } }, 2.4, 0.0f);
            const auto slurred = perform(w.index, after, true, { { 0.0, 0.96, w.key, 0.75f }, { 0.9, 0.9, w.key + 2, 0.75f } }, 2.4, 0.0f);
            // 5 ms frames: the transition (0.85-1.2 s) against the held notes' own (0.5-0.85 s and
            // 1.3-1.7 s; the detuned pair beats, so its frames swing by themselves).
            const auto frames = [&](const Played& p, double from, double to, double& lo, double& hi) {
                const auto env = brassmetrics::envelope({ p.l, p.r, 48000.0, p.onset + std::size_t(from * 48000) }, 5.0, to - from);
                lo = 1.0e9, hi = -1.0e9;
                for (double e : env) lo = std::min(lo, brassmetrics::toDb(e)), hi = std::max(hi, brassmetrics::toDb(e));
            };
            double low = 0, high = 0, loA = 0, hiA = 0, loB = 0, hiB = 0;
            frames(slurred, 0.85, 1.2, low, high);
            frames(slurred, 0.5, 0.85, loA, hiA);
            frames(slurred, 1.3, 1.7, loB, hiB);
            s.check(low >= std::min(loA, loB) - 6.0 && high <= std::max(hiA, hiB) + 2.0,
                    nameOf(w.index) + ": a slurred step has no gap and no new attack (its 5 ms frames within -6 / +2 dB of the held notes' own)",
                    "transition " + fmt(low) + " .. " + fmt(high) + " dB, held " + fmt(loA) + " .. " + fmt(hiA) + " / " + fmt(loB) + " .. " + fmt(hiB) + " dB");
            const auto envT = brassmetrics::envelope({ tongued.l, tongued.r, 48000.0, tongued.onset + std::size_t(0.85 * 48000) }, 5.0, 0.35);
            double lowT = 1.0e9;
            for (double e : envT) lowT = std::min(lowT, brassmetrics::toDb(e));
            s.check(low > lowT + 3.0, nameOf(w.index) + ": legato is joined, a tongued step is not (the slurred dip 3 dB or more shallower)", fmt(low) + " vs " + fmt(lowT) + " dB");
            double at = 0;
            const auto m = monoBetween(slurred.l, slurred.r, slurred.onset + 36000, slurred.onset + 60000);
            const double jump = clickRatio(m, 0, m.size(), &at);
            const auto plain = heldNote(w.index, w.key, 0.75f, 1.6, after);
            const double own = clickRatio(monoBetween(plain.l, plain.r, plain.onset + 24000, plain.onset + 48000), 0, 24000);
            s.check(jump < std::max(12.0, 1.5 * own), nameOf(w.index) + ": the legato transition adds no click", fmt(jump, 1) + "x (a held note " + fmt(own, 1) + "x)");
            // In tune after the glide.
            const double c = centsFrom(monoBetween(slurred.l, slurred.r, slurred.onset + std::size_t(1.15 * 48000), slurred.onset + std::size_t(1.7 * 48000)), 48000.0,
                                       440.0 * std::exp2((w.key + 2 + dc::inst::design(w.index).transpose - 69) / 12.0));
            s.check(std::abs(c) <= 12.0, nameOf(w.index) + ": the slurred note lands in tune (within 12 cents)", fmt(c, 1) + " cents");
        }
        // A chord (notes within 30 ms) is not slurred: every note keeps sounding.
        const auto chordP = perform(22, after, true, held({ 60, 64, 67 }, 0.75f), 1.8, 0.0f);
        const auto one = perform(22, after, true, held({ 60 }, 0.75f), 1.8, 0.0f);
        s.check(rmsDb(chordP, 0.6, 1.2) > rmsDb(one, 0.6, 1.2) + 2.5, "a chord is not slurred (its notes all sound)", fmt(rmsDb(one, 0.6, 1.2)) + " -> " + fmt(rmsDb(chordP, 0.6, 1.2)) + " dB");
    }

    s.section("realwinds: character (distinct, each its own)");
    if (part("character")) {
        // Distinct from their siblings (the instruments group's measure and limit).
        for (const auto& w : winds()) {
            const auto mine = melodyAt(w.index, after);
            const int loc = dc::inst::locationOf(w.index);
            double closest = 1.0e9;
            std::string with;
            for (int k = 0; k < dc::inst::instrumentCount; ++k) {
                if (k == w.index || dc::inst::locationOf(k) != loc) continue;
                const double sc = siblingScore(mine, melodyAt(k, after));
                if (sc < closest) closest = sc, with = nameOf(k);
            }
            s.check(closest > 4.0, nameOf(w.index) + ": still distinct from its siblings (score over 4)", "closest " + with + ": " + fmt(closest, 1));
        }
        // The four winds from one another (they share recordings: the flute three times).
        const std::array<int, 3> flutes { 22, 14, 29 };
        for (std::size_t a = 0; a < flutes.size(); ++a)
            for (std::size_t b = a + 1; b < flutes.size(); ++b) {
                const double sc = siblingScore(melodyAt(flutes[a], after), melodyAt(flutes[b], after));
                s.check(sc > 3.0, nameOf(flutes[a]) + " / " + nameOf(flutes[b]) + ": the flutes are three different winds (score over 3)", fmt(sc, 1));
            }
        // Each its own: the whistle small (little below 400 Hz), the wooden flute and the reed
        // dark, the shepherd's flute breathy (noise between the harmonics), the reed low.
        const auto bands = [](int k) {
            const auto r = melodyAt(k, after);
            return dstests::bandsDb(r.l, r.r);
        };
        const auto vg = bands(22), fe = bands(14), fh = bands(29), bog = bands(27);
        const auto below = [](const std::vector<double>& b, double hz) {
            double sum = 0;
            for (std::size_t i = 0; i < b.size(); ++i)
                if (63.0 * std::pow(2.0, double(i) / 3.0) < hz) sum += std::pow(10.0, b[i] / 10.0);
            return 10.0 * std::log10(std::max(sum, 1.0e-30));
        };
        s.check(below(vg, 400) < below(fh, 400) - 10.0, "Tin Whistle: small (under 400 Hz 10 dB or more further down than the Shepherd's Flute on the same melody)",
                fmt(below(vg, 400)) + " vs " + fmt(below(fh, 400)) + " dB");
        s.check(dstests::topDb(fe, 4000) < dstests::topDb(vg, 4000) - 3.0, "Wooden Flute: darker than the whistle (above 4 kHz 3 dB or more down)",
                fmt(dstests::topDb(fe, 4000)) + " vs " + fmt(dstests::topDb(vg, 4000)) + " dB");
        s.check(dstests::topDb(bog, 2000) < -12.0 && below(bog, 400) > -12.0, "Murky Reed: low and murky (above 2 kHz 12 dB or more down, its body below 400 Hz)",
                "top " + fmt(dstests::topDb(bog, 2000)) + " dB, below 400 Hz " + fmt(below(bog, 400)) + " dB");
        {
            // The Foothills echo: one short note in its place, its echoes 0.4-2.4 s later against
            // the note, at sound 15's fit and with the Echoes slider set back to sound 14's.
            const auto tailOf = [](float reverbSlider, float echoesSlider) {
                InstHost h;
                h.p->setSoundGeneration(after);
                h.play(29);
                h.set(dc::ids::mixGroups[0], reverbSlider);
                h.set(dc::ids::mixGroups[1], echoesSlider);
                h.render(0.3);
                h.note(0.3, 0.25, 64, 0.8f);
                h.render(3.0);
                const double note = brassmetrics::toDb(brassmetrics::rmsBetween({ h.outL, h.outR, 48000.0, h.idx(0.3) }, 0.0, 0.25));
                return brassmetrics::toDb(brassmetrics::rmsBetween({ h.outL, h.outR, 48000.0, h.idx(0.3) }, 0.45, 2.4)) - note;
            };
            // Sound 14's fit on the same recorded flute: the sliders scaled back by the new fit.
            const auto f14 = dc::inst::mixFit(29, before), f15 = dc::inst::mixFit(29, after);
            const double t14 = tailOf(f14.reverb / f15.reverb, f14.echoes / f15.echoes), t15 = tailOf(1.0f, 1.0f);
            s.check(t15 > t14 + 1.0, "Shepherd's Flute: echoing further across the hills (its space and echoes 1 dB or more up against the note)", fmt(t14) + " -> " + fmt(t15) + " dB");
        }
    }

    s.section("realwinds: the DUNGEON SYNTH roller on them");
    if (part("roller")) {
        for (const auto& w : winds()) {
            const auto take = [&](float amount, int era) {
                InstHost h;
                h.p->setSoundGeneration(after);
                h.play(w.index);
                h.set(dc::ids::dungeonSynth, amount);
                h.set(dc::ids::dungeonSynthEra, float(era));
                h.render(0.05);
                melody(h, 0.1, w.key - 5);
                h.render(0.05 + melodySeconds + 1.5);
                return MelodyRender { h.outL, h.outR, h.sr };
            };
            const auto off = take(0.0f, 0), classic = take(1.0f, 0), modern = take(1.0f, 1);
            // The roller's level change on the stand-in (sound 14), the reference for "keeps its level".
            const auto take14 = [&](float amount, int era) {
                InstHost h;
                h.p->setSoundGeneration(before);
                h.play(w.index);
                h.set(dc::ids::dungeonSynth, amount);
                h.set(dc::ids::dungeonSynthEra, float(era));
                h.render(0.05);
                melody(h, 0.1, w.key - 5);
                h.render(0.05 + melodySeconds + 1.5);
                return loudness(h.outL, h.outR, 48000.0);
            };
            const double o14 = take14(0.0f, 0), c14 = take14(1.0f, 0) - o14, m14 = take14(1.0f, 1) - o14;
            const double dClassic = dstests::spectralDistance(dstests::bandsDb(off.l, off.r), dstests::bandsDb(classic.l, classic.r));
            const double dModern = dstests::spectralDistance(dstests::bandsDb(off.l, off.r), dstests::bandsDb(modern.l, modern.r));
            const double lo = loudness(off.l, off.r, 48000.0), lc = loudness(classic.l, classic.r, 48000.0), lm = loudness(modern.l, modern.r, 48000.0);
            double peak = 0;
            for (const auto* r : { &classic, &modern })
                for (std::size_t i = 0; i < r->l.size(); ++i) peak = std::max({ peak, double(std::abs(r->l[i])), double(std::abs(r->r[i])) });
            s.check(dClassic > 2.0 && dModern > 1.0, nameOf(w.index) + ": the roller at 100 % changes it (Classic 2 dB, Modern 1 dB of spectrum or more)", fmt(dClassic) + " / " + fmt(dModern) + " dB");
            s.check(brassmetrics::toDb(peak) <= -0.1, nameOf(w.index) + ": the roller at 100 % never clips", fmt(brassmetrics::toDb(peak)) + " dBFS");
            s.check(std::abs(lc - lo) <= 1.5 && std::abs(lm - lo) <= 1.5, nameOf(w.index) + ": and keeps its level in its place (within 1.5 dB of 0 %, as the dungeonsynth group asks)",
                    "Classic " + fmt(lc - lo) + " (the synthesis, sound 14: " + fmt(c14) + "), Modern " + fmt(lm - lo) + " (" + fmt(m14) + ") dB");
        }
    }
}

// `--realwinds-report`: the numbers behind the checks.
int realWindsReport() {
    using namespace realwinds;
    std::cout << wb::samples::Bank::get().message() << "\n";
    if (!haveWinds()) {
        std::cout << "no recorded woodwinds in this build\n";
        return 1;
    }
    if (const int vc = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_VCURVE", "-1").getIntValue(); vc >= 0) {
        const int key = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_KEY", "62").getIntValue();
        std::cout << "velocity:";
        for (int k = 4; k <= 20; ++k) {
            const auto p = perform(vc, after, true, { { 0.0, 0.9, key, float(k) / 20.0f } }, 1.2);
            std::cout << " " << fmt(k / 20.0, 2) << ":" << fmt(loudness(p.l, p.r, 48000.0), 1);
        }
        std::cout << "\nwheel:";
        for (float wv : { 0.0f, 0.25f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f }) {
            const auto p = heldNote(vc, key, 0.7f, 1.6, after, -1.0f, wv);
            std::cout << " " << fmt(wv, 2) << ":" << fmt(rmsDb(p, 0.6, 1.4)) << "/" << fmt(centroid(p.l, p.r, p.onset + 28800, p.onset + 67200), 0);
        }
        std::cout << "\n";
        return 0;
    }
    if (const int dsi = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_DS", "-1").getIntValue(); dsi >= 0) {
        for (int sound : { before, after })
            for (bool inPlace : { false, true }) {
                double l[3] {};
                for (int m = 0; m < 3; ++m) {
                    InstHost h;
                    h.p->setSoundGeneration(sound);
                    h.play(dsi);
                    if (!inPlace) h.dry();
                    h.set(dc::ids::dungeonSynth, m == 0 ? 0.0f : 1.0f);
                    h.set(dc::ids::dungeonSynthEra, m == 2 ? 1.0f : 0.0f);
                    h.render(0.05);
                    melody(h, 0.1, juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_KEY", "57").getIntValue());
                    h.render(0.05 + melodySeconds + 1.5);
                    l[m] = loudness(h.outL, h.outR, 48000.0);
                }
                std::cout << "sound " << sound << (inPlace ? " in its place" : " dry") << ": Classic " << fmt(l[1] - l[0], 2) << ", Modern " << fmt(l[2] - l[0], 2) << " dB\n";
            }
        return 0;
    }
    if (const auto pair = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_PAIR", {}); pair.isNotEmpty()) {
        const int key = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_KEY", "74").getIntValue();
        for (const auto& id : juce::StringArray::fromTokens(pair, ",", "")) {
            const auto v = voiceSet(id.toRawUTF8(), key, 0.7f, 1.8, 0.1);
            Played p { v.l, v.r, 0 };
            std::cout << id << " key " << key << ": loudness " << fmt(loudnessOf(v)) << " dB, held rms " << fmt(rmsDb(p, 0.6, 1.4)) << " dB, centroid "
                      << fmt(centroid(v.l, v.r, 28800, 67200), 0) << " Hz\n";
        }
        // The pair in one voice, the vibrato take's share 0 and 1.
        for (float vb : { 0.0f, 1.0f }) {
            auto voice = std::make_unique<dc::inst::WoodwindVoice>();
            voice->reset();
            dc::inst::WoodwindVoice::Player p;
            p.sus = wb::samples::Bank::get().instrument("ww-flute-sus");
            p.vib = wb::samples::Bank::get().instrument("ww-flute-vib");
            voice->addPlayer(p);
            voice->stacLevel = 0;
            voice->vibBase = vb;
            voice->start(float(key), 0.7f, 0.3f, 48000.0f, 0.001f, 7);
            Voiced v;
            for (int i = 0; i < int(1.8 * 48000); ++i) {
                if (i % wb::dsp::chunk == 0) voice->update(0.0f, 0.0f, float(wb::dsp::chunk) / 48000.0f);
                float l = 0, r = 0;
                voice->tick(l, r);
                v.l.push_back(l);
                v.r.push_back(r);
            }
            Played pl { v.l, v.r, 0 };
            std::cout << "together, vibBase " << vb << ": loudness " << fmt(loudnessOf(v)) << " dB, held rms " << fmt(rmsDb(pl, 0.6, 1.4)) << " dB, readers " << voice->readers << "\n";
        }
        return 0;
    }
    if (const int sl = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_SLUR", "-1").getIntValue(); sl >= 0) {
        const int key = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_KEY", "62").getIntValue();
        const auto slurred = perform(sl, after, true, { { 0.0, 0.96, key, 0.75f }, { 0.9, 0.9, key + 2, 0.75f } }, 2.4, 0.0f);
        const auto env = brassmetrics::envelope({ slurred.l, slurred.r, 48000.0, slurred.onset + std::size_t(0.8 * 48000) }, 5.0, 0.45);
        for (std::size_t i = 0; i < env.size(); ++i) std::cout << fmt(0.8 + 0.005 * double(i), 3) << " " << fmt(brassmetrics::toDb(env[i])) << "\n";
        double at = 0;
        const auto m = monoBetween(slurred.l, slurred.r, slurred.onset + 36000, slurred.onset + 60000);
        std::cout << "click " << fmt(clickRatio(m, 0, m.size(), &at), 1) << "x at " << fmt(0.75 + at, 4) << " s\n";
        const auto i0 = slurred.onset + 36000 + std::size_t(at * 48000.0);
        for (std::size_t i = i0 - 6; i < i0 + 6; ++i) std::cout << "  " << slurred.l[i] << "\n";
        return 0;
    }
    const auto only = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_ONLY", {});
    if (only.isEmpty() || only == "sets")
        for (const char* id : sets) {
            const auto* inst = wb::samples::Bank::get().instrument(id);
            std::cout << id << ": " << inst->zones.size() << " notes, roots " << fmt(inst->lowRoot) << "-" << fmt(inst->highRoot) << ", layer medians";
            for (int l = 0; l < inst->layers; ++l) std::cout << " " << fmt(inst->medianDb(l));
            std::cout << " dB\n  cents (key: soft / loud):";
            for (int key = int(std::ceil(inst->lowRoot)); key <= int(std::floor(inst->highRoot)); ++key)
                std::cout << " " << key << ": " << fmt(setCents(id, key, 0.25f), 1) << " / " << fmt(setCents(id, key, 0.9f), 1);
            std::cout << "\n";
        }
    for (const auto& w : winds()) {
        if (only.isNotEmpty() && only != "sets" && only.getIntValue() != w.index) continue;
        const double was = melodyLevel(w.index, before, false), now = melodyLevel(w.index, after, false);
        const double wasPlaced = melodyLevel(w.index, before, true), nowPlaced = melodyLevel(w.index, after, true);
        std::cout << "\n" << nameOf(w.index) << "\n  melody: sound 14 " << fmt(was) << " dB, 15 " << fmt(now) << " dB (" << fmt(now - was) << "); in its place against dry: 14 " << fmt(wasPlaced - was)
                  << ", 15 " << fmt(nowPlaced - now) << " dB\n  cents by key:";
        for (int d = -12; d <= 19; d += 1) {
            const int key = w.key + d;
            const auto p = heldNote(w.index, key, 0.75f, 1.6, after, w.index == 14 ? 0.0f : -1.0f);
            const double hz = 440.0 * std::exp2((key + dc::inst::design(w.index).transpose - 69) / 12.0);
            std::cout << " " << key << ": " << fmt(centsFrom(monoBetween(p.l, p.r, p.onset + 24000, p.onset + 62000), 48000.0, hz), 1);
        }
        const auto a = heldNote(w.index, w.key, 0.75f, 1.6, before), b = heldNote(w.index, w.key, 0.75f, 1.6, after);
        std::cout << "\n  held note: " << line(a.m()) << "\n          -> " << line(b.m()) << "\n  centroid " << fmt(centroid(a.l, a.r, a.onset + 28800, a.onset + 67200), 0) << " -> "
                  << fmt(centroid(b.l, b.r, b.onset + 28800, b.onset + 67200), 0) << " Hz\n";
    }
    return 0;
}

// `--realwinds-calibrate`: each recorded wind's trim so its dry melody (the instruments'
// level measure) is as loud as sound 14's, by bisection; then the make-up in its place.
int realWindsCalibrate() {
    using namespace realwinds;
    if (!haveWinds()) return 1;
    const int only = juce::SystemStats::getEnvironmentVariable("DC_REALWINDS_ONLY", "-1").getIntValue();
    for (const auto& w : winds()) {
        if (only >= 0 && w.index != only) continue;
        const float now = dc::inst::realWindTrimDb(w.index);
        const double target = melodyLevel(w.index, before, false);
        double lo = now - 36.0, hi = now + 36.0;
        for (int step = 0; step < 12; ++step) {
            const double mid = 0.5 * (lo + hi);
            dc::inst::overrideRealWindTrimDb(w.index, float(mid));
            (melodyLevel(w.index, after, false) > target ? hi : lo) = mid;
        }
        dc::inst::overrideRealWindTrimDb(w.index, float(0.5 * (lo + hi)));
        const double got = melodyLevel(w.index, after, false);
        const double placedWas = melodyLevel(w.index, before, true) - target, placedNow = melodyLevel(w.index, after, true) - got;
        dc::inst::overrideRealWindTrimDb(w.index, std::numeric_limits<float>::quiet_NaN());
        std::cout << nameOf(w.index) << ": trim " << fmt(now, 2) << " dB -> " << fmt(0.5 * (lo + hi), 2) << " dB (melody target " << fmt(target) << ", got " << fmt(got)
                  << " dB); in its place against dry 14 " << fmt(placedWas, 2) << ", 15 " << fmt(placedNow, 2) << " dB: make-up " << fmt(placedWas - placedNow, 2) << " dB more\n";
    }
    return 0;
}

// `--listen-realwinds dir`: each woodwind, its phrase, before (sound 14, the synthesis) and
// after (15, the recordings), dry and in its place; an Expression swell; the four in a row.
int listenRealWinds(const juce::String& directory) {
    using namespace realwinds;
    juce::File dir(directory);
    dir.createDirectory();
    if (!haveWinds()) std::cout << "note: no recorded woodwinds in this build: 'after' plays the synthesis\n";
    for (const auto& w : winds()) {
        const auto phrase = windPhrase(w.key);
        for (bool dry : { true, false })
            for (int sound : { before, after }) {
                const auto p = perform(w.index, sound, dry, phrase, windPhraseSeconds());
                std::vector<float> l(p.l.begin() + long(p.onset), p.l.end()), r(p.r.begin() + long(p.onset), p.r.end());
                writeWav(dir.getChildFile(juce::String(w.file) + (dry ? "-dry-" : "-in-its-place-") + (sound == before ? "1-before" : "2-after") + ".wav"), l, r);
            }
        // A long note, the wheel rising from rest to full and back (Expression), in its place.
        InstHost h;
        h.p->setSoundGeneration(after);
        h.play(w.index);
        h.render(0.2);
        h.note(0.2, 5.0, w.key, 0.7f);
        for (int i = 0; i <= 100; ++i) h.cc(0.7 + 0.02 * i, 1, (127 * i) / 100);
        for (int i = 0; i <= 100; ++i) h.cc(3.0 + 0.02 * i, 1, 127 - (127 * i) / 100);
        h.render(7.5);
        writeWav(dir.getChildFile(juce::String(w.file) + "-expression-swell.wav"), h.outL, h.outR);
    }
    dir.getChildFile("realwinds-sets.txt").replaceWithText(
        "Tin Whistle (Village Green): ww-flute-sus/vib/stac, ww-piccolo-sus/stac (from E6)\n"
        "Wooden Flute (Forest Edge): ww-flute-sus/vib/stac, two players\n"
        "Shepherd's Flute (Foothills): ww-flute-sus/vib/stac\n"
        "Murky Reed (Bog): ww-clarinet-sus/stac, ww-bassoon-sus/vib/stac (below D3), ww-oboe-sus/vib/stac (faint, from B-flat 3)\n");
    return 0;
}
