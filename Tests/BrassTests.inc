// Session 12: the horns that hit, and LIGHT SEEKER's Punch UI (included by
// DungeonCrawlerTests.cpp, "brass" group).
//   hit        each horn and brass (Tower Top's horn, the Great Hall's fanfare, the Final
//              Boss Lair's doom brass, the Dwarven Hall's low brass) against Session 11's
//              sound: a much louder hit over the same sustain, a quicker rise, a brighter
//              edge on the attack, a steeper velocity response, never above full scale
//   reference  at least LIGHT SEEKER's brass (Fanfare Trumpets, Heroic Horns), measured with
//              the same code (Tests/BrassMetrics.h, tools/brass-reference)
//   place      in its place the dry hit lands first (the reverb drier, later, ducked)
//   sessions   a session saved in Session 11 keeps its sound and its Punch; one saved
//              before keeps the classic sound; new instances, Init and presets get the latest
//              (sound 12 then; 13 since Session 13, the recorded brass)
//   punch ui   Punch and Punch Time as in LIGHT SEEKER: names, dB / ms readouts that
//              follow the instrument, typed values, double-click default, their place

#include "BrassMetrics.h"

namespace brasstests {

// The four horns and brass of the notes, and which LIGHT SEEKER brass each is held against.
struct Horn { int index; const char* lightSeeker; };
const std::array<Horn, 4> horns { { { 9, "Heroic Horns" }, { 6, "Fanfare Trumpets" }, { 4, "Heroic Horns" }, { 32, nullptr } } };

// LIGHT SEEKER at its commit 7ae48c2 (Mix and Punch), measured by tools/brass-reference
// with Tests/BrassMetrics.h: a held C major chord (60 64 67) at velocity 0.95, dry, its
// default Punch, without the recordings (as here). Attack-to-sustain and rise.
struct Reference { const char* name; double attackToSustainDb, riseMs; };
// From tools/brass-reference's light-seeker-brass.txt (Tests/reference/brass, the DEVLOG).
const std::array<Reference, 2> lightSeeker { { { "Fanfare Trumpets", 8.51, 8.0 }, { "Heroic Horns", 10.68, 12.0 } } };
const Reference* referenceFor(const char* name) {
    for (const auto& r : lightSeeker)
        if (name != nullptr && std::string(r.name) == name) return &r;
    return nullptr;
}

struct Played {
    std::vector<float> l, r;
    std::size_t onset = 0;
    brassmetrics::Result m() const { return brassmetrics::measure({ l, r, 48000.0, onset }); }
};

// An instrument at a sound generation, dry or in its place, playing timed notes (seconds
// from the onset); Punch at its default unless given.
Played perform(int instrument, int sound, bool dry, const std::vector<brassmetrics::PhraseNote>& notes, double seconds, float punch = -1.0f, float level = 0.0f) {
    InstHost h;
    h.p->setSoundGeneration(sound);
    h.play(instrument);
    if (dry) h.dry();
    if (punch >= 0.0f) h.set(dc::ids::instPunch, punch);
    if (level != 0.0f) h.set(dc::ids::instLevel, level);
    h.render(0.4);   // the Level glides: settled before the first note
    for (const auto& n : notes) h.note(0.4 + n.at, n.length, n.key, n.velocity);
    h.render(seconds);
    Played p;
    p.l = h.outL;
    p.r = h.outR;
    p.onset = h.idx(0.4);
    return p;
}
std::vector<brassmetrics::PhraseNote> held(std::initializer_list<int> keys, float velocity) {
    std::vector<brassmetrics::PhraseNote> notes;
    for (int k : keys) notes.push_back({ 0.0, 1.4, k, velocity });
    return notes;
}
Played chord(int instrument, int sound, float velocity = 0.95f, bool dry = true, float punch = -1.0f) {
    return perform(instrument, sound, dry, held({ 60, 64, 67 }, velocity), 1.8, punch);
}

void writeWav(const juce::File& file, const std::vector<float>& l, const std::vector<float>& r) {
    juce::AudioBuffer<float> b(2, int(l.size()));
    for (int i = 0; i < b.getNumSamples(); ++i) { b.setSample(0, i, l[std::size_t(i)]); b.setSample(1, i, r[std::size_t(i)]); }
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    if (auto* writer = wav.createWriterFor(stream.get(), 48000, 2, 24, {}, 0)) {
        stream.release();
        std::unique_ptr<juce::AudioFormatWriter> w(writer);
        w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
    }
    std::cout << "wrote " << file.getFullPathName() << "\n";
}

// What a slider's text box shows.
juce::String shownText(juce::Slider& slider) {
    for (auto* c : slider.getChildren())
        if (auto* label = dynamic_cast<juce::Label*>(c)) return label->getText();
    return {};
}

std::string line(const brassmetrics::Result& m) {
    return "attack-to-sustain " + fmt(m.attackToSustainDb) + " dB, rise " + fmt(m.riseMs, 1) + " ms, hit at " + fmt(m.hitMs, 0) + " ms, sustain " + fmt(m.sustainDb) + " dBFS, peak " + fmt(m.peakDb) + " dBFS, presence "
           + fmt(m.presenceDb) + " dB, edge " + fmt(m.edgeDb) + " dB";
}
} // namespace brasstests

