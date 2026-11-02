// Session 9: the Effects roller (included by DungeonCrawlerTests.cpp, "effects" group).
// At 0 / 100 / 150 % in the FX and the instrument: the effect energy (what the place adds
// to the dry signal) scales with it, the level stays (the trims and the level matcher),
// moving it is click-free; it is automatable, saved with presets and sessions, and a
// session saved before it existed opens at 100 %.

namespace effectstests {
struct Take { std::vector<float> l, r; };
// How much the place changes the sound (dB): the output and the same input rendered at
// Effects 0 (the dry signal) in octave bands (63 Hz .. 8 kHz) over 43 ms frames, their
// overall levels aligned, the mean absolute difference per band and frame. Blind to phase
// (choruses, pitch voices and delays count as fully as reverbs), it grows as the tails fill
// the gaps, the echoes repeat and the character effects colour the sound.
double effectShare(const std::vector<float>& outL, const std::vector<float>& outR, const std::vector<float>& dryL, const std::vector<float>& dryR,
                   std::size_t from, std::size_t to) {
    constexpr int order = 11, size = 1 << order, hop = size / 2;
    juce::dsp::FFT fft(order);
    std::vector<float> window(size), buffer(2 * size);
    for (int i = 0; i < size; ++i) window[std::size_t(i)] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * float(i) / float(size));
    to = std::min({ to, outL.size(), dryL.size() });
    const auto bands = [&](const std::vector<float>& l, const std::vector<float>& r, std::size_t at) {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        for (int i = 0; i < size; ++i) buffer[std::size_t(i)] = 0.5f * (l[at + std::size_t(i)] + r[at + std::size_t(i)]) * window[std::size_t(i)];
        fft.performFrequencyOnlyForwardTransform(buffer.data());
        std::array<double, 8> e {};
        for (int bin = 1; bin < size / 2; ++bin) {
            const double hz = bin * 48000.0 / size;
            const int band = int(std::floor(std::log2(hz / 44.0)));
            if (band >= 0 && band < 8) e[std::size_t(band)] += double(buffer[std::size_t(bin)]) * buffer[std::size_t(bin)];
        }
        return e;
    };
    std::vector<std::array<double, 8>> a, d;
    double ea = 0, ed = 0;
    for (std::size_t at = from; at + size <= to; at += hop) {
        a.push_back(bands(outL, outR, at));
        d.push_back(bands(dryL, dryR, at));
        for (int k = 0; k < 8; ++k) { ea += a.back()[std::size_t(k)]; ed += d.back()[std::size_t(k)]; }
    }
    if (a.empty() || ea <= 0.0 || ed <= 0.0) return 0.0;
    const double offset = 10.0 * std::log10(ea / ed), floorA = ea / double(a.size()) * 1.0e-6, floorD = ed / double(d.size()) * 1.0e-6;
    double sum = 0;
    for (std::size_t f = 0; f < a.size(); ++f)
        for (int k = 0; k < 8; ++k)
            sum += std::min(40.0, std::abs(10.0 * std::log10((a[f][std::size_t(k)] + floorA) / (d[f][std::size_t(k)] + floorD)) - offset));
    return sum / double(a.size() * 8);
}
// The energy the place adds against the output: the difference from the dry render (dB of
// the output). Saturates once a place has no coherent dry left (then effectShare still grows).
double differenceShare(const std::vector<float>& outL, const std::vector<float>& outR, const std::vector<float>& dryL, const std::vector<float>& dryR,
                       std::size_t from, std::size_t to) {
    double diff = 0, all = 0;
    to = std::min({ to, outL.size(), dryL.size() });
    for (std::size_t i = from; i < to; ++i) {
        const double dl = double(outL[i]) - dryL[i], dr = double(outR[i]) - dryR[i];
        diff += dl * dl + dr * dr;
        all += double(outL[i]) * outL[i] + double(outR[i]) * outR[i];
    }
    return 10.0 * std::log10(std::max(diff, 1.0e-20) / std::max(all, 1.0e-20));
}
// Whether the effects grow 0 -> 50 -> 100 % and on to 150 % (on either measure there: a
// place that is already fully wet saturates one of them), with the numbers for the log.
bool grows(const Take& dry, const Take& half, const Take& full, const Take& strong, std::size_t from, std::size_t to, std::string& detail) {
    const double dHalf = effectShare(half.l, half.r, dry.l, dry.r, from, to), dFull = effectShare(full.l, full.r, dry.l, dry.r, from, to),
                 dStrong = effectShare(strong.l, strong.r, dry.l, dry.r, from, to);
    const double xHalf = differenceShare(half.l, half.r, dry.l, dry.r, from, to), xFull = differenceShare(full.l, full.r, dry.l, dry.r, from, to),
                 xStrong = differenceShare(strong.l, strong.r, dry.l, dry.r, from, to);
    detail = "spectral distance " + fmt(dHalf) + " / " + fmt(dFull) + " / " + fmt(dStrong) + " dB, difference " + fmt(xHalf) + " / " + fmt(xFull) + " / "
             + fmt(xStrong) + " dB of the output";
    return dHalf > 0.5 && dHalf < dFull && (dStrong > dFull || xStrong > xFull);
}
double levelDb(const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to) {
    double sum = 0;
    to = std::min(to, l.size());
    for (std::size_t i = from; i < to; ++i) sum += 0.5 * (double(l[i]) * l[i] + double(r[i]) * r[i]);
    return toDb(std::sqrt(sum / double(std::max<std::size_t>(1, to - from))));
}
// A pluck phrase and a chord under it: transients for the tails, a held part for the level.
Signal material() {
    const auto p = plucks(0.35f), c = sawChord(0.12f);
    return [p, c](int ch, long long n, double sr) { return p(ch, n, sr) + c(ch, n, sr); };
}
Take fxTake(int location, int sub, float effects, double seconds) {
    Host h;
    h.go(location, sub);
    h.set(dc::ids::effects, effects);
    h.render(seconds, material());
    return { h.outL, h.outR };
}
Take instTake(int instrument, float effects, double tail) {
    // The melody the instrument's level trims are calibrated on (renderMelody).
    InstHost h;
    h.play(instrument);
    h.set(dc::ids::effects, effects);
    h.render(0.05);
    melody(h, 0.1);
    h.render(0.05 + melodySeconds + tail);
    return { h.outL, h.outR };
}
} // namespace effectstests

