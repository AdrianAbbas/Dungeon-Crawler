// Offline tests through the real processBlock. Exit code 0 only if every check passes.
//   DungeonCrawlerTests                      run everything except the report-only cpu group
//   DungeonCrawlerTests --only a,b           run only some groups (smoke, locations, travel...)
//   DungeonCrawlerTests --snapshot f.png     also write an image of the editor
//   DungeonCrawlerTests --render f.wav location [seconds]   a test phrase through a location (listening aid)
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <string>
#include <vector>

// Count C++ allocations on the calling thread while enabled.
namespace allocationProbe {
thread_local bool enabled = false;
thread_local std::size_t calls = 0;
}
void* operator new(std::size_t n) {
    if (allocationProbe::enabled) ++allocationProbe::calls;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

struct Suite {
    int passed = 0, failed = 0;
    bool check(bool okay, const std::string& name, const std::string& detail = {}) {
        okay ? ++passed : ++failed;
        if (!okay) std::cout << "FAIL: " << name << (detail.empty() ? "" : " :: " + detail) << "\n";
        return okay;
    }
    void section(const char* title) { std::cout << "[test] " << title << std::endl; }
    void metric(const std::string& name, double value, const char* unit = "") { std::cout << "  [metric] " << name << " = " << value << unit << "\n"; }
};

double toDb(double x) { return 20.0 * std::log10(std::max(x, 1.0e-12)); }
std::string fmt(double v, int decimals = 1) { return juce::String(v, decimals).toStdString(); }
std::string placeName(int i) { return dc::locationNames[static_cast<std::size_t>(i)]; }

// Test signals, as functions of (channel, sample index, sample rate).
using Signal = std::function<float(int, long long, double)>;
Signal sine(double hz, float amplitude) {
    return [=](int, long long n, double sr) { return amplitude * static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * hz * static_cast<double>(n) / sr)); };
}
// A bright sustained chord (naive saws on A2, E3, A3, C#4): a synth pad stand-in.
Signal sawChord(float amplitude) {
    return [=](int ch, long long n, double sr) {
        const double hz[] = { 110.0, 164.81, 220.0, 277.18 };
        double v = 0;
        for (int k = 0; k < 4; ++k) {
            const double phase = std::fmod(hz[k] * (1.0 + 0.002 * ch) * static_cast<double>(n) / sr, 1.0);
            v += 2.0 * phase - 1.0;
        }
        return amplitude * static_cast<float>(v * 0.25);
    };
}
// Plucked notes (decaying harmonics every 0.5 s): a guitar stand-in.
Signal plucks(float amplitude) {
    return [=](int, long long n, double sr) {
        const double hz[] = { 196.0, 246.94, 293.66, 392.0 };
        const long long period = static_cast<long long>(0.5 * sr);
        const double t = static_cast<double>(n % period) / sr;
        const double f = hz[(n / period) % 4];
        double v = 0;
        for (int h = 1; h <= 6; ++h) v += std::sin(2.0 * juce::MathConstants<double>::pi * f * h * t) / h;
        return amplitude * static_cast<float>(v * std::exp(-t * 6.0) * 0.6);
    };
}
Signal silence() { return [](int, long long, double) { return 0.0f; }; }

struct Host {
    std::unique_ptr<DungeonCrawlerProcessor> p = std::make_unique<DungeonCrawlerProcessor>();
    double sr;
    int block, ins, outs;
    long long position = 0;
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;
    std::vector<float> outL, outR, inL;   // everything rendered so far

