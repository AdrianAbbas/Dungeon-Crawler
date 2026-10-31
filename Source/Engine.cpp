#include "Engine.h"
#include <cmath>

namespace dc {

Frame Engine::targetFrame(int location, float intensity) noexcept {
    const auto neutral = neutralFrame();
    const auto place = locationFrame(location);
    Frame f = interpolate(neutral, place, dsp::clamp01(intensity));
    // Choices keep the place's value at any intensity (with the mixes at zero they are
    // inaudible), so Intensity never flips a chamber or an interval half-way.
    for (std::size_t i = 0; i < Count; ++i)
        if (moduleSpecs[i].glide == Glide::Step) f[i] = place[i];
    return f;
}

Frame Engine::interpolate(const Frame& a, const Frame& b, float t) noexcept {
    Frame f {};
    for (std::size_t i = 0; i < Count; ++i) {
        switch (moduleSpecs[i].glide) {
            case Glide::Lin: f[i] = dsp::lerp(a[i], b[i], t); break;
            case Glide::Log: f[i] = a[i] * std::pow(b[i] / a[i], t); break;   // all Log ranges are > 0
            case Glide::Step: f[i] = t < 0.5f ? a[i] : b[i]; break;
        }
    }
    return f;
}

void Engine::prepare(double sampleRate) {
    sr = sampleRate;
    context.sampleRate = sr;
    abyss.prepare(sr); blood.prepare(sr); choir.prepare(sr);
    wraith.prepare(sr); echoes.prepare(sr); crypt.prepare(sr);
    levelMatcher.prepare(sr);
    for (auto& slot : slots) slot.gain.prepare(0.02, sr);
    warmUpSamples = std::max(1, static_cast<int>(0.005 * sr));
    settleSamples = static_cast<int>(0.5 * sr);
    travelStep = static_cast<float>(1.0 / (travelSeconds * sr));
    reset();
}

void Engine::reset() {
    abyss.reset(); blood.reset(); choir.reset(); wraith.reset(); echoes.reset(); crypt.reset();
    levelMatcher.reset();
    for (auto& slot : slots) { slot.gain.value = 0; slot.idle = true; slot.warmUp = 0; }
    target = -1;   // the next process() arrives at its location at once
    travel = 1;
}

template <class Module>
void Engine::runSlot(Module& module, Slot& slot, bool enabled, float* l, float* r, int n) noexcept {
    const float wanted = enabled ? 1.0f : 0.0f;
    if (slot.gain.value == 0 && !enabled) { slot.idle = true; return; }
    if (slot.idle) { module.reset(); slot.idle = false; slot.warmUp = 0; }
    std::copy(l, l + n, pre[0].begin());
    std::copy(r, r + n, pre[1].begin());
    // A module that starts again starts from cleared buffers. Its input fades in, or
    // the hard onset would come back later as a click (the first echo, the first
    // reverb pass).
    for (int i = 0; i < n && slot.warmUp < warmUpSamples; ++i, ++slot.warmUp) {
        const float ramp = 0.5f - 0.5f * std::cos(dsp::twoPi * 0.5f * static_cast<float>(slot.warmUp) / static_cast<float>(warmUpSamples));
        l[i] *= ramp;
        r[i] *= ramp;
    }
    module.process(l, r, n, current, context);
    for (int i = 0; i < n; ++i) {
        const float g = slot.gain.next(wanted);
        const auto k = static_cast<std::size_t>(i);
        l[i] = pre[0][k] + (l[i] - pre[0][k]) * g;
        r[i] = pre[1][k] + (r[i] - pre[1][k]) * g;
    }
}

void Engine::process(float* l, float* r, int n, const float* dryL, const float* dryR,
                     int location, float intensity, double bpm) noexcept {
    context.bpm = bpm;
    location = std::clamp(location, 0, static_cast<int>(LocationCount) - 1);
    intensity = dsp::clamp01(intensity);
    if (target < 0) {
        // First block after prepare: be there already.
        target = location; targetIntensity = intensity;
        current = from = to = targetFrame(location, intensity);
        travel = 1;
    } else if (location != target) {
        // Travel from wherever we are (possibly mid-way) to the new place.
        from = current;
        target = location; targetIntensity = intensity;
        to = targetFrame(location, intensity);
        travel = 0;
    } else if (intensity != targetIntensity) {
        targetIntensity = intensity;
        to = targetFrame(location, intensity);
        if (travel >= 1.0f) current = to;   // the modules smooth the change themselves
    }
    if (travel < 1.0f) {
        travel = std::min(1.0f, travel + travelStep * static_cast<float>(n));
        const float eased = travel * travel * (3.0f - 2.0f * travel);
        current = travel >= 1.0f ? to : interpolate(from, to, eased);
    }

    // A module runs while the current frame asks anything of it; at neutral it is
    // skipped (and starts clean when it is needed again).
    const auto& p = current;
    const bool abyssActive = p[Cutoff] < 19500.0f || p[HighPass] > 21.0f || p[Darkness] > 1.0e-3f || p[Tomb] > 1.0e-3f;
    runSlot(abyss, slots[0], abyssActive, l, r, n);
    blood.process(l, r, n, p, context);
    runSlot(choir, slots[1], p[ChoirBlend] > 1.0e-4f || p[Drone] > 1.0e-4f, l, r, n);
    runSlot(wraith, slots[2], p[WraithMix] > 1.0e-4f, l, r, n);
    runSlot(echoes, slots[3], p[DelayMix] > 1.0e-4f, l, r, n);
    runSlot(crypt, slots[4], p[ReverbMix] > 1.0e-4f, l, r, n);
    // The matcher stays agile while travelling and for half a second after arriving.
    settle = travel < 1.0f ? settleSamples : std::max(0, settle - n);
    levelMatcher.process(dryL, dryR, l, r, n, true, settle > 0);
}
}
