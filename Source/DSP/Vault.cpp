#include "Modules.h"
#include "Convolver.h"
#include <map>
#include <mutex>

// IX. THE VAULT (Direction Update 2). The developer's effect captures were to give
// these spaces their impulse responses; the Samples repository holds no captures (see
// docs/DEVLOG.md, Session 6), so every space is generated from a recipe instead:
// three-band exponentially decaying noise (its own RT60 per band), a slow bloom,
// pre-delay, early reflections, far discrete echoes, bright glints (coins, crystals,
// stars), a low-mid body, and optionally reversed (a swell drawn in towards the note).
// Deterministic (fixed seeds). Each IR is normalised to unit energy per channel.
namespace dc {
using namespace dsp;

namespace {
struct IrRecipe {
    float seconds = 2, preMs = 0, bloomMs = 10;
    float rtLow = 2, rtMid = 2, rtHigh = 1.5f;     // RT60 below 300 Hz, 300 Hz-3 kHz, above 3 kHz
    float lowCut = 60, highCut = 12000;            // on the whole IR
    float bodyHz = 250, bodyDb = 0;                // low-mid body
    float width = 1;                               // 0 = same tail both sides, 1 = decorrelated
    int early = 0; float earlyMs = 60, earlyGain = .25f, earlyLp = 8000;
    int echoes = 0; float echoMs[4] {}, echoGain[4] {}; float echoBlurMs = 12, echoLp = 4000;
    float glints = 0, glintHz = 5000, glintGain = .15f;   // per second
    bool reverse = false;
    float tailGain = 1;                            // the diffuse tail against reflections/echoes
};

// In kind order (vaultChoices in ModuleParams.h).
const IrRecipe recipes[Vault::kinds] = {
    {},
    // 1 Black Vault (Gates): wide and dark, a heavy low-mid, distant dark echoes.
    { .seconds = 6, .preMs = 35, .bloomMs = 140, .rtLow = 6.5f, .rtMid = 5, .rtHigh = 2.2f, .lowCut = 45, .highCut = 4200,
      .bodyHz = 230, .bodyDb = 4, .width = 1, .early = 4, .earlyMs = 90, .earlyGain = .12f, .earlyLp = 3000,
      .echoes = 2, .echoMs = { 740, 1530 }, .echoGain = { .32f, .2f }, .echoBlurMs = 30, .echoLp = 1800 },
    // 2 Ghost Swell (Graveyard): a cold reversed swell.
    { .seconds = 1.5f, .bloomMs = 20, .rtLow = 2.6f, .rtMid = 2.6f, .rtHigh = 2.2f, .lowCut = 180, .highCut = 7000, .width = 1,
      .reverse = true },
    // 3 Open Hills (Foothills): open air, a soft, thin tail.
    { .seconds = 2.2f, .preMs = 20, .bloomMs = 40, .rtLow = 1.4f, .rtMid = 1.8f, .rtHigh = 1.1f, .lowCut = 140, .highCut = 8000, .width = 1,
      .tailGain = .7f },
    // 4 Canyon Walls (Mountain Pass): massive rock, hard reflections.
    { .seconds = 4.5f, .preMs = 25, .bloomMs = 50, .rtLow = 4.2f, .rtMid = 3.8f, .rtHigh = 2.2f, .lowCut = 60, .highCut = 7000,
      .bodyHz = 400, .bodyDb = 2, .width = 1, .early = 6, .earlyMs = 140, .earlyGain = .22f, .earlyLp = 5000 },
    // 5 Ice Shards (Frozen Peak): glassy, the highs ring longest, glints of ice.
    { .seconds = 2.6f, .preMs = 8, .bloomMs = 5, .rtLow = .6f, .rtMid = 2.0f, .rtHigh = 2.2f, .lowCut = 500, .highCut = 8500, .width = 1,
      .early = 6, .earlyMs = 35, .earlyGain = .2f, .earlyLp = 8000, .glints = 30, .glintHz = 2600, .glintGain = .22f },
    // 6 Stone Hall (Dwarven Hall): deep stone, warm low-mid body.
    { .seconds = 5, .preMs = 40, .bloomMs = 90, .rtLow = 5.2f, .rtMid = 4.2f, .rtHigh = 1.8f, .lowCut = 35, .highCut = 5000,
      .bodyHz = 200, .bodyDb = 5, .width = .9f, .early = 5, .earlyMs = 70, .earlyGain = .16f, .earlyLp = 3500 },
    // 7 Crypt Swell (Crypt): a dark reversed swell.
    { .seconds = 1.4f, .bloomMs = 20, .rtLow = 2.4f, .rtMid = 2.2f, .rtHigh = 1.2f, .lowCut = 60, .highCut = 2600,
      .bodyHz = 220, .bodyDb = 3, .width = .9f, .reverse = true },
    // 8 Grand Nave: the longest, most majestic space, long pre-delay, slow bloom.
    { .seconds = 7.5f, .preMs = 110, .bloomMs = 420, .rtLow = 8.5f, .rtMid = 7.5f, .rtHigh = 4.2f, .lowCut = 40, .highCut = 7500,
      .bodyHz = 300, .bodyDb = 1.5f, .width = 1, .early = 3, .earlyMs = 160, .earlyGain = .07f, .earlyLp = 4000 },
    // 9 Belfry (Bell Tower): a hard, bright stone tower.
    { .seconds = 3.5f, .preMs = 18, .bloomMs = 20, .rtLow = 2.8f, .rtMid = 3.2f, .rtHigh = 2.4f, .lowCut = 90, .highCut = 10000,
      .width = 1, .early = 5, .earlyMs = 60, .earlyGain = .18f, .earlyLp = 8000 },
    // 10 Rock Mouth (Cave Mouth): enclosed rock, dense early reflections, a warm tail.
    { .seconds = 2.4f, .preMs = 6, .bloomMs = 15, .rtLow = 2.3f, .rtMid = 1.9f, .rtHigh = .9f, .lowCut = 60, .highCut = 5200,
      .bodyHz = 280, .bodyDb = 3, .width = .85f, .early = 6, .earlyMs = 45, .earlyGain = .26f, .earlyLp = 4500 },
    // 11 Crystal Hall (Crystal Cavern): bright, long highs, dense crystal glints.
    { .seconds = 4.5f, .preMs = 20, .bloomMs = 60, .rtLow = 1.6f, .rtMid = 3.8f, .rtHigh = 4.5f, .lowCut = 300, .highCut = 16000,
      .width = 1, .early = 4, .earlyMs = 50, .earlyGain = .12f, .earlyLp = 12000, .glints = 45, .glintHz = 3000, .glintGain = .3f },
    // 12 Vast Cavern (Dragon's Hoard): enormous and dry-dark, far distinct echoes, a
    // metallic sheen of coins on the reflections.
    { .seconds = 6.5f, .preMs = 90, .bloomMs = 180, .rtLow = 7.5f, .rtMid = 6, .rtHigh = 2.4f, .lowCut = 30, .highCut = 4200,
      .bodyHz = 140, .bodyDb = 3, .width = 1, .early = 3, .earlyMs = 200, .earlyGain = .1f, .earlyLp = 3000,
      .echoes = 3, .echoMs = { 640, 1390, 2250 }, .echoGain = { .42f, .3f, .2f }, .echoBlurMs = 22, .echoLp = 3000,
      .glints = 9, .glintHz = 4500, .glintGain = .16f, .tailGain = .8f },
    // 13 Vaulted Hall (Entrance Hall): a big stone vault, torch-warm.
    { .seconds = 4, .preMs = 38, .bloomMs = 60, .rtLow = 4, .rtMid = 3.6f, .rtHigh = 1.8f, .lowCut = 45, .highCut = 6000,
      .bodyHz = 260, .bodyDb = 2, .width = 1, .early = 5, .earlyMs = 110, .earlyGain = .18f, .earlyLp = 5000 },
    // 14 Golden Vault (Treasure Room): rich and bright, glittering gold.
    { .seconds = 3.5f, .preMs = 15, .bloomMs = 50, .rtLow = 2.6f, .rtMid = 3.2f, .rtHigh = 3.2f, .lowCut = 120, .highCut = 15000,
      .width = 1, .early = 5, .earlyMs = 40, .earlyGain = .14f, .earlyLp = 12000, .glints = 35, .glintHz = 5200, .glintGain = .28f },
    // 15 Infernal Abyss (Final Boss Lair): colossal and dark, slow ominous echoes.
    { .seconds = 7.5f, .preMs = 70, .bloomMs = 320, .rtLow = 10, .rtMid = 7.5f, .rtHigh = 2.5f, .lowCut = 25, .highCut = 3000,
      .bodyHz = 110, .bodyDb = 5, .width = 1, .echoes = 2, .echoMs = { 900, 1800 }, .echoGain = { .3f, .2f }, .echoBlurMs = 40, .echoLp = 1500 },
    // 16 Stone Court (Courtyard): open stone, fluttering reflections between walls.
    { .seconds = 2.4f, .preMs = 12, .bloomMs = 20, .rtLow = 1.8f, .rtMid = 2.2f, .rtHigh = 1.6f, .lowCut = 90, .highCut = 9000,
      .width = 1, .early = 6, .earlyMs = 160, .earlyGain = .2f, .earlyLp = 7000 },
    // 17 Great Hall: massive, long, wide and grand.
    { .seconds = 5.5f, .preMs = 55, .bloomMs = 200, .rtLow = 6, .rtMid = 5.5f, .rtHigh = 3.2f, .lowCut = 40, .highCut = 8000,
      .bodyHz = 250, .bodyDb = 1, .width = 1, .early = 6, .earlyMs = 130, .earlyGain = .12f, .earlyLp = 6000 },
    // 18 Regal Hall (Throne Room): warm, rich, controlled.
    { .seconds = 3.8f, .preMs = 35, .bloomMs = 90, .rtLow = 3.6f, .rtMid = 3.4f, .rtHigh = 2, .lowCut = 60, .highCut = 7000,
      .bodyHz = 350, .bodyDb = 2.5f, .width = 1, .early = 5, .earlyMs = 70, .earlyGain = .14f, .earlyLp = 6000 },
    // 19 Small Chapel: close, warm, softly muffled.
    { .seconds = 2.2f, .preMs = 10, .bloomMs = 70, .rtLow = 2.2f, .rtMid = 1.9f, .rtHigh = .8f, .lowCut = 80, .highCut = 3600,
      .bodyHz = 300, .bodyDb = 2, .width = .8f, .early = 4, .earlyMs = 25, .earlyGain = .12f, .earlyLp = 3000 },
    // 20 Open Sky (Tower Top): almost no reflections, long distant echoes, airy.
    { .seconds = 3, .preMs = 30, .bloomMs = 60, .rtLow = 1.2f, .rtMid = 2, .rtHigh = 1.8f, .lowCut = 200, .highCut = 12000, .width = 1,
      .echoes = 3, .echoMs = { 480, 1050, 1720 }, .echoGain = { .36f, .24f, .15f }, .echoBlurMs = 18, .echoLp = 6000, .tailGain = .55f },
    // 21 Dark Wood (Forest Edge): dull, close trunks, a tense dark tail.
    { .seconds = 2.4f, .preMs = 15, .bloomMs = 50, .rtLow = 2.2f, .rtMid = 1.8f, .rtHigh = .7f, .lowCut = 70, .highCut = 2800,
      .width = 1, .early = 6, .earlyMs = 120, .earlyGain = .12f, .earlyLp = 2500 },
    // 22 Breathing Grove (Ancient Grove): a soft, slowly blooming halo.
    { .seconds = 5, .preMs = 40, .bloomMs = 600, .rtLow = 4, .rtMid = 4.8f, .rtHigh = 3.6f, .lowCut = 90, .highCut = 9000,
      .bodyHz = 450, .bodyDb = 1.5f, .width = 1, .glints = 6, .glintHz = 3500, .glintGain = .12f },
    // 23 Murky Pool (Mire): dark wet reflections, a long murky tail.
    { .seconds = 4.5f, .preMs = 25, .bloomMs = 80, .rtLow = 4.8f, .rtMid = 3.8f, .rtHigh = 1, .lowCut = 40, .highCut = 2000,
      .bodyHz = 200, .bodyDb = 3, .width = 1, .early = 5, .earlyMs = 90, .earlyGain = .2f, .earlyLp = 1800,
      .echoes = 2, .echoMs = { 390, 810 }, .echoGain = { .25f, .15f }, .echoBlurMs = 35, .echoLp = 1400 },
    // 24 Star Dome (Observatory): vast and bright, starlight glints, slow bloom.
    { .seconds = 6.5f, .preMs = 80, .bloomMs = 380, .rtLow = 4.5f, .rtMid = 6.5f, .rtHigh = 6, .lowCut = 120, .highCut = 16000,
      .width = 1, .glints = 25, .glintHz = 5000, .glintGain = .22f },
    // 25 Spiral Stair (Stairwell): tall, narrow, hard stone.
    { .seconds = 2.6f, .preMs = 5, .bloomMs = 15, .rtLow = 2, .rtMid = 2.4f, .rtHigh = 1.6f, .lowCut = 120, .highCut = 8000,
      .width = 1, .early = 6, .earlyMs = 100, .earlyGain = .18f, .earlyLp = 6000 },
    // 26 Glass Room (Alchemy Lab): small, glassy, bright.
    { .seconds = 1.4f, .preMs = 4, .bloomMs = 5, .rtLow = .8f, .rtMid = 1.2f, .rtHigh = 1.4f, .lowCut = 200, .highCut = 14000,
      .width = 1, .early = 5, .earlyMs = 20, .earlyGain = .15f, .earlyLp = 10000, .glints = 12, .glintHz = 3800, .glintGain = .15f },
    // 27 Old Cloister: arcades of bright stone (the Cloister's boost).
    { .seconds = 4, .preMs = 28, .bloomMs = 60, .rtLow = 3.6f, .rtMid = 4, .rtHigh = 2.8f, .lowCut = 80, .highCut = 11000,
      .width = 1, .early = 6, .earlyMs = 110, .earlyGain = .12f, .earlyLp = 8000 },
    // 28 Hut Glow (Witch's Hut): a small warm glow, sweet and close.
    { .seconds = 1.2f, .preMs = 3, .bloomMs = 30, .rtLow = 1, .rtMid = 1.1f, .rtHigh = .9f, .lowCut = 120, .highCut = 9000,
      .bodyHz = 600, .bodyDb = 2, .width = 1 },
    // 29 Dead Square (Market Square): dark, damped, sparse slaps.
    { .seconds = 1.8f, .preMs = 10, .bloomMs = 10, .rtLow = 1.6f, .rtMid = 1.2f, .rtHigh = .4f, .lowCut = 80, .highCut = 2400,
      .width = 1, .echoes = 2, .echoMs = { 165, 390 }, .echoGain = { .4f, .22f }, .echoBlurMs = 6, .echoLp = 2200, .tailGain = .6f },
    // 30 Forge Room (Blacksmith): hard, bright, metallic reflections.
    { .seconds = 1.3f, .preMs = 3, .bloomMs = 3, .rtLow = .7f, .rtMid = 1.1f, .rtHigh = 1.2f, .lowCut = 150, .highCut = 13000,
      .width = 1, .early = 6, .earlyMs = 30, .earlyGain = .28f, .earlyLp = 12000, .glints = 18, .glintHz = 3000, .glintGain = .2f },
    // 31 Tavern Room: a small wooden room, soft and slightly dull.
    { .seconds = .9f, .preMs = 4, .bloomMs = 15, .rtLow = .9f, .rtMid = .75f, .rtHigh = .35f, .lowCut = 90, .highCut = 3200,
      .bodyHz = 320, .bodyDb = 3, .width = .8f, .early = 5, .earlyMs = 18, .earlyGain = .18f, .earlyLp = 3000 },
    // 32 Hush Room (Library): small, close, paper-soft.
    { .seconds = .7f, .preMs = 2, .bloomMs = 10, .rtLow = .5f, .rtMid = .45f, .rtHigh = .2f, .lowCut = 120, .highCut = 2600,
      .width = .7f, .early = 4, .earlyMs = 12, .earlyGain = .12f, .earlyLp = 2400 },
    // 33 Soft Green (Village Green): soft open air between cottages.
    { .seconds = 1.8f, .preMs = 15, .bloomMs = 80, .rtLow = 1.3f, .rtMid = 1.5f, .rtHigh = .7f, .lowCut = 100, .highCut = 5000,
      .width = 1, .early = 4, .earlyMs = 70, .earlyGain = .1f, .earlyLp = 4000 },
};

double lengthScale(double sampleRate) { return sampleRate > 50000.0 ? 0.6 : 1.0; }   // memory at 88.2/96 kHz

struct OnePole {
    float a = 0, s = 0;
    void set(double hz, double sr) { a = poleForHz(hz, sr); }
    float lp(float x) { s += a * (x - s); return s; }
    float hp(float x) { s += a * (x - s); return x - s; }
};

std::array<std::vector<float>, 2> generate(const IrRecipe& r, double sr, std::uint32_t seed) {
    const auto total = static_cast<std::size_t>(r.seconds * lengthScale(sr) * sr);
    const auto pre = static_cast<std::size_t>(r.preMs * 0.001 * sr);
    std::array<std::vector<float>, 2> out;
    Random random; random.seed(seed);
    std::array<std::vector<float>, 2> noise;
    for (auto& n : noise) { n.resize(total); for (auto& v : n) v = random.bipolar(); }
    for (std::size_t i = 0; i < total; ++i) noise[1][i] = r.width * noise[1][i] + (1.0f - r.width) * noise[0][i];
    for (int ch = 0; ch < 2; ++ch) {
        auto& h = out[static_cast<std::size_t>(ch)];
        h.assign(total + pre, 0.0f);
        OnePole lowBand, highBand; lowBand.set(300, sr); highBand.set(3000, sr);
        const double kL = 6.9078 / (r.rtLow * sr), kM = 6.9078 / (r.rtMid * sr), kH = 6.9078 / (r.rtHigh * sr);
        const double bloom = std::max(0.002, r.bloomMs * 0.001) * sr;
        double tailEnergy = 0;
        std::vector<float> tail(total);
        for (std::size_t i = 0; i < total; ++i) {
            const float n = noise[static_cast<std::size_t>(ch)][i];
            const float low = lowBand.lp(n), high = n - highBand.lp(n), mid = n - low - high;
            const double t = static_cast<double>(i);
            const double v = low * std::exp(-kL * t) + mid * std::exp(-kM * t) + high * std::exp(-kH * t);
            tail[i] = static_cast<float>(v * (1.0 - std::exp(-t / bloom)));
            tailEnergy += double(tail[i]) * tail[i];
        }
        const float tailScale = r.tailGain / static_cast<float>(std::sqrt(std::max(1.0e-20, tailEnergy)));
        // Early reflections and far echoes: discrete taps (echoes as short clusters).
        std::vector<float> taps(total, 0.0f), echoes(total, 0.0f), glints(total, 0.0f);
        Random tapRandom; tapRandom.seed(seed * 7u + 11u);
        for (int k = 0; k < r.early; ++k) {
            const float frac = (static_cast<float>(k) + 0.3f + 0.6f * tapRandom.unit()) / static_cast<float>(std::max(1, r.early));
            const auto at = static_cast<std::size_t>(frac * r.earlyMs * 0.001f * sr);
            const float pan = (k & 1) ? 0.7f : -0.7f;
            const float g = r.earlyGain * (1.0f - 0.55f * static_cast<float>(k) / static_cast<float>(std::max(1, r.early)))
                          * (1.0f + (ch == 0 ? -pan : pan) * 0.8f) * (tapRandom.unit() < 0.5f ? -1.0f : 1.0f);
            if (at < total) taps[at] += g;
        }
        for (int e = 0; e < r.echoes; ++e) {
            const auto at = static_cast<std::size_t>(r.echoMs[e] * 0.001f * sr);
            const float pan = (e & 1) ? 0.6f : -0.6f;
            for (int c = 0; c < 5; ++c) {
                const auto pos = at + static_cast<std::size_t>(tapRandom.unit() * r.echoBlurMs * 0.001f * sr);
                if (pos < total) echoes[pos] += r.echoGain[e] / std::sqrt(5.0f) * (1.0f + (ch == 0 ? -pan : pan) * 0.7f)
                                                * (c == 0 ? 1.0f : (tapRandom.unit() < 0.5f ? -0.7f : 0.7f));
            }
        }
        // Glints: short decaying sines high up, following the tail's envelope.
        if (r.glints > 0) {
            Random glintRandom; glintRandom.seed(seed * 13u + static_cast<std::uint32_t>(ch) * 101u + 3u);
            const double perSample = r.glints / sr;
            for (std::size_t i = 0; i < total; ++i) {
                if (glintRandom.unit() >= perSample) continue;
                const double hz = r.glintHz * (1.0 + 1.2 * glintRandom.unit());
                const double life = (0.006 + 0.02 * glintRandom.unit()) * sr;
                const double env0 = std::exp(-kM * static_cast<double>(i)) * (1.0 - std::exp(-static_cast<double>(i) / bloom));
                const float amp = static_cast<float>(r.glintGain * env0 * (0.4 + 0.6 * glintRandom.unit()));
                for (std::size_t j = 0; j < static_cast<std::size_t>(4 * life) && i + j < total; ++j)
                    glints[i + j] += amp * static_cast<float>(std::sin(2.0 * pi * hz * j / sr) * std::exp(-static_cast<double>(j) / life));
            }
        }
        OnePole earlyLp, echoLp; earlyLp.set(r.earlyLp, sr); echoLp.set(r.echoLp, sr);
        OnePole lc, hc1, hc2; lc.set(r.lowCut, sr); hc1.set(r.highCut, sr); hc2.set(r.highCut, sr);
        Biquad body; body.peaking(sr, r.bodyHz, 0.8, r.bodyDb);
        for (std::size_t i = 0; i < total; ++i) {
            float v = tail[i] * tailScale + earlyLp.lp(taps[i]) + echoLp.lp(echoes[i]);
            v = hc2.lp(hc1.lp(lc.hp(v)));
            v = body.process(v) + glints[i] * tailScale * 7.0f;
            h[pre + i] = v;
        }
        if (r.reverse) {
            std::reverse(h.begin() + static_cast<long>(pre), h.end());
            const auto fadeIn = static_cast<std::size_t>(0.01 * sr), fadeOut = static_cast<std::size_t>(0.025 * sr);
            for (std::size_t i = 0; i < fadeIn && pre + i < h.size(); ++i) h[pre + i] *= static_cast<float>(i) / static_cast<float>(fadeIn);
            for (std::size_t i = 0; i < fadeOut && i < h.size(); ++i) h[h.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(fadeOut);
        } else {
            // A short fade at the very end (no truncation click).
            const auto fade = static_cast<std::size_t>(0.05 * sr);
            for (std::size_t i = 0; i < fade && i < h.size(); ++i) h[h.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);
        }
    }
    double energy = 0;
    for (const auto& h : out) for (float v : h) energy += double(v) * v;
    const float norm = static_cast<float>(std::sqrt(2.0 / std::max(1.0e-20, energy)));
    for (auto& h : out) for (auto& v : h) v *= norm;
    return out;
}

// The shared set: every kind's spectra for one sample rate, built on first use.
struct IrSet {
    std::vector<std::shared_ptr<const conv::IrSpectra>> owned;
    std::vector<std::array<const conv::IrSpectra*, 2>> table;
    int maxLength = conv::minLength;
};
std::shared_ptr<const IrSet> irSetFor(double sr) {
    static std::mutex mutex;
    static std::map<long, std::weak_ptr<const IrSet>> sets;
    std::lock_guard<std::mutex> lock(mutex);
    const long key = std::lround(sr);
    if (auto existing = sets[key].lock()) return existing;
    auto set = std::make_shared<IrSet>();
    set->table.resize(Vault::kinds, { nullptr, nullptr });
    for (int k = 1; k < Vault::kinds; ++k) {
        const auto ir = generate(recipes[k], sr, 0x5eed0000u + static_cast<std::uint32_t>(k) * 977u);
        for (int ch = 0; ch < 2; ++ch) {
            auto spectra = conv::buildSpectra(ir[static_cast<std::size_t>(ch)].data(), static_cast<int>(ir[static_cast<std::size_t>(ch)].size()));
            set->maxLength = std::max(set->maxLength, spectra->length);
            set->table[static_cast<std::size_t>(k)][static_cast<std::size_t>(ch)] = spectra.get();
            set->owned.push_back(std::move(spectra));
        }
    }
    std::shared_ptr<const IrSet> result = set;
    sets[key] = result;
    return result;
}
}

double Vault::kindSeconds(int k) noexcept { return k > 0 && k < kinds ? recipes[k].seconds + recipes[k].preMs * 0.001 : 0.0; }
const char* Vault::kindName(int k) noexcept {
    static const auto names = [] {
        std::array<std::array<char, 32>, kinds> out {};
        const char* s = vaultChoices;
        for (auto& name : out) {
            std::size_t i = 0;
            while (*s != 0 && *s != '|') { if (i + 1 < name.size()) name[i++] = *s; ++s; }
            if (*s == '|') ++s;
        }
        return out;
    }();
    return names[static_cast<std::size_t>(std::clamp(k, 0, kinds - 1))].data();
}

Vault::Vault() : left(std::make_unique<conv::Convolver>()), right(std::make_unique<conv::Convolver>()) {}
Vault::~Vault() = default;

void Vault::prepare(double sampleRate) {
    sr = sampleRate;
    auto set = irSetFor(sr);
    irs = &set->table;
    left->prepare(set->maxLength);
    right->prepare(set->maxLength);
    cache = set;
    for (auto& s : shifters) s.prepare(sr, 0.06);
    for (auto* s : { &shimmer, &toneHz, &duck }) s->setTime(0.05, sr);
    envAttack = timeCoefficient(0.01, sr);
    envRelease = timeCoefficient(0.35, sr);
    tailDecay = static_cast<float>(std::exp(-1.0 / (0.25 * sr)));
    reset();
}

void Vault::reset() {
    left->reset(); right->reset();
    for (auto& s : shifters) s.clear();
    shimmerLp = toneState1 = toneState2 = {};
    for (auto& f : back) f.fill(0.0f);
    envelope = 0; tail = 0;
    shimmer.snap(0); duck.snap(0); toneHz.value = -1;
}

void Vault::setKind(int k) noexcept {
    kind = std::clamp(k, 0, kinds - 1);
    const bool ok = irs != nullptr && kind > 0;
    left->setIr(ok ? (*irs)[static_cast<std::size_t>(kind)][0] : nullptr);
    right->setIr(ok ? (*irs)[static_cast<std::size_t>(kind)][1] : nullptr);
}

void Vault::process(const float* inL, const float* inR, float* wetL, float* wetR, int n, const Frame& p) noexcept {
    if (kind <= 0 || !left->hasIr()) {
        std::fill(wetL, wetL + n, 0.0f); std::fill(wetR, wetR + n, 0.0f);
        tail = 0;
        return;
    }
    shimmer.set(clamp01(p[VaultShimmer]));
    duck.set(clamp01(p[VaultDuck]));
    toneHz.set(std::clamp(p[VaultTone], 300.0f, 20000.0f));
    if (toneHz.value < 0) toneHz.snap(toneHz.target);
    const float toneCoefficient = poleForHz(toneHz.skip(n), sr);
    const bool toneOn = toneHz.value < 19000.0f;
    const auto N = static_cast<std::size_t>(n);
    // Input: the dry plus last chunk's wet an octave up (the shimmer cascade), each
    // side into its own decorrelated IR.
    for (std::size_t i = 0; i < N; ++i) {
        const float sh = shimmer.next();
        float fl = finiteOr(inL[i]), fr = finiteOr(inR[i]);
        if (sh > 1.0e-4f) {
            for (int ch = 0; ch < 2; ++ch) {
                const auto C = static_cast<std::size_t>(ch);
                float up = shifters[C].process(back[C][i], 2.0f);
                shimmerLp[C] += 0.25f * (up - shimmerLp[C]);   // tame the grain buzz
                (ch == 0 ? fl : fr) += 0.55f * sh * softLimit(up - 0.3f * shimmerLp[C], 1.0f);
            }
        }
        feed[0][i] = fl; feed[1][i] = fr;
        const float level = std::max(std::abs(inL[i]), std::abs(inR[i]));
        envelope += (level > envelope ? envAttack : envRelease) * (level - envelope);
    }
    left->process(feed[0].data(), wetL, n);
    right->process(feed[1].data(), wetR, n);
    for (std::size_t i = 0; i < N; ++i) {
        float l = wetL[i], r = wetR[i];
        back[0][i] = l; back[1][i] = r;
        if (toneOn) {
            toneState1[0] += toneCoefficient * (l - toneState1[0]); toneState2[0] += toneCoefficient * (toneState1[0] - toneState2[0]); l = toneState2[0];
            toneState1[1] += toneCoefficient * (r - toneState1[1]); toneState2[1] += toneCoefficient * (toneState1[1] - toneState2[1]); r = toneState2[1];
        }
        const float g = 1.0f - duck.next() * std::min(0.75f, envelope * 6.0f);
        l = finiteOr(l * g); r = finiteOr(r * g);
        wetL[i] = l; wetR[i] = r;
        tail = std::max(std::max(std::abs(l), std::abs(r)), tail * tailDecay);
    }
}
}
