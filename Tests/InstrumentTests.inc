// Direction Update 3: DUNGEON CRAWLER, the instrument. Included by DungeonCrawlerTests.cpp.
//   --only instrument        the engine: identity, MIDI, pedal, bend, travel with held notes, lock, stealing
//   --only instruments       every instrument: plays from MIDI, in tune, velocity, ends, siblings distinct, levels
//   --calibrate-instruments Source/Instrument/Trims.h    measure and write the level trims
//   --listen-instruments dir   the same melody on every instrument, dry and then in its place

// A host for the instrument: no input, MIDI scheduled at sample positions.
struct InstHost {
    std::unique_ptr<DungeonCrawlerProcessor> p = std::make_unique<DungeonCrawlerProcessor>(PluginKind::Instrument);
    TestPlayHead transport;
    double sr;
    int block, outs;
    long long position = 0;
    juce::AudioBuffer<float> buffer;
    std::vector<std::pair<long long, juce::MidiMessage>> events;
    std::vector<float> outL, outR;
    explicit InstHost(double sampleRate = 48000, int blockSize = 512, int outputs = 2) : sr(sampleRate), block(blockSize), outs(outputs) {
        juce::AudioProcessor::BusesLayout layout;
        layout.outputBuses.add(outputs == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        p->setBusesLayout(layout);
        p->setRateAndBufferSizeDetails(sr, block);
        p->setPlayHead(&transport);
        p->prepareToPlay(sr, block);
        buffer.setSize(outputs, block);
        midi.ensureSize(8192);
        outL.reserve(std::size_t(sr * 60));
        outR.reserve(std::size_t(sr * 60));
    }
    juce::MidiBuffer midi;   // kept (its storage reused: the real-time test counts allocations)
    void set(const juce::String& id, float plain) {
        auto* parameter = p->parameters.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
    }
    void go(int location, int sub) { set(dc::ids::location, float(location)); set(dc::ids::subLocation, float(sub)); }
    // Locks to an instrument and puts it in its own place.
    void play(int instrument, bool inItsPlace = true) {
        set(dc::ids::instLock, 1);
        set(dc::ids::instrument, float(instrument));
        if (inItsPlace) go(dc::inst::locationOf(instrument), dc::inst::subOf(instrument));
    }
    void dry() { set(dc::ids::intensity, 0); }
    long long at(double seconds) const { return static_cast<long long>(seconds * sr); }
    void on(double t, int key, float velocity = 0.8f) { events.push_back({ at(t), juce::MidiMessage::noteOn(1, key, velocity) }); }
    void off(double t, int key) { events.push_back({ at(t), juce::MidiMessage::noteOff(1, key) }); }
    void cc(double t, int controller, int value) { events.push_back({ at(t), juce::MidiMessage::controllerEvent(1, controller, value) }); }
    void bend(double t, int value) { events.push_back({ at(t), juce::MidiMessage::pitchWheel(1, value) }); }
    void note(double t, double length, int key, float velocity = 0.8f) { on(t, key, velocity); off(t + length, key); }
    void processOne(int n) {
        buffer.setSize(outs, n, false, false, true);
        buffer.clear();
        midi.clear();
        for (const auto& [when, message] : events)
            if (when >= position && when < position + n) midi.addEvent(message, int(when - position));
        p->processBlock(buffer, midi);
        for (int i = 0; i < n; ++i) {
            outL.push_back(buffer.getSample(0, i));
            outR.push_back(buffer.getSample(outs > 1 ? 1 : 0, i));
        }
        position += n;
        transport.ppq += double(n) / sr * transport.bpm / 60.0;
    }
    void render(double seconds) {
        auto remaining = at(seconds);
        while (remaining > 0) {
            const int n = int(std::min<long long>(block, remaining));
            processOne(n);
            remaining -= n;
        }
    }
    std::size_t idx(double seconds) const { return std::min(outL.size(), static_cast<std::size_t>(seconds * sr)); }
    double rms(double from, double to) const {
        double sum = 0;
        const auto a = idx(from), b = idx(to);
        for (std::size_t i = a; i < b; ++i) sum += 0.5 * (double(outL[i]) * outL[i] + double(outR[i]) * outR[i]);
        return std::sqrt(sum / double(std::max<std::size_t>(1, b - a)));
    }
    double peak(double from, double to) const {
        double m = 0;
        for (std::size_t i = idx(from), b = idx(to); i < b; ++i) m = std::max({ m, std::abs(double(outL[i])), std::abs(double(outR[i])) });
        return m;
    }
    bool finite() const {
        for (std::size_t i = 0; i < outL.size(); ++i) if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) return false;
        return true;
    }
    double maxStep(double from, double to) const {
        double m = 0;
        for (std::size_t i = std::max<std::size_t>(idx(from), 2), b = idx(to); i < b; ++i) {
            m = std::max(m, std::abs(double(outL[i]) - 2.0 * outL[i - 1] + outL[i - 2]));
            m = std::max(m, std::abs(double(outR[i]) - 2.0 * outR[i - 1] + outR[i - 2]));
        }
        return m;
    }
    // Renders until the engine has no voice left (or `limit` seconds); the seconds taken.
    double untilSilent(double limit) {
        double t = 0;
        while (t < limit && p->instrumentEngine().activeVoices() > 0) { render(0.1); t += 0.1; }
        return t;
    }
};