    explicit Host(double sampleRate = 48000, int blockSize = 512, int inputs = 2, int outputs = 2)
        : sr(sampleRate), block(blockSize), ins(inputs), outs(outputs) {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(inputs == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses.add(outputs == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        p->setBusesLayout(layout);
        p->setRateAndBufferSizeDetails(sr, block);
        p->prepareToPlay(sr, block);
        buffer.setSize(std::max(inputs, outputs), block);
    }
    void set(const char* id, float plain) {
        auto* parameter = p->parameters.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
    }
    void location(int index) { set(dc::ids::location, static_cast<float>(index)); }
    void processOne(int n, const Signal& signal) {
        buffer.setSize(std::max(ins, outs), n, false, false, true);
        buffer.clear();
        for (int ch = 0; ch < ins; ++ch)
            for (int i = 0; i < n; ++i) buffer.setSample(ch, i, signal(ch, position + i, sr));
        for (int i = 0; i < n; ++i) inL.push_back(buffer.getSample(0, i));
        p->processBlock(buffer, midi);
        for (int i = 0; i < n; ++i) {
            outL.push_back(buffer.getSample(0, i));
            outR.push_back(buffer.getSample(outs > 1 ? 1 : 0, i));
        }
        position += n;
    }
    void render(double seconds, const Signal& signal) {
        auto remaining = static_cast<long long>(seconds * sr);
        while (remaining > 0) {
            const int n = static_cast<int>(std::min<long long>(block, remaining));
            processOne(n, signal);
            remaining -= n;
        }
    }
    std::size_t at(double seconds) const { return static_cast<std::size_t>(seconds * sr); }
    std::size_t end() const { return outL.size(); }
    double rms(std::size_t from, std::size_t to) const {
        double sum = 0;
        to = std::min(to, outL.size());
        for (std::size_t i = from; i < to; ++i) sum += 0.5 * (double(outL[i]) * outL[i] + double(outR[i]) * outR[i]);
        return std::sqrt(sum / static_cast<double>(std::max<std::size_t>(1, to - from)));
    }
    double inputRms(std::size_t from, std::size_t to) const {
        double sum = 0;
        to = std::min(to, inL.size());
        for (std::size_t i = from; i < to; ++i) sum += double(inL[i]) * inL[i];
        return std::sqrt(sum / static_cast<double>(std::max<std::size_t>(1, to - from)));
    }
    // Output (left) minus the input `latency` samples earlier, relative to the input, in dB:
    // the figure the AU harness measures.
    double residualDb(std::size_t from, std::size_t to) const {
        const auto latency = static_cast<std::size_t>(p->getLatencySamples());
        double diff = 0, ref = 0;
        to = std::min(to, outL.size());
        for (std::size_t i = std::max(from, latency); i < to; ++i) {
            const double in = inL[i - latency], d = outL[i] - in;
            diff += d * d; ref += in * in;
        }
        return toDb(std::sqrt(diff)) - toDb(std::sqrt(ref));
    }
    double peak(std::size_t from, std::size_t to) const {
        double m = 0;
        to = std::min(to, outL.size());
        for (std::size_t i = from; i < to; ++i) m = std::max({ m, std::abs(double(outL[i])), std::abs(double(outR[i])) });
        return m;
    }
    bool finite() const {
        for (std::size_t i = 0; i < outL.size(); ++i) if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) return false;
        return true;
    }
    // Largest sample-to-sample step of the left output: a click shows up as a step much
    // larger than the signal's own.
    double maxStep(std::size_t from, std::size_t to) const {
        double m = 0;
        to = std::min(to, outL.size());
        for (std::size_t i = std::max<std::size_t>(from, 2); i < to; ++i)
            m = std::max(m, std::abs(double(outL[i]) - 2.0 * outL[i - 1] + outL[i - 2]));
        return m;
    }
};

// Octave-band levels (62 Hz .. 8 kHz) of the left output over a window, in dB.
std::vector<double> bandLevels(const Host& h, std::size_t from, std::size_t to) {
    std::vector<double> levels;
    for (double centre = 62.5; centre <= 8000.0; centre *= 2.0) {
        juce::IIRFilter low, high;
        high.setCoefficients(juce::IIRCoefficients::makeHighPass(h.sr, centre / 1.414));
        low.setCoefficients(juce::IIRCoefficients::makeLowPass(h.sr, centre * 1.414));
        double sum = 0;
        for (std::size_t i = from; i < to; ++i) {
            const float y = low.processSingleSampleRaw(high.processSingleSampleRaw(h.outL[i]));
            if (i > from + 2000) sum += double(y) * y;
        }
        levels.push_back(toDb(std::sqrt(sum / double(to - from))));
    }
    return levels;
}

//==============================================================================
void testParameters(Suite& s) {
    s.section("parameters");
    DungeonCrawlerProcessor p;
    const char* expected[] = { "location", "intensity", "mix", "input", "output", "bypass" };
    for (const auto* id : expected) s.check(p.parameters.getParameter(id) != nullptr, std::string("parameter ") + id + " exists");
    auto* location = dynamic_cast<juce::AudioParameterChoice*>(p.parameters.getParameter(dc::ids::location));
    if (s.check(location != nullptr, "Location is a choice")) {
        s.check(location->choices.size() == dc::LocationCount, "Location lists every place", std::to_string(location->choices.size()));
        s.check(location->getIndex() == dc::Dungeon, "Location starts in the Dungeon");
        s.check(location->isAutomatable(), "Location is automatable");
    }
    s.check(p.getBypassParameter() == p.parameters.getParameter(dc::ids::bypass), "host bypass is the Bypass parameter");
    // Every location differs from neutral, and every Log-glide range is positive.
    for (std::size_t i = 0; i < dc::Count; ++i)
        if (dc::moduleSpecs[i].glide == dc::Glide::Log) s.check(dc::moduleSpecs[i].min > 0, std::string("log range > 0: ") + dc::moduleSpecs[i].id);
    for (int l = 0; l < dc::LocationCount; ++l) {
        const auto f = dc::locationFrame(l);
        bool inRange = true;
        for (std::size_t i = 0; i < dc::Count; ++i) inRange = inRange && f[i] >= dc::moduleSpecs[i].min && f[i] <= dc::moduleSpecs[i].max;
        s.check(inRange, placeName(l) + ": every setting within its range");
    }
}

void testSmoke(Suite& s) {
    s.section("smoke: default place, harness figures, state");
    // The default (Dungeon) is audible and changes the sound.
    Host h;
    h.render(3.0, sine(440.0, 0.25f));
    s.check(h.finite(), "output is finite");
    s.check(toDb(h.rms(h.at(1.0), h.end())) > -40.0, "the Dungeon is audible", fmt(toDb(h.rms(h.at(1.0), h.end()))) + " dBFS");
    s.check(h.residualDb(h.at(1.0), h.end()) > -20.0, "the Dungeon changes the signal", fmt(h.residualDb(h.at(1.0), h.end())) + " dB");
    // The AU harness's Location check, in its exact order and timing: Intensity 0
    // (3 s), then Intensity 1 in the Village (3.5 s, last 1 s measured), then the
    // Cathedral (the same).
    Host hv;
    hv.set(dc::ids::intensity, 0.0f); hv.render(3.0, sine(440.0, 0.25f));
    hv.set(dc::ids::intensity, 1.0f); hv.location(dc::Village); hv.render(3.5, sine(440.0, 0.25f));
    const double village = hv.residualDb(hv.end() - hv.at(1.0), hv.end());
    hv.location(dc::Cathedral); hv.render(3.5, sine(440.0, 0.25f));
    const double cathedral = hv.residualDb(hv.end() - hv.at(1.0), hv.end());
    s.metric("harness sequence: residual Village", village, " dB");
    s.metric("harness sequence: residual Cathedral", cathedral, " dB");
    s.check(std::abs(cathedral - village) > 4.0, "Village and Cathedral differ from the input by clearly different amounts (the harness needs > 3 dB)",
            fmt(village) + " vs " + fmt(cathedral));
    // The harness's Intensity check: from the Dungeon (3 s) to Intensity 0, 3 s, the
    // last 1 s measured. CI measured -39.6 dB here before the matcher fix.
    Host hi;
    hi.render(3.0, sine(440.0, 0.25f));
    hi.set(dc::ids::intensity, 0.0f); hi.render(3.0, sine(440.0, 0.25f));
    s.metric("harness sequence: Intensity 0 residual", hi.residualDb(hi.end() - hi.at(1.0), hi.end()), " dB");
    s.check(hi.residualDb(hi.end() - hi.at(1.0), hi.end()) < -60.0, "Intensity 0 after the Dungeon is transparent within 2 s (the harness needs < -40 dB)",
            fmt(hi.residualDb(hi.end() - hi.at(1.0), hi.end())) + " dB");
    // Intensity 0 is transparent.
    Host n; n.set(dc::ids::intensity, 0.0f); n.render(3.0, sine(440.0, 0.25f));
    s.check(n.residualDb(n.at(1.0), n.end()) < -60.0, "Intensity 0 passes the input through", fmt(n.residualDb(n.at(1.0), n.end())) + " dB");
    // State round trip of every parameter.
    Host a;
    a.location(dc::Swamp); a.set(dc::ids::intensity, 0.4f); a.set(dc::ids::mix, 0.7f); a.set(dc::ids::input, 3.0f); a.set(dc::ids::output, -2.0f);
    juce::MemoryBlock state;
    a.p->getStateInformation(state);
    DungeonCrawlerProcessor b;
    b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    bool same = true;
    for (int i = 0; i < a.p->getParameters().size(); ++i)
        same = same && std::abs(a.p->getParameters()[i]->getValue() - b.getParameters()[i]->getValue()) < 1.0e-6f;
    s.check(same, "state restores every parameter in a fresh instance");
}

void testState(Suite& s) {
    s.section("state");
    DungeonCrawlerProcessor p;
    p.parameters.getParameter(dc::ids::location)->setValueNotifyingHost(p.parameters.getParameter(dc::ids::location)->convertTo0to1(4));
    const char junk[] = "not a state at all";
    p.setStateInformation(junk, sizeof(junk));
    p.setStateInformation(nullptr, 0);
    s.check(juce::roundToInt(p.parameters.getRawParameterValue(dc::ids::location)->load()) == 4, "malformed state is ignored");
    // A state with a NaN value is rejected as a whole.
    auto tree = p.parameters.copyState();
    for (auto child : tree) if (child.getProperty("id").toString() == dc::ids::mix) child.setProperty("value", std::nan(""), nullptr);
    juce::MemoryBlock bad;
    if (auto xml = tree.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, bad);
    p.setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
    s.check(std::isfinite(p.parameters.getRawParameterValue(dc::ids::mix)->load()), "a state with NaN is rejected");
    // A state missing parameters (an older version) keeps their defaults.
    juce::ValueTree partial(p.parameters.state.getType());
    juce::ValueTree only("PARAM"); only.setProperty("id", dc::ids::location, nullptr); only.setProperty("value", 7, nullptr);
    partial.addChild(only, -1, nullptr);
    juce::MemoryBlock older;
    if (auto xml = partial.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, older);
    p.setStateInformation(older.getData(), static_cast<int>(older.getSize()));
    s.check(juce::roundToInt(p.parameters.getRawParameterValue(dc::ids::location)->load()) == 7, "a partial state sets what it has");
    s.check(std::abs(p.parameters.getRawParameterValue(dc::ids::intensity)->load() - 1.0f) < 1.0e-6f, "a partial state leaves the rest at defaults");
    // Out-of-range values are clamped.
    juce::ValueTree wild(p.parameters.state.getType());
    juce::ValueTree loud("PARAM"); loud.setProperty("id", dc::ids::output, nullptr); loud.setProperty("value", 99.0, nullptr);
    wild.addChild(loud, -1, nullptr);
    juce::MemoryBlock wildData;
    if (auto xml = wild.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, wildData);
    p.setStateInformation(wildData.getData(), static_cast<int>(wildData.getSize()));
    s.check(p.parameters.getRawParameterValue(dc::ids::output)->load() <= 12.0f, "out-of-range values are clamped");
}

void testLayouts(Suite& s) {
    s.section("bus layouts");
    DungeonCrawlerProcessor p;
    const auto layout = [](juce::AudioChannelSet in, juce::AudioChannelSet out) {
        juce::AudioProcessor::BusesLayout l; l.inputBuses.add(in); l.outputBuses.add(out); return l;
    };
    s.check(p.checkBusesLayoutSupported(layout(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())), "stereo -> stereo");
    s.check(p.checkBusesLayoutSupported(layout(juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())), "mono -> mono");
    s.check(p.checkBusesLayoutSupported(layout(juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())), "mono -> stereo");
    s.check(!p.checkBusesLayoutSupported(layout(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono())), "stereo -> mono is refused");
    Host mono(48000, 512, 1, 1);
    mono.render(2.0, sine(330.0, 0.25f));
    s.check(mono.finite() && toDb(mono.rms(mono.at(1.0), mono.end())) > -40.0, "mono -> mono is audible");
    Host wide(48000, 512, 1, 2);
    wide.render(2.0, sine(330.0, 0.25f));
    double right = 0;
    for (std::size_t i = wide.at(1.0); i < wide.end(); ++i) right += double(wide.outR[i]) * wide.outR[i];
    s.check(toDb(std::sqrt(right / double(wide.end() - wide.at(1.0)))) > -40.0, "mono -> stereo writes the right output");
}

void testTransparency(Suite& s) {
    s.section("transparency, bypass and latency");
    const int latency = Host().p->getLatencySamples();
    s.metric("latency at 48 kHz", latency, " samples");
    s.check(latency > 0 && latency < 480, "latency is reported and short", std::to_string(latency));
    for (int l = 0; l < dc::LocationCount; l += 3) {
        Host h; h.location(l); h.set(dc::ids::mix, 0.0f);
        h.render(2.0, sawChord(0.3f));
        s.check(h.residualDb(h.at(0.2), h.end()) < -100.0, placeName(l) + ": Mix 0 is the input exactly (latency-aligned)",
                fmt(h.residualDb(h.at(0.2), h.end())) + " dB");
        Host b; b.location(l); b.set(dc::ids::bypass, 1.0f);
        b.render(2.0, sawChord(0.3f));
        s.check(b.residualDb(b.at(0.2), b.end()) < -100.0, placeName(l) + ": Bypass is the input exactly", fmt(b.residualDb(b.at(0.2), b.end())) + " dB");
        Host z; z.location(l); z.set(dc::ids::intensity, 0.0f);
        z.render(3.0, sawChord(0.3f));
        s.check(z.residualDb(z.at(1.5), z.end()) < -60.0, placeName(l) + ": Intensity 0 is transparent", fmt(z.residualDb(z.at(1.5), z.end())) + " dB");
    }
    // The bypass switch fades (no click) both ways.
    Host t;
    t.render(2.0, sine(220.0, 0.3f));
    const double steady = t.maxStep(t.at(1.0), t.end());
    t.set(dc::ids::bypass, 1.0f); t.render(0.5, sine(220.0, 0.3f));
    t.set(dc::ids::bypass, 0.0f); t.render(0.5, sine(220.0, 0.3f));
    s.check(t.maxStep(t.at(2.0), t.end()) < 3.0 * steady, "Bypass on and off without clicks", fmt(t.maxStep(t.at(2.0), t.end()) / steady, 2) + "x");
}

void testLocations(Suite& s) {
    s.section("locations: level matching and distinctness");
    std::vector<std::vector<double>> profiles;
    for (int l = 0; l < dc::LocationCount; ++l) {
        // A sustained bright chord for 4 s, then 2 s of silence (the tail).
        Host h; h.location(l);
        h.render(4.0, sawChord(0.3f));
        h.render(2.0, silence());
        const double in = h.inputRms(h.at(2.0), h.at(4.0));
        const double out = h.rms(h.at(2.0), h.at(4.0));
        const double level = toDb(out) - toDb(in);
        s.metric(placeName(l) + " level vs input (sustained chord)", level, " dB");
        s.check(std::abs(level) < 3.0, placeName(l) + ": level-matched to the input within 3 dB", fmt(level) + " dB");
        s.check(h.peak(0, h.end()) < 0.98, placeName(l) + ": peak stays below 0 dBFS", fmt(h.peak(0, h.end()), 3));
        auto profile = bandLevels(h, h.at(2.0), h.at(4.0));
        const double tail = toDb(h.rms(h.at(4.3), h.at(4.8))) - toDb(out);
        s.metric(placeName(l) + " tail 0.3-0.8 s after the chord", tail, " dB");
        profile.push_back(tail);
        profiles.push_back(profile);
        // Guitar-like plucks too (report: the matcher holds during decays by design).
        Host g; g.location(l);
        g.render(4.0, plucks(0.4f));
        s.metric(placeName(l) + " level vs input (plucks)", toDb(g.rms(g.at(1.0), g.end())) - toDb(g.inputRms(g.at(1.0), g.end())), " dB");
        s.check(g.finite(), placeName(l) + ": plucks finite");
    }
    // Every pair differs somewhere: in a band's level or in the tail.
    for (int a = 0; a < dc::LocationCount; ++a)
        for (int b = a + 1; b < dc::LocationCount; ++b) {
            double distance = 0;
            for (std::size_t k = 0; k < profiles[0].size(); ++k)
                distance = std::max(distance, std::abs(profiles[std::size_t(a)][k] - profiles[std::size_t(b)][k]));
            s.check(distance > 2.0, placeName(a) + " and " + placeName(b) + " sound different", fmt(distance) + " dB");
        }
}

void testTravel(Suite& s) {
    s.section("travel: smooth, click-free, level-steady location changes");
    // From the Dungeon to every place and back, on a low sine (the most revealing
    // signal for clicks) and on the chord (for loudness steps).
    double worstStep = 0, worstLoudness = 0;
    for (int l = 1; l < dc::LocationCount; ++l) {
        Host h;
        h.render(3.0, sine(220.0, 0.25f));
        const double before = h.maxStep(h.at(1.5), h.at(3.0));
        h.location(l);
        h.render(2.0, sine(220.0, 0.25f));
        h.render(1.5, sine(220.0, 0.25f));
        const double after = h.maxStep(h.at(5.0), h.at(6.5));
        const double during = h.maxStep(h.at(3.0), h.at(5.0));
        const double ratio = during / std::max(before, after);
        worstStep = std::max(worstStep, ratio);
        s.check(h.finite(), "Dungeon -> " + placeName(l) + ": finite");
        s.check(ratio < 2.5, "Dungeon -> " + placeName(l) + ": no click while travelling", fmt(ratio, 2) + "x the steady step");
        s.check(std::abs(h.p->engine().travelProgress() - 1.0f) < 1.0e-6f && h.p->engine().destination() == l, "Dungeon -> " + placeName(l) + ": arrives");

        Host c;
        c.render(3.0, sawChord(0.3f));
        c.location(l);
        c.render(3.0, sawChord(0.3f));
        // 50 ms windows through the journey vs the level before leaving.
        const double reference = toDb(c.rms(c.at(2.0), c.at(3.0)));
        double deviation = 0;
        for (double t = 3.0; t < 5.95; t += 0.05) deviation = std::max(deviation, std::abs(toDb(c.rms(c.at(t), c.at(t + 0.05))) - reference));
        worstLoudness = std::max(worstLoudness, deviation);
        s.check(deviation < 4.0, "Dungeon -> " + placeName(l) + ": loudness steady while travelling (< 4 dB)", fmt(deviation) + " dB");
    }
    s.metric("worst click ratio while travelling", worstStep, "x");
    s.metric("worst loudness deviation while travelling", worstLoudness, " dB");
    // Changing destination mid-way, many times, at random block sizes.
    Host h(48000, 128);
    juce::Random random(7);
    for (int k = 0; k < 40; ++k) {
        h.location(random.nextInt(dc::LocationCount));
        h.set(dc::ids::intensity, random.nextFloat());
        h.render(0.1 + random.nextFloat() * 0.6, plucks(0.4f));
    }
    s.check(h.finite() && h.peak(0, h.end()) < 0.98, "rapid location and intensity changes stay finite and below 0 dBFS", fmt(h.peak(0, h.end()), 3));
    // The interpolation itself: Log params glide geometrically, Step params switch half-way.
    const auto a = dc::locationFrame(dc::Swamp), b = dc::locationFrame(dc::Castle);
    const auto mid = dc::Engine::interpolate(a, b, 0.5f);
    s.check(std::abs(mid[dc::Cutoff] - std::sqrt(a[dc::Cutoff] * b[dc::Cutoff])) < 1.0f, "Cutoff glides geometrically");
    s.check(dc::Engine::interpolate(a, b, 0.49f)[dc::Character] == a[dc::Character] && mid[dc::Character] == b[dc::Character], "Chamber switches half-way");
}

void testModules(Suite& s) {
    s.section("modules: audit fixes");
    using namespace dc;
    // Crypt: chamber and size changes on a held sine must not click (VEIL glided the
    // line lengths and switched the reflections at once).
    {
        Crypt crypt; crypt.prepare(48000);
        auto f = neutralFrame();
        f[ReverbMix] = 0.6f; f[Decay] = 4; f[Character] = 0; f[Space] = 0.3f;
        Context c;
        std::vector<float> out;
        std::array<float, maxChunk> l {}, r {};
        long long n = 0;
        for (int chunk = 0; chunk < 48000 * 8 / maxChunk; ++chunk) {
            if (chunk % 1500 == 0) { f[Character] = static_cast<float>((chunk / 1500) % 4); f[Space] = (chunk / 1500) % 2 ? 0.9f : 0.2f; }
            for (int i = 0; i < maxChunk; ++i, ++n) l[std::size_t(i)] = r[std::size_t(i)] = 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * float(n) / 48000.0f);
            crypt.process(l.data(), r.data(), maxChunk, f, c);
            out.insert(out.end(), l.begin(), l.end());
        }
        double steady = 0, worst = 0;
        for (std::size_t i = 2; i < out.size(); ++i) {
            const double step = std::abs(double(out[i]) - 2.0 * out[i - 1] + out[i - 2]);
            // Steady: late in each setting (its own sound); worst: just after each change.
            const auto chunkInSegment = (i / maxChunk) % 1500;
            if (chunkInSegment >= 600) steady = std::max(steady, step);
            if (chunkInSegment < 200 && i > 96000) worst = std::max(worst, step);
        }
        s.metric("Crypt chamber/size change step ratio", worst / steady, "x");
        s.check(worst < 2.5 * steady, "Crypt: chamber and size changes without clicks", fmt(worst / steady, 2) + "x");
    }
    // Ancient Blood: Drive 0 -> 36 dB at once must glide, not jump.
    {
        AncientBlood blood; blood.prepare(48000);
        auto f = neutralFrame();
        f[BloodMix] = 1; f[Drive] = 0; f[Texture] = 0.5f;
        Context c;
        std::array<float, maxChunk> l {}, r {};
        std::vector<float> out;
        long long n = 0;
        for (int chunk = 0; chunk < 48000 / maxChunk; ++chunk) {
            if (chunk == 400) { f[Drive] = 36; f[Age] = 0.6f; f[Instability] = 0.8f; }
            for (int i = 0; i < maxChunk; ++i, ++n) l[std::size_t(i)] = r[std::size_t(i)] = 0.2f * std::sin(2.0f * 3.14159265f * 110.0f * float(n) / 48000.0f);
            blood.process(l.data(), r.data(), maxChunk, f, c);
            out.insert(out.end(), l.begin(), l.end());
        }
        double before = 0, around = 0;
        for (std::size_t i = 2; i < out.size(); ++i) {
            const double step = std::abs(double(out[i]) - 2.0 * out[i - 1] + out[i - 2]);
            // The steady steps of both the clean and the driven sound.
            if ((i > 10000 && i < 25000) || i > 35000) before = std::max(before, step);
            if (i >= 25600 && i < 25600 + 256) around = std::max(around, step);
        }
        s.metric("Blood drive jump: first 256 samples' step vs steady", around / before, "x");
        s.check(around < 3.0 * before, "Ancient Blood: a drive jump glides (no click)", fmt(around / before, 2) + "x");
    }
}

