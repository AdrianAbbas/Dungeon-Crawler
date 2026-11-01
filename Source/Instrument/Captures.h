#pragma once
// The developer's effect captures (the private Samples repository: a shimmer patch's
// static take, a dark reverb + delay, a chorus-modulator reverb), turned into impulse
// responses by tools/captures (LIMINAL's capture tools, same developer) and shipped in
// the instrument's bundle (Contents/Resources/Samples/captures). Direction Update 3
// asks for them where they fit: an instrument can send part of its own sound into one of
// them before it enters its place (the Crystal Cavern's bells into the shimmer...), so it
// carries a little of that unit's character with it. The FX does not use them (its
// places are unchanged).
//
// The IRs are read once per process on the first prepare (not the audio thread); each
// instrument instance builds the spectra for its sample rate and runs a zero-latency
// convolver per capture, only while something was sent to it within the IR's length.
#include "../DSP/Convolver.h"
#include "wb/Instruments.h"
#include <array>
#include <memory>
#include <vector>

namespace dc::inst {
enum Capture : int { CaptureNone = 0, CaptureShimmer, CaptureDarkHall, CaptureChorusVerb, CaptureCount };

// The IRs as loaded (48 kHz, unit energy per channel), shared and immutable.
struct CaptureIr { std::vector<float> l, r; double rate = 48000; bool stereo = false; };
const CaptureIr* captureIr(int capture);    // nullptr when not shipped; loads on first use (not real-time safe)
int capturesLoaded();                       // how many of the three were found

class CaptureBus {
public:
    static constexpr int block = 512;
    // Not real-time safe: builds the spectra at this rate.
    void prepare(double sampleRate);
    void reset() noexcept;
    bool available(int capture) const noexcept { return capture > 0 && capture < CaptureCount && slot(capture).ready; }
    // Per sample: what a voice sends (mono), into this block's buffer.
    void send(int capture, int index, float x) noexcept { if (available(capture)) slot(capture).in[std::size_t(index)] += x; }
    // Adds the wet of every active capture for the block's first n samples, then clears
    // the send buffers. n <= block.
    void process(float* l, float* r, int n) noexcept;

private:
    struct Slot {
        bool ready = false;
        std::shared_ptr<const conv::IrSpectra> spectraL, spectraR;
        conv::Convolver convL, convR;
        std::array<float, block> in {}, wetL {}, wetR {}, octave {};
        int quietFor = 0, tail = 0;   // samples since the last send; the IR's length
        wb::dsp::OctaveUp up;         // the shimmer's octave (the static capture has none)
        float octaveLevel = 0;
    };
    Slot& slot(int c) noexcept { return slots[std::size_t(c - 1)]; }
    const Slot& slot(int c) const noexcept { return slots[std::size_t(c - 1)]; }
    std::array<Slot, CaptureCount - 1> slots;
    double rate = 48000;
};
}
