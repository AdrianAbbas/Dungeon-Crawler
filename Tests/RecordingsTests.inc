// Session 8: the developer's new recordings (voice, banjo, djembes, piano) in the
// instrument and the FX's voice places. Included by DungeonCrawlerTests.cpp after
// InstrumentTests.inc.
//   --only recordings          the changed instruments: in tune from MIDI (cents report),
//                              velocity rising smoothly, no clicks at loops or note-offs,
//                              stand-ins level-matched; the FX's voice places
//   --listen-recordings dir    the same melody and chords on every changed instrument, dry
//                              then in its place; the FX's voice places before vs after

// The instruments that use the new recordings (place order).
// DC_ONLY_INSTRUMENT=name (development aid) limits the per-instrument checks to the
// changed instruments whose name contains it.
std::vector<int> changedInstruments();
std::vector<int> changedInstrumentsFiltered() {
    auto all = changedInstruments();
    if (const char* only = std::getenv("DC_ONLY_INSTRUMENT"))
        all.erase(std::remove_if(all.begin(), all.end(), [&](int k) { return instName(k).find(only) == std::string::npos; }), all.end());
    return all;
}
std::vector<int> changedInstruments() {
    return { dc::inst::designFor(dc::Dungeon, 4), dc::inst::designFor(dc::Cathedral, 0), dc::inst::designFor(dc::Cathedral, 2),
             dc::inst::designFor(dc::HauntedForest, 2), dc::inst::designFor(dc::HauntedForest, 3), dc::inst::designFor(dc::Swamp, 0),
             dc::inst::designFor(dc::Mountains, 1), dc::inst::designFor(dc::Mountains, 3), dc::inst::designFor(dc::Necropolis, 0),
             dc::inst::designFor(dc::Necropolis, 2) };
}
// Which recordings each one plays (for the reports).
std::string recordingsOf(int k) {
    const int l = dc::inst::locationOf(k), s = dc::inst::subOf(k);
    if (l == dc::Dungeon) return "voice-aah/ooh, choir stacks, grit; piano";
    if (l == dc::Cathedral && s == 0) return "voice-ooh/aah, choir stacks";
    if (l == dc::Cathedral) return "drone-hum, drone-chant, voice-mm/oh";
    if (l == dc::HauntedForest && s == 2) return "voice-mm (soft)";
    if (l == dc::HauntedForest) return "voice-whispers, voice-breaths";
    if (l == dc::Swamp) return "banjo, banjo-g5";
    if (l == dc::Mountains && s == 1) return "djembe (10 strokes)";
    if (l == dc::Mountains) return "drone-chant, drone-aah, voice-grit";
    if (l == dc::Necropolis && s == 0) return "voice-aah/ooh, choir stacks";
    return "piano-long";
}
// No dependable single pitch: the struck djembes, the tolling bell (its tierce and hum).
bool inharmonicRecorded(int k) {
    const int l = dc::inst::locationOf(k), s = dc::inst::subOf(k);
    return (l == dc::Mountains && s == 1) || (l == dc::Necropolis && s == 2);
}

// The pitch (Hz) of a single sounding note: YIN over a few frames, the median.
double yinPitch(const std::vector<float>& x, std::size_t from, std::size_t to, double sr, double fmin = 38.0, double fmax = 1500.0) {
    const auto tauMax = static_cast<std::size_t>(sr / fmin), tauMin = static_cast<std::size_t>(sr / fmax);
    constexpr std::size_t window = 2048;
    std::vector<double> found, d(tauMax + 1), cmnd(tauMax + 1);
    for (std::size_t start = from; start + window + tauMax < std::min(to, x.size()); start += window / 2) {
        for (std::size_t tau = 1; tau <= tauMax; ++tau) {
            double sum = 0;
            for (std::size_t i = 0; i < window; ++i) {
                const double e = double(x[start + i]) - double(x[start + i + tau]);
                sum += e * e;
            }
            d[tau] = sum;
        }
        double running = 0;
        cmnd[0] = 1;
        for (std::size_t tau = 1; tau <= tauMax; ++tau) {
            running += d[tau];
            cmnd[tau] = running > 0 ? d[tau] * double(tau) / running : 1.0;
        }
        std::size_t best = 0;
        for (std::size_t tau = std::max<std::size_t>(tauMin, 2); tau < tauMax; ++tau)
            if (cmnd[tau] < 0.2) {
                while (tau + 1 < tauMax && cmnd[tau + 1] < cmnd[tau]) ++tau;
                best = tau;
                break;
            }
        if (best == 0) continue;
        const double a = cmnd[best - 1], b = cmnd[best], c = cmnd[best + 1];
        const double shift = (a - 2 * b + c) != 0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0.0;
        found.push_back(sr / (double(best) + shift));
    }
    if (found.size() < 3) return 0;
    std::nth_element(found.begin(), found.begin() + std::ptrdiff_t(found.size() / 2), found.end());
    return found[found.size() / 2];
}
// Cents from the pressed key's pitch class (octave-agnostic: the instruments transpose).
double centsFromKey(double hz, int key) {
    const double c = 1200.0 * std::log2(hz / (440.0 * std::pow(2.0, (key - 69) / 12.0)));
    return std::fmod(std::fmod(c + 600.0, 1200.0) + 1200.0, 1200.0) - 600.0;
}

