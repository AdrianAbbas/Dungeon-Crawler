#include "Engine.h"
#include "Instrument/SampleBank.h"
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
    if (c.intensity > 1.0f) strengthen(f, place, std::min(c.intensity, maxIntensity));
    // Session 11: the MIX section and the reverb pre-delay offset (identity at defaults).
    applyMix(f, c.mix);
    if (c.reverbPreDelay != 0.0f && std::isfinite(c.reverbPreDelay))
        f[PreDelay] = std::clamp(f[PreDelay] + c.reverbPreDelay, moduleSpecs[PreDelay].min, moduleSpecs[PreDelay].max);
    return f;
}

int Engine::signatureGroup(int kind) noexcept {
    switch (kind) {
        case 1: case 2: case 6: case 7: case 10: case 11: case 15: case 25: case 30: case 31: case 37: return MixEchoes;
        case 4: case 5: case 9: case 16: case 17: case 18: case 23: case 28: case 29: case 32: case 39: case 40: return MixModulation;
        case 12: case 35: return MixReverb;
        default: return MixCharacter;
    }
}

int Engine::mixGroupOf(P id, const Frame& frame) noexcept {
    if (id == SigAmount) return signatureGroup(static_cast<int>(std::lround(frame[SigKind])));
    switch (id) {
        case ReverbMix: case VaultMix: return MixReverb;
        case DelayMix: case TapeMix: return MixEchoes;
        case EnsMix: case Spiral: case Shimmer: case VaultShimmer: case FilterMovement: case Instability: case WraithMix:
        case ChoirBlend: case Drone: case VoiceBloom: case Flicker: return MixModulation;
        case Darkness: case Tomb: case Drive: case Age: case Dust: case BloodMix: case Metal: case Wind: case Drips: case Drops:
        case VoiceImprint: case Whispers: case Resonance: case Cutoff: case HighPass: return MixCharacter;
        default: return -1;
    }
}

// The MIX section (Session 11): every amount of a group moves along the line from neutral
// to the frame (filters geometrically), as Intensity does for the whole place; above 1 it
// goes further, bounded like the Effects roller (strengthen): the tails (Decay, Space,
// Feedback) only grow above 1 and never shrink below it, so turning a group down only
// lowers its share. At 1 nothing changes (the frame is returned as it was).
void Engine::applyMix(Frame& f, const std::array<float, 4>& mix) noexcept {
    bool any = false;
    for (float m : mix) any = any || m != 1.0f;
    if (!any) return;
    const auto neutral = neutralFrame();
    const Frame designed = f;
    const auto along = [&](P id, float t, float limit) {
        const auto& spec = moduleSpecs[id];
        const float a = neutral[id], b = designed[id];
        float v = spec.glide == Glide::Log ? a * std::pow(b / a, t) : a + (b - a) * t;
        if (t > 1.0f) v = std::min(v, std::max(std::min(limit, spec.max), std::min(b, spec.max)));
        f[id] = std::clamp(v, spec.min, spec.max);
    };
    for (std::size_t i = 0; i < Count; ++i) {
        const int g = mixGroupOf(static_cast<P>(i), designed);
        if (g < 0) continue;
        const float t = std::clamp(std::isfinite(mix[static_cast<std::size_t>(g)]) ? mix[static_cast<std::size_t>(g)] : 1.0f, 0.0f, 1.5f);
        if (t != 1.0f) along(static_cast<P>(i), t, moduleSpecs[i].max);
    }
    const float reverb = std::clamp(mix[MixReverb], 0.0f, 1.5f), echoes = std::clamp(mix[MixEchoes], 0.0f, 1.5f);
    if (reverb > 1.0f) { along(Decay, reverb, designed[Decay] * 1.5f); along(Space, reverb, 1.0f); }
    if (echoes > 1.0f) { along(Feedback, echoes, 0.85f); along(TapeFeedback, echoes, 0.8f); }
}