void testBrassHit(Suite& s) {
    using namespace brasstests;
    s.section("brass: the horns hit (Session 12 against Session 11, and LIGHT SEEKER)");
    for (const auto& horn : horns) {
        const std::string name = dc::inst::design(horn.index).name;
        const bool chant = horn.index == 32;
        const auto before = chord(horn.index, dc::inst::SoundMix).m(), after = chord(horn.index, dc::inst::SoundHorns).m();
        const auto softBefore = chord(horn.index, dc::inst::SoundMix, 0.6f).m(), softAfter = chord(horn.index, dc::inst::SoundHorns, 0.6f).m();
        std::cout << "    " << name << " before: " << line(before) << "\n    " << name << " after:  " << line(after) << "\n";
        s.metric(name + ": attack-to-sustain, Session 11 -> 12", after.attackToSustainDb - before.attackToSustainDb, " dB");
        s.metric(name + ": rise 10-90 %, Session 11 -> 12", after.riseMs - before.riseMs, " ms");
        s.metric(name + ": the hit within 3 dB of its peak, Session 11 -> 12", after.hitMs - before.hitMs, " ms");
        // A much stronger hit: +4 dB over Session 11 (the Dwarven Chant, whose brass is a layer
        // under the voices, +2 dB).
        const double more = chant ? 2.0 : 4.0;
        s.check(after.attackToSustainDb >= before.attackToSustainDb + more, name + ": the hit over the sustain " + fmt(more, 0) + " dB or more above Session 11's",
                fmt(before.attackToSustainDb) + " -> " + fmt(after.attackToSustainDb) + " dB");
        // Faster: the hit arrives sooner (the Dwarven Chant's chord swells with its voices:
        // reported only).
        if (!chant)
            s.check(after.hitMs < before.hitMs || after.hitMs <= 6.0, name + ": the hit arrives sooner (within 3 dB of its peak)",
                    fmt(before.hitMs, 0) + " -> " + fmt(after.hitMs, 0) + " ms");
        s.check(std::abs(after.sustainDb - before.sustainDb) <= 0.5, name + ": the same sustain level (within 0.5 dB)", fmt(before.sustainDb) + " -> " + fmt(after.sustainDb) + " dBFS");
        s.check(after.peakDb <= -0.1, name + ": never above full scale (dry chord, velocity 0.95)", fmt(after.peakDb) + " dBFS");
        if (horn.index == 6) {
            // The Brass Fanfare: its edge is held back by the instruments' level check (the
            // brightest, hottest brass; a stronger edge put its melody 4-5 dB above the
            // others'): reported only.
            s.metric(name + ": edge on the attack, Session 11 -> 12 (report only)", after.edgeDb - before.edgeDb, " dB");
        } else if (!chant) {
            s.check(after.edgeDb >= before.edgeDb + 1.0, name + ": a brighter edge on the attack (2-4 kHz, 30-120 ms, against the held note)",
                    fmt(before.edgeDb) + " -> " + fmt(after.edgeDb) + " dB");
            const double spanBefore = before.attackToSustainDb - softBefore.attackToSustainDb, spanAfter = after.attackToSustainDb - softAfter.attackToSustainDb;
            s.check(spanAfter >= spanBefore + 1.0, name + ": the hit follows the velocity more (velocity 0.6 to 0.95)", fmt(spanBefore) + " -> " + fmt(spanAfter) + " dB");
        }
        if (const auto* ref = referenceFor(horn.lightSeeker)) {
            s.metric(name + ": attack-to-sustain against LIGHT SEEKER's " + ref->name, after.attackToSustainDb - ref->attackToSustainDb, " dB");
            s.check(after.attackToSustainDb >= ref->attackToSustainDb, name + ": at least the punch of LIGHT SEEKER's " + std::string(ref->name),
                    fmt(after.attackToSustainDb) + " vs " + fmt(ref->attackToSustainDb) + " dB");
            s.check(after.riseMs <= ref->riseMs + 2.0, name + ": and a rise as quick (within 2 ms)", fmt(after.riseMs, 1) + " vs " + fmt(ref->riseMs, 1) + " ms");
        }
    }
    {
        // The loudest case never clips: a six-note chord at velocity 1, Punch 200 % (+18 dB on
        // the brass), Level +12 dB, in its place.
        std::string fail;
        for (int i : { 9, 6, 4 }) {
            std::vector<brassmetrics::PhraseNote> notes;
            for (int k : { 48, 55, 60, 64, 67, 72 }) notes.push_back({ 0.0, 1.0, k, 1.0f });
            const auto p = perform(i, dc::inst::SoundHorns, false, notes, 2.0, 2.0f, 12.0f);
            const auto m = p.m();
            bool finite = true;
            for (std::size_t k = 0; k < p.l.size(); ++k) finite &= std::isfinite(p.l[k]) && std::isfinite(p.r[k]);
            if (!finite || m.peakDb > 0.0) fail += std::string(dc::inst::design(i).name) + " (" + fmt(m.peakDb) + " dBFS); ";
        }
        s.check(fail.empty(), "brass at Punch 200 %, Level +12 dB, velocity 1: never clips", fail);
    }
    {
        // Punch: the brass's own is higher, its range larger; the others as before.
        using namespace dc::inst;
        s.check(designPunchDb(9) >= 12.0f && designPunchDb(4) >= 15.0f && designPunchDb(6) > designPunchDb(6, SoundMix) && designPunchDb(32) > designPunchDb(32, SoundMix),
                "Punch 100 %: the brass's own higher (horn +12, doom +18, fanfare +10 dB; Session 11: +9), the Dwarven Chant's more");
        s.check(punchDb(6, 2.0f) == 18.0f && punchDb(10, 2.0f) == 12.0f && punchDb(30, 2.0f) == 12.0f && punchDb(6, 0.0f) == 0.0f,
                "Punch 200 %: +18 dB on the brass, +12 dB on the others; 0 % none");
        s.check(designPunchDb(6, SoundMix) == 9.0f && punchDb(6, 2.0f, SoundMix) == 12.0f && designPunchDb(6, SoundClassic) == 0.0f,
                "Session 11's and the classic sound's Punch unchanged (9 dB, at most 12; none)");
        s.check(punchVelocityScale(6, 1.0f) > punchVelocityScale(6, 1.0f, SoundMix) - 1.0e-6f && punchVelocityScale(6, 0.4f) < punchVelocityScale(6, 0.4f, SoundMix),
                "the brass's Punch follows the velocity more steeply (as much at the top, less when soft)");
    }
}