// Clicks: 2.5 ms windows of the second difference, each against the windows around it
// (+-100 ms, their 90th percentile); a window far above its neighbours (and audible) is
// a click. Returns how many, and where the worst one is.
std::string clickTimes;
int countClicks(const std::vector<float>& x, std::size_t from, std::size_t to, double sr, double& worstAt, double& worstRatio, double ratio = 8.0) {
    const auto w = std::max<std::size_t>(8, static_cast<std::size_t>(0.0025 * sr));
    std::vector<double> m;
    for (std::size_t i = std::max<std::size_t>(from, 2); i + w <= std::min(to, x.size()); i += w) {
        double top = 0;
        for (std::size_t k = i; k < i + w; ++k) top = std::max(top, std::abs(double(x[k]) - 2.0 * x[k - 1] + x[k - 2]));
        m.push_back(top);
    }
    double peak = 0;
    for (std::size_t i = from; i < std::min(to, x.size()); ++i) peak = std::max(peak, double(std::abs(x[i])));
    const auto span = static_cast<std::size_t>(0.1 * sr / double(w));
    int clicks = 0;
    worstAt = -1;
    worstRatio = 0;
    std::vector<double> around;
    for (std::size_t i = 0; i < m.size(); ++i) {
        around.clear();
        for (std::size_t k = i > span ? i - span : 0; k < std::min(m.size(), i + span + 1); ++k)
            if (k + 2 < i || k > i + 2) around.push_back(m[k]);
        if (around.empty()) continue;
        // The 90th percentile of the neighbours: a periodic sharp edge (a driven brass wave,
        // a pulse) is part of the sound and of the baseline; a click is alone.
        const auto q = std::ptrdiff_t(around.size() * 9 / 10);
        std::nth_element(around.begin(), around.begin() + q, around.end());
        const double base = around[std::size_t(q)];
        const double r = m[i] / std::max(base, 1.0e-9);
        if (m[i] > 2.0e-3 * peak + 1.0e-5 && r > worstRatio) worstRatio = r, worstAt = double(from + i * w) / sr;
        if (m[i] > 2.0e-3 * peak + 1.0e-5 && r > ratio) {
            if (clicks < 6) clickTimes += " " + fmt(double(from + i * w) / sr, 3);
            ++clicks;
        }
    }
    return clicks;
}