void testRates(Suite& s) {
    s.section("sample rates and block sizes");
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int blockSize : { 1, 17, 64, 333, 512, 4096 }) {
            if (blockSize == 1 && rate > 50000) continue;
            Host h(rate, blockSize);
            h.location(static_cast<int>(rate / 1000 + blockSize) % dc::LocationCount);
            h.render(blockSize == 1 ? 0.6 : 1.5, sawChord(0.3f));
            const double level = toDb(h.rms(h.at(0.3), h.end()));
            s.check(h.finite() && level > -40.0 && h.peak(0, h.end()) < 0.98,
                    "@" + fmt(rate, 0) + " / " + std::to_string(blockSize) + ": finite, audible, below 0 dBFS", fmt(level) + " dBFS");
        }
    // Host blocks that change size every call.
    Host h(48000, 4096);
    juce::Random random(3);
    for (int k = 0; k < 300; ++k) h.processOne(1 + random.nextInt(2048), plucks(0.4f));
    s.check(h.finite(), "varying block sizes stay finite");
}

void testExtremes(Suite& s) {
    s.section("extremes");
    // Very loud noise with +24 dB Input and +12 dB Output: the limiter holds.
    for (int l = 0; l < dc::LocationCount; ++l) {
        Host h; h.location(l); h.set(dc::ids::input, 24.0f); h.set(dc::ids::output, 12.0f);
        juce::Random random(l + 1);
        h.render(2.0, [&](int, long long, double) { return random.nextFloat() * 2.0f - 1.0f; });
        s.check(h.finite() && h.peak(0, h.end()) < 0.97, placeName(l) + ": full-scale noise +36 dB stays below -0.3 dBFS", fmt(h.peak(0, h.end()), 3));
    }
    // NaN, infinity and denormals in the input never reach the output.
    Host bad;
    bad.render(1.0, [](int, long long n, double) {
        if (n % 997 == 0) return std::numeric_limits<float>::quiet_NaN();
        if (n % 1009 == 0) return std::numeric_limits<float>::infinity();
        return n % 3 == 0 ? 1.0e-39f : 0.1f;
    });
    s.check(bad.finite(), "NaN / Inf / denormal input gives finite output");
    // After a burst, every place falls silent (no self-oscillation).
    for (int l = 0; l < dc::LocationCount; ++l) {
        Host h; h.location(l);
        h.render(1.0, sawChord(0.4f));
        h.render(16.0, silence());
        const double last = toDb(h.rms(h.at(16.0), h.end()));
        s.check(last < -70.0, placeName(l) + ": silent 15 s after the input stops", fmt(last) + " dBFS");
    }
}