// The Effects roller above 100 % (Session 9): the place's wet amounts, sends, character
// amounts and tails go further along the way from neutral to the place (t > 1); times,
// rates, pitches, filters and choices stay as designed. Bounded: feedback stays below
// where the echoes would build up, decays below 1.6 times the place's own.
void Engine::strengthen(Frame& f, const Frame& place, float t) noexcept {
    static constexpr P amounts[] = { Darkness, FilterMovement, Tomb, Drive, Age, Dust, Instability, BloodMix, ChoirBlend, Drone,
                                     Ghost, WraithMix, DelayMix, Shimmer, ReverbMix, Flicker, Metal, Wind, Drips, SigAmount,
                                     VaultMix, VaultShimmer, EnsMix, Spiral, TapeMix, Drops, VoiceImprint, Whispers, VoiceBloom };
    const auto neutral = neutralFrame();
    const auto further = [&](P id, float limit) {
        const auto& spec = moduleSpecs[id];
        const float a = neutral[id], b = place[id];
        float v = spec.glide == Glide::Log ? a * std::pow(b / a, t) : a + (b - a) * t;
        v = std::clamp(v, spec.min, std::max(std::min(limit, spec.max), std::min(b, spec.max)));
        f[id] = std::clamp(v, spec.min, spec.max);
    };
    for (P id : amounts) further(id, moduleSpecs[id].max);
    further(Space, 1.0f);
    further(Feedback, 0.85f);
    further(TapeFeedback, 0.8f);
    further(Decay, place[Decay] * 1.6f);
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
    wraith.prepare(sr); echoes.prepare(sr); ensemble.prepare(sr);
    for (auto& v : vaults) v.prepare(sr);
    for (auto& v : vaultSend) v.setTime(0.03, sr);
    vaultDry.setTime(0.04, sr);
    for (auto& sig : signatures) sig.prepare(sr);
    for (auto& room : rooms) { room.prepare(sr); room.setWetOnly(true); }
    roomDry.setTime(0.04, sr);
    levelMatcher.prepare(sr);
    for (auto& slot : slots) slot.gain.prepare(0.02, sr);
    for (auto& slot : sigSlots) slot.gain.prepare(0.02, sr);
    trimGain.setTime(0.05, sr);
    for (auto& d : vaultDelay) d.prepare(static_cast<std::size_t>(0.13 * sr) + 8);
    vaultDelayNow.setTime(0.06, sr);
    duckAttack = dsp::timeCoefficient(0.008, sr);
    duckRelease = dsp::timeCoefficient(0.35, sr);
    duckSmooth = dsp::timeCoefficient(0.02, sr);
    warmUpSamples = std::max(1, static_cast<int>(0.005 * sr));
    settleSamples = static_cast<int>(0.5 * sr);
    reset();
}

void Engine::reset() {
    abyss.reset(); blood.reset(); relics.reset(); choir.reset(); wraith.reset(); echoes.reset(); ensemble.reset();
    for (auto& v : vaults) v.reset();
    vaultIdle.fill(true);
    for (auto& v : vaultSend) v.snap(0.0f);
    activeVault = 0; vaultCrossfade = false;
    vaultDry.snap(1.0f);
    for (auto& sig : signatures) sig.reset();
    for (auto& slot : sigSlots) { slot.gain.value = 0; slot.idle = true; slot.warmUp = 0; }
    activeSig = 0; latestDeparting = -1; sigCrossfade = false; sigDeparting.fill(false);
    for (auto& room : rooms) room.reset();
    roomIdle.fill(true);
    activeRoom = 0; departingRoom = -1; roomCrossfade = false;
    roomDry.snap(1.0f);
    for (auto& d : vaultDelay) d.clear();
    vaultDelayNow.snap(preDelayTarget > 0.0f ? preDelayTarget * 0.001f * static_cast<float>(sr) : 0.0f);
    for (auto& pair : lowCut) for (auto& f : pair) f.clear();
    lowCutNow.fill(lowCutTarget);
    duckEnv = 0;
    duckDepth = duckAmount;
    levelMatcher.reset();
    for (auto& slot : slots) { slot.gain.value = 0; slot.idle = true; slot.warmUp = 0; }
    place = {};   // the next setControls() arrives at its place at once
    travel = 1;
}

void Engine::swapSignature() noexcept {
    // The old kind rings out in its instance; the new one starts clean in an idle one
    // (or, if all ring, the quietest).
    sigFromAmount = current[SigAmount];
    const auto A = static_cast<std::size_t>(activeSig);
    latestDeparting = -1;
    if (!sigSlots[A].idle) {
        sigFrames[A] = current;
        signatures[A].depart();
        sigDeparting[A] = true;
        latestDeparting = activeSig;
    }
    int best = -1;
    for (int k = 0; k < sigCount && best < 0; ++k)
        if (k != activeSig && sigSlots[static_cast<std::size_t>(k)].idle) best = k;
    if (best < 0)
        for (int k = 0; k < sigCount; ++k)
            if (k != activeSig && (best < 0 || signatures[static_cast<std::size_t>(k)].tailLevel() < signatures[static_cast<std::size_t>(best)].tailLevel())) best = k;
    activeSig = best;
    const auto B = static_cast<std::size_t>(best);
    sigDeparting[B] = false;
    sigSlots[B].idle = true;   // starts clean (runSignatures resets it)
    sigSlots[B].gain.value = 0;
    sigCrossfade = true;
}