void testBrassPlace(Suite& s) {
    using namespace brasstests;
    s.section("brass: in its place the dry hit lands first");
    using namespace dc::inst;
    for (int i : { 9, 6, 4 }) {
        const auto now = mixFit(i), then = mixFit(i, SoundMix);
        s.check(now.reverb < then.reverb && now.preDelayMs > then.preDelayMs && now.duck > 0.2f,
                std::string(design(i).name) + ": less reverb, a later pre-delay, ducked under the hit",
                fmt(then.reverb) + "/" + fmt(then.preDelayMs, 0) + " ms -> " + fmt(now.reverb) + "/" + fmt(now.preDelayMs, 0) + " ms, duck " + fmt(now.duck));
        // Measured: the first 120 ms of the note in its place against the same with MIX Reverb
        // at 0 % (the same place, its colour, its echoes: only the reverbs taken out); what
        // is left over is the reverb on the hit. Session 11 against 12.
        const auto share = [&](int sound) {
            const auto take = [&](float reverb) {
                InstHost h;
                h.p->setSoundGeneration(sound);
                h.play(i);
                h.set(dc::ids::mixGroups[0], reverb);
                h.render(0.4);
                h.note(0.4, 0.9, 60, 0.9f);
                h.render(1.4);
                return std::make_pair(h.outL, h.outR);
            };
            const auto placed = take(1.0f), none = take(0.0f);
            const std::size_t from = 19200, to = from + 5760;
            double xy = 0, yy = 0, oo = 0;
            for (std::size_t k = from; k < to; ++k) {
                xy += double(placed.first[k]) * none.first[k] + double(placed.second[k]) * none.second[k];
                yy += double(none.first[k]) * none.first[k] + double(none.second[k]) * none.second[k];
                oo += double(placed.first[k]) * placed.first[k] + double(placed.second[k]) * placed.second[k];
            }
            const double g = xy / std::max(yy, 1.0e-20), hit = g * g * yy, rest = std::max(oo - hit, 1.0e-20);
            return 10.0 * std::log10(std::max(hit, 1.0e-20) / rest);
        };
        const double before = share(SoundMix), after = share(SoundHorns);
        s.metric(std::string(design(i).name) + ": the hit against its reverb in the first 120 ms, Session 11 -> 12", after - before, " dB");
        s.check(after >= before + 1.0, std::string(design(i).name) + ": the hit lands drier in its place (against its reverb, first 120 ms, +1 dB or more)",
                fmt(before) + " -> " + fmt(after) + " dB");
    }
}