void testRealtime(Suite& s) {
    s.section("real-time safety");
    Host h(48000, 512);
    h.render(1.0, plucks(0.4f));
    juce::Random random(11);
    std::size_t allocations = 0;
    for (int k = 0; k < 400; ++k) {
        if (k % 20 == 0) h.location(random.nextInt(dc::LocationCount));
        if (k % 33 == 0) h.set(dc::ids::intensity, random.nextFloat());
        if (k % 91 == 0) h.set(dc::ids::bypass, k % 2 ? 1.0f : 0.0f);
        // Fill the input outside the probe, then process inside it.
        const int n = k % 3 == 0 ? 512 : 64 + k;
        h.buffer.setSize(2, n, false, false, true);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) h.buffer.setSample(ch, i, 0.2f * std::sin(0.01f * float(i + k)));
        allocationProbe::calls = 0;
        allocationProbe::enabled = true;
        h.p->processBlock(h.buffer, h.midi);
        allocationProbe::enabled = false;
        allocations += allocationProbe::calls;
    }
    s.check(allocations == 0, "processBlock performs no heap allocations (travel, intensity, bypass)", std::to_string(allocations));
}

void testCpu(Suite& s) {
    s.section("cpu (report only)");
    for (int l = 0; l < dc::LocationCount; ++l) {
        Host h(48000, 512);
        h.location(l);
        h.render(1.0, sawChord(0.3f));
        const auto begin = std::chrono::steady_clock::now();
        h.render(10.0, sawChord(0.3f));
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        s.metric(placeName(l) + " CPU (one core, 48 kHz stereo)", 100.0 * seconds / 10.0, " %");
        s.check(h.finite(), placeName(l) + ": finite");
    }
    Host h(48000, 512);
    h.render(1.0, sawChord(0.3f));
    const auto begin = std::chrono::steady_clock::now();
    for (int k = 0; k < 10; ++k) { h.location(k); h.render(1.0, sawChord(0.3f)); }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    s.metric("travelling every second (one core, 48 kHz stereo)", 100.0 * seconds / 10.0, " %");
}

