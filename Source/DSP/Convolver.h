#pragma once
// Zero-latency non-uniform partitioned convolution (Direction Update 2, the Vault).
// Adapted from LIMINAL's Source/DSP/Convolver (MIT licence, (c) 2026 LIMINAL
// contributors, same developer): a 128-tap direct-form head, then frequency-domain
// stages of 128, 512, 2048 and 8192 samples, each starting at twice its partition and
// spreading its FFTs and multiply-adds over a whole partition period (no CPU spikes).
//
// DUNGEON CRAWLER change: the IR's spectra (IrSpectra, immutable, built once per
// sample rate and shared by every instance) are separate from the convolution state
// (Convolver). Every IR is at least `minLength` long, so all IRs share the same stage
// layout except the number of 8192-sample parts; a Convolver prepared for the longest
// IR can switch to any IR in real time (setIr after reset, no allocation).
#include <memory>
#include <vector>

namespace juce::dsp { class FFT; }

namespace dc::conv {
inline constexpr int headSize = 128, growth = 4, maxPartition = 8192;
inline constexpr int minLength = 2 * maxPartition + 1;   // the last stage always exists

struct StageSpec {
    int partition = 0, offset = 0, parts = 0, slices = 0, fftSize = 0, fftSlice = 0, ifftSlice = 0;
    std::vector<float> filters;   // parts x (fftSize + 2), interleaved complex
};
struct IrSpectra {
    int length = 0;
    std::vector<float> head;      // the first headSize taps, reversed
    std::vector<StageSpec> stages;
};
// Not real-time safe. `ir` is padded to minLength.
std::shared_ptr<const IrSpectra> buildSpectra(const float* ir, int length);

class Convolver {
public:
    Convolver();
    ~Convolver();
    // Not real-time safe: allocates for IRs of up to `maxLength` samples.
    void prepare(int maxLength);
    void reset() noexcept;
    // Real-time safe; call after reset() (the state starts clean for the new IR).
    void setIr(const IrSpectra* ir) noexcept { spectra = ir; }
    bool hasIr() const noexcept { return spectra != nullptr; }
    // In place allowed. Real-time safe.
    void process(const float* in, float* out, int samples) noexcept;
private:
    struct Stage {
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> fdl, frame, acc;
        int ringParts = 0, fdlHead = 0, macDone = 0;
    };
    void runSlice(std::size_t s, int slice) noexcept;
    void mac(std::size_t s, int fromPart, int toPart) noexcept;
    const IrSpectra* spectra = nullptr;
    std::vector<float> headHistory;
    int headPos = 0;
    std::vector<Stage> stages;
    std::vector<float> inRing, outRing;
    int inMask = 0, outMask = 0;
    long long time = 0;
    int blockFill = 0;
};
}
