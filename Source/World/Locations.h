#pragma once
// The world's locations. Milestone 0 gives each one a first sketch of its sound (a
// Frame of module settings); Milestone 1 adds sub-locations and macros, Milestone 2
// replaces these sketches with the real sound design.
#include "DSP/ModuleParams.h"
#include <array>
#include <initializer_list>
#include <utility>

namespace dc {
enum Location : int {
    Dungeon, Castle, Cathedral, HauntedForest, Caverns, Village, Swamp, Mountains, Necropolis, WizardsTower,
    LocationCount
};

// Host-visible names, in parameter order. Never reorder: automation stores the index.
inline constexpr std::array<const char*, LocationCount> locationNames {
    "Dungeon", "Castle", "Cathedral", "Haunted Forest", "Caverns",
    "Village", "Swamp", "Mountains", "Necropolis", "Wizard's Tower"
};

namespace detail {
inline Frame with(std::initializer_list<std::pair<P, float>> changes) noexcept {
    auto f = neutralFrame();
    for (const auto& [id, value] : changes) f[id] = value;
    return f;
}
}

// Character values: 0 Forgotten Chapel, 1 Ancient Cathedral, 2 Underground Crypt, 3 Endless Void.
inline Frame locationFrame(int location) noexcept {
    using detail::with;
    switch (location) {
        case Dungeon:   // stone, damp darkness
            return with({ { Cutoff, 3800 }, { Darkness, .45f }, { HighPass, 55 }, { Resonance, .12f },
                          { Drive, 5 }, { BloodMix, .3f }, { Age, .12f },
                          { WraithMix, .12f }, { Blur, .45f }, { Ghost, .15f },
                          { Character, 2 }, { Space, .65f }, { Decay, 3.2f }, { Damping, .62f }, { PreDelay, 14 }, { ReverbMix, .42f } });
        case Castle:    // a grand, bright stone hall with a slap from the far wall
            return with({ { HighPass, 45 }, { Darkness, .05f },
                          { DelayTime, 190 }, { Feedback, .18f }, { Diffusion, .55f }, { DelayDamping, .35f }, { Spread, .7f }, { DelayMix, .16f },
                          { Character, 0 }, { Space, .8f }, { Decay, 2.4f }, { Damping, .3f }, { PreDelay, 28 }, { ReverbMix, .36f } });
        case Cathedral: // very long, dark gothic reverb, a faint choir
            return with({ { Cutoff, 7000 }, { Darkness, .3f },
                          { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .14f }, { Formant, .55f }, { Drone, .08f },
                          { Character, 1 }, { Space, 1 }, { Decay, 9.5f }, { Damping, .55f }, { PreDelay, 55 }, { Shimmer, .1f }, { ReverbMix, .52f } });
        case HauntedForest: // damped open air, eerie spectral blur
            return with({ { Cutoff, 6500 }, { Darkness, .22f }, { FilterMovement, .22f }, { FilterRate, .07f },
                          { WraithMix, .32f }, { Blur, .72f }, { Ghost, .35f }, { WraithDetune, 22 }, { Drift, .55f },
                          { DelayTime, 520 }, { Feedback, .32f }, { DelayDamping, .7f }, { Diffusion, .7f }, { DelayMix, .18f },
                          { Character, 0 }, { Space, .55f }, { Decay, 2.4f }, { Damping, .7f }, { ReverbMix, .22f } });
        case Caverns:   // vast rock, long dripping echoes
            return with({ { HighPass, 70 }, { Cutoff, 9000 },
                          { DelayTime, 290 }, { Feedback, .42f }, { Diffusion, .3f }, { DelayDamping, .35f }, { Spread, .85f }, { DelayMix, .2f },
                          { Character, 1 }, { Space, .9f }, { Decay, 5.5f }, { Damping, .3f }, { PreDelay, 62 }, { ReverbMix, .42f } });
        case Village:   // a warm wooden tavern room
            return with({ { Cutoff, 12000 }, { Darkness, .1f },
                          { Drive, 4 }, { Texture, .2f }, { Age, .08f }, { BloodMix, .3f },
                          { DelayTime, 105 }, { Feedback, .08f }, { DelayDamping, .55f }, { DelayMix, .08f },
                          { Character, 0 }, { Space, .22f }, { Decay, .9f }, { Damping, .65f }, { PreDelay, 6 }, { ReverbMix, .24f } });
        case Swamp:     // murky, low-passed, wobbling
            return with({ { Cutoff, 1500 }, { Resonance, .32f }, { Slope, 1 }, { Darkness, .3f }, { FilterMovement, .4f }, { FilterRate, .3f },
                          { Drive, 4 }, { Instability, .5f }, { Age, .22f }, { BloodMix, .4f },
                          { DelayTime, 430 }, { Feedback, .38f }, { DelayDamping, .8f }, { Diffusion, .5f }, { DelayMix, .22f },
                          { Character, 2 }, { Space, .5f }, { Decay, 2.4f }, { Damping, .8f }, { ReverbMix, .26f } });
        case Mountains: // long open echoes, thin cold air
            return with({ { HighPass, 110 },
                          { DelayTime, 640 }, { Feedback, .42f }, { Diffusion, .2f }, { DelayDamping, .45f }, { Spread, .9f }, { DelayMix, .3f },
                          { Character, 3 }, { Space, .65f }, { Decay, 4.5f }, { Damping, .25f }, { PreDelay, 80 }, { ReverbLowCut, 220 }, { ReverbMix, .22f } });
        case Necropolis: // cold, sparse, ghostly
            return with({ { HighPass, 140 }, { Darkness, .1f },
                          { WraithMix, .38f }, { Blur, .55f }, { Ghost, .55f }, { WraithDetune, 30 }, { Drift, .6f },
                          { DelayTime, 700 }, { Feedback, .3f }, { Reverse, 1 }, { DelayMix, .14f },
                          { Character, 0 }, { Space, .6f }, { Decay, 3.8f }, { Damping, .2f }, { ReverbMix, .3f } });
        case WizardsTower: // strange shimmering arcana
            return with({ { Voices, 2 }, { Interval, 4 }, { ChoirBlend, .18f }, { ChoirDetune, 18 }, { ChoirMovement, .5f },
                          { WraithMix, .18f }, { Ghost, .3f }, { Blur, .4f },
                          { DelayTime, 375 }, { Feedback, .35f }, { DelayMix, .16f },
                          { Character, 1 }, { Space, .8f }, { Decay, 6.5f }, { Shimmer, .4f }, { Damping, .3f }, { ReverbMix, .36f } });
        default:
            return neutralFrame();
    }
}
}