// The test melody (A natural minor, a dark little phrase), starting at `t`; 4.4 s long.
void melody(InstHost& h, double t, int root = 57, float velocity = 0.8f) {
    static constexpr int steps[] { 0, 3, 7, 5, 3, 2, 0, -2, 0 };
    static constexpr double lengths[] { 0.4, 0.4, 0.6, 0.4, 0.4, 0.4, 0.4, 0.4, 1.0 };
    for (std::size_t k = 0; k < std::size(steps); ++k) {
        h.note(t, lengths[k] * 0.9, root + steps[k], velocity * (k % 3 == 0 ? 1.0f : 0.85f));
        t += lengths[k];
    }
}
constexpr double melodySeconds = 4.4;

// Energy per pitch class (Goertzel on every note C2..C7) over a window of the left output.
std::array<double, 12> chroma(const std::vector<float>& x, double sr, std::size_t from, std::size_t to) {
    std::array<double, 12> c {};
    to = std::min(to, x.size());
    for (int key = 36; key <= 96; ++key) {
        const double f = 440.0 * std::pow(2.0, (key - 69) / 12.0);
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, coeff = 2.0 * std::cos(w);
        double s1 = 0, s2 = 0;
        for (std::size_t i = from; i < to; ++i) {
            const double win = 0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * double(i - from) / double(to - from));
            const double s0 = x[i] * win + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        c[std::size_t(key % 12)] += s1 * s1 + s2 * s2 - coeff * s1 * s2;
    }
    return c;
}

// Third-octave band levels (50 Hz .. 12.5 kHz) of a whole render, dB, level removed.
std::vector<double> thirdOctaves(const std::vector<float>& l, double sr) {
    std::vector<double> bands;
    for (double centre = 50.0; centre <= 12500.0; centre *= std::pow(2.0, 1.0 / 3.0)) {
        juce::IIRFilter low, high;
        high.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, centre / 1.122));
        low.setCoefficients(juce::IIRCoefficients::makeLowPass(sr, centre * 1.122));
        double sum = 0;
        for (float v : l) {
            const float y = low.processSingleSampleRaw(high.processSingleSampleRaw(v));
            sum += double(y) * y;
        }
        bands.push_back(toDb(std::sqrt(sum / double(std::max<std::size_t>(1, l.size()))) + 1.0e-9));
    }
    double mean = 0;
    for (double b : bands) mean += b;
    mean /= double(bands.size());
    for (double& b : bands) b = std::max(b - mean, -40.0);
    return bands;
}

// The envelope shape: 20 ms RMS frames, dB relative to the loudest (floored at -50).
std::vector<double> envelopeShape(const std::vector<float>& l, double sr) {
    const auto frame = static_cast<std::size_t>(0.02 * sr);
    std::vector<double> e;
    for (std::size_t i = 0; i + frame <= l.size(); i += frame) {
        double sum = 0;
        for (std::size_t k = i; k < i + frame; ++k) sum += double(l[k]) * l[k];
        e.push_back(toDb(std::sqrt(sum / double(frame)) + 1.0e-9));
    }
    const double top = e.empty() ? 0.0 : *std::max_element(e.begin(), e.end());
    for (double& v : e) v = std::max(v - top, -50.0);
    return e;
}

// Loudness: the loudest 400 ms of the left+right output, after a K-weighting-like filter
// (a 60 Hz high-pass and +4 dB above 1.7 kHz), dB.
double loudness(const std::vector<float>& l, const std::vector<float>& r, double sr) {
    juce::IIRFilter hp1, hp2, shelf1, shelf2;
    hp1.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 60.0));
    hp2.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 60.0));
    shelf1.setCoefficients(juce::IIRCoefficients::makeHighShelf(sr, 1700.0, 0.7, 1.585f));
    shelf2.setCoefficients(juce::IIRCoefficients::makeHighShelf(sr, 1700.0, 0.7, 1.585f));
    std::vector<double> power(l.size());
    for (std::size_t i = 0; i < l.size(); ++i) {
        const double a = shelf1.processSingleSampleRaw(hp1.processSingleSampleRaw(l[i]));
        const double b = shelf2.processSingleSampleRaw(hp2.processSingleSampleRaw(r[i]));
        power[i] = 0.5 * (a * a + b * b);
    }
    const auto window = static_cast<std::size_t>(0.4 * sr), hop = static_cast<std::size_t>(0.05 * sr);
    double best = 1.0e-20;
    for (std::size_t i = 0; i + window <= power.size(); i += hop) {
        double sum = 0;
        for (std::size_t k = i; k < i + window; ++k) sum += power[k];
        best = std::max(best, sum / double(window));
    }
    return 10.0 * std::log10(best);
}

std::string instName(int k) { return std::string(dc::inst::design(k).name); }

