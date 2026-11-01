#include "Convolver.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cstring>

namespace dc::conv {
namespace {
int ceilPow2(int n) { int p = 1; while (p < n) p <<= 1; return p; }
int log2i(int n) { int o = 0; while ((1 << o) < n) ++o; return o; }

// The stage layout for an IR of `length` (>= minLength) samples.
std::vector<StageSpec> layout(int length) {
    std::vector<StageSpec> out;
    int offset = headSize, partition = headSize;
    while (offset < length) {
        StageSpec st;
        st.partition = partition;
        st.offset = offset;
        const int next = std::min(partition * growth, maxPartition);
        int end = next > partition ? 2 * next : length;
        end = std::min(end, length);
        st.parts = std::max(1, (end - offset + partition - 1) / partition);
        st.fftSize = 2 * partition;
        st.slices = partition / headSize;
        const int index = static_cast<int>(out.size());
        st.fftSlice = st.slices == 1 ? 0 : std::min(index, st.slices / 2 - 1);
        st.ifftSlice = st.slices == 1 ? 0 : st.slices - std::max(1, index);
        offset += st.parts * partition;
        out.push_back(std::move(st));
        partition = next;
    }
    return out;
}
}

std::shared_ptr<const IrSpectra> buildSpectra(const float* ir, int len) {
    auto spectra = std::make_shared<IrSpectra>();
    const int length = std::max(len, minLength);
    std::vector<float> h(static_cast<std::size_t>(length), 0.0f);
    std::copy(ir, ir + len, h.begin());
    spectra->length = length;
    spectra->head.assign(headSize, 0.0f);
    for (int i = 0; i < headSize; ++i) spectra->head[static_cast<std::size_t>(headSize - 1 - i)] = h[static_cast<std::size_t>(i)];
    spectra->stages = layout(length);
    for (auto& st : spectra->stages) {
        juce::dsp::FFT fft(log2i(st.fftSize));
        const auto bins = static_cast<std::size_t>(st.fftSize + 2);
        st.filters.assign(bins * static_cast<std::size_t>(st.parts), 0.0f);
        std::vector<float> work(static_cast<std::size_t>(2 * st.fftSize));
        for (int k = 0; k < st.parts; ++k) {
            std::fill(work.begin(), work.end(), 0.0f);
            const int a = st.offset + k * st.partition;
            for (int i = 0; i < st.partition && a + i < length; ++i) work[static_cast<std::size_t>(i)] = h[static_cast<std::size_t>(a + i)];
            fft.performRealOnlyForwardTransform(work.data(), true);
            std::memcpy(&st.filters[bins * static_cast<std::size_t>(k)], work.data(), bins * sizeof(float));
        }
    }
    return spectra;
}

Convolver::Convolver() = default;
Convolver::~Convolver() = default;

void Convolver::prepare(int maxLength) {
    const auto specs = layout(std::max(maxLength, minLength));
    stages.clear();
    for (const auto& sp : specs) {
        Stage st;
        st.fft = std::make_unique<juce::dsp::FFT>(log2i(sp.fftSize));
        const auto bins = static_cast<std::size_t>(sp.fftSize + 2);
        st.ringParts = sp.parts;
        st.fdl.assign(bins * static_cast<std::size_t>(sp.parts), 0.0f);
        st.frame.assign(static_cast<std::size_t>(2 * sp.fftSize), 0.0f);
        st.acc.assign(bins, 0.0f);
        stages.push_back(std::move(st));
    }
    headHistory.assign(2 * headSize, 0.0f);
    const int ringIn = ceilPow2(2 * maxPartition + 8 * headSize);
    const int ringOut = ceilPow2(4 * maxPartition + 2 * headSize);
    inRing.assign(static_cast<std::size_t>(ringIn), 0.0f);
    outRing.assign(static_cast<std::size_t>(ringOut), 0.0f);
    inMask = ringIn - 1;
    outMask = ringOut - 1;
    reset();
}

void Convolver::reset() noexcept {
    std::fill(headHistory.begin(), headHistory.end(), 0.0f);
    std::fill(inRing.begin(), inRing.end(), 0.0f);
    std::fill(outRing.begin(), outRing.end(), 0.0f);
    for (auto& st : stages) {
        std::fill(st.fdl.begin(), st.fdl.end(), 0.0f);
        std::fill(st.acc.begin(), st.acc.end(), 0.0f);
        st.fdlHead = 0;
        st.macDone = 0;
    }
    headPos = 0;
    time = 0;
    blockFill = 0;
}

void Convolver::mac(std::size_t s, int from, int to) noexcept {
    auto& st = stages[s];
    const auto& sp = spectra->stages[s];
    const auto bins = static_cast<std::size_t>(sp.fftSize + 2);
    float* acc = st.acc.data();
    for (int k = from; k < to; ++k) {
        const int slot = (st.fdlHead - k + st.ringParts) % st.ringParts;
        const float* x = &st.fdl[bins * static_cast<std::size_t>(slot)];
        const float* hk = &sp.filters[bins * static_cast<std::size_t>(k)];
        for (std::size_t b = 0; b < bins; b += 2) {
            const float xr = x[b], xi = x[b + 1], hr = hk[b], hi = hk[b + 1];
            acc[b] += xr * hr - xi * hi;
            acc[b + 1] += xr * hi + xi * hr;
        }
    }
}

void Convolver::runSlice(std::size_t s, int slice) noexcept {
    auto& st = stages[s];
    const auto& sp = spectra->stages[s];
    const auto bins = static_cast<std::size_t>(sp.fftSize + 2);
    const int S = sp.slices;
    const int parts = std::min(sp.parts, st.ringParts);
    if (slice == sp.fftSlice) {
        st.fdlHead = (st.fdlHead + 1) % st.ringParts;
        float* f = st.frame.data();
        const long long start = time - static_cast<long long>(sp.fftSlice) * headSize - 2 * sp.partition;
        for (int i = 0; i < sp.fftSize; ++i) f[i] = start + i >= 0 ? inRing[static_cast<std::size_t>((start + i) & inMask)] : 0.0f;
        std::fill(f + sp.fftSize, f + 2 * sp.fftSize, 0.0f);
        st.fft->performRealOnlyForwardTransform(f, true);
        std::memcpy(&st.fdl[bins * static_cast<std::size_t>(st.fdlHead)], f, bins * sizeof(float));
        std::fill(st.acc.begin(), st.acc.end(), 0.0f);
        st.macDone = 0;
        if (S == 1) { mac(s, 0, parts); st.macDone = parts; }
        else return;
    }
    if (slice < sp.fftSlice) return;
    if (S > 1 && slice < sp.ifftSlice) {
        const int workers = std::max(1, sp.ifftSlice - sp.fftSlice - 1);
        const int to = std::min(parts, static_cast<int>(static_cast<long long>(parts) * (slice - sp.fftSlice) / workers));
        if (to > st.macDone) { mac(s, st.macDone, to); st.macDone = to; }
        return;
    }
    if (slice > sp.ifftSlice) return;
    if (st.macDone < parts) { mac(s, st.macDone, parts); st.macDone = parts; }
    float* f = st.frame.data();
    std::memcpy(f, st.acc.data(), bins * sizeof(float));
    std::fill(f + bins, f + 2 * sp.fftSize, 0.0f);
    st.fft->performRealOnlyInverseTransform(f);
    const long long t0 = time - static_cast<long long>(sp.ifftSlice) * headSize;
    const long long dst = t0 - sp.partition + sp.offset;
    for (int i = 0; i < sp.partition; ++i) outRing[static_cast<std::size_t>((dst + i) & outMask)] += f[sp.partition + i];
}

void Convolver::process(const float* in, float* out, int samples) noexcept {
    if (spectra == nullptr || stages.size() != spectra->stages.size()) {
        std::fill(out, out + samples, 0.0f);
        return;
    }
    const float* head = spectra->head.data();
    for (int n = 0; n < samples; ++n) {
        const float x = in[n];
        headHistory[static_cast<std::size_t>(headPos)] = x;
        headHistory[static_cast<std::size_t>(headPos + headSize)] = x;
        headPos = (headPos + 1) & (headSize - 1);
        const float* w = &headHistory[static_cast<std::size_t>(headPos)];
        float y = 0.0f;
        for (int i = 0; i < headSize; ++i) y += w[i] * head[i];
        inRing[static_cast<std::size_t>(time & inMask)] = x;
        float& o = outRing[static_cast<std::size_t>(time & outMask)];
        y += o;
        o = 0.0f;
        out[n] = y;
        ++time;
        if (++blockFill == headSize) {
            blockFill = 0;
            for (std::size_t s = 0; s < stages.size(); ++s)
                runSlice(s, static_cast<int>((time / headSize) % spectra->stages[s].slices));
        }
    }
}
}