void testEffectsRoller(Suite& s) {
    using namespace effectstests;
    s.section("effects roller: 0 / 100 / 150 % (Session 9)");
    // The parameter: automatable, 0..150 %, 100 % by default.
    {
        DungeonCrawlerProcessor fx, inst(PluginKind::Instrument);
        for (auto* p : { &fx, &inst }) {
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(p->parameters.getParameter(dc::ids::effects));
            const bool ok = parameter != nullptr && parameter->isAutomatable() && std::abs(parameter->convertFrom0to1(parameter->getDefaultValue()) - 1.0f) < 1.0e-6f
                            && parameter->getNormalisableRange().start == 0.0f && std::abs(parameter->getNormalisableRange().end - 1.5f) < 1.0e-6f
                            && parameter->getCurrentValueAsText() == "100 %";
            s.check(ok, std::string(p->isInstrument() ? "instrument" : "FX") + ": Effects is an automatable parameter, 0-150 %, 100 % by default");
        }
        // A session saved before the roller existed (no "effects" in it) opens at 100 %.
        for (auto* p : { &fx, &inst }) {
            p->parameters.getParameter(dc::ids::effects)->setValueNotifyingHost(p->parameters.getParameter(dc::ids::effects)->convertTo0to1(0.3f));
            juce::MemoryBlock block;
            p->getStateInformation(block);
            auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
            for (auto* child = xml->getFirstChildElement(); child != nullptr;) {
                auto* next = child->getNextElement();
                if (child->getStringAttribute("id") == dc::ids::effects) xml->removeChildElement(child, true);
                child = next;
            }
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary(*xml, old);
            p->setStateInformation(old.getData(), static_cast<int>(old.getSize()));
            s.check(std::abs(p->effectsAmount() - 1.0f) < 1.0e-6f, std::string(p->isInstrument() ? "instrument" : "FX") + ": an older session opens at 100 %");
            // Saved with the session (and so with user presets, which are the state).
            p->parameters.getParameter(dc::ids::effects)->setValueNotifyingHost(p->parameters.getParameter(dc::ids::effects)->convertTo0to1(1.35f));
            p->getStateInformation(block);
            DungeonCrawlerProcessor reopened(p->kind());
            reopened.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
            s.check(std::abs(reopened.effectsAmount() - 1.35f) < 1.0e-3f, std::string(p->isInstrument() ? "instrument" : "FX") + ": saved with the session (135 %)");
            // Factory presets set it to 100 % (they start from the defaults); user presets keep theirs.
            p->applyFactoryPreset(0);
            s.check(std::abs(p->effectsAmount() - 1.0f) < 1.0e-6f, std::string(p->isInstrument() ? "instrument" : "FX") + ": a factory preset plays at 100 %");
            juce::File preset = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("dc-effects-test.dcpreset");
            p->parameters.getParameter(dc::ids::effects)->setValueNotifyingHost(p->parameters.getParameter(dc::ids::effects)->convertTo0to1(0.45f));
            p->getStateInformation(block);
            if (auto x = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()))) x->writeTo(preset);
            p->applyFactoryPreset(0);
            p->loadUserPreset(preset);
            preset.deleteFile();
            s.check(std::abs(p->effectsAmount() - 0.45f) < 1.0e-3f, std::string(p->isInstrument() ? "instrument" : "FX") + ": a user preset brings its Effects back (45 %)");
        }
    }
    // The FX: a spread of places (reverbs, echoes, choirs, character effects).
    const std::vector<std::pair<int, int>> places { { dc::Dungeon, 0 }, { dc::Dungeon, 4 }, { dc::Castle, 2 }, { dc::Cathedral, 2 }, { dc::HauntedForest, 3 },
                                                    { dc::Caverns, 3 }, { dc::Village, 2 }, { dc::Swamp, 1 }, { dc::Mountains, 3 }, { dc::Necropolis, 2 },
                                                    { dc::WizardsTower, 3 } };
    const double seconds = 7.0;
    const std::size_t from = std::size_t(4.0 * 48000), to = std::size_t(seconds * 48000);
    int scaled = 0;
    double worstLevel = 0;
    for (const auto& [l, k] : places) {
        const auto dry = fxTake(l, k, 0.0f, seconds), full = fxTake(l, k, 1.0f, seconds), strong = fxTake(l, k, 1.5f, seconds),
                   half = fxTake(l, k, 0.5f, seconds);
        std::string detail;
        const bool up = grows(dry, half, full, strong, from, to, detail);
        scaled += up ? 1 : 0;
        s.check(up, "FX " + placeName(l, k) + ": the effects grow with the roller (50 / 100 / 150 %)", detail);
        const double lDry = levelDb(dry.l, dry.r, from, to), lFull = levelDb(full.l, full.r, from, to), lStrong = levelDb(strong.l, strong.r, from, to);
        const double dev = std::max(std::abs(lDry - lFull), std::abs(lStrong - lFull));
        worstLevel = std::max(worstLevel, dev);
        s.check(dev < 3.0, "FX " + placeName(l, k) + ": the level is kept (0 / 100 / 150 % within 3 dB)",
                fmt(lDry - lFull) + " / 0 / " + fmt(lStrong - lFull) + " dB");
    }
    s.metric("FX places whose effects scale with the roller", scaled, std::string(" of " + std::to_string(places.size())).c_str());
    s.metric("FX worst level deviation 0 / 150 % vs 100 %", worstLevel, " dB");
    // 0 %: the dry path (the input exactly, latency-aligned).
    {
        Host h;
        h.go(dc::Cathedral, 2);
        h.set(dc::ids::effects, 0.0f);
        h.render(3.0, material());
        const int latency = h.p->getLatencySamples();
        double diff = 0, all = 0;
        for (std::size_t i = h.at(1.0) + std::size_t(latency); i < h.end(); ++i) {
            const double d = double(h.outL[i]) - h.inL[i - std::size_t(latency)];
            diff += d * d;
            all += double(h.outL[i]) * h.outL[i];
        }
        s.check(toDb(std::sqrt(diff / std::max(all, 1.0e-20))) < -60.0, "FX: at 0 % the Nave is the dry input", fmt(toDb(std::sqrt(diff / std::max(all, 1.0e-20)))) + " dB");
    }
    // Moving the roller is click-free: a held sine, the roller swept 100 -> 0 -> 150 -> 100 %
    // in steps every block (automation), then a jump.
    for (const auto& [l, k] : std::vector<std::pair<int, int>> { { dc::Cathedral, 2 }, { dc::Dungeon, 4 }, { dc::WizardsTower, 2 } }) {
        Host h;
        h.go(l, k);
        h.render(3.0, sine(220.0, 0.25f));
        const double before = h.maxStep(h.at(2.0), h.at(3.0));
        const int blocks = int(4.0 * 48000 / 512);
        for (int b = 0; b < blocks; ++b) {
            const float t = float(b) / float(blocks);
            const float v = t < 0.33f ? 1.0f - t / 0.33f : (t < 0.75f ? 1.5f * (t - 0.33f) / 0.42f : 1.5f - 0.5f * (t - 0.75f) / 0.25f);
            h.set(dc::ids::effects, v);
            h.render(512.0 / 48000.0, sine(220.0, 0.25f));
        }
        h.set(dc::ids::effects, 0.0f);   // a jump
        h.render(1.0, sine(220.0, 0.25f));
        h.set(dc::ids::effects, 1.5f);
        h.render(2.0, sine(220.0, 0.25f));
        const double during = h.maxStep(h.at(3.0), h.end());
        const double after = h.maxStep(h.end() - h.at(0.5), h.end());
        s.check(h.finite() && during < 2.5 * std::max(before, after), "FX " + placeName(l, k) + ": moving the roller (sweeps and jumps) is click-free",
                fmt(during / std::max(before, after), 2) + "x the steady step");
    }
    // The instrument: a few instruments in their places, on the calibration melody.
    for (int index : { dc::inst::designFor(dc::Dungeon, 0), dc::inst::designFor(dc::Cathedral, 2), dc::inst::designFor(dc::Caverns, 1),
                       dc::inst::designFor(dc::Village, 1), dc::inst::designFor(dc::WizardsTower, 3) }) {
        const auto dry = instTake(index, 0.0f, 3.0), full = instTake(index, 1.0f, 3.0), strong = instTake(index, 1.5f, 3.0),
                   half = instTake(index, 0.5f, 3.0);
        const std::size_t from = 0, to = dry.l.size();
        const auto name = std::string(dc::inst::design(index).name);
        std::string detail;
        s.check(grows(dry, half, full, strong, from, to, detail), "instrument " + name + ": the effects grow with the roller (50 / 100 / 150 %)", detail);
        // Loudness as the instrument's trims are calibrated (the loudest 400 ms, K-weighted).
        const double lDry = loudness(dry.l, dry.r, 48000), lFull = loudness(full.l, full.r, 48000), lStrong = loudness(strong.l, strong.r, 48000);
        s.check(std::abs(lDry - lFull) < 3.0 && std::abs(lStrong - lFull) < 3.0, "instrument " + name + ": the level is kept (0 / 100 / 150 % within 3 dB)",
                fmt(lDry - lFull) + " / 0 / " + fmt(lStrong - lFull) + " dB");
    }
    {
        // A held chord while the roller sweeps 100 -> 0 -> 150 %: no click. The reference is
        // the same chord held at the values swept through (the dry strings have much larger
        // sample-to-sample steps of their own than in their place).
        const int index = dc::inst::designFor(dc::Castle, 2);   // the string ensemble: sustained
        const auto steps = [&](float fixed) {
            InstHost h;
            h.play(index);
            if (fixed >= 0.0f) h.set(dc::ids::effects, fixed);
            h.on(0.1, 57, 0.7f);
            h.on(0.1, 64, 0.6f);
            h.render(3.0);
            const int blocks = int(3.0 * 48000 / 512);
            for (int b = 0; b < blocks; ++b) {
                const float t = float(b) / float(blocks);
                if (fixed < 0.0f) h.set(dc::ids::effects, t < 0.5f ? 1.0f - 2.0f * t : 3.0f * (t - 0.5f));
                h.render(512.0 / 48000.0);
            }
            double m = 0;
            for (std::size_t i = h.idx(3.0); i < h.outL.size(); ++i) m = std::max(m, std::abs(double(h.outL[i]) - 2.0 * h.outL[i - 1] + h.outL[i - 2]));
            return m;
        };
        const double reference = std::max({ steps(0.0f), steps(1.0f), steps(1.5f) }), during = steps(-1.0f);
        s.check(during < 1.5 * reference, "instrument: a held string chord while the roller sweeps 100 -> 0 -> 150 % is click-free",
                fmt(during / std::max(reference, 1.0e-9), 2) + "x the largest steady step at 0 / 100 / 150 %");
    }
    // Level and the voices' controls leave the sound as it was at their defaults (the
    // panel's new controls change nothing until moved).
    {
        const int choir = dc::inst::designFor(dc::Cathedral, 0);   // the Soft Choir (the recorded voice when present)
        InstHost a, b;
        a.play(choir);
        b.play(choir);
        b.set(dc::ids::instLevel, 0.0f);
        b.set(dc::ids::instVowel, 0.5f);
        b.set(dc::ids::instEnsemble, 0.5f);
        for (auto* h : { &a, &b }) { h->note(0.1, 1.0, 60, 0.7f); h->render(2.0); }
        double diff = 0;
        for (std::size_t i = 0; i < a.outL.size(); ++i) diff = std::max(diff, std::abs(double(a.outL[i]) - b.outL[i]));
        s.check(diff == 0.0, "instrument: Level 0 dB, Vowel and Ensemble at 50 % play exactly as before");
        InstHost c;
        c.play(choir);
        c.set(dc::ids::instLevel, -12.0f);
        c.note(0.1, 1.0, 60, 0.7f);
        c.render(2.0);
        const double drop = toDb(c.rms(0.3, 1.0)) - toDb(a.rms(0.3, 1.0));
        s.check(std::abs(drop + 12.0) < 1.5, "instrument: Level -12 dB lowers it by about 12 dB", fmt(drop) + " dB");
    }
}