void Engine::startTravel(double seconds) noexcept {
    if (current[SigKind] != to[SigKind]) swapSignature();
    if (current[VaultKind] != to[VaultKind] && to[VaultKind] > 0.5f) {
        // The new space starts clean in the other (quieter) instance.
        vaultFromSend = vaultSend[static_cast<std::size_t>(activeVault)].target;
        activeVault = 1 - activeVault;
        vaultIdle[static_cast<std::size_t>(activeVault)] = true;
        vaultCrossfade = true;
    }
    from = current;
    trimFrom = trimDb;
    duration = std::max(minTravelSeconds, seconds);
    if (dippedChoicesDiffer(from, to)) duration = std::max(duration, crossfadeSeconds);
    travel = 0;
}

void Engine::setControls(const Controls& c) noexcept {
    context.bpm = c.bpm;
    lowCutTarget = std::isfinite(c.reverbLowCut) ? std::clamp(c.reverbLowCut, 20.0f, 1000.0f) : 20.0f;
    preDelayTarget = std::isfinite(c.reverbPreDelay) ? std::clamp(c.reverbPreDelay, -250.0f, 120.0f) : 0.0f;
    duckAmount = std::isfinite(c.reverbDuck) ? dsp::clamp01(c.reverbDuck) : 0.0f;
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
        if (roomIdle[K]) { room.reset(); roomIdle[K] = false; roomWarmUp[K] = 0; }
        std::copy(roomIn[0].begin(), roomIn[0].begin() + n, roomWet[0].begin());
        std::copy(roomIn[1].begin(), roomIn[1].begin() + n, roomWet[1].begin());
        // A room that starts again starts from cleared lines: its input fades in over
        // 5 ms, or the hard onset comes back later as a click (early reflections, the
        // first loop pass, Rising mode's reversed segments). As runSlot does.
        auto& warm = roomWarmUp[K];
        for (int i = 0; i < n && warm < warmUpSamples; ++i, ++warm) {
            const float ramp = 0.5f - 0.5f * std::cos(dsp::twoPi * 0.5f * static_cast<float>(warm) / static_cast<float>(warmUpSamples));
            roomWet[0][static_cast<std::size_t>(i)] *= ramp;
            roomWet[1][static_cast<std::size_t>(i)] *= ramp;
        }
        room.process(roomWet[0].data(), roomWet[1].data(), n, f, context);
        for (int i = 0; i < n; ++i) {
            roomSum[0][static_cast<std::size_t>(i)] += roomWet[0][static_cast<std::size_t>(i)];
            roomSum[1][static_cast<std::size_t>(i)] += roomWet[1][static_cast<std::size_t>(i)];
        }
        any = true;
    }
    roomDry.set(std::sqrt(1.0f - dsp::clamp01(m)));
    if (!any && roomDry.value == 1.0f && roomDry.target == 1.0f) return;
    if (any) shapeWet(roomSum, n, 0);
    for (int i = 0; i < n; ++i) {
        const auto k = static_cast<std::size_t>(i);
        float g = roomDry.next();
        if (std::abs(g - roomDry.target) < 1.0e-6f) g = roomDry.value = roomDry.target;
        l[i] = roomIn[0][k] * g + roomSum[0][k];
        r[i] = roomIn[1][k] * g + roomSum[1][k];
    }
}

