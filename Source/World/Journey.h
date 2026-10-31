#pragma once
// Milestone 9: journeys and roaming, as pure functions of the musical position so they
// are deterministic (the same bar always gives the same place, for any seed) and follow
// the host's transport when it plays.
#include "Locations.h"
#include <cmath>
#include <cstdint>

namespace dc {
struct Stop { int location = 0, sub = 0; };
inline constexpr int maxStops = 8;
enum JourneyMode { JourneyLoop, JourneyOneWay, JourneyPingPong };

// The stop at a musical position (beats from the start), `beatsPerStop` beats each.
inline int journeyStop(double beats, int stops, double beatsPerStop, int mode) noexcept {
    if (stops <= 1) return 0;
    const auto n = static_cast<long long>(std::floor(std::max(0.0, beats) / std::max(1.0e-3, beatsPerStop)));
    switch (mode) {
        case JourneyOneWay: return static_cast<int>(std::min<long long>(n, stops - 1));
        case JourneyPingPong: {
            const long long period = 2LL * (stops - 1), m = n % period;
            return static_cast<int>(m < stops ? m : period - m);
        }
        default: return static_cast<int>(n % stops);
    }
}

inline std::uint32_t roamHash(std::uint32_t seed, long long step, std::uint32_t salt) noexcept {
    std::uint64_t h = (static_cast<std::uint64_t>(seed) * 0x9e3779b97f4a7c15ULL) ^ (static_cast<std::uint64_t>(step) * 0xbf58476d1ce4e5b9ULL) ^ (salt * 0x94d049bb133111ebULL);
    h ^= h >> 31; h *= 0xd6e8feb96f4b7f1dULL; h ^= h >> 29;
    return static_cast<std::uint32_t>(h >> 32);
}

// Roaming between a location's places (its scene and sub-locations): at step n it is in
// place n (in order) or, with probability `randomness`, in a random other place.
inline int roamPlace(int location, long long step, float randomness, int seed) noexcept {
    const int count = subLocationCount(location) + 1;
    if (count <= 1) return 0;
    const int ordered = static_cast<int>(((step % count) + count) % count);
    const float u = static_cast<float>(roamHash(static_cast<std::uint32_t>(seed), step, 1) & 0xffffff) / 16777216.0f;
    if (u >= randomness) return ordered;
    const int pick = static_cast<int>(roamHash(static_cast<std::uint32_t>(seed), step, 2) % static_cast<std::uint32_t>(count - 1));
    return pick >= ordered ? pick + 1 : pick;   // a place other than the ordered one
}

// MIDI travel: which place a note chooses, relative to the travel octave's C (note
// `base`). C..A = the ten locations (arriving at their scene), the octave above C..E =
// a sub-location of the current location. Returns false for other notes.
inline bool midiPlace(int note, int base, int currentLocation, Stop& out) noexcept {
    const int k = note - base;
    if (k >= 0 && k < LocationCount) { out = { k, 0 }; return true; }
    if (k >= 12 && k < 12 + placesPerLocation) { out = { currentLocation, clampSubLocation(currentLocation, k - 12) }; return true; }
    return false;
}
}