// The horn phrase's level in its place moves with its dry level (Session 11 -> 12): the drier,
// ducked space does not make the horn quieter where it is heard.
void testBrassPlaceLevel(Suite& s) {
    using namespace brasstests;
    s.section("brass: as loud in its place as before, against its dry self");
    const auto phrase = brassmetrics::hornPhrase();
    for (const auto& horn : horns) {
        const auto level = [&](int sound, bool dry) {
            const auto p = perform(horn.index, sound, dry, phrase, brassmetrics::hornPhraseSeconds());
            double sum = 0;
            for (std::size_t k = p.onset; k < p.l.size(); ++k) sum += 0.5 * (double(p.l[k]) * p.l[k] + double(p.r[k]) * p.r[k]);
            return 10.0 * std::log10(std::max(sum, 1.0e-20) / double(p.l.size() - p.onset));
        };
        const double dry = level(dc::inst::SoundHorns, true) - level(dc::inst::SoundMix, true);
        const double placed = level(dc::inst::SoundHorns, false) - level(dc::inst::SoundMix, false);
        const std::string name = dc::inst::design(horn.index).name;
        s.check(std::abs(placed - dry) < 0.75 && dry < 1.5, name + ": the phrase in its place changes as it does dry (and dry by under 1.5 dB)",
                "dry " + fmt(dry) + " dB, in its place " + fmt(placed) + " dB");
    }
}