// The melody on one instrument, dry (Intensity 0) or in its place.
struct MelodyRender { std::vector<float> l, r; double sr = 48000; };
MelodyRender renderMelody(int instrument, bool inPlace, double tail = 1.5) {
    InstHost h;
    h.play(instrument);
    if (!inPlace) h.dry();
    h.render(0.05);
    melody(h, 0.1);
    h.render(0.05 + melodySeconds + tail);
    return { h.outL, h.outR, h.sr };
}

// Instruments whose "pitch" is a cloud of inharmonic modes: the in-tune check reports them.
bool unpitched(int k) {
    const std::string n = instName(k);
    return n == "War Drums" || n == "Iron Chains" || n == "Hoard Gong" || n == "Tuned Anvil";
}

void testInstrumentEngine(Suite& s) {
    s.section("instrument: two plug-ins from one source");
    {
        DungeonCrawlerProcessor fx, instrument(PluginKind::Instrument);
        s.check(fx.kind() == PluginKind::Effect && fx.getName() == "DUNGEON CRAWLER FX", "the effect is DUNGEON CRAWLER FX");
        s.check(instrument.isInstrument() && instrument.getName() == "DUNGEON CRAWLER", "the instrument is DUNGEON CRAWLER");
        s.check(instrument.getBusCount(true) == 0 && instrument.getBusCount(false) == 1, "the instrument has no input, one output");
        s.check(fx.getBusCount(true) == 1, "the effect keeps its input");
        s.check(instrument.acceptsMidi(), "the instrument takes MIDI");
        s.check(fx.parameters.getParameter(dc::ids::instrument) == nullptr && fx.parameters.getParameter(dc::ids::instLock) == nullptr,
                "the effect has none of the instrument's parameters (its IDs are as before)");
        s.check(instrument.parameters.getParameter(dc::ids::instrument) != nullptr && instrument.parameters.getParameter(dc::ids::instMacros[4]) != nullptr,
                "the instrument has Instrument, Lock and five macros");
        s.check(fx.getParameters().size() + 8 == instrument.getParameters().size(), "the instrument adds exactly eight parameters",
                std::to_string(fx.getParameters().size()) + " / " + std::to_string(instrument.getParameters().size()));
        // Every place has its own instrument, and back.
        std::set<int> seen;
        bool roundTrip = true;
        for (int l = 0; l < dc::LocationCount; ++l)
            for (int k = 0; k <= dc::subLocationCount(l); ++k) {
                const int d = dc::inst::designFor(l, k);
                seen.insert(d);
                roundTrip &= dc::inst::locationOf(d) == l && dc::inst::subOf(d) == k;
            }
        s.check(seen.size() == std::size_t(dc::inst::instrumentCount), "forty places, forty instruments", std::to_string(seen.size()));
        s.check(roundTrip, "instrument and place map both ways");
        std::set<std::string> names;
        for (int k = 0; k < dc::inst::instrumentCount; ++k) names.insert(instName(k));
        s.check(names.size() == std::size_t(dc::inst::instrumentCount), "every instrument has its own name");
    }

    s.section("instrument: silence, notes, mono");
    {
        InstHost h;
        h.render(1.0);
        s.check(h.peak(0, 1.0) < 1.0e-5, "no notes, no sound", std::to_string(h.peak(0, 1.0)));
        h.note(1.1, 0.5, 60);
        h.render(1.5);
        s.check(h.peak(1.1, 2.5) > 1.0e-3, "a note sounds (the place's instrument)");
        s.check(h.p->playingInstrument.load() == dc::inst::designFor(0, 0), "the default place plays its instrument (Corridors: contrabass)");
        InstHost mono(48000, 512, 1);
        mono.note(0.05, 0.4, 48);
        mono.render(1.0);
        s.check(mono.peak(0, 1.0) > 1.0e-3 && mono.finite(), "a mono output plays");
    }

    s.section("instrument: velocity, pedal, bend, wheel");
    {
        const int celesta = dc::inst::designFor(dc::Dungeon, 3), choir = dc::inst::designFor(dc::Cathedral, 0), whistle = dc::inst::designFor(dc::Village, 0);
        double soft = 0, hard = 0;
        for (float v : { 0.3f, 1.0f }) {
            InstHost h;
            h.play(celesta, false);
            h.dry();
            h.note(0.05, 0.4, 72, v);
            h.render(1.0);
            (v < 0.5f ? soft : hard) = h.rms(0.05, 0.6);
        }
        s.check(toDb(hard) - toDb(soft) > 4.0, "velocity: harder is louder", std::to_string(toDb(hard) - toDb(soft)) + " dB");

        double free = 0, pedalled = 0;
        for (bool pedal : { false, true }) {
            InstHost h;
            h.play(choir, false);
            h.dry();
            if (pedal) h.cc(0.02, 64, 127);
            h.note(0.05, 0.5, 64);
            if (pedal) h.cc(2.0, 64, 0);
            h.render(4.5);
            (pedal ? pedalled : free) = h.rms(1.5, 1.9);
            if (pedal) s.check(h.rms(4.0, 4.5) < 0.05 * h.rms(1.5, 1.9), "the pedal up lets the held note go");
        }
        s.check(pedalled > 4.0 * free, "the sustain pedal holds a note after its key", std::to_string(toDb(pedalled) - toDb(free)) + " dB");

        // Pitch bend: full up is +2 semitones (the default range).
        const auto pitchOf = [](const InstHost& h, double from, double to) {
            // The strongest of fine Goertzel bins 300..700 Hz (1 Hz steps).
            double bestHz = 0, best = -1;
            const auto a = h.idx(from), b = h.idx(to);
            for (double f = 300; f <= 700; f += 0.5) {
                const double w = 2.0 * juce::MathConstants<double>::pi * f / h.sr, coeff = 2.0 * std::cos(w);
                double s1 = 0, s2 = 0;
                for (std::size_t i = a; i < b; ++i) { const double s0 = h.outL[i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
                const double p = s1 * s1 + s2 * s2 - coeff * s1 * s2;
                if (p > best) best = p, bestHz = f;
            }
            return bestHz;
        };
        InstHost h;
        h.play(whistle, false);
        h.dry();
        h.on(0.05, 57, 0.7f);   // it sounds an octave up: A4
        h.bend(1.2, 16383);
        h.off(2.6, 57);
        h.render(3.0);
        const double plain = pitchOf(h, 0.5, 1.1), bent = pitchOf(h, 1.8, 2.5);
        const double cents = 1200.0 * std::log2(bent / std::max(plain, 1.0));
        s.check(std::abs(plain - 440.0) < 6.0, "the tin whistle plays A4 at 440 Hz", std::to_string(plain));
        s.check(std::abs(cents - 200.0) < 25.0, "pitch bend full up: two semitones", std::to_string(cents) + " cents");
        s.check(h.p->shownBend.load() > 0.9f, "the panel sees the bend");

        InstHost a, b;
        for (auto* x : { &a, &b }) { x->play(choir, false); x->dry(); }
        b.cc(0.01, 1, 127);
        a.note(0.05, 2.0, 64);
        b.note(0.05, 2.0, 64);
        a.render(2.2);
        b.render(2.2);
        double diff = 0;
        for (std::size_t i = a.idx(1.0); i < a.idx(2.0); ++i) diff += std::abs(double(a.outL[i]) - b.outL[i]);
        s.check(diff > 1.0, "the mod wheel changes the sound (vibrato on the choir)");
        s.check(b.p->shownMod.load() > 0.95f, "the panel sees the mod wheel");
    }

    s.section("instrument: travel with a held note, lock, stealing");
    {
        // A cello held in the Entrance Hall while travelling to the Courtyard.
        InstHost h;
        h.go(dc::Dungeon, 1);
        h.set(dc::ids::travelTime, 1.5f);
        h.on(0.05, 48, 0.8f);
        h.render(1.0);
        const double before = h.rms(0.6, 1.0), stepBefore = h.maxStep(0.3, 1.0);
        h.go(dc::Castle, 0);
        h.render(2.0);
        double lowest = 1.0e9;
        for (double t = 1.0; t < 2.9; t += 0.1) lowest = std::min(lowest, h.rms(t, t + 0.1));
        // Not cut (a cut note falls 60 dB and more); the spaces crossfade around it and the new
        // place's level correction applies, so it may dip (measured about -11 dB here).
        s.check(toDb(lowest) > toDb(before) - 18.0, "a note held through travel keeps sounding", std::to_string(toDb(lowest) - toDb(before)) + " dB");
        s.metric("held cello, Entrance Hall to Courtyard: deepest dip", toDb(lowest) - toDb(before), " dB");
        s.check(h.maxStep(1.0, 3.0) < 2.5 * stepBefore + 1.0e-4, "travel with a held note: no click", std::to_string(h.maxStep(1.0, 3.0) / stepBefore));
        s.check(h.p->playingInstrument.load() == dc::inst::designFor(dc::Castle, 0), "the new place gives the next notes its instrument (the lute)");
        h.on(3.0, 64, 0.8f);
        h.render(0.5);
        h.off(3.6, 48);
        h.off(3.6, 64);
        h.render(0.2);
        const double left = h.untilSilent(20.0);
        s.check(h.p->instrumentEngine().activeVoices() == 0, "no hung notes after travel", std::to_string(left) + " s to silence");

        // Many quick travels with chords held and released: never a hung note.
        InstHost q;
        q.set(dc::ids::travelTime, 0.2f);
        for (int k = 0; k < 24; ++k) {
            q.go(k % dc::LocationCount, k % 3);
            for (int n : { 48, 55, 60, 63 }) q.on(q.position / q.sr + 0.01, n + k % 5, 0.7f);
            q.render(0.15);
            for (int n : { 48, 55, 60, 63 }) q.off(q.position / q.sr + 0.01, n + k % 5);
            q.render(0.1);
        }
        q.untilSilent(30.0);
        s.check(q.p->instrumentEngine().activeVoices() == 0 && q.finite(), "24 journeys with chords: no hung notes, finite");

        // Lock: the Tavern's guitar in the Final Boss Lair.
        InstHost l;
        l.go(dc::Village, 1);
        l.render(0.1);
        l.p->setInstrumentLock(true);
        l.go(dc::Dungeon, 4);
        l.render(0.2);
        s.check(l.p->playingInstrument.load() == dc::inst::designFor(dc::Village, 1), "Lock Instrument keeps the Tavern guitar in the Final Boss Lair");
        l.p->setInstrumentLock(false);
        l.render(0.1);
        s.check(l.p->playingInstrument.load() == dc::inst::designFor(dc::Dungeon, 4), "unlocked, the place's own instrument returns");
        l.p->chooseInstrument(dc::inst::designFor(dc::WizardsTower, 3));
        l.render(0.1);
        s.check(l.p->instrumentLocked() && l.p->playingInstrument.load() == dc::inst::designFor(dc::WizardsTower, 3), "choosing an instrument locks it");

        // Stealing: a run of 60 fast notes on a 12-voice celesta, every one ringing.
        InstHost r;
        r.play(dc::inst::designFor(dc::Dungeon, 3), false);
        r.dry();
        r.note(0.05, 0.05, 72, 0.9f);
        r.render(0.5);
        const double single = r.maxStep(0.05, 0.5);
        for (int k = 0; k < 60; ++k) r.note(0.6 + 0.03 * k, 0.02, 60 + (k * 7) % 24, 0.9f);
        r.render(2.5);
        s.check(r.maxStep(0.6, 3.0) < 4.0 * single, "voice stealing does not click", std::to_string(r.maxStep(0.6, 3.0) / single));
        s.check(r.p->instrumentEngine().activeVoices() <= dc::inst::maxVoices, "voices stay within the pool");
    }

    s.section("instrument: the effect captures");
    if (dc::inst::capturesLoaded() > 0) {
        InstHost h;
        h.play(dc::inst::designFor(dc::Caverns, 1), false);
        h.dry();
        h.render(0.05);
        s.check(h.p->instrumentEngine().capturesReady(dc::inst::CaptureShimmer), "the shimmer capture is loaded", std::to_string(dc::inst::capturesLoaded()) + " of 3");
        h.note(0.1, 0.3, 69, 0.9f);
        h.render(4.0);
        // The bells die away in about 4 s; the shimmer's tail keeps ringing after them.
        s.check(h.rms(3.0, 4.0) > 0.02 * h.rms(0.1, 0.6) && h.finite(), "the crystal bells carry the shimmer's long tail",
                std::to_string(toDb(h.rms(3.0, 4.0)) - toDb(h.rms(0.1, 0.6))) + " dB");
    } else {
        s.metric("effect captures not shipped in this build (the instruments play without them)", 0);
    }

    s.section("instrument: the panel's keyboard, state, real time");
    {
        InstHost h;
        h.p->keyboardNote(60, 0.8f);
        h.render(0.3);
        s.check(h.p->instrumentEngine().keyLit(60), "a key from the panel's keyboard plays and lights");
        h.p->keyboardNote(60, 0.0f);
        h.render(0.2);
        s.check(!h.p->instrumentEngine().keyLit(60), "and lets go");

        InstHost a;
        a.p->chooseInstrument(17);
        a.set(dc::ids::instMacros[4], 0.8f);
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        InstHost b;
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        b.render(0.05);
        s.check(b.p->instrumentLocked() && b.p->playingInstrument.load() == 17, "state: Lock and the instrument come back");
        s.check(std::abs(b.p->parameters.getRawParameterValue(dc::ids::instMacros[4])->load() - 0.8f) < 1.0e-4f, "state: the macros come back");
        // A state from the effect loads into the instrument (shared parameters) and back.
        Host fx;
        fx.go(dc::Swamp, 2);
        juce::MemoryBlock fxState;
        fx.p->getStateInformation(fxState);
        InstHost c;
        c.p->setStateInformation(fxState.getData(), int(fxState.getSize()));
        c.render(0.05);
        s.check(c.p->playingInstrument.load() == dc::inst::designFor(dc::Swamp, 2), "an effect state loads into the instrument");

        InstHost rt;
        rt.render(0.1);
        for (int k = 0; k < 40; ++k) {
            rt.on(rt.position / rt.sr + 0.001, 40 + k % 40, 0.6f);
            rt.bend(rt.position / rt.sr + 0.002, 8192 + 200 * (k % 5));
            rt.cc(rt.position / rt.sr + 0.003, 64, k % 2 ? 127 : 0);
            rt.cc(rt.position / rt.sr + 0.004, 1, k * 3 % 128);
            rt.go(k % dc::LocationCount, k % 4);
            rt.set(dc::ids::instMacros[std::size_t(k % 5)], float(k % 10) / 10.0f);
            allocationProbe::calls = 0;
            allocationProbe::enabled = true;
            rt.processOne(256);
            allocationProbe::enabled = false;
            if (allocationProbe::calls != 0) {
                s.check(false, "no allocation in processBlock (instrument)", std::to_string(allocationProbe::calls) + " at block " + std::to_string(k));
                break;
            }
            rt.off(rt.position / rt.sr + 0.001, 40 + k % 40);
        }
        s.check(rt.finite(), "real time: finite through MIDI, bends, pedal and travel");
    }
}

void testInstruments(Suite& s) {
    s.section("instruments: every instrument from MIDI");
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        InstHost h;
        h.play(k);
        h.on(0.05, 60, 0.8f);
        h.render(1.2);
        h.off(h.position / h.sr, 60);
        h.render(1.5);
        const std::string name = instName(k);
        s.check(h.peak(0.05, 2.7) > 3.0e-3, name + ": plays from MIDI", std::to_string(h.peak(0.05, 2.7)));
        s.check(h.finite() && h.peak(0, 2.7) < 1.0, name + ": finite and below full scale", std::to_string(h.peak(0, 2.7)));
        const double t = h.untilSilent(25.0);
        s.check(h.p->instrumentEngine().activeVoices() == 0, name + ": the note ends after its key", std::to_string(t) + " s");
    }

    s.section("instruments: in their places");
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        const auto dryRender = renderMelody(k, false), placed = renderMelody(k, true, 3.0);
        double peak = 0;
        bool finite = true;
        for (std::size_t i = 0; i < placed.l.size(); ++i) {
            finite &= std::isfinite(placed.l[i]) && std::isfinite(placed.r[i]);
            peak = std::max({ peak, double(std::abs(placed.l[i])), double(std::abs(placed.r[i])) });
        }
        const double gap = loudness(placed.l, placed.r, placed.sr) - loudness(dryRender.l, dryRender.r, dryRender.sr);
        s.check(finite && peak < 1.0, instName(k) + ": in its place, finite and below full scale", std::to_string(peak));
        s.check(std::abs(gap) < 6.0, instName(k) + ": in its place about as loud as dry", std::to_string(gap) + " dB");
    }

    s.section("instruments: in tune (dry)");
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        int wrong = 0;
        std::string detail;
        for (int key : { 57, 64 }) {
            InstHost h;
            h.play(k, false);
            h.dry();
            h.note(0.05, 1.0, key, 0.75f);
            h.render(1.3);
            const auto c = chroma(h.outL, h.sr, h.idx(0.15), h.idx(1.05));
            const double strongest = *std::max_element(c.begin(), c.end());
            const double own = c[std::size_t(key % 12)];
            if (10.0 * std::log10((own + 1.0e-30) / (strongest + 1.0e-30)) < -3.0) {
                ++wrong;
                detail += " key " + std::to_string(key) + ": pitch class " + std::to_string(std::max_element(c.begin(), c.end()) - c.begin());
            }
        }
        if (unpitched(k)) s.metric(instName(k) + " (inharmonic): keys whose pitch class is not the strongest", wrong);
        else s.check(wrong == 0, instName(k) + ": in tune (the pressed pitch class strongest)", detail);
    }

    s.section("instruments: velocity");
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        double level[2] {};
        for (int v = 0; v < 2; ++v) {
            InstHost h;
            h.play(k, false);
            h.dry();
            h.note(0.05, 0.6, 60, v == 0 ? 0.3f : 1.0f);
            h.render(1.0);
            level[v] = h.rms(0.05, 1.0);
        }
        s.check(toDb(level[1]) - toDb(level[0]) > 2.0, instName(k) + ": harder is louder", std::to_string(toDb(level[1]) - toDb(level[0])) + " dB");
    }

    s.section("instruments: distinct from their siblings (dry melody)");
    std::vector<std::vector<double>> prints(dc::inst::instrumentCount), shapes(dc::inst::instrumentCount);
    std::vector<double> levels(dc::inst::instrumentCount);
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        const auto r = renderMelody(k, false);
        prints[std::size_t(k)] = thirdOctaves(r.l, r.sr);
        shapes[std::size_t(k)] = envelopeShape(r.l, r.sr);
        levels[std::size_t(k)] = loudness(r.l, r.r, r.sr);
    }
    double closest = 1.0e9;
    std::string closestPair;
    for (int a = 0; a < dc::inst::instrumentCount; ++a)
        for (int b = a + 1; b < dc::inst::instrumentCount; ++b) {
            if (dc::inst::locationOf(a) != dc::inst::locationOf(b)) continue;
            double colour = 0, motion = 0;
            for (std::size_t i = 0; i < prints[std::size_t(a)].size(); ++i) colour += std::abs(prints[std::size_t(a)][i] - prints[std::size_t(b)][i]);
            colour /= double(prints[std::size_t(a)].size());
            const auto& ea = shapes[std::size_t(a)];
            const auto& eb = shapes[std::size_t(b)];
            const std::size_t n = std::min(ea.size(), eb.size());
            for (std::size_t i = 0; i < n; ++i) motion += std::abs(ea[i] - eb[i]);
            motion /= double(std::max<std::size_t>(1, n));
            const double score = colour + 0.5 * motion;
            const std::string pair = instName(a) + " / " + instName(b);
            if (score < closest) closest = score, closestPair = pair;
            s.check(score > 4.0, pair + ": siblings sound distinct", "colour " + std::to_string(colour) + " dB, envelope " + std::to_string(motion) + " dB");
        }
    s.metric("closest siblings (" + closestPair + ")", closest, " dB");

    s.section("instruments: level-matched (dry)");
    std::vector<double> sorted = levels;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[sorted.size() / 2];
    for (int k = 0; k < dc::inst::instrumentCount; ++k)
        s.check(std::abs(levels[std::size_t(k)] - median) < 3.0, instName(k) + ": level within 3 dB of the others",
                std::to_string(levels[std::size_t(k)] - median) + " dB");
    s.metric("loudness spread (min..max)", sorted.back() - sorted.front(), " dB");
}