void Engine::runVaults(float* l, float* r, int n, float eased) noexcept {
    if (vaultCrossfade && travel >= 1.0f) vaultCrossfade = false;
    const float m = dsp::clamp01(current[VaultMix]);
    const int kindNow = static_cast<int>(std::lround(vaultCrossfade ? to[VaultKind] : current[VaultKind]));
    bool any = false;
    std::copy(l, l + n, vaultIn[0].begin());
    std::copy(r, r + n, vaultIn[1].begin());
    std::fill(vaultSum[0].begin(), vaultSum[0].begin() + n, 0.0f);
    std::fill(vaultSum[1].begin(), vaultSum[1].begin() + n, 0.0f);
    for (int k = 0; k < vaultCount; ++k) {
        const auto K = static_cast<std::size_t>(k);
        auto& v = vaults[K];
        float send = 0;
        if (k == activeVault) send = std::sqrt(vaultCrossfade ? dsp::clamp01(to[VaultMix]) * eased : m);
        else if (vaultCrossfade) send = vaultFromSend * (1.0f - eased);
        vaultSend[K].set(send);
        if (send <= 1.0e-4f && vaultSend[K].value <= 1.0e-4f && (vaultIdle[K] || !v.ringing())) { vaultIdle[K] = true; continue; }
        if (vaultIdle[K]) {
            v.reset();
            v.setKind(k == activeVault ? kindNow : v.kindNow());
            vaultIdle[K] = false;
            vaultWarmUp[K] = 0;
            vaultSend[K].snap(0.0f);
        }
        if (k == activeVault && v.kindNow() != kindNow && kindNow > 0) { v.reset(); v.setKind(kindNow); }
        auto& warm = vaultWarmUp[K];
        for (int i = 0; i < n; ++i) {
            float g = vaultSend[K].next();
            if (warm < warmUpSamples) { g *= 0.5f - 0.5f * std::cos(dsp::twoPi * 0.5f * static_cast<float>(warm) / static_cast<float>(warmUpSamples)); ++warm; }
            vaultWet[0][static_cast<std::size_t>(i)] = vaultIn[0][static_cast<std::size_t>(i)] * g;
            vaultWet[1][static_cast<std::size_t>(i)] = vaultIn[1][static_cast<std::size_t>(i)] * g;
        }
        v.process(vaultWet[0].data(), vaultWet[1].data(), vaultWet[0].data(), vaultWet[1].data(), n, k == activeVault ? (vaultCrossfade ? to : current) : current);
        for (int i = 0; i < n; ++i) {
            vaultSum[0][static_cast<std::size_t>(i)] += vaultWet[0][static_cast<std::size_t>(i)];
            vaultSum[1][static_cast<std::size_t>(i)] += vaultWet[1][static_cast<std::size_t>(i)];
        }
        any = true;
    }
    vaultDry.set(std::sqrt(1.0f - m));
    if (!any && vaultDry.value == 1.0f && vaultDry.target == 1.0f) return;
    // What the rooms' helpers take away passes the vaults' dry path as their signal does.
    const bool carried = removedAny;
    std::array<std::array<float, maxChunk>, 2> carriedIn;
    if (carried) {
        std::copy(removed[0].begin(), removed[0].begin() + n, carriedIn[0].begin());
        std::copy(removed[1].begin(), removed[1].begin() + n, carriedIn[1].begin());
        std::fill(removed[0].begin(), removed[0].begin() + n, 0.0f);
        std::fill(removed[1].begin(), removed[1].begin() + n, 0.0f);
    }
    if (any) shapeWet(vaultSum, n, 1);
    for (int i = 0; i < n; ++i) {
        const auto k = static_cast<std::size_t>(i);
        float g = vaultDry.next();
        if (std::abs(g - vaultDry.target) < 1.0e-6f) g = vaultDry.value = vaultDry.target;
        l[i] = vaultIn[0][k] * g + vaultSum[0][k];
        r[i] = vaultIn[1][k] * g + vaultSum[1][k];
        if (carried) { removed[0][k] += carriedIn[0][k] * g; removed[1][k] += carriedIn[1][k] * g; }
    }
}

