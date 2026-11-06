#pragma once
// The sound engine: seven modules in a fixed chain, the place being travelled to, the
// travel itself, level trims and the level matcher. Real-time safe: everything is
// allocated in prepare(); setControls() and process() never allocate, lock or log.
#include "DSP/GainStage.h"
#include "DSP/Modules.h"
#include "World/Locations.h"

namespace dc {
// Everything the host controls that shapes the sound, read once per block.
struct Controls {
    int location = 0, sub = 0;
    // 0 neutral, 1 the place as designed; up to Engine::maxIntensity stronger (Intensity
    // times the Effects roller).
    float intensity = 1;
    std::array<float, macroCount> macros { 0.5f, 0.5f, 0.5f, 0.5f };
    // The Advanced drawer: for Lin/Log parameters an offset (-1..1 of the range; Log in
    // octaves of the whole range), for Step parameters an override (0 = the place's
    // own value, k = choice k - 1).
    Frame advanced {};
    double travelSeconds = 1.5;
    double bpm = 120;
    // Session 11, the MIX section: how much of each group of effects (1 = as designed,
    // 0 none, up to 1.5 more): Reverb / Space, Echoes, Modulation, Character.
    std::array<float, 4> mix { 1, 1, 1, 1 };
    // The reverbs' helpers: a low cut on their wet signal (<= 20 Hz: off), an offset to
    // their pre-delay (ms), and how far their tails dip while the input plays (0 = off).
    float reverbLowCut = 20, reverbPreDelay = 0, reverbDuck = 0;
};
enum MixGroup { MixReverb, MixEchoes, MixModulation, MixCharacter };

class Engine {
public:
    static constexpr double minTravelSeconds = 0.02;    // "Instant": still click-free
    static constexpr double crossfadeSeconds = 0.4;     // a choice that must crossfade (Choir interval, Tomb note...)

    void prepare(double sampleRate);
    // Clears every tail. Hosts call AudioProcessor::reset() on transport stops, so the
    // processor only calls this from prepareToPlay.
    void reset();
    // Constant for a given sample rate: Ancient Blood's oversampling and wow centre.
    int latencySamples() const noexcept { return blood.latencySamples(); }
    // Calibration only: no trims and no level matcher (the chain's raw level).
    void setLevelMatching(bool on) noexcept { matching = on; }

    // Quality (Milestone 11): 0 Eco (the Wraith in mono), 1 Standard, 2 High.
    void setQuality(int q) noexcept { wraith.setMono(q == 0); }
    // Once per host block, before process().
    void setControls(const Controls&) noexcept;
    // One chunk (n <= maxChunk) in place. dryL/dryR are the unprocessed input aligned
    // with the latency (the level matcher's reference). `addL`/`addR` (Session 15, optional):
    // DUNGEON SYNTH's hall, joined to the place's wet signal before the trims and the level
    // matcher, so the place and the hall are levelled together (null: exactly as before).
    void process(float* l, float* r, int n, const float* dryL, const float* dryR, const float* addL = nullptr, const float* addR = nullptr) noexcept;

    int destination() const noexcept { return place.location; }
    int destinationSub() const noexcept { return place.sub; }
    float travelProgress() const noexcept { return travel; }
    double travelDuration() const noexcept { return duration; }
    const Frame& frame() const noexcept { return current; }
    // Tests (Session 11): the level matcher's gain now.
    float matcherGain() const noexcept { return levelMatcher.gain; }

    // A place's frame with macros and Advanced applied, scaled by Intensity (0 neutral,
    // 1 as designed). Choices keep the place's value at any intensity.
    static Frame targetFrame(const Controls&) noexcept;
    static Frame targetFrame(int location, float intensity) noexcept;
    // Intensity above 1 (the Effects roller over 100 %, up to maxIntensity): stronger.
    static constexpr float maxIntensity = 1.5f;
    static void strengthen(Frame&, const Frame& place, float t) noexcept;
    // The MIX section on a frame: each group's amounts along the way from neutral to the
    // frame (t = 0 none, 1 as is, up to 1.5 further; bounded as strengthen()).
    static void applyMix(Frame&, const std::array<float, 4>& mix) noexcept;
    // Which group a module parameter belongs to (-1: none; times, rates, choices...). The
    // signature's amount joins the group its kind sounds like (`frame` gives the kind):
    // echoes under Echoes, wobbles, choruses and shimmers under Modulation, the reversed
    // swell and the vault's cascade under Reverb, the rest (resonances, drive...) Character.
    static int mixGroupOf(P id, const Frame& frame) noexcept;
    static int signatureGroup(int kind) noexcept;
    static Frame interpolate(const Frame& a, const Frame& b, float t) noexcept;
    // Layers whose choices cannot morph: during a journey that changes one, the layer
    // dips out, the choice switches while it is silent, and the layer returns.
    static void applyDips(Frame&, const Frame& a, const Frame& b, float linearT, double seconds) noexcept;

private:
    struct Slot { dsp::Ramp gain; bool idle = true; int warmUp = 0; };
    struct Place { int location = -1, sub = 0; bool operator!=(const Place& o) const noexcept { return location != o.location || sub != o.sub; } };
    template <class Module> void runSlot(Module&, Slot&, bool enabled, float* l, float* r, int n, const Frame* frame = nullptr) noexcept;
    void startTravel(double seconds) noexcept;