// Measures each instrument's dry loudness, and its loudness in its own place, and writes
// the trims that bring them together (Trims.h: `trims`, then `placeTrims`).
int calibrateInstruments(const juce::String& path) {
    std::vector<double> levels, placed;
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        const auto r = renderMelody(k, false, 0.5);
        levels.push_back(loudness(r.l, r.r, r.sr));
        const auto p = renderMelody(k, true, 0.5);
        placed.push_back(loudness(p.l, p.r, p.sr));
    }
    constexpr double target = -20.0;
    juce::String text = "#pragma once\n"
                        "// Level trims (dB) per instrument, in place order: measured by\n"
                        "// `DungeonCrawlerTests --calibrate-instruments Source/Instrument/Trims.h` (the loudest\n"
                        "// 400 ms of the test melody, K-weighted, brought to the same loudness).\n"
                        "#include <array>\n\nnamespace dc::inst {\ninline constexpr std::array<float, 40> trims {\n";
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        const double trim = juce::jlimit(-30.0, 30.0, double(dc::inst::designTrimDb(k)) + target - levels[std::size_t(k)]);
        text << "    " << juce::String(trim, 2) << "f,   // " << dc::inst::design(k).name << " (measured " << juce::String(levels[std::size_t(k)], 1) << " dB)\n";
    }
    text << "};\n// The wet path in each place (instrument only), so the place's instrument is as loud in\n// its place as dry.\n"
            "inline constexpr std::array<float, 40> placeTrims {\n";
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        const int l = dc::inst::locationOf(k), sub = dc::inst::subOf(k);
        const double trim = juce::jlimit(-12.0, 12.0, double(dc::inst::placeTrimDb(l, sub)) + levels[std::size_t(k)] - placed[std::size_t(k)]);
        text << "    " << juce::String(trim, 2) << "f,   // " << dc::subLocationName(l, sub) << " (in place " << juce::String(placed[std::size_t(k)] - levels[std::size_t(k)], 1) << " dB)\n";
    }
    text << "};\n}\n";
    juce::File(juce::File::getCurrentWorkingDirectory().getChildFile(path)).replaceWithText(text);
    std::cout << text;
    return 0;
}

