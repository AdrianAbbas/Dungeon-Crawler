#include "Atmosphere.h"
#include "../World/Journey.h"
#include "../Instrument/SampleBank.h"

namespace dc {
using namespace dsp;

namespace {
using S = AtmosSound;
// The sounds of every place (Milestone 10), in place order.
constexpr AtmosSet sets[LocationCount][placesPerLocation] = {
    { { { S::Drip, S::TorchCrackle, S::Chains }, 3 }, { { S::TorchCrackle, S::Wind, S::Drip }, 3 }, { { S::Chains, S::Drip, S::Whisper }, 3 },
      { { S::Tinkle, S::TorchCrackle, S::Drip }, 3 }, { { S::Growl, S::FireCrackle, S::Chains }, 3 } },                                        // Dungeon
    { { { S::Wind, S::Crow, S::Clink }, 3 }, { { S::FireCrackle, S::Murmur, S::Clink }, 3 }, { { S::TorchCrackle, S::Murmur, S::Clink }, 3 },
      { { S::BellToll, S::Whisper, S::TorchCrackle }, 3 }, { { S::Wind, S::Crow, S::BellToll }, 3 } },                                           // Castle
    { { { S::Wind, S::Owl, S::Drip }, 3 }, { { S::BellToll, S::Whisper, S::Drip }, 3 }, { { S::Drip, S::Whisper, S::Chains }, 3 },
      { { S::BellToll, S::Wind, S::Crow }, 3 }, {} },                                                                                          // Cathedral
    { { { S::Owl, S::Wind, S::Crow }, 3 }, { { S::Owl, S::Whisper, S::Crow }, 3 }, { { S::Bubbles, S::FireCrackle, S::Crow }, 3 },
      { { S::Owl, S::Whisper, S::Wind }, 3 }, {} },                                                                                            // Haunted Forest (the grove whispers: Session 8)
    { { { S::Drip, S::Wind, S::WaterLap }, 3 }, { { S::Tinkle, S::Drip, S::Drip }, 2 }, { { S::WaterLap, S::Drip, S::Drip }, 2 },
      { { S::Growl, S::Tinkle, S::Drip }, 3 }, {} },                                                                                           // Caverns
    { { { S::Crow, S::Wind, S::Murmur }, 3 }, { { S::Murmur, S::FireCrackle, S::Clink }, 3 }, { { S::Murmur, S::Clink, S::Crow }, 3 },
      { { S::Clink, S::FireCrackle, S::Clink }, 2 }, {} },                                                                                     // Village
    { { { S::Frog, S::Wind, S::Bubbles }, 3 }, { { S::Bubbles, S::Frog, S::Drip }, 3 }, { { S::WaterLap, S::Drip, S::Frog }, 3 }, {}, {} },   // Swamp
    { { { S::Wind, S::Crow, S::Howl }, 3 }, { { S::Wind, S::Howl, S::Crow }, 3 }, { { S::Wind, S::IceCreak, S::Howl }, 3 },
      { { S::Clink, S::FireCrackle, S::Murmur }, 3 }, {} },                                                                                    // Mountains
    { { { S::Crow, S::Wind, S::Chains }, 3 }, { { S::Crow, S::Owl, S::Whisper }, 3 }, { { S::Drip, S::Whisper, S::Chains }, 3 }, {}, {} },  // Necropolis
    { { { S::TorchCrackle, S::Wind, S::Rustle }, 3 }, { { S::Rustle, S::TorchCrackle, S::Owl }, 3 }, { { S::Bubbles, S::Clink, S::Drip }, 3 },
      { { S::Tinkle, S::Wind, S::Owl }, 3 }, {} },                                                                                             // Wizard's Tower
};
constexpr const char* names[] = { "Torch crackle", "Drip", "Wind", "Chains", "Growl", "Bubbles", "Crow", "Bell toll", "Owl", "Murmur",
                                   "Clink", "Frog", "Howl", "Ice creak", "Rustle", "Tinkle", "Water lap", "Fire crackle", "Whisper" };
inline float unit(std::uint32_t h, int shift) noexcept { return static_cast<float>((h >> shift) & 0xffu) / 255.0f; }
}

const char* atmosSoundName(AtmosSound s) noexcept { return names[std::clamp(static_cast<int>(s), 0, static_cast<int>(AtmosSound::Count) - 1)]; }

AtmosSet atmosphereOf(int location, int sub) noexcept {
    location = std::clamp(location, 0, LocationCount - 1);
    sub = clampSubLocation(location, sub);
    return sets[location][sub];
}

void Atmosphere::prepare(double sampleRate) {
    sr = sampleRate;
    attack = timeCoefficient(0.01, sr);
    release = timeCoefficient(0.4, sr);
    reset();
}

void Atmosphere::reset() {
    for (auto& v : voices) v = Voice {};
    lastSlot = -1;
    duck = 1; envelope = 0; smoothLevel = 0;
}

int Atmosphere::activeVoices() const noexcept {
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void Atmosphere::start(AtmosSound sound, std::uint32_t h, float) noexcept {
    Voice* slot = nullptr;
    for (auto& v : voices) if (!v.active) { slot = &v; break; }
    if (slot == nullptr) return;   // all busy: skip (sparse by design)
    Voice& v = *slot;
    v = Voice {};
    v.active = true;
    v.sound = sound;
    v.random.seed(h | 1u);
    v.pan = (unit(h, 0) - 0.5f) * 1.4f;
    v.pitch = 0.85f + 0.3f * unit(h, 8);
    v.gain = 0.7f + 0.3f * unit(h, 16);
    const auto bp = [&](Svf& f, double hz, float q) { f.set(hz, sr, 1.0f / q); };
    switch (sound) {
        case S::TorchCrackle: v.length = 0.6; bp(v.filter, 3000 * v.pitch, 0.8f); break;
        case S::FireCrackle: v.length = 1.6; bp(v.filter, 2500 * v.pitch, 0.8f); bp(v.filter2, 260, 0.7f); break;
        case S::Drip: v.length = 0.3; v.state = 900.0f + 1300.0f * unit(h, 24); break;
        case S::Wind: v.length = 3.0 + 2.0 * unit(h, 24); v.state = 0; bp(v.filter, 420 * v.pitch, 1.6f); break;
        case S::Chains: v.length = 0.9; v.count = 3 + static_cast<int>(unit(h, 24) * 3.0f); v.state = 1500.0f * v.pitch; break;
        case S::Growl: v.length = 2.6; bp(v.filter, 140, 0.8f); bp(v.filter2, 320, 0.9f); v.gain *= 0.9f; break;
        case S::Bubbles: v.length = 1.2; v.count = 4 + static_cast<int>(unit(h, 24) * 5.0f); v.state = 320.0f * v.pitch; break;
        case S::Crow: v.length = 1.1; v.count = 2 + static_cast<int>(unit(h, 24) * 1.9f); bp(v.filter, 1300 * v.pitch, 2.5f); bp(v.filter2, 2600 * v.pitch, 3.0f); break;
        case S::BellToll: v.length = 6.0; v.state = 196.0f * v.pitch; bp(v.filter, 1800, 0.7f); break;
        case S::Owl: v.length = 1.2; v.state = 380.0f * v.pitch; break;
        case S::Murmur: v.length = 3.0; bp(v.filter, 520 * v.pitch, 1.6f); bp(v.filter2, 1400 * v.pitch, 2.0f); break;
        case S::Clink: v.length = 1.2; v.state = 880.0f * v.pitch; break;
        case S::Frog: v.length = 0.9; v.count = 3 + static_cast<int>(unit(h, 24) * 3.0f); bp(v.filter, 420 * v.pitch, 4.0f); break;
        case S::Howl: v.length = 3.0; v.state = 380.0f * v.pitch; bp(v.filter, 1600, 0.7f); v.gain *= 0.7f; break;
        case S::IceCreak: v.length = 0.9; bp(v.filter, 700 * v.pitch, 2.0f); break;
        case S::Rustle: v.length = 0.5; bp(v.filter, 4200 * v.pitch, 0.7f); break;
        case S::Tinkle: v.length = 0.9; v.count = 3 + static_cast<int>(unit(h, 24) * 3.0f); v.state = 2400.0f * v.pitch; break;
        case S::WaterLap: v.length = 0.9; bp(v.filter, 420 * v.pitch, 0.7f); break;
        case S::Whisper:
            v.length = 1.8;
            bp(v.filter, 900, 3.0f);
            // In the Graveyard, the Crypt and the Ancient Grove the whisper is the
            // developer's own (a moment from anywhere in one of his three takes).
            if (recordedWhispers) {
                const auto& bank = wb::samples::Bank::get();
                if (const auto* takes = bank.ready() ? bank.instrument("voice-whispers") : nullptr; takes != nullptr && !takes->zones.empty()) {
                    const auto& z = takes->zones[static_cast<std::size_t>(unit(h, 24) * 0.999f * static_cast<float>(takes->zones.size()))];
                    const int need = static_cast<int>(v.length * takes->rate) + 8;
                    if (z.length > need) {
                        v.data = z.data;
                        v.pos = static_cast<double>(unit(h, 4)) * static_cast<double>(z.length - need);
                        v.step = static_cast<float>(takes->rate / sr) * v.pitch;
                    }
                }
            }
            break;
        default: v.active = false; break;
    }
}

float Atmosphere::render(Voice& v) noexcept {
    const float t = static_cast<float>(v.age), len = static_cast<float>(v.length);
    const float dt = static_cast<float>(1.0 / sr);
    const float u = t / len;
    const float bell = std::sin(static_cast<float>(pi) * clamp01(u));   // a gentle in-out envelope
    float y = 0;
    switch (v.sound) {
        case S::TorchCrackle: case S::FireCrackle: case S::IceCreak: {
            const float density = v.sound == S::IceCreak ? 0.0016f : 0.0035f;
            const float impulse = v.random.unit() < density * (1.0f - u) ? v.random.bipolar() : 0.0f;
            if (v.sound == S::IceCreak) v.filter.set(500.0 + 900.0 * u, sr, 0.5f);
            y = v.filter.bandPass(impulse) * 2.5f;
            if (v.sound == S::FireCrackle) y += v.filter2.low(v.random.bipolar()) * 0.25f * bell;
            break;
        }
        case S::Drip: {
            v.state *= 1.0f + 14.0f * dt;
            v.phase += v.state * dt; v.phase -= std::floor(v.phase);
            y = std::sin(twoPi * v.phase) * std::exp(-45.0f * t) * 0.5f;
            break;
        }
        case S::Wind: {
            v.state2 += dt * 0.4f;
            v.filter.set(380.0 + 160.0 * std::sin(twoPi * v.state2), sr, 0.6f);
            y = v.filter.bandPass(v.random.bipolar()) * 0.5f * bell * bell;
            break;
        }
        case S::Chains: case S::Bubbles: case S::Tinkle: case S::Frog: {
            // A burst of short events.
            if (v.next <= 0 && v.count > 0) {
                --v.count;
                v.env = 1;
                v.next = static_cast<int>(sr * (v.sound == S::Bubbles ? 0.08 + 0.12 * v.random.unit() : v.sound == S::Frog ? 0.15 : 0.06 + 0.08 * v.random.unit()));
                v.phase = v.phase2 = 0;
                v.state2 = v.sound == S::Bubbles ? v.state * (0.8f + 0.6f * v.random.unit()) : v.state * (0.9f + 0.25f * v.random.unit());
            }
            --v.next;
            if (v.sound == S::Chains) {
                v.phase += v.state2 * dt; v.phase -= std::floor(v.phase);
                v.phase2 += v.state2 * 2.76f * dt; v.phase2 -= std::floor(v.phase2);
                y = (std::sin(twoPi * v.phase) + 0.6f * std::sin(twoPi * v.phase2)) * v.env * 0.35f;
                v.env *= 1.0f - 28.0f * dt;
            } else if (v.sound == S::Bubbles) {
                v.state2 *= 1.0f + 30.0f * dt;
                v.phase += v.state2 * dt; v.phase -= std::floor(v.phase);
                y = std::sin(twoPi * v.phase) * v.env * 0.4f;
                v.env *= 1.0f - 45.0f * dt;
            } else if (v.sound == S::Tinkle) {
                v.phase += v.state2 * dt; v.phase -= std::floor(v.phase);
                y = std::sin(twoPi * v.phase) * v.env * 0.25f;
                v.env *= 1.0f - 9.0f * dt;
            } else {   // frog: a pulse train through a narrow resonance
                v.phase += 32.0f * dt;
                const float pulse = v.phase >= 1.0f ? 1.0f : 0.0f;
                v.phase -= std::floor(v.phase);
                y = v.filter.bandPass(pulse * v.env) * 3.0f;
                v.env *= 1.0f - 14.0f * dt;
            }
            break;
        }
        case S::Growl: {
            v.phase += 43.0f * dt; v.phase -= std::floor(v.phase);
            const float saw = 2.0f * v.phase - 1.0f;
            const float throb = 0.6f + 0.4f * std::sin(twoPi * 5.5f * t);
            y = (v.filter.bandPass(v.random.bipolar()) * 0.8f + v.filter2.low(saw) * 0.5f) * throb * bell;
            break;
        }
        case S::Crow: {
            const float caw = 0.33f;
            const float local = std::fmod(t, caw + 0.12f);
            const float on = local < caw && t < static_cast<float>(v.count) * (caw + 0.12f) ? 1.0f : 0.0f;
            const float f = 640.0f * v.pitch * (1.0f - 0.3f * local / caw);
            v.phase += f * dt; v.phase -= std::floor(v.phase);
            const float env = on * std::sin(static_cast<float>(pi) * clamp01(local / caw));
            const float saw = 2.0f * v.phase - 1.0f;
            y = (v.filter.bandPass(saw) + 0.6f * v.filter2.bandPass(saw)) * env * 0.5f;
            break;
        }
        case S::BellToll: {
            static constexpr float ratios[] = { 0.5f, 1.0f, 1.19f, 1.5f, 2.0f, 2.51f };
            static constexpr float decays[] = { 0.35f, 0.5f, 0.7f, 0.8f, 1.0f, 1.4f };
            float sum = 0;
            for (int k = 0; k < 6; ++k) sum += std::sin(twoPi * v.state * ratios[k] * t) * std::exp(-decays[k] * t) / (1.0f + 0.4f * static_cast<float>(k));
            y = v.filter.low(sum) * 0.35f * std::min(1.0f, t * 200.0f);
            break;
        }
        case S::Owl: {
            const float hoot = 0.42f;
            const float local = std::fmod(t, hoot + 0.25f);
            const float on = t < 2.0f * (hoot + 0.25f) && local < hoot ? std::sin(static_cast<float>(pi) * local / hoot) : 0.0f;
            v.phase += v.state * (1.0f - 0.08f * local) * dt; v.phase -= std::floor(v.phase);
            y = std::sin(twoPi * v.phase) * on * on * 0.35f;
            break;
        }
        case S::Murmur: {
            if (--v.next <= 0) { v.next = static_cast<int>(sr * (0.12 + 0.2 * v.random.unit())); v.state = v.random.unit(); }
            v.env += 0.002f * (v.state - v.env);
            const float n = v.random.bipolar();
            y = (v.filter.bandPass(n) + 0.5f * v.filter2.bandPass(n)) * v.env * bell * 0.6f;
            break;
        }
        case S::Clink: {
            static constexpr float ratios[] = { 1.0f, 1.47f, 2.09f, 2.56f, 2.98f };
            float sum = 0;
            for (int k = 0; k < 5; ++k) sum += std::sin(twoPi * v.state * ratios[k] * t) * std::exp(-(5.0f + 2.0f * static_cast<float>(k)) * t) / (1.0f + 0.5f * static_cast<float>(k));
            y = sum * 0.3f * std::min(1.0f, t * 400.0f);
            break;
        }
        case S::Howl: {
            const float glide = u < 0.4f ? u / 0.4f : 1.0f - (u - 0.4f) / 0.6f * 0.6f;
            const float f = v.state * (1.0f + 0.45f * glide) * (1.0f + 0.01f * std::sin(twoPi * 5.0f * t));
            v.phase += f * dt; v.phase -= std::floor(v.phase);
            y = v.filter.low(std::sin(twoPi * v.phase)) * bell * bell * 0.3f;
            break;
        }
        case S::Rustle: y = v.filter.bandPass(v.random.bipolar()) * bell * 0.35f; break;
        case S::WaterLap: y = v.filter.low(v.random.bipolar()) * bell * bell * 0.6f; break;
        case S::Whisper: {
            if (v.data != nullptr) {
                const auto i0 = static_cast<int>(v.pos);
                const float f = static_cast<float>(v.pos - i0);
                y = (v.data[i0] + f * static_cast<float>(v.data[i0 + 1] - v.data[i0])) * (1.0f / 32768.0f) * bell * 0.9f;
                v.pos += v.step;
                break;
            }
            v.filter.set(700.0 + 1800.0 * u, sr, 0.35f);
            y = v.filter.bandPass(v.random.bipolar()) * bell * bell * 0.4f;
            break;
        }
        default: break;
    }
    v.age += 1.0 / sr;
    if (v.age >= v.length) v.active = false;
    return finiteOr(y) * v.gain;
}

void Atmosphere::process(float* l, float* r, int n, double beats, double beatsPerSample, int location, int sub,
                         float level, float density, int seed, const float* dryL, const float* dryR) noexcept {
    level = std::clamp(level, 0.0f, 1.5f);   // up to 150 % with the MIX section (Session 11)
    // Schedule: one chance per eighth note, decided by a hash of (seed, slot, place).
    const auto slotAt = [](double b) { return static_cast<long long>(std::floor(std::max(0.0, b) * 2.0)); };
    const long long first = slotAt(beats), last = slotAt(beats + beatsPerSample * n);
    if (lastSlot < 0 || first < lastSlot || first - lastSlot > 64) lastSlot = first - 1;   // a jump in position
    const float chance = clamp01(density) * 0.3f;
    recordedWhispers = (location == Necropolis && sub == 1) || (location == Cathedral && sub == 2) || (location == HauntedForest && sub == 3);
    const auto set = atmosphereOf(location, sub);
    for (long long slot = lastSlot + 1; slot <= last && level > 1.0e-4f && set.count > 0; ++slot) {
        const auto h = roamHash(static_cast<std::uint32_t>(seed) * 977u + 13u, slot, static_cast<std::uint32_t>(location * 8 + sub + 101));
        if (static_cast<float>(h & 0xffffu) / 65535.0f >= chance) continue;
        // Weighted choice: the first sound most often.
        const float pick = unit(h, 16);
        const int index = set.count == 1 ? 0 : (pick < 0.55f ? 0 : (pick < 0.85f || set.count == 2 ? 1 : 2));
        start(set.sounds[static_cast<std::size_t>(index)], h, level);
    }
    lastSlot = std::max(lastSlot, last);

    const float levelStep = timeCoefficient(0.05, sr);
    for (int i = 0; i < n; ++i) {
        smoothLevel += levelStep * (level - smoothLevel);
        // Ducking: under the instrument the layer steps back (-12 dB when it is loud).
        const float in = std::max(std::abs(dryL[i]), std::abs(dryR[i]));
        envelope += (in > envelope ? attack : release) * (in - envelope);
        duck = 1.0f / (1.0f + 6.0f * envelope);
        float sl = 0, sr2 = 0;
        for (auto& v : voices) {
            if (!v.active) continue;
            const float y = render(v);
            sl += y * (1.0f - v.pan) * 0.7f;
            sr2 += y * (1.0f + v.pan) * 0.7f;
        }
        // At Level 100 % an event peaks around -20 dBFS: a background, never a feature.
        const float g = smoothLevel * smoothLevel * duck * 0.18f;
        l[i] += sl * g;
        r[i] += sr2 * g;
    }
}
}