    double sr = 48000;
    Abyss abyss;
    AncientBlood blood;
    Relics relics;
    Choir choir;
    Wraith wraith;
    Echoes echoes;
    Ensemble ensemble;
    std::array<Slot, 6> slots;   // abyss, relics, choir, wraith, echoes, ensemble (Blood runs always: it carries the latency)
    // The Vault (Direction Update 2): two instances. A journey to another space starts it
    // clean in the other instance; the old one stops taking input and rings out.
    static constexpr int vaultCount = 2;
    void runVaults(float* l, float* r, int n, float eased) noexcept;
    std::array<Vault, vaultCount> vaults;
    std::array<bool, vaultCount> vaultIdle { true, true };
    std::array<dsp::Smoothed, vaultCount> vaultSend;
    std::array<int, vaultCount> vaultWarmUp {};
    int activeVault = 0;
    bool vaultCrossfade = false;
    float vaultFromSend = 0;
    dsp::Smoothed vaultDry;
    std::array<std::array<float, maxChunk>, 2> vaultIn {}, vaultWet {}, vaultSum {};
    // Signatures: a pool of three instances. A journey to another kind starts the new
    // kind in an idle instance and lets the old one ring out (its tails continue);
    // quick successive journeys never have to cut a tail that still rings.
    static constexpr int sigCount = 3;
    void runSignatures(float* l, float* r, int n, float eased) noexcept;
    void swapSignature() noexcept;
    bool anyDeparting() const noexcept;
    std::array<Signature, sigCount> signatures;
    std::array<Slot, sigCount> sigSlots;
    std::array<Frame, sigCount> sigFrames {};
    std::array<bool, sigCount> sigDeparting {};
    int activeSig = 0, latestDeparting = -1;
    bool sigCrossfade = false;
    float sigFromAmount = 0;
    // The Crypt runs as a small pool of rooms. A journey to a place with a different
    // room starts the new room in a fresh instance and lets the old one ring out with
    // its own decay (tails continue); within a place the active room follows the frame.
    static constexpr int roomCount = 3;
    void runRooms(float* l, float* r, int n, float eased) noexcept;
    std::array<Crypt, roomCount> rooms;
    std::array<Frame, roomCount> roomFrames {};
    std::array<bool, roomCount> roomIdle { true, true, true };
    std::array<int, roomCount> roomWarmUp {};
    int activeRoom = 0, departingRoom = -1;
    bool roomCrossfade = false;
    dsp::Smoothed roomDry;
    std::array<std::array<float, maxChunk>, 2> roomIn {}, roomWet {}, roomSum {};
    // Session 11: the reverbs' wet signals (rooms and vaults) shaped before they join:
    // a low cut, a pre-delay offset (the vaults; the rooms take it in their frame), and
    // the duck that dips them while the input plays.
    void shapeWet(std::array<std::array<float, maxChunk>, 2>& wet, int n, int which) noexcept;
    float lowCutTarget = 20, preDelayTarget = 0, duckAmount = 0, duckEnv = 0;
    std::array<float, 2> lowCutNow { 20, 20 };
    std::array<std::array<dsp::Svf, 2>, 2> lowCut {};
    std::array<dsp::DelayLine, 2> vaultDelay;
    dsp::Smoothed vaultDelayNow;
    std::array<float, maxChunk> duckGain {};
    // What the helpers removed this chunk (the level matcher measures as if they had not).
    std::array<std::array<float, maxChunk>, 2> removed {};
    bool removedAny = false;
    bool ducking = false;
    float duckAttack = 0, duckRelease = 0, duckSmooth = 0, duckDepth = 0;
    LevelMatcher levelMatcher;
    Context context;
    std::array<std::array<float, maxChunk>, 2> pre {};
    Frame from {}, to {}, current {};
    Place place;
    float travel = 1, trimFrom = 0, trimTo = 0, trimDb = 0;
    double duration = 1.5;
    dsp::Smoothed trimGain;
    bool matching = true;
    int warmUpSamples = 240, settleSamples = 24000, settle = 0;
};
}
