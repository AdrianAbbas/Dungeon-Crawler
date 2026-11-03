// Session 12: LIGHT SEEKER's Fanfare Trumpets and Heroic Horns, dry and in their places,
// measured with Tests/BrassMetrics.h and rendered with the same horn phrase as DUNGEON
// CRAWLER's `--listen-brass`. LIGHT SEEKER's test file is included whole (its helpers and
// hosts), its main renamed; nothing of LIGHT SEEKER is changed.
#define main lightSeekerTestsMain
#include "LightSeekerTests.cpp"
#undef main
#include "BrassMetrics.h"

namespace {
struct Out { std::vector<float> l, r; std::size_t onset = 0; };

// Plays timed notes (seconds) on a LIGHT SEEKER instrument host, sample-accurately.
Out perform(const char* name, bool dry, const std::vector<brassmetrics::PhraseNote>& notes, double seconds, float punch = -1.0f) {
    InstrumentHost h;
    h.instrument(mixsong::instrumentNamed(name));
    if (dry) h.dry();
    if (punch >= 0.0f) h.set(dc::ids::punch, punch);
    h.render(0.4);   // the Level trim settles
    const auto start = h.outL.size();
    struct Ev { long long at; juce::MidiMessage m; };
    std::vector<Ev> events;
    for (const auto& n : notes) {
        events.push_back({ (long long)(n.at * h.sr), juce::MidiMessage::noteOn(1, n.key, n.velocity) });
        events.push_back({ (long long)((n.at + n.length) * h.sr), juce::MidiMessage::noteOff(1, n.key) });
    }
    const long long total = (long long)(seconds * h.sr);
    for (long long pos = 0; pos < total;) {
        const int n = int(std::min<long long>(h.block, total - pos));
        h.buffer.setSize(h.buffer.getNumChannels(), n, false, false, true);
        h.buffer.clear();
        h.midi.clear();
        for (const auto& e : events)
            if (e.at >= pos && e.at < pos + n) h.midi.addEvent(e.m, int(e.at - pos));
        h.p->processBlock(h.buffer, h.midi);
        for (int i = 0; i < n; ++i) {
            h.outL.push_back(h.buffer.getSample(0, i));
            h.outR.push_back(h.buffer.getSample(h.buffer.getNumChannels() > 1 ? 1 : 0, i));
        }
        pos += n;
    }
    Out o;
    o.l.assign(h.outL.begin() + long(start), h.outL.end());
    o.r.assign(h.outR.begin() + long(start), h.outR.end());
    return o;
}

void writeWav(const juce::File& file, const std::vector<float>& l, const std::vector<float>& r) {
    juce::AudioBuffer<float> b(2, int(l.size()));
    for (int i = 0; i < b.getNumSamples(); ++i) { b.setSample(0, i, l[std::size_t(i)]); b.setSample(1, i, r[std::size_t(i)]); }
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    if (auto* writer = wav.createWriterFor(stream.get(), 48000, 2, 16, {}, 0)) {
        stream.release();
        std::unique_ptr<juce::AudioFormatWriter> w(writer);
        w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
    }
    std::cout << "wrote " << file.getFullPathName() << "\n";
}
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir(argc > 1 ? argv[1] : "brass-reference");
    dir.createDirectory();
    juce::String report;
    for (const char* name : { "Fanfare Trumpets", "Heroic Horns" }) {
        // The held chord (LIGHT SEEKER's brass report: C major, velocity 0.95, dry), and the
        // same at velocity 0.6, and a single note.
        for (auto [label, notes] : { std::pair<const char*, std::vector<int>> { "chord v0.95", { 60, 64, 67 } }, { "note v0.95", { 60 } } }) {
            std::vector<brassmetrics::PhraseNote> held;
            for (int k : notes) held.push_back({ 0.0, 1.4, k, 0.95f });
            const auto o = perform(name, true, held, 1.8);
            const auto m = brassmetrics::measure({ o.l, o.r, 48000.0, 0 });
            report << name << " (" << label << ", dry): attack-to-sustain " << juce::String(m.attackToSustainDb, 2) << " dB, rise " << juce::String(m.riseMs, 1)
                   << " ms, sustain " << juce::String(m.sustainDb, 2) << " dBFS, peak " << juce::String(m.peakDb, 2) << " dBFS, presence "
                   << juce::String(m.presenceDb, 2) << " dB, edge " << juce::String(m.edgeDb, 2) << " dB\n";
        }
        std::vector<brassmetrics::PhraseNote> soft;
        for (int k : { 60, 64, 67 }) soft.push_back({ 0.0, 1.4, k, 0.6f });
        const auto o = perform(name, true, soft, 1.8);
        const auto m = brassmetrics::measure({ o.l, o.r, 48000.0, 0 });
        report << name << " (chord v0.6, dry): attack-to-sustain " << juce::String(m.attackToSustainDb, 2) << " dB, rise " << juce::String(m.riseMs, 1) << " ms, sustain "
               << juce::String(m.sustainDb, 2) << " dBFS\n";
        const juce::String file = juce::String(name).toLowerCase().replace(" ", "-");
        const auto phrase = brassmetrics::hornPhrase();
        const auto dry = perform(name, true, phrase, brassmetrics::hornPhraseSeconds());
        writeWav(dir.getChildFile("light-seeker-" + file + "-dry.wav"), dry.l, dry.r);
        const auto placed = perform(name, false, phrase, brassmetrics::hornPhraseSeconds());
        writeWav(dir.getChildFile("light-seeker-" + file + "-in-its-place.wav"), placed.l, placed.r);
    }
    std::cout << report;
    dir.getChildFile("light-seeker-brass.txt").replaceWithText(report);
    return 0;
}
