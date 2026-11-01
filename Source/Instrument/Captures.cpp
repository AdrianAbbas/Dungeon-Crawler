#include "Captures.h"
#include "SampleBank.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <mutex>

namespace dc::inst {
namespace {
struct Store {
    std::once_flag once;
    std::array<std::unique_ptr<CaptureIr>, CaptureCount> irs;
    int loaded = 0;
};
Store& store() {
    static Store s;
    return s;
}

juce::File captureFolder() {
    // Next to the samples (the bank knows where they are), or the development folder.
    auto& bank = wb::samples::Bank::get();
    bank.waitUntilLoaded(3000);
    if (!bank.ready()) return {};   // no recordings (or the tests' --no-samples): no captures either
    auto folder = bank.folder().getChildFile("captures");
    if (folder.isDirectory()) return folder;
   #ifdef DC_DEV_SAMPLES_DIR
    if (juce::String(DC_DEV_SAMPLES_DIR).isNotEmpty()) return juce::File(DC_DEV_SAMPLES_DIR).getChildFile("captures");
   #endif
    return {};
}

void loadAll() {
    auto& s = store();
    static constexpr const char* files[CaptureCount] { nullptr, "shimmer.wav", "darkhall.wav", "chorusverb.wav" };
    const auto folder = captureFolder();
    juce::WavAudioFormat wav;
    for (int c = 1; c < CaptureCount; ++c) {
        const auto file = folder.getChildFile(files[c]);
        if (!file.existsAsFile()) continue;
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(), true));
        if (reader == nullptr || reader->lengthInSamples < 1000 || reader->numChannels < 1) continue;
        const int length = int(std::min<juce::int64>(reader->lengthInSamples, juce::int64(10.0 * reader->sampleRate)));
        juce::AudioBuffer<float> buffer(int(std::min(2u, reader->numChannels)), length);
        reader->read(&buffer, 0, length, 0, true, buffer.getNumChannels() > 1);
        auto ir = std::make_unique<CaptureIr>();
        ir->rate = reader->sampleRate;
        ir->stereo = buffer.getNumChannels() > 1;
        // Unit energy per channel (the tools write them so; normalised again after the
        // 24-bit peak scaling).
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto& target = ch == 0 ? ir->l : ir->r;
            target.assign(buffer.getReadPointer(ch), buffer.getReadPointer(ch) + length);
            double energy = 0;
            for (float v : target) energy += double(v) * v;
            const float g = energy > 0 ? float(1.0 / std::sqrt(energy)) : 0.0f;
            for (float& v : target) v *= g;
        }
        if (!ir->stereo) ir->r = ir->l;
        s.irs[std::size_t(c)] = std::move(ir);
        ++s.loaded;
    }
}

// Linear resampling to the host rate (a reverb tail: no audible cost).
std::vector<float> resampled(const std::vector<float>& x, double from, double to) {
    if (std::abs(from - to) < 0.5) return x;
    const double ratio = from / to;
    const auto n = std::size_t(double(x.size()) / ratio);
    std::vector<float> y(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double p = double(i) * ratio;
        const auto k = std::size_t(p);
        const float t = float(p - double(k));
        const float a = x[std::min(k, x.size() - 1)], b = x[std::min(k + 1, x.size() - 1)];
        y[i] = (a + (b - a) * t) * float(std::sqrt(ratio));   // energy kept
    }
    return y;
}
}

const CaptureIr* captureIr(int capture) {
    if (capture <= 0 || capture >= CaptureCount) return nullptr;
    auto& s = store();
    std::call_once(s.once, loadAll);
    return s.irs[std::size_t(capture)].get();
}

int capturesLoaded() {
    auto& s = store();
    std::call_once(s.once, loadAll);
    return s.loaded;
}

void CaptureBus::prepare(double sampleRate) {
    rate = sampleRate;
    for (int c = 1; c < CaptureCount; ++c) {
        auto& sl = slot(c);
        sl.ready = false;
        const auto* ir = captureIr(c);
        if (ir == nullptr) continue;
        const auto l = resampled(ir->l, ir->rate, rate), r = resampled(ir->r, ir->rate, rate);
        sl.spectraL = conv::buildSpectra(l.data(), int(l.size()));
        sl.spectraR = conv::buildSpectra(r.data(), int(r.size()));
        const int length = std::max(sl.spectraL->length, sl.spectraR->length);
        sl.convL.prepare(length);
        sl.convR.prepare(length);
        sl.convL.setIr(sl.spectraL.get());
        sl.convR.setIr(sl.spectraR.get());
        sl.tail = length;
        sl.quietFor = length;
        sl.up.prepare(float(rate));
        // LIMINAL's measurement: the shimmer's octave sits 11.5 dB under its linear response.
        sl.octaveLevel = c == CaptureShimmer ? 0.27f : 0.0f;
        sl.ready = true;
    }
}

void CaptureBus::reset() noexcept {
    for (auto& sl : slots) {
        if (!sl.ready) continue;
        sl.convL.reset();
        sl.convR.reset();
        sl.convL.setIr(sl.spectraL.get());
        sl.convR.setIr(sl.spectraR.get());
        sl.up.clear();
        sl.in.fill(0);
        sl.quietFor = sl.tail;
    }
}

void CaptureBus::process(float* l, float* r, int n) noexcept {
    for (auto& sl : slots) {
        if (!sl.ready) continue;
        bool sent = false;
        for (int i = 0; i < n && !sent; ++i) sent = sl.in[std::size_t(i)] != 0.0f;
        sl.quietFor = sent ? 0 : std::min(sl.quietFor + n, sl.tail + 1);
        // Idle (nothing sent for an IR's length): its tail is over and nothing runs.
        if (sl.quietFor > sl.tail) continue;
        if (sl.octaveLevel > 0.0f)
            for (int i = 0; i < n; ++i) sl.in[std::size_t(i)] += sl.octaveLevel * sl.up.process(sl.in[std::size_t(i)]);
        sl.convL.process(sl.in.data(), sl.wetL.data(), n);
        sl.convR.process(sl.in.data(), sl.wetR.data(), n);
        for (int i = 0; i < n; ++i) {
            l[i] += sl.wetL[std::size_t(i)];
            r[i] += sl.wetR[std::size_t(i)];
        }
        std::fill(sl.in.begin(), sl.in.begin() + n, 0.0f);
    }
}
}