void testBrassSessions(Suite& s) {
    using namespace brasstests;
    s.section("brass: sessions keep their sound and their Punch");
    const auto saved = [](int sound, float punch, bool withSound) {
        DungeonCrawlerProcessor p(PluginKind::Instrument);
        p.setSoundGeneration(sound);
        auto* q = p.parameters.getParameter(dc::ids::instPunch);
        q->setValueNotifyingHost(q->convertTo0to1(punch));
        juce::MemoryBlock block;
        p.getStateInformation(block);
        if (!withSound) {
            auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), int(block.getSize()));
            xml->removeAttribute("sound");
            block.reset();
            juce::AudioProcessor::copyXmlToBinary(*xml, block);
        }
        return block;
    };
    const auto punchOf = [](DungeonCrawlerProcessor& p) { auto* q = p.parameters.getParameter(dc::ids::instPunch); return q->convertFrom0to1(q->getValue()); };
    {
        DungeonCrawlerProcessor fresh(PluginKind::Instrument);
        // Session 13: the latest sound is 13 (the recorded brass; its Punch the same as 12's).
        s.check(fresh.soundGenerationUsed() == dc::inst::SoundLatest && std::abs(punchOf(fresh) - 1.0f) < 1.0e-6f, "a new instance plays the latest sound at Punch 100 % (the horns' +12 dB)");
    }
    for (int sound : { int(dc::inst::SoundMix), int(dc::inst::SoundHorns) }) {
        const auto block = saved(sound, 1.37f, true);
        DungeonCrawlerProcessor r(PluginKind::Instrument);
        r.setStateInformation(block.getData(), int(block.getSize()));
        s.check(r.soundGenerationUsed() == sound && std::abs(punchOf(r) - 1.37f) < 1.0e-3f,
                "a session saved with sound " + std::to_string(sound) + " reopens with it and its Punch (137 %)",
                "sound " + std::to_string(r.soundGenerationUsed()) + ", Punch " + fmt(punchOf(r), 3));
    }
    {
        const auto block = saved(dc::inst::SoundMix, 0.4f, false);
        DungeonCrawlerProcessor r(PluginKind::Instrument);
        r.setStateInformation(block.getData(), int(block.getSize()));
        s.check(r.usesClassicSound() && std::abs(punchOf(r) - 0.4f) < 1.0e-3f, "a session saved before Session 11 (no sound) keeps the classic sound and its Punch");
    }
    {
        // The Session 11 sound, reopened, plays sample for sample as Session 11's (the same
        // processor at sound 11), and differs from sound 12.
        const auto play = [](const juce::MemoryBlock* state, int sound) {
            InstHost h;
            if (state != nullptr) h.p->setStateInformation(state->getData(), int(state->getSize()));
            else h.p->setSoundGeneration(sound);
            h.play(9);
            h.set(dc::ids::instPunch, 1.37f);
            h.render(0.1);
            for (int k : { 60, 64, 67 }) h.note(0.2, 0.8, k, 0.9f);
            h.render(1.5);
            return std::make_pair(h.outL, h.outR);
        };
        const auto block = saved(dc::inst::SoundMix, 1.37f, true);
        const auto reopened = play(&block, 0), direct = play(nullptr, dc::inst::SoundMix), latest = play(nullptr, dc::inst::SoundHorns);
        s.check(reopened == direct && reopened.first != latest.first, "a Session 11 session plays its horn exactly as Session 11's sound (and not as sound 12)");
    }
    {
        // Init and a factory preset bring the latest sound.
        DungeonCrawlerProcessor p(PluginKind::Instrument);
        p.setSoundGeneration(dc::inst::SoundMix);
        p.applyFactoryPreset(1);
        const bool preset = p.soundGenerationUsed() == dc::inst::SoundLatest;
        p.setSoundGeneration(dc::inst::SoundMix);
        p.applyInit();
        s.check(preset && p.soundGenerationUsed() == dc::inst::SoundLatest, "a factory preset and Init bring the latest sound");
    }
}