void Engine::shapeWet(std::array<std::array<float, maxChunk>, 2>& wet, int n, int which) noexcept {
    const auto W = static_cast<std::size_t>(which);
    const bool helpers = lowCutTarget > 20.5f || lowCutNow[W] > 20.5f || ducking;
    std::array<std::array<float, maxChunk>, 2> before;
    if (helpers) {
        std::copy(wet[0].begin(), wet[0].begin() + n, before[0].begin());
        std::copy(wet[1].begin(), wet[1].begin() + n, before[1].begin());
    }
    // The vaults' pre-delay offset (the rooms take theirs in the frame's Pre-Delay). At 0
    // the signal passes untouched; the line is always fed so a change has history.
    if (which == 1) {
        vaultDelayNow.set(std::max(0.0f, preDelayTarget) * 0.001f * static_cast<float>(sr));
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            vaultDelay[0].push(wet[0][k]);
            vaultDelay[1].push(wet[1][k]);
            const float d = vaultDelayNow.next();
            if (d >= 0.5f) {
                wet[0][k] = vaultDelay[0].read(1.0f + d);
                wet[1][k] = vaultDelay[1].read(1.0f + d);
            } else if (d > 1.0e-6f) {
                // Just leaving 0: crossfade into the line over its first half sample.
                wet[0][k] += 2.0f * d * (vaultDelay[0].read(1.5f) - wet[0][k]);
                wet[1][k] += 2.0f * d * (vaultDelay[1].read(1.5f) - wet[1][k]);
            } else vaultDelayNow.value = 0.0f;
        }
    }
    // The low cut: 12 dB per octave, its corner gliding (no zipper); off at 20 Hz.
    auto& now = lowCutNow[W];
    if (lowCutTarget > 20.5f || now > 20.5f) {
        now += 0.25f * (lowCutTarget - now);
        if (now <= 20.5f && lowCutTarget <= 20.5f) { now = 20.0f; for (auto& f : lowCut[W]) f.clear(); }
        else {
            for (auto& f : lowCut[W]) f.set(now, sr, 1.4142f);
            for (int i = 0; i < n; ++i) {
                const auto k = static_cast<std::size_t>(i);
                wet[0][k] = lowCut[W][0].highPass(wet[0][k]);
                wet[1][k] = lowCut[W][1].highPass(wet[1][k]);
            }
        }
    }
    // The duck (gains from the input, computed at the start of process()).
    if (ducking)
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            wet[0][k] *= duckGain[k];
            wet[1][k] *= duckGain[k];
        }
    if (helpers) {
        // The wet goes on untouched; what the helpers take away is subtracted at the end,
        // after the level matcher has measured (so they never move its gain).
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            removed[0][k] += before[0][k] - wet[0][k];
            removed[1][k] += before[1][k] - wet[1][k];
            wet[0][k] = before[0][k];
            wet[1][k] = before[1][k];
        }
        removedAny = true;
    }
}

void Engine::runSignatures(float* l, float* r, int n, float eased) noexcept {
    if (sigCrossfade && travel >= 1.0f) sigCrossfade = false;
    // Departing instances first (their serial part fading out, their tails ringing on),
    // then the active one (fading in during a journey).
    for (int k = 0; k < sigCount; ++k) {
        const auto K = static_cast<std::size_t>(k);
        if (k == activeSig || !sigDeparting[K]) continue;
        auto& f = sigFrames[K];
        f[SigAmount] = sigCrossfade && k == latestDeparting ? sigFromAmount * (1.0f - eased) : 0.0f;
        auto& sig = signatures[K];
        if (f[SigAmount] <= 1.0e-4f && !sig.ringing()) {
            sigSlots[K].idle = true; sigSlots[K].gain.value = 0; sigDeparting[K] = false;
            if (latestDeparting == k) latestDeparting = -1;
            continue;
        }
        sigSlots[K].gain.value = 1.0f;
        runSlot(sig, sigSlots[K], true, l, r, n, &f);
    }
    const auto A = static_cast<std::size_t>(activeSig);
    auto& f = sigFrames[A];
    if (sigCrossfade) { f = to; f[SigAmount] = to[SigAmount] * eased; }
    else f = current;
    runSlot(signatures[A], sigSlots[A], f[SigAmount] > 1.0e-4f, l, r, n, &f);
}