void testEditor(Suite& s, const juce::String& snapshot) {
    s.section("editor");
    DungeonCrawlerProcessor p;
    p.setRateAndBufferSizeDetails(48000, 512);
    p.prepareToPlay(48000, 512);
    std::unique_ptr<juce::AudioProcessorEditor> base(p.createEditor());
    auto* editor = dynamic_cast<DungeonCrawlerEditor*>(base.get());
    if (!s.check(editor != nullptr, "editor is created")) return;
    s.check(editor->getWidth() == DungeonCrawlerEditor::defaultWidth && editor->getHeight() == DungeonCrawlerEditor::defaultHeight, "editor has its size");
    s.check(editor->shownLocation() == dc::Dungeon && editor->locationButton(dc::Dungeon).getToggleState(), "the Dungeon is marked at start");
    editor->locationButton(dc::Mountains).onClick();   // what a click runs (triggerClick is asynchronous)
    s.check(juce::roundToInt(p.parameters.getRawParameterValue(dc::ids::location)->load()) == dc::Mountains, "clicking a place sets Location");
    p.parameters.getParameter(dc::ids::location)->setValueNotifyingHost(p.parameters.getParameter(dc::ids::location)->convertTo0to1(dc::Necropolis));
    s.check(editor->shownLocation() == dc::Necropolis && editor->locationButton(dc::Necropolis).getToggleState()
            && !editor->locationButton(dc::Mountains).getToggleState(), "automation of Location moves the marker");
    const auto begin = std::chrono::steady_clock::now();
    auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    for (int k = 0; k < 9; ++k) image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() / 10.0;
    s.metric("full editor repaint (software)", ms, " ms");
    s.check(ms < 33.0, "a full repaint fits in a 30 fps frame", fmt(ms) + " ms");
    s.check(image.isValid(), "editor paints");
    if (snapshot.isNotEmpty()) {
        juce::File file(snapshot);
        file.deleteFile();
        juce::FileOutputStream stream(file);
        juce::PNGImageFormat().writeImageToStream(image, stream);
        std::cout << "snapshot written to " << file.getFullPathName() << "\n";
    }
}