// The same melody on every instrument: dry, a second's pause, then in its place.
int listenInstruments(const juce::String& directory) {
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    std::vector<float> allL, allR;
    juce::String cues;
    for (int k = 0; k < dc::inst::instrumentCount; ++k) {
        const auto dryRender = renderMelody(k, false, 2.5);
        const auto placed = renderMelody(k, true, 3.5);
        std::vector<float> l = dryRender.l, r = dryRender.r;
        // The dry part ends with a 50 ms fade (a ringing note is not cut into the pause).
        const auto fade = std::size_t(0.05 * dryRender.sr);
        for (std::size_t i = 0; i < fade && i < l.size(); ++i) {
            const float g = float(i) / float(fade);
            l[l.size() - 1 - i] *= g;
            r[r.size() - 1 - i] *= g;
        }
        l.insert(l.end(), std::size_t(dryRender.sr), 0.0f);
        r.insert(r.end(), std::size_t(dryRender.sr), 0.0f);
        l.insert(l.end(), placed.l.begin(), placed.l.end());
        r.insert(r.end(), placed.r.begin(), placed.r.end());
        const int location = dc::inst::locationOf(k), sub = dc::inst::subOf(k);
        const juce::String name = juce::String(k + 1).paddedLeft('0', 2) + "-" + juce::String(dc::locationNames[std::size_t(location)]).removeCharacters("' ") + "-" +
                                  juce::String(dc::subLocationName(location, sub)).removeCharacters("' ") + "-" + juce::String(dc::inst::design(k).name).removeCharacters("'&, ");
        writeWav(dir.getChildFile(name + "-dry-then-in-place.wav"), l, r, dryRender.sr);
        cues << juce::String(double(allL.size()) / dryRender.sr, 1) << " s  " << name << "\n";
        allL.insert(allL.end(), l.begin(), l.end());
        allR.insert(allR.end(), r.begin(), r.end());
        std::cout << name << std::endl;
    }
    writeWav(dir.getChildFile("all-instruments.wav"), allL, allR, 48000);
    dir.getChildFile("all-instruments-cues.txt").replaceWithText(cues);
    return 0;
}