void testBrassPunchUi(Suite& s) {
    s.section("brass: Punch and Punch Time on the instrument panel (LIGHT SEEKER's)");
    windowtests::Window w(true);
    w.go(dc::Castle, 4);   // Tower Top's horn
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int k = 0; k < 4; ++k) w.p->processBlock(buffer, midi);
    auto* panel = w.editor->instrumentPanel();
    if (panel == nullptr) {
        s.check(false, "the instrument panel exists");
        return;
    }
    panel->pollForTest();
    using P = dc::ui::InstrumentPanel;
    auto& punch = panel->sliderForTest(int(P::punchSlot));
    auto& time = panel->sliderForTest(int(P::punchTimeSlot));
    s.check(panel->macroLabel(int(P::punchSlot)) == "Punch" && panel->macroLabel(int(P::punchTimeSlot)) == "Punch Time", "labelled Punch and Punch Time",
            (panel->macroLabel(int(P::punchSlot)) + " / " + panel->macroLabel(int(P::punchTimeSlot))).toStdString());
    s.check(punch.getTextFromValue(punch.getValue()) == "+12.0 dB" && time.getTextFromValue(time.getValue()) == "120 ms",
            "their readouts: the hit in dB for the instrument (the horn's +12.0 dB) and the time in ms",
            (punch.getTextFromValue(punch.getValue()) + " / " + time.getTextFromValue(time.getValue())).toStdString());
    punch.setValue(0.0, juce::sendNotificationSync);
    const auto off = punch.getTextFromValue(punch.getValue());
    punch.setValue(2.0, juce::sendNotificationSync);
    const auto most = punch.getTextFromValue(punch.getValue());
    s.check(off == "Off" && most == "+18.0 dB", "Off at none, +18.0 dB at the most on the brass", (off + " / " + most).toStdString());
    // Typed: a value in dB, or Off (the text box is editable, as in LIGHT SEEKER).
    s.check(punch.isTextBoxEditable() && time.isTextBoxEditable(), "their readouts can be typed into");
    punch.setValue(punch.getValueFromText("+6 dB"), juce::sendNotificationSync);
    const float typed = w.p->parameters.getRawParameterValue(dc::ids::instPunch)->load();
    s.check(std::abs(typed - 0.5f) < 0.01f, "typing +6 dB sets half the horn's own Punch (50 %)", fmt(typed, 3));
    // Double-click: the default.
    punch.setValue(punch.getDoubleClickReturnValue(), juce::sendNotificationSync);
    s.check(std::abs(w.p->parameters.getRawParameterValue(dc::ids::instPunch)->load() - 1.0f) < 1.0e-4f && punch.isDoubleClickReturnEnabled(), "double-click returns it to the instrument's own");
    // The readout follows the instrument: an organ has no Punch of its own.
    w.p->chooseInstrument(8);
    for (int k = 0; k < 2; ++k) w.p->processBlock(buffer, midi);
    panel->pollForTest();
    s.check(punch.getTextFromValue(punch.getValue()) == "Off" && brasstests::shownText(punch) == "Off", "on the Chamber Organ the same Punch reads Off (its own is none)",
            brasstests::shownText(punch).toStdString());
    // Where they sit: after Intensity, before the Effects roller, wide enough, not overlapping.
    const auto intensity = panel->sliderForTest(int(P::intensitySlot)).getBounds();
    const auto pb = punch.getBounds(), tb = time.getBounds(), roller = panel->effectsRoller().getBounds();
    s.check(pb.getX() > intensity.getRight() && tb.getX() >= pb.getRight() && tb.getRight() < roller.getX() && pb.getWidth() >= 54 && tb.getWidth() >= 54,
            "after Intensity, before the Effects roller, in their wider slots",
            std::to_string(pb.getX()) + ".." + std::to_string(tb.getRight()) + " (roller at " + std::to_string(roller.getX()) + ")");
}

void testBrass(Suite& s) {
    testBrassPunchUi(s);
    testBrassSessions(s);
    testBrassHit(s);
    testBrassPlace(s);
    testBrassPlaceLevel(s);
}

// `--brass-report`: the numbers above, printed (Session 11 and 12, chord and note).
int brassReport() {
    using namespace brasstests;
    {
        // The instruments' level check (InstrumentTests: the melody dry, its loudest 400 ms).
        const auto level = [](int k, int sound, bool inPlace, float punch = -1.0f) {
            InstHost h;
            h.p->setSoundGeneration(sound);
            h.play(k);
            if (punch >= 0.0f) h.set(dc::ids::instPunch, punch);
            if (!inPlace) h.dry();
            h.render(0.05);
            melody(h, 0.1);
            h.render(0.05 + melodySeconds + (inPlace ? 3.0 : 1.5));
            return loudness(h.outL, h.outR, h.sr);
        };
        std::vector<double> all;
        for (int k = 0; k < dc::inst::instrumentCount; ++k) all.push_back(level(k, dc::inst::SoundHorns, false));
        auto sorted = all;
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];
        for (int i : { 9, 6, 4, 32 })
            std::cout << dc::inst::design(i).name << ": melody level against the median, sound 11 " << fmt(level(i, dc::inst::SoundMix, false) - median) << " dB, sound 12 "
                      << fmt(all[std::size_t(i)] - median) << " dB; in its place against dry " << fmt(level(i, dc::inst::SoundHorns, true) - all[std::size_t(i)])
                      << " dB; at Punch 0: sound 11 " << fmt(level(i, dc::inst::SoundMix, false, 0.0f) - median) << ", sound 12 " << fmt(level(i, dc::inst::SoundHorns, false, 0.0f) - median) << " dB\n";
    }
    for (int i : { 9, 6, 4 }) {
        // The synth alone (before the place, the Level and the output's limiter).
        dc::inst::Synth synth;
        synth.prepare(48000);
        synth.setDesign(i);
        synth.setSound(dc::inst::SoundHorns);
        std::vector<float> l(48000 * 2, 0.0f), r(l.size(), 0.0f);
        for (int k : { 60, 64, 67 }) synth.noteOn(k, 0.95f);
        synth.render(l.data(), r.data(), int(l.size()));
        float peak = 0;
        std::size_t at = 0;
        for (std::size_t k = 0; k < l.size(); ++k)
            if (std::max(std::abs(l[k]), std::abs(r[k])) > peak) { peak = std::max(std::abs(l[k]), std::abs(r[k])); at = k; }
        std::cout << dc::inst::design(i).name << ": the synth's own peak " << fmt(brassmetrics::toDb(peak)) << " dBFS at " << fmt(double(at) / 48.0, 1) << " ms\n";
    }
    for (const auto& horn : horns) {
        for (int sound : { int(dc::inst::SoundMix), int(dc::inst::SoundHorns) }) {
            std::cout << dc::inst::design(horn.index).name << " sound " << sound << "\n";
            {
                const auto p = chord(horn.index, sound);
                const auto e = brassmetrics::envelope({ p.l, p.r, 48000.0, p.onset }, 2.0, 0.06);
                std::cout << "  envelope (2 ms, dBFS):";
                for (double v : e) std::cout << " " << fmt(brassmetrics::toDb(v), 0);
                std::cout << "\n";
            }
            std::cout << "  chord v0.95: " << line(chord(horn.index, sound).m()) << "\n";
            std::cout << "  chord v0.6:  " << line(chord(horn.index, sound, 0.6f).m()) << "\n";
            std::cout << "  note v0.95:  " << line(perform(horn.index, sound, true, held({ 60 }, 0.95f), 1.8).m()) << "\n";
        }
    }
    return 0;
}