int renderToFile(const juce::String& path, int location, double seconds) {
    Host h(48000, 512);
    h.location(location);
    h.render(seconds * 0.5, plucks(0.4f));
    h.render(seconds * 0.5, sawChord(0.25f));
    h.render(3.0, silence());
    juce::AudioBuffer<float> out(2, static_cast<int>(h.end()));
    for (std::size_t i = 0; i < h.end(); ++i) { out.setSample(0, int(i), h.outL[i]); out.setSample(1, int(i), h.outR[i]); }
    juce::File file(path);
    file.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    std::unique_ptr<juce::AudioFormatWriter> writer(juce::WavAudioFormat().createWriterFor(stream.get(), 48000, 2, 24, {}, 0));
    if (writer == nullptr) return 1;
    stream.release();
    writer->writeFromAudioSampleBuffer(out, 0, out.getNumSamples());
    std::cout << "wrote " << file.getFullPathName() << "\n";
    return 0;
}
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String snapshot;
    juce::StringArray only;
    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]);
        if (arg == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
        else if (arg == "--only" && i + 1 < argc) only = juce::StringArray::fromTokens(argv[++i], ",", "");
        else if (arg == "--render" && i + 2 < argc)
            return renderToFile(argv[i + 1], juce::String(argv[i + 2]).getIntValue(), i + 3 < argc ? juce::String(argv[i + 3]).getDoubleValue() : 8.0);
    }
    const auto run = [&](const char* name) { return only.isEmpty() || only.contains(name); };
    Suite s;
    if (run("parameters")) testParameters(s);
    if (run("smoke")) testSmoke(s);
    if (run("state")) testState(s);
    if (run("layouts")) testLayouts(s);
    if (run("transparency")) testTransparency(s);
    if (run("locations")) testLocations(s);
    if (run("travel")) testTravel(s);
    if (run("modules")) testModules(s);
    if (run("rates")) testRates(s);
    if (run("extremes")) testExtremes(s);
    if (run("realtime")) testRealtime(s);
    if (run("editor")) testEditor(s, snapshot);
    if (only.contains("cpu")) testCpu(s);   // report only: run explicitly
    std::cout << "\n" << s.passed << " passed, " << s.failed << " failed" << std::endl;
    return s.failed == 0 ? 0 : 1;
}
