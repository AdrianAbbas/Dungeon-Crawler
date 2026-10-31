#include "Engine.h"
#include <cmath>
#include <cstring>

namespace dc {
namespace {
// Choices whose change cannot be morphed, and the amount that fades their layer.
struct DipGroup { P amount; std::array<P, 3> choices; int count; };
constexpr DipGroup dipGroups[] = {
    { ChoirBlend, { Voices, Interval, Interval }, 2 },
    { Tomb, { TombNote, TombOctave, TombOctave }, 2 },
    { Metal, { MetalKind, MetalNote, MetalOctave }, 3 },
};
bool choicesDiffer(const Frame& a, const Frame& b) noexcept {
    for (std::size_t i = 0; i < Count; ++i)
        if (moduleSpecs[i].glide == Glide::Step && a[i] != b[i]) return true;
    return false;
}
// Whether two frames describe different rooms (anything but the reverb's mix and width).
bool roomsDiffer(const Frame& a, const Frame& b) noexcept {
    for (P id : { Space, Decay, PreDelay, Damping, ReverbLowCut, Shimmer, Character, CryptMode, RiseTime, Eternal })
        if (std::abs(a[id] - b[id]) > 1.0e-3f * std::max(1.0f, std::abs(a[id]))) return true;
    return false;
}
bool dippedChoicesDiffer(const Frame& a, const Frame& b) noexcept {
    for (const auto& g : dipGroups)
        for (int k = 0; k < g.count; ++k)
            if (a[g.choices[static_cast<std::size_t>(k)]] != b[g.choices[static_cast<std::size_t>(k)]]) return true;
    return false;
}
}

Frame Engine::targetFrame(const Controls& c) noexcept {
    const int location = std::clamp(c.location, 0, static_cast<int>(LocationCount) - 1);
    auto place = placeFrame(location, c.sub);
    applyMacros(place, location, c.macros);
    for (std::size_t i = 0; i < Count; ++i) {
        const auto& spec = moduleSpecs[i];
        const float a = c.advanced[i];
        if (a == 0.0f || !std::isfinite(a)) continue;
        switch (spec.glide) {
            case Glide::Lin: place[i] += std::clamp(a, -1.0f, 1.0f) * (spec.max - spec.min); break;
            case Glide::Log: place[i] *= std::pow(spec.max / spec.min, std::clamp(a, -1.0f, 1.0f)); break;
            case Glide::Step: place[i] = spec.min + std::round(a) - 1.0f; break;
        }
        place[i] = std::clamp(place[i], spec.min, spec.max);
    }
    Frame f = interpolate(neutralFrame(), place, dsp::clamp01(c.intensity));
    // Choices keep the place's value at any intensity (with the mixes at zero they are
    // inaudible), so Intensity never flips a chamber or an interval half-way.
    for (std::size_t i = 0; i < Count; ++i)
        if (moduleSpecs[i].glide == Glide::Step) f[i] = place[i];
    return f;
}

Frame Engine::targetFrame(int location, float intensity) noexcept {
    Controls c;
    c.location = location;
    c.intensity = intensity;
    return targetFrame(c);
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

void Engine::applyDips(Frame& f, const Frame& a, const Frame& b, float t, double seconds) noexcept {
    // Silent for ~80 ms on each side of the switch (the modules smooth their amounts
    // over 40 ms), fading over ~120 ms around that.
    const float plateau = static_cast<float>(std::min(0.2, 0.08 / std::max(1.0e-3, seconds)));
    const float width = std::max(1.0e-3f, std::min(0.5f - plateau, static_cast<float>(0.12 / std::max(1.0e-3, seconds))));
    for (const auto& g : dipGroups) {
        bool differ = false;
        for (int k = 0; k < g.count; ++k) differ = differ || a[g.choices[static_cast<std::size_t>(k)]] != b[g.choices[static_cast<std::size_t>(k)]];
        if (!differ) continue;
        float d = std::clamp((std::abs(t - 0.5f) - plateau) / width, 0.0f, 1.0f);
        d = d * d * (3.0f - 2.0f * d);
        f[g.amount] *= d;
    }
}

void Engine::prepare(double sampleRate) {
    sr = sampleRate;
    context.sampleRate = sr;
    abyss.prepare(sr); blood.prepare(sr); relics.prepare(sr); choir.prepare(sr);
    wraith.prepare(sr); echoes.prepare(sr);
    for (auto& sig : signatures) sig.prepare(sr);
    for (auto& room : rooms) { room.prepare(sr); room.setWetOnly(true); }
    roomDry.setTime(0.04, sr);
    levelMatcher.prepare(sr);
    for (auto& slot : slots) slot.gain.prepare(0.02, sr);
    for (auto& slot : sigSlots) slot.gain.prepare(0.02, sr);
    trimGain.setTime(0.05, sr);
    warmUpSamples = std::max(1, static_cast<int>(0.005 * sr));
    settleSamples = static_cast<int>(0.5 * sr);
    reset();
}

void Engine::reset() {
    abyss.reset(); blood.reset(); relics.reset(); choir.reset(); wraith.reset(); echoes.reset();
    for (auto& sig : signatures) sig.reset();
    for (auto& slot : sigSlots) { slot.gain.value = 0; slot.idle = true; slot.warmUp = 0; }
    activeSig = 0; departingSig = -1; sigCrossfade = false;
    for (auto& room : rooms) room.reset();
    roomIdle.fill(true);
    activeRoom = 0; departingRoom = -1; roomCrossfade = false;
    roomDry.snap(1.0f);
    levelMatcher.reset();
    for (auto& slot : slots) { slot.gain.value = 0; slot.idle = true; slot.warmUp = 0; }
    place = {};   // the next setControls() arrives at its place at once
    travel = 1;
}

void Engine::swapSignature() noexcept {
    // The old kind rings out in its instance; the new one starts clean in the other.
    sigFromAmount = current[SigAmount];
    sigFrames[static_cast<std::size_t>(activeSig)] = current;
    if (!sigSlots[static_cast<std::size_t>(activeSig)].idle) {
        signatures[static_cast<std::size_t>(activeSig)].depart();
        departingSig = activeSig;
    } else departingSig = -1;
    activeSig = 1 - activeSig;
    auto& fresh = sigSlots[static_cast<std::size_t>(activeSig)];
    fresh.idle = true;   // starts clean (runSignatures resets it)
    sigCrossfade = true;
}

void Engine::startTravel(double seconds) noexcept {
    if (current[SigKind] != to[SigKind]) swapSignature();
    from = current;
    trimFrom = trimDb;
    duration = std::max(minTravelSeconds, seconds);
    if (dippedChoicesDiffer(from, to)) duration = std::max(duration, crossfadeSeconds);
    travel = 0;
}

void Engine::setControls(const Controls& c) noexcept {
    context.bpm = c.bpm;
    const int location = std::clamp(c.location, 0, static_cast<int>(LocationCount) - 1);
    const Place wanted { location, clampSubLocation(location, c.sub) };
    const Frame target = targetFrame(c);
    const float trim = placeTrimDb(wanted.location, wanted.sub) * dsp::clamp01(c.intensity);
    if (place.location < 0) {
        // First block after prepare: be there already.
        place = wanted;
        current = from = to = target;
        trimDb = trimFrom = trimTo = trim;
        trimGain.snap(dsp::dbToGain(trim));
        travel = 1;
        return;
    }
    const bool frameChanged = std::memcmp(target.data(), to.data(), sizeof(Frame)) != 0;
    if (wanted != place) {
        // Travel from wherever we are (possibly mid-way) to the new place.
        place = wanted;
        to = target;
        trimTo = trim;
        startTravel(c.travelSeconds);
        if (roomsDiffer(current, to) && to[ReverbMix] > 1.0e-4f) {
            // A new room for the new place, in the quietest other instance.
            int quietest = -1;
            for (int k = 0; k < roomCount; ++k)
                if (k != activeRoom && (quietest < 0 || rooms[static_cast<std::size_t>(k)].level() < rooms[static_cast<std::size_t>(quietest)].level())) quietest = k;
            departingRoom = activeRoom;
            activeRoom = quietest;
            roomIdle[static_cast<std::size_t>(activeRoom)] = true;   // starts clean
            roomCrossfade = true;
        }
    } else if (frameChanged || trim != trimTo) {
        // Macros, Advanced or Intensity: the matcher follows quickly; continuous values
        // are smoothed by the modules, choices switch through a short crossfade.
        settle = settleSamples;
        to = target;
        trimTo = trim;
        if (travel >= 1.0f) {
            if (choicesDiffer(current, to)) startTravel(0.05);
            else { current = to; trimDb = trimTo; }
        }
    }
}

void Engine::runRooms(float* l, float* r, int n, float eased) noexcept {
    const float m = current[ReverbMix];
    if (roomCrossfade && travel >= 1.0f) { roomCrossfade = false; departingRoom = -1; }
    bool any = false;
    std::fill(roomSum[0].begin(), roomSum[0].begin() + n, 0.0f);
    std::fill(roomSum[1].begin(), roomSum[1].begin() + n, 0.0f);
    std::copy(l, l + n, roomIn[0].begin());
    std::copy(r, r + n, roomIn[1].begin());
    for (int k = 0; k < roomCount; ++k) {
        const auto K = static_cast<std::size_t>(k);
        Frame& f = roomFrames[K];
        if (k == activeRoom) {
            if (roomCrossfade) { f = to; f[ReverbMix] = m * eased; }   // the destination's room, fading in
            else f = current;
        } else if (k == departingRoom && roomCrossfade) {
            f[ReverbMix] = m * (1.0f - eased);                         // the old room, fading out
        } else {
            f[ReverbMix] = 0.0f;                                       // ringing out, no new input
        }
        auto& room = rooms[K];
        if (f[ReverbMix] <= 1.0e-4f && !room.ringing() && !roomIdle[K]) { roomIdle[K] = true; continue; }
        if (f[ReverbMix] <= 1.0e-4f && roomIdle[K]) continue;
        if (roomIdle[K]) { room.reset(); roomIdle[K] = false; }
        std::copy(roomIn[0].begin(), roomIn[0].begin() + n, roomWet[0].begin());
        std::copy(roomIn[1].begin(), roomIn[1].begin() + n, roomWet[1].begin());
        room.process(roomWet[0].data(), roomWet[1].data(), n, f, context);
        for (int i = 0; i < n; ++i) {
            roomSum[0][static_cast<std::size_t>(i)] += roomWet[0][static_cast<std::size_t>(i)];
            roomSum[1][static_cast<std::size_t>(i)] += roomWet[1][static_cast<std::size_t>(i)];
        }
        any = true;
    }
    roomDry.set(std::sqrt(1.0f - dsp::clamp01(m)));
    if (!any && roomDry.value == 1.0f && roomDry.target == 1.0f) return;
    for (int i = 0; i < n; ++i) {
        const auto k = static_cast<std::size_t>(i);
        float g = roomDry.next();
        if (std::abs(g - roomDry.target) < 1.0e-6f) g = roomDry.value = roomDry.target;
        l[i] = roomIn[0][k] * g + roomSum[0][k];
        r[i] = roomIn[1][k] * g + roomSum[1][k];
    }
}

void Engine::runSignatures(float* l, float* r, int n, float eased) noexcept {
    if (sigCrossfade && travel >= 1.0f) sigCrossfade = false;
    // The departing instance first (its serial part fading out, its tails ringing on),
    // then the active one (fading in during a journey).
    if (departingSig >= 0) {
        const auto D = static_cast<std::size_t>(departingSig);
        auto& f = sigFrames[D];
        f[SigAmount] = sigCrossfade ? sigFromAmount * (1.0f - eased) : 0.0f;
        auto& sig = signatures[D];
        if (f[SigAmount] <= 1.0e-4f && !sig.ringing()) { sigSlots[D].idle = true; sigSlots[D].gain.value = 0; departingSig = -1; }
        else {
            sigSlots[D].gain.value = 1.0f;
            runSlot(sig, sigSlots[D], true, l, r, n, &f);
        }
    }
    const auto A = static_cast<std::size_t>(activeSig);
    auto& f = sigFrames[A];
    if (sigCrossfade) { f = to; f[SigAmount] = to[SigAmount] * eased; }
    else f = current;
    runSlot(signatures[A], sigSlots[A], f[SigAmount] > 1.0e-4f, l, r, n, &f);
}

template <class Module>
void Engine::runSlot(Module& module, Slot& slot, bool enabled, float* l, float* r, int n, const Frame* frame) noexcept {
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
    module.process(l, r, n, frame != nullptr ? *frame : current, context);
    for (int i = 0; i < n; ++i) {
        const float g = slot.gain.next(wanted);
        const auto k = static_cast<std::size_t>(i);
        l[i] = pre[0][k] + (l[i] - pre[0][k]) * g;
        r[i] = pre[1][k] + (r[i] - pre[1][k]) * g;
    }
}

void Engine::process(float* l, float* r, int n, const float* dryL, const float* dryR) noexcept {
    if (place.location < 0) { Controls c; setControls(c); }
    if (travel < 1.0f) {
        travel = std::min(1.0f, travel + static_cast<float>(n / (duration * sr)));
        const float eased = travel * travel * (3.0f - 2.0f * travel);
        if (travel >= 1.0f) { current = to; trimDb = trimTo; }
        else {
            current = interpolate(from, to, eased);
            applyDips(current, from, to, travel, duration);
            trimDb = dsp::lerp(trimFrom, trimTo, eased);
        }
    }

    // A module runs while the current frame asks anything of it; at neutral it is
    // skipped (and starts clean when it is needed again). The Echoes and the Crypt
    // keep running while their tails ring after their mix went to zero.
    const auto& p = current;
    const bool abyssActive = p[Cutoff] < 19500.0f || p[HighPass] > 21.0f || p[Darkness] > 1.0e-3f || p[Tomb] > 1.0e-3f;
    const bool relicsActive = p[Flicker] > 1.0e-4f || p[Metal] > 1.0e-4f || p[Wind] > 1.0e-4f || p[Drips] > 1.0e-4f;
    const bool echoesActive = p[DelayMix] > 1.0e-4f || echoes.ringing();
    bool cryptActive = p[ReverbMix] > 1.0e-4f;
    for (const auto& room : rooms) cryptActive = cryptActive || room.ringing();
    const bool wetActive = p[ChoirBlend] > 1.0e-4f || p[Drone] > 1.0e-4f || p[WraithMix] > 1.0e-4f
                        || echoesActive || cryptActive || p[BloodMix] > 1.0e-4f || relicsActive || p[SigAmount] > 1.0e-4f
                        || departingSig >= 0 || to[SigAmount] > 1.0e-4f;
    runSlot(abyss, slots[0], abyssActive, l, r, n);
    blood.process(l, r, n, p, context);
    runSlot(relics, slots[1], relicsActive, l, r, n);
    runSlot(choir, slots[2], p[ChoirBlend] > 1.0e-4f || p[Drone] > 1.0e-4f, l, r, n);
    runSlot(wraith, slots[3], p[WraithMix] > 1.0e-4f, l, r, n);
    runSignatures(l, r, n, travel * travel * (3.0f - 2.0f * travel));
    runSlot(echoes, slots[4], echoesActive, l, r, n);
    runRooms(l, r, n, travel * travel * (3.0f - 2.0f * travel));
    if (!matching) return;
    trimGain.set(dsp::dbToGain(trimDb));
    for (int i = 0; i < n; ++i) { const float g = trimGain.next(); l[i] *= g; r[i] *= g; }
    // The matcher stays agile while travelling and for half a second after arriving.
    // At neutral nothing changes the level, so the matcher returns to unity.
    settle = travel < 1.0f ? settleSamples : std::max(0, settle - n);
    levelMatcher.process(dryL, dryR, l, r, n, abyssActive || wetActive, settle > 0);
}
}