// The instrument's editor: the Instrument button, its slide-up panel, the macros named
// for the instrument, Lock and the chooser, the keyboard lit by notes and playable.
void testInstrumentEditor(Suite& s, const juce::String& snapshot) {
    s.section("instrument editor");
    {
        DungeonCrawlerProcessor fx;
        fx.prepareToPlay(48000, 512);
        std::unique_ptr<juce::AudioProcessorEditor> base(fx.createEditor());
        auto& view = dynamic_cast<DungeonCrawlerEditor*>(base.get())->view();
        s.check(view.instrumentPanelForTest() == nullptr && !view.instrumentButton().isVisible(), "the FX editor has no instrument panel or button");
    }
    InstHost h;
    std::unique_ptr<juce::AudioProcessorEditor> base(h.p->createEditor());
    auto* wrapper = dynamic_cast<DungeonCrawlerEditor*>(base.get());
    if (!s.check(wrapper != nullptr, "the instrument's editor is created")) return;
    auto& view = wrapper->view();
    auto* panel = view.instrumentPanelForTest();
    if (!s.check(panel != nullptr && view.instrumentButton().isVisible(), "the instrument's editor has the Instrument button")) return;
    nav::click(view.instrumentButton());
    s.check(view.instrumentOpen(), "the Instrument button opens the panel");
    h.render(0.05);
    panel->pollForTest();
    s.check(panel->titleText() == dc::inst::design(0).name, "the panel names the instrument", panel->titleText().toStdString());
    s.check(panel->placeText().contains("Corridors"), "and its place", panel->placeText().toStdString());
    bool labels = true;
    for (int m = 0; m < dc::inst::macroCount; ++m) labels &= panel->macroLabel(m) == dc::inst::design(0).macros[std::size_t(m)];
    s.check(labels, "the macros carry the instrument's names");
    // A new place: a new instrument, new names.
    h.go(dc::Village, 1);
    h.render(0.05);
    panel->pollForTest();
    const int tavern = dc::inst::designFor(dc::Village, 1);
    s.check(panel->titleText() == dc::inst::design(tavern).name && panel->macroLabel(4) == dc::inst::design(tavern).macros[4],
            "travelling to the Tavern names its guitar and its macros");
    view.followForTest();
    s.check(view.placeCrumbText().contains(dc::inst::design(tavern).name) || view.showingMap(), "the crumb names the instrument that plays here");
    // Lock, then travel: the guitar stays.
    nav::click(panel->lockButton());
    h.go(dc::Dungeon, 4);
    h.render(0.05);
    panel->pollForTest();
    s.check(h.p->instrumentLocked() && panel->titleText() == dc::inst::design(tavern).name && panel->placeText().contains("Final Boss Lair"),
            "Lock instrument: the Tavern guitar plays in the Final Boss Lair", panel->placeText().toStdString());
    // The chooser picks (and locks) any instrument.
    panel->chooser().setSelectedId(dc::inst::designFor(dc::Cathedral, 3) + 1, juce::sendNotificationSync);
    h.render(0.05);
    panel->pollForTest();
    s.check(h.p->playingInstrument.load() == dc::inst::designFor(dc::Cathedral, 3), "the chooser picks the church bells");
    // Notes light the keyboard; the keyboard plays.
    auto* kb = dynamic_cast<dc::ui::InstrumentPanel::Keyboard*>(&panel->keyboardForTest());
    h.on(h.position / h.sr + 0.001, 60, 0.8f);
    h.render(0.1);
    panel->pollForTest();
    s.check(kb != nullptr && ((kb->lit[0] >> 60) & 1u) != 0, "a host note lights the keyboard");
    {
        const int asked = panel->repaintRequestCount();
        panel->pollForTest();
        panel->pollForTest();
        s.check(panel->repaintRequestCount() == asked, "nothing repaints while nothing changes");
        h.off(h.position / h.sr + 0.001, 60);
        h.render(0.05);
        panel->pollForTest();
        s.check(panel->repaintRequestCount() == asked + 1, "a key let go repaints the keyboard once");
    }
    if (s.check(kb != nullptr, "the keyboard is there")) {
        const auto centre = kb->keyBounds(64).getCentre().withY(kb->keyBounds(64).getBottom() - 5.0f);
        s.check(kb->keyAt(centre) == 64, "a key on the keyboard is found where it is drawn");
        juce::ModifierKeys mods;
        const auto event = juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), centre, mods, 1.0f, 0, 0, 0, 0, kb, kb, juce::Time(), centre, juce::Time(), 1, false);
        kb->mouseDown(event);
        h.render(0.2);
        s.check(h.p->instrumentEngine().keyLit(64) && h.peak(h.position / h.sr - 0.15, h.position / h.sr) > 1.0e-3, "clicking a key plays it");
        kb->mouseUp(event);
        h.render(0.1);
        s.check(!h.p->instrumentEngine().keyLit(64), "and lets go");
    }
    h.cc(h.position / h.sr + 0.001, 64, 127);
    h.bend(h.position / h.sr + 0.002, 12000);
    h.render(0.05);
    panel->pollForTest();
    s.check(h.p->shownSustain.load() && h.p->shownBend.load() > 0.4f, "the pedal and the bend reach the panel");
    if (snapshot.isNotEmpty()) {
        view.settleForTest();
        auto image = view.createComponentSnapshot(view.getLocalBounds(), true, 1.0f);
        juce::File file(juce::File(snapshot).getSiblingFile(juce::File(snapshot).getFileNameWithoutExtension() + "-instrument.png"));
        file.deleteFile();
        juce::FileOutputStream stream(file);
        juce::PNGImageFormat().writeImageToStream(image, stream);
    }
    // Esc closes it.
    s.check(view.pressEscapeForTest() && !view.instrumentOpen(), "Esc closes the panel");
}