void testRecordings(Suite& s) {
    const auto changed = changedInstrumentsFiltered();
    // The bank loads when the first processor is made; this group may run first.
    wb::samples::Bank::get().load();
    wb::samples::Bank::get().waitUntilLoaded(20000);
    const bool recordings = wb::samples::Bank::get().ready();
    s.section("recordings: the new sample sets are loaded");
    for (const char* id : { "voice-ooh", "voice-aah", "voice-ee", "voice-mm", "voice-oh", "choir-ooh", "choir-aah", "choir-mm", "drone-hum", "drone-chant", "drone-aah",
                            "voice-grit", "voice-whispers", "voice-breaths", "voice-shh", "banjo", "banjo-muted", "banjo-g5", "djembe-big-bass", "djembe-small-mslap",
                            "piano", "piano-long" }) {
        const auto* inst = wb::samples::Bank::get().instrument(id);
        judge(s, inst != nullptr && !inst->zones.empty(), true, std::string("the sample set ") + id + " is there", recordings ? "missing" : "no recordings in this build");
    }
    if (const auto* p = wb::samples::Bank::get().instrument("piano"))
        s.check(p->layers == 3 && p->zones.size() >= 250, "the piano has three velocity layers over (nearly) every key", std::to_string(p->zones.size()) + " notes");
    if (const auto* c = wb::samples::Bank::get().instrument("choir-ooh"))
        s.check(c->envelopeDb.size() == 19, "the choir stacks carry the voice's spectral envelope (for the FX)", std::to_string(c->envelopeDb.size()) + " bands");

    s.section("recordings: in tune from MIDI (cents from the pressed key, dry)");
    for (int k : changed) {
        std::string report;
        std::vector<double> cents;
        for (int key : { 45, 50, 55, 57, 60, 64, 67, 72, 76 }) {
            InstHost h;
            h.play(k, false);
            h.dry();
            h.note(0.05, 1.4, key, 0.7f);
            h.render(1.6);
            const double hz = yinPitch(h.outL, h.idx(0.35), h.idx(1.35), h.sr);
            if (hz <= 0) {
                report += " " + std::to_string(key) + ":-";
                continue;
            }
            const double c = centsFromKey(hz, key);
            cents.push_back(c);
            report += " " + std::to_string(key) + ":" + fmt(c, 0);
        }
        std::cout << "  [cents] " << instName(k) << " (" << recordingsOf(k) << "):" << report << "\n";
        if (inharmonicRecorded(k)) {
            s.metric(instName(k) + " (struck, inharmonic): keys measured", double(cents.size()));
            continue;
        }
        std::vector<double> sorted = cents;
        std::sort(sorted.begin(), sorted.end(), [](double a, double b) { return std::abs(a) < std::abs(b); });
        const double median = sorted.empty() ? 999.0 : std::abs(sorted[sorted.size() / 2]);
        const double worst = sorted.empty() ? 999.0 : std::abs(sorted.back());
        s.metric(instName(k) + ": median |cents|", median);
        judge(s, cents.size() >= 6 && median < 15.0, true, instName(k) + ": in tune (median within 15 cents)", report);
        judge(s, cents.size() >= 6 && worst < 40.0, true, instName(k) + ": every measured key within 40 cents", report);
    }

    s.section("recordings: velocity rises smoothly (ten steps, key 60, dry)");
    for (int k : changed) {
        std::vector<double> lv;
        for (int v = 1; v <= 10; ++v) {
            InstHost h;
            h.play(k, false);
            h.dry();
            h.note(0.05, 0.9, 60, 0.1f * float(v));
            h.render(1.3);
            lv.push_back(loudness(h.outL, h.outR, h.sr));
        }
        double worstDrop = 0, worstJump = 0;
        std::string curve;
        for (std::size_t i = 0; i < lv.size(); ++i) {
            curve += " " + fmt(lv[i] - lv.back(), 1);
            if (i > 0) {
                worstDrop = std::max(worstDrop, lv[i - 1] - lv[i]);
                worstJump = std::max(worstJump, lv[i] - lv[i - 1]);
            }
        }
        std::cout << "  [velocity] " << instName(k) << " (dB re full):" << curve << "\n";
        s.check(lv.back() - lv.front() > 3.0, instName(k) + ": velocity 0.1 to 1.0 rises", fmt(lv.back() - lv.front()) + " dB");
        s.check(worstDrop < 0.75, instName(k) + ": no step down as the velocity rises", fmt(worstDrop) + " dB");
        s.check(worstJump < 6.5, instName(k) + ": no jump between neighbouring velocities", fmt(worstJump) + " dB");
    }

    s.section("recordings: no clicks at loops, release tails or note-offs (dry)");
    {
        // The detector itself: silent on a clean tone, alert to a one-sample jump in it.
        std::vector<float> tone(48000);
        for (std::size_t i = 0; i < tone.size(); ++i) tone[i] = 0.3f * float(std::sin(2.0 * juce::MathConstants<double>::pi * 220.0 * double(i) / 48000.0));
        double at = 0, ratio = 0;
        const int clean = countClicks(tone, 0, tone.size(), 48000, at, ratio);
        for (std::size_t i = 24000; i < tone.size(); ++i) tone[i] += 0.02f;
        const int jump = countClicks(tone, 0, tone.size(), 48000, at, ratio);
        s.check(clean == 0 && jump >= 1, "the click detector hears a 0.02 step in a tone and nothing in the clean tone", std::to_string(clean) + " / " + std::to_string(jump));
    }
    for (int k : changed) {
        for (int key : { 52, 64 }) {
            InstHost h;
            h.play(k, false);
            h.dry();
            // Held through several turns of every loop, then let go.
            h.note(0.05, 5.0, key, 0.75f);
            // A short note let go early (mid-attack or mid-loop).
            h.note(6.0, 0.35, key + 5, 0.6f);
            h.render(8.5);
            double atL = 0, rL = 0, atR = 0, rR = 0;
            clickTimes.clear();
            const int clicks = countClicks(h.outL, h.idx(0.3), h.idx(5.95), h.sr, atL, rL) + countClicks(h.outL, h.idx(6.25), h.idx(8.5), h.sr, atR, rR);
            const double worst = std::max(rL, rR), at = rL >= rR ? atL : atR;
            judge(s, clicks == 0 && h.finite(), true, instName(k) + ": key " + std::to_string(key) + " held and let go without clicks",
                  std::to_string(clicks) + " (worst " + fmt(worst) + "x its neighbours at " + fmt(at, 2) + " s; first at" + clickTimes + ")");
        }
    }

    s.section("recordings: the stand-ins without the recordings (playable, level-matched)");
    {
        // The instruments' dry loudness with the recordings, and with them switched off.
        std::vector<double> all;
        for (int k = 0; k < dc::inst::instrumentCount; ++k) {
            const auto r = renderMelody(k, false);
            all.push_back(loudness(r.l, r.r, r.sr));
        }
        std::vector<double> sorted = all;
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];
        for (int k : changed) s.metric(instName(k) + ": recorded, level re the median", all[std::size_t(k)] - median, " dB");
        wb::samples::Bank::bypassForTests(true);
        for (int k : changed) {
            const auto r = renderMelody(k, false);
            double peak = 0;
            bool finite = true;
            for (std::size_t i = 0; i < r.l.size(); ++i) {
                finite &= std::isfinite(r.l[i]) && std::isfinite(r.r[i]);
                peak = std::max(peak, double(std::abs(r.l[i])));
            }
            const double level = loudness(r.l, r.r, r.sr);
            s.check(finite && peak > 1.0e-3 && peak < 1.0, instName(k) + ": the stand-in plays (no recordings)", fmt(peak, 3));
            judge(s, std::abs(level - median) < 3.0, !recordings, instName(k) + ": the stand-in is level-matched (within 3 dB of the median)", fmt(level - median) + " dB");
        }
        wb::samples::Bank::bypassForTests(false);
    }

    s.section("recordings: the FX's voice places (with the voice vs without)");
    {
        struct VoicePlace { int location, sub; const char* what; bool whispers; };
        const VoicePlace places[] { { dc::Necropolis, 1, "whispers", true }, { dc::HauntedForest, 3, "whispers", true }, { dc::Cathedral, 2, "whispers", true },
                                    { dc::Dungeon, 4, "sub-octave choir imprint", false }, { dc::Castle, 3, "choir imprint", false }, { dc::Cathedral, 0, "choir bloom", false } };
        for (const auto& vp : places) {
            const auto with = strengthRender(vp.location, vp.sub);
            wb::samples::Bank::bypassForTests(true);
            const auto without = strengthRender(vp.location, vp.sub);
            wb::samples::Bank::bypassForTests(false);
            const std::size_t n = std::min(with.outL.size(), without.outL.size());
            double diff = 0, out = 0, dry = 0;
            for (std::size_t i = 0; i < n; ++i) {
                const double d = double(with.outL[i]) - without.outL[i];
                diff += d * d;
                out += double(without.outL[i]) * without.outL[i];
            }
            for (std::size_t i = 0; i < std::min(n, with.dryL.size()); ++i) dry += double(with.dryL[i]) * with.dryL[i];
            const double addedDb = 10.0 * std::log10((diff + 1e-20) / (out + 1e-20));
            const double levelDb = loudness(with.outL, with.outR, with.sr) - loudness(without.outL, without.outR, without.sr);
            const std::string name = placeName(vp.location, vp.sub) + " (" + vp.what + ")";
            s.metric(name + ": change re the place's output", addedDb, " dB");
            judge(s, addedDb > -40.0, !recordings, name + ": the developer's voice is there", fmt(addedDb) + " dB");
            if (vp.whispers) judge(s, addedDb < -12.0, !recordings, name + ": the whispers stay faint (at least 12 dB under the place)", fmt(addedDb) + " dB");
            judge(s, std::abs(levelDb) < 1.5, !recordings, name + ": the place keeps its loudness (within 1.5 dB)", fmt(levelDb) + " dB");
            (void) dry;
        }
        // The Atmosphere layer whispers with the voice in the same three places.
        s.check(dc::atmosphereOf(dc::HauntedForest, 3).sounds[1] == dc::AtmosSound::Whisper, "the Ancient Grove's atmosphere whispers");
    }
}