bool Engine::anyDeparting() const noexcept {
    for (bool d : sigDeparting) if (d) return true;
    return false;
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

void Engine::process(float* l, float* r, int n, const float* dryL, const float* dryR, const float* addL, const float* addR) noexcept {
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

    if (removedAny) {
        std::fill(removed[0].begin(), removed[0].begin() + n, 0.0f);
        std::fill(removed[1].begin(), removed[1].begin() + n, 0.0f);
        removedAny = false;
    }
    // Session 11: the reverb duck follows the input (what is played): its tails dip by
    // up to 18 dB while the input is loud (from -45 dBFS, fully at -15) and bloom again
    // in the gaps (8 ms down, 350 ms back up).
    ducking = duckAmount > 1.0e-4f || duckDepth > 1.0e-5f;
    if (ducking) {
        for (int i = 0; i < n; ++i) {
            const float x = std::max(std::abs(l[i]), std::abs(r[i]));
            duckEnv += (x > duckEnv ? duckAttack : duckRelease) * (x - duckEnv);
            duckDepth += duckSmooth * (duckAmount - duckDepth);   // the amount itself glides (20 ms)
            const float db = 20.0f * std::log10(std::max(duckEnv, 1.0e-6f));
            duckGain[static_cast<std::size_t>(i)] = dsp::dbToGain(-18.0f * duckDepth * dsp::clamp01((db + 45.0f) / 30.0f));
        }
        if (duckAmount <= 1.0e-4f && duckDepth <= 1.0e-5f) duckDepth = 0.0f;
    }
    // A module runs while the current frame asks anything of it; at neutral it is
    // skipped (and starts clean when it is needed again). The Echoes and the Crypt
    // keep running while their tails ring after their mix went to zero.
    const auto& p = current;
    const bool abyssActive = p[Cutoff] < 19500.0f || p[HighPass] > 21.0f || p[Darkness] > 1.0e-3f || p[Tomb] > 1.0e-3f;
    const bool relicsActive = p[Flicker] > 1.0e-4f || p[Metal] > 1.0e-4f || p[Wind] > 1.0e-4f || p[Drips] > 1.0e-4f || p[Whispers] > 1.0e-4f;
    const bool echoesActive = p[DelayMix] > 1.0e-4f || echoes.ringing();
    const bool ensembleActive = p[EnsMix] > 1.0e-4f || p[Spiral] > 1.0e-4f || p[TapeMix] > 1.0e-4f || p[Drops] > 1.0e-4f || ensemble.ringing();
    bool vaultActive = p[VaultMix] > 1.0e-4f || to[VaultMix] > 1.0e-4f;
    for (const auto& v : vaults) vaultActive = vaultActive || v.ringing();
    bool cryptActive = p[ReverbMix] > 1.0e-4f;
    for (const auto& room : rooms) cryptActive = cryptActive || room.ringing();
    // Voice Bloom (Session 8) sings only with the developer's recordings.
    const bool bloom = p[VoiceBloom] > 1.0e-4f && wb::samples::Bank::get().ready();
    const bool wetActive = p[ChoirBlend] > 1.0e-4f || bloom || p[Drone] > 1.0e-4f || p[WraithMix] > 1.0e-4f
                        || echoesActive || cryptActive || p[BloodMix] > 1.0e-4f || relicsActive || p[SigAmount] > 1.0e-4f
                        || anyDeparting() || to[SigAmount] > 1.0e-4f || ensembleActive || vaultActive;
    runSlot(abyss, slots[0], abyssActive, l, r, n);
    blood.process(l, r, n, p, context);
    runSlot(relics, slots[1], relicsActive, l, r, n);
    runSlot(choir, slots[2], p[ChoirBlend] > 1.0e-4f || bloom || p[Drone] > 1.0e-4f, l, r, n);
    runSlot(wraith, slots[3], p[WraithMix] > 1.0e-4f, l, r, n);
    runSignatures(l, r, n, travel * travel * (3.0f - 2.0f * travel));
    runSlot(ensemble, slots[5], ensembleActive, l, r, n);
    runSlot(echoes, slots[4], echoesActive, l, r, n);
    runRooms(l, r, n, travel * travel * (3.0f - 2.0f * travel));
    if (vaultActive || vaultDry.value != 1.0f) runVaults(l, r, n, travel * travel * (3.0f - 2.0f * travel));
    if (addL != nullptr && addR != nullptr)
        for (int i = 0; i < n; ++i) { l[i] += addL[i]; r[i] += addR[i]; }
    if (!matching) {
        if (removedAny)
            for (int i = 0; i < n; ++i) { l[i] -= removed[0][static_cast<std::size_t>(i)]; r[i] -= removed[1][static_cast<std::size_t>(i)]; }
        return;
    }
    trimGain.set(dsp::dbToGain(trimDb));
    for (int i = 0; i < n; ++i) {
        const float g = trimGain.next();
        l[i] *= g;
        r[i] *= g;
        if (removedAny) { removed[0][static_cast<std::size_t>(i)] *= g; removed[1][static_cast<std::size_t>(i)] *= g; }
    }
    // The matcher stays agile while travelling and for half a second after arriving.
    // At neutral nothing changes the level, so the matcher returns to unity.
    settle = travel < 1.0f ? settleSamples : std::max(0, settle - n);
    levelMatcher.process(dryL, dryR, l, r, n, abyssActive || wetActive || addL != nullptr, settle > 0, removedAny ? removed[0].data() : nullptr,
                         removedAny ? removed[1].data() : nullptr);
}
}