// `--listen-brass dir` (CI test-logs `brass/`): every horn and brass, the same horn phrase,
// Session 11's sound then Session 12's, dry and in its place; and the report.
int listenBrass(const juce::String& directory) {
    using namespace brasstests;
    juce::File dir(directory);
    dir.createDirectory();
    const auto phrase = brassmetrics::hornPhrase();
    const double seconds = brassmetrics::hornPhraseSeconds();
    for (const auto& horn : horns) {
        const juce::String name = juce::String(dc::inst::design(horn.index).name).toLowerCase().replace(" & ", "-and-").replace(" ", "-");
        for (bool dry : { true, false })
            for (int sound : { int(dc::inst::SoundMix), int(dc::inst::SoundHorns) }) {
                const auto p = perform(horn.index, sound, dry, phrase, seconds);
                std::vector<float> l(p.l.begin() + long(p.onset), p.l.end()), r(p.r.begin() + long(p.onset), p.r.end());
                writeWav(dir.getChildFile(name + (dry ? "-dry-" : "-in-its-place-") + (sound == dc::inst::SoundMix ? "1-before" : "2-after") + ".wav"), l, r);
            }
    }
    return 0;
}

// `--punch-snapshots dir` (docs/snapshots/session12): the instrument panel with LIGHT
// SEEKER's Punch and Punch Time, at 2x, and the whole window.
int punchSnapshots(const juce::String& directory) {
    juce::File dir(directory);
    dir.createDirectory();
    int written = 0;
    const auto save = [&](const juce::Image& image, const char* name) {
        auto file = dir.getChildFile(name);
        file.deleteFile();
        juce::FileOutputStream stream(file);
        if (stream.openedOk() && juce::PNGImageFormat().writeImageToStream(image, stream)) ++written;
    };
    windowtests::Window w(true);
    w.go(dc::Castle, 4);   // Tower Top's horn
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int k = 0; k < 4; ++k) w.p->processBlock(buffer, midi);
    w.view().setAnimation(false);
    if (auto* panel = w.editor->instrumentPanel()) {
        panel->pollForTest();
        save(w.editor->createComponentSnapshot(panel->getBounds(), true, 2.0f), "s12-instrument-panel-punch-2x.png");
        // The Punch group alone, larger.
        const auto a = panel->sliderForTest(int(dc::ui::InstrumentPanel::punchSlot)).getBoundsInParent().getUnion(
                           panel->sliderForTest(int(dc::ui::InstrumentPanel::punchTimeSlot)).getBoundsInParent());
        save(panel->createComponentSnapshot(a.expanded(14, 0).withTop(4).withBottom(108), true, 3.0f), "s12-punch-controls-3x.png");
    }
    save(w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f), "s12-instrument-window.png");
    return written == 3 ? 0 : 1;
}