// The melody, then four chords (A minor, F, G, E minor), on one instrument.
MelodyRender renderMelodyAndChords(int instrument, bool inPlace, double tail) {
    InstHost h;
    h.play(instrument);
    if (!inPlace) h.dry();
    h.render(0.05);
    melody(h, 0.1);
    static constexpr int chords[4][3] { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 }, { 52, 55, 59 } };
    double t = 0.1 + melodySeconds + 0.3;
    for (const auto& chord : chords) {
        for (int key : chord) h.note(t, 1.1, key, 0.7f);
        t += 1.3;
    }
    h.render(t + tail);
    return { h.outL, h.outR, h.sr };
}

int listenRecordings(const juce::String& directory) {
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    juce::String cues;
    std::vector<float> allL, allR;
    int index = 0;
    const auto clean = [](juce::String t) { return t.removeCharacters("'&, ").replace(" / ", "-"); };
    for (int k : changedInstrumentsFiltered()) {
        // Dry, a second's pause, in its place; then the same without the recordings (the
        // stand-in), dry.
        const auto dryRender = renderMelodyAndChords(k, false, 2.0);
        const auto placed = renderMelodyAndChords(k, true, 3.0);
        wb::samples::Bank::bypassForTests(true);
        const auto standIn = renderMelodyAndChords(k, false, 2.0);
        wb::samples::Bank::bypassForTests(false);
        std::vector<float> l, r;
        for (const auto* part : { &dryRender, &placed, &standIn }) {
            l.insert(l.end(), part->l.begin(), part->l.end());
            r.insert(r.end(), part->r.begin(), part->r.end());
            const auto fade = std::size_t(0.05 * part->sr);
            for (std::size_t i = 0; i < fade && i < l.size(); ++i) {
                l[l.size() - 1 - i] *= float(i) / float(fade);
                r[r.size() - 1 - i] *= float(i) / float(fade);
            }
            l.insert(l.end(), std::size_t(part->sr), 0.0f);
            r.insert(r.end(), std::size_t(part->sr), 0.0f);
        }
        const int location = dc::inst::locationOf(k), sub = dc::inst::subOf(k);
        const juce::String name = juce::String(++index).paddedLeft('0', 2) + "-" + clean(dc::subLocationName(location, sub)) + "-" + clean(dc::inst::design(k).name);
        writeWav(dir.getChildFile(name + "-dry-in-place-then-stand-in.wav"), l, r, dryRender.sr);
        cues << juce::String(double(allL.size()) / dryRender.sr, 1) << " s  " << name << "  (" << recordingsOf(k) << ")\n";
        allL.insert(allL.end(), l.begin(), l.end());
        allR.insert(allR.end(), r.begin(), r.end());
        std::cout << name << std::endl;
    }
    writeWav(dir.getChildFile("all-changed-instruments.wav"), allL, allR, 48000);
    dir.getChildFile("all-changed-instruments-cues.txt").replaceWithText(cues);
    // The FX's voice places: the guitar and synth phrase through each, without the voice
    // (before Session 8: the same build with the recordings switched off), then with it.
    const std::pair<int, int> fxPlaces[] { { dc::Necropolis, 1 }, { dc::HauntedForest, 3 }, { dc::Cathedral, 2 }, { dc::Dungeon, 4 }, { dc::Castle, 3 }, { dc::Cathedral, 0 } };
    for (const auto& [l, sub] : fxPlaces) {
        wb::samples::Bank::bypassForTests(true);
        const auto before = strengthRender(l, sub, 1.0f, 44100);
        wb::samples::Bank::bypassForTests(false);
        const auto after = strengthRender(l, sub, 1.0f, 44100);
        std::vector<float> L(before.outL), R(before.outR);
        L.insert(L.end(), std::size_t(44100), 0.0f);
        R.insert(R.end(), std::size_t(44100), 0.0f);
        L.insert(L.end(), after.outL.begin(), after.outL.end());
        R.insert(R.end(), after.outR.begin(), after.outR.end());
        const auto name = "fx-" + clean(placeName(l, sub)) + "-before-then-after.wav";
        writeWav(dir.getChildFile(name), L, R, 44100);
        std::cout << name << std::endl;
    }
    return 0;
}
