#include "Locations.h"
#include "LevelTrims.h"
#include <algorithm>
#include <cmath>

namespace dc {
namespace {
// Character (Chamber): 0 Forgotten Chapel, 1 Ancient Cathedral, 2 Underground Crypt, 3 Endless Void.
// Interval: 0 octave below, 1 fifth below, 2 fourth above, 3 fifth above, 4 octave above, 5 octave + fifth.
// MetalKind: 0 iron bars, 1 bell, 2 crystal. MetalOctave and TombOctave are indices (0 = lowest).
std::vector<LocationDesign> buildWorld() {
    std::vector<LocationDesign> w;

    //--------------------------------------------------------------------------
    w.push_back({ "Dungeon", "Corridors",
        // Stone corridors, damp darkness, torches on the walls.
        { { Cutoff, 4200 }, { Darkness, .4f }, { HighPass, 60 }, { Resonance, .1f },
          { Drive, 4 }, { BloodMix, .25f }, { Age, .1f },
          { Flicker, .22f }, { FlickerRate, 3.5f }, { Drips, .2f },
          { DelayTime, 230 }, { Feedback, .25f }, { DelayDamping, .7f }, { Diffusion, .6f }, { DelayMix, .12f },
          { Character, 2 }, { Space, .6f }, { Decay, 2.8f }, { Damping, .6f }, { PreDelay, 18 }, { ReverbMix, .4f } },
        {
          { "Entrance Hall", { { Cutoff, 7500 }, { Darkness, .2f }, { Flicker, .35f }, { Drips, .08f },
                               { DelayTime, 310 }, { DelayMix, .1f },
                               { Character, 1 }, { Space, .85f }, { Decay, 4.2f }, { Damping, .45f }, { PreDelay, 35 }, { ReverbMix, .45f } } },
          { "Prison Cells", { { Cutoff, 2200 }, { Resonance, .2f }, { Darkness, .5f }, { HighPass, 120 },
                              { Age, .3f }, { Dust, .2f }, { BloodMix, .3f },
                              { Metal, .35f }, { MetalKind, 0 }, { MetalOctave, 2 }, { MetalDecay, .6f }, { Drips, .3f }, { Flicker, .15f },
                              { DelayTime, 85 }, { Feedback, .3f }, { Diffusion, .2f }, { DelayMix, .14f },
                              { Character, 0 }, { Space, .15f }, { Decay, .9f }, { Damping, .7f }, { PreDelay, 4 }, { ReverbMix, .35f } } },
          { "Treasure Room", { { Cutoff, 20000 }, { Darkness, 0 }, { HighPass, 80 }, { Resonance, 0 }, { BloodMix, 0 }, { Drive, 0 },
                               { Metal, .25f }, { MetalKind, 2 }, { MetalOctave, 3 }, { MetalDecay, 1.8f }, { Flicker, .1f }, { Drips, 0 },
                               { Voices, 1 }, { Interval, 4 }, { ChoirBlend, .1f }, { Formant, .8f },
                               { DelayTime, 180 }, { Feedback, .35f }, { Spread, .9f }, { DelayDamping, .15f }, { DelayMix, .14f },
                               { Character, 1 }, { Space, .7f }, { Decay, 4.5f }, { Damping, .15f }, { PreDelay, 22 }, { Shimmer, .45f }, { ReverbMix, .42f } } },
          { "Final Boss Lair", { { Cutoff, 3200 }, { Darkness, .55f }, { Drive, 14 }, { BloodMix, .5f }, { Texture, .55f },
                                 { Voices, 3 }, { Interval, 0 }, { ChoirBlend, .3f }, { Formant, .2f }, { Drone, .25f },
                                 { Tomb, .2f }, { TombNote, 2 }, { TombOctave, 1 }, { Flicker, .3f }, { FlickerRate, 2 }, { Drips, 0 },
                                 { WraithMix, .1f },
                                 { DelayTime, 750 }, { Feedback, .45f }, { DelayDamping, .65f }, { Descent, 1 }, { DelayMix, .2f },
                                 { Character, 3 }, { Space, .95f }, { Decay, 8 }, { Damping, .55f }, { PreDelay, 60 }, { ReverbMix, .45f } } },
        },
        {
          { "Depth", { { Space, -.35f, .35f }, { Decay, -1, 1 }, { PreDelay, -12, 30 }, { ReverbMix, -.15f, .15f } } },
          { "Damp", { { Damping, -.3f, .3f }, { Cutoff, 1.5f, -1.5f }, { Darkness, -.2f, .3f }, { Drips, -.25f, .4f } } },
          { "Echo", { { DelayMix, -.12f, .2f }, { Feedback, -.2f, .25f }, { DelayTime, -.5f, .6f } } },
          { "Dread", { { Drive, -4, 12 }, { BloodMix, -.2f, .3f }, { Drone, 0, .35f }, { Tomb, 0, .3f }, { Cutoff, 1, -.7f }, { Darkness, -.3f, .2f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Castle", "Courtyard",
        // An open stone courtyard: a slap from the walls, a little wind.
        { { HighPass, 45 }, { Darkness, .05f }, { Wind, .1f },
          { DelayTime, 160 }, { Feedback, .15f }, { Diffusion, .4f }, { DelayDamping, .35f }, { Spread, .7f }, { DelayMix, .16f },
          { Character, 0 }, { Space, .7f }, { Decay, 1.8f }, { Damping, .35f }, { PreDelay, 25 }, { ReverbMix, .28f } },
        {
          { "Great Hall", { { Cutoff, 12000 }, { Darkness, .1f }, { Drive, 3 }, { Texture, .2f }, { BloodMix, .15f }, { Flicker, .12f }, { Wind, 0 },
                            { DelayTime, 120 }, { DelayMix, .06f },
                            { Character, 1 }, { Space, .85f }, { Decay, 3.2f }, { Damping, .45f }, { PreDelay, 32 }, { ReverbMix, .42f } } },
          { "Throne Room", { { HighPass, 40 }, { Wind, 0 }, { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .12f }, { Formant, .5f },
                             { DelayTime, 260 }, { Feedback, .2f }, { DelayMix, .08f },
                             { Character, 1 }, { Space, .95f }, { Decay, 4.5f }, { Damping, .3f }, { PreDelay, 45 }, { Shimmer, .15f }, { ReverbMix, .45f } } },
          { "Chapel", { { Cutoff, 9000 }, { Wind, 0 }, { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .22f }, { Formant, .55f }, { ChoirMovement, .4f }, { Drone, .1f },
                        { DelayMix, 0 },
                        { Character, 1 }, { Space, .9f }, { Decay, 7 }, { Damping, .5f }, { PreDelay, 50 }, { Shimmer, .1f }, { ReverbMix, .52f } } },
          { "Tower Top", { { HighPass, 200 }, { Cutoff, 6500 }, { Darkness, .2f }, { Wind, .35f },
                           { WraithMix, .12f }, { Blur, .6f },
                           { DelayTime, 480 }, { Feedback, .35f }, { DelayDamping, .6f }, { Diffusion, .5f }, { Spread, 1 }, { DelayMix, .25f },
                           { Character, 3 }, { Space, .4f }, { Decay, 2 }, { Damping, .5f }, { PreDelay, 90 }, { ReverbLowCut, 300 }, { ReverbMix, .2f } } },
        },
        {
          { "Hall Size", { { Space, -.35f, .3f }, { Decay, -1, 1 }, { PreDelay, -15, 30 } } },
          { "Grandeur", { { Shimmer, -.1f, .3f }, { ChoirBlend, -.1f, .2f }, { ReverbWidth, -.4f, 0 }, { Drone, 0, .15f } } },
          { "Distance", { { ReverbMix, -.2f, .25f }, { Cutoff, 1, -1.5f }, { DelayMix, -.08f, .12f }, { HighPass, -.5f, 1 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Cathedral", "Cloister",
        // Vaulted arcades around a garth: a long reverb, a slap from the arches.
        { { Cutoff, 9000 }, { Darkness, .15f }, { Wind, .08f },
          { DelayTime, 140 }, { Feedback, .18f }, { DelayMix, .08f },
          { Character, 0 }, { Space, .75f }, { Decay, 4 }, { Damping, .5f }, { PreDelay, 30 }, { ReverbMix, .4f } },
        {
          { "Nave", { { Cutoff, 10000 }, { Wind, 0 }, { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .06f }, { Formant, .55f }, { DelayMix, 0 },
                      { Character, 1 }, { Space, 1 }, { Decay, 11 }, { Damping, .45f }, { PreDelay, 70 }, { Shimmer, .08f }, { ReverbMix, .6f } } },
          { "Crypt", { { Cutoff, 1800 }, { Darkness, .6f }, { HighPass, 30 }, { Wind, 0 }, { Drips, .15f },
                       { Tomb, .3f }, { TombOctave, 1 }, { Drive, 3 }, { Age, .15f }, { BloodMix, .2f },
                       { Voices, 1 }, { Interval, 0 }, { ChoirBlend, .15f }, { Formant, .1f }, { DelayMix, 0 },
                       { Character, 2 }, { Space, .35f }, { Decay, 2.2f }, { Damping, .8f }, { PreDelay, 6 }, { ReverbMix, .45f } } },
          { "Bell Tower", { { HighPass, 90 }, { Cutoff, 14000 }, { Wind, .2f },
                            { Metal, .45f }, { MetalKind, 1 }, { MetalOctave, 1 }, { MetalDecay, 3.5f },
                            { DelayTime, 420 }, { Feedback, .3f }, { DelayMix, .14f },
                            { Character, 0 }, { Space, .6f }, { Decay, 3.5f }, { PreDelay, 40 }, { Shimmer, .1f }, { ReverbMix, .35f } } },
        },
        {
          { "Vastness", { { Space, -.4f, 0 }, { Decay, -1.3f, .8f }, { PreDelay, -20, 40 }, { ReverbMix, -.15f, .12f } } },
          { "Choir", { { ChoirBlend, -.15f, .25f }, { Drone, 0, .2f }, { Voices, 0, 1 } } },
          { "Bells", { { Metal, -.3f, .4f }, { MetalDecay, -1, 1 }, { Shimmer, -.08f, .2f } } },
          { "Gloom", { { Cutoff, 1, -2 }, { Darkness, -.15f, .4f }, { Damping, -.2f, .3f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Haunted Forest", "Forest Edge",
        // Damped open air under the trees, a faint spectral haze, wind in the leaves.
        { { Cutoff, 9000 }, { Darkness, .15f }, { Wind, .3f },
          { WraithMix, .15f }, { Blur, .5f }, { Ghost, .2f },
          { DelayTime, 380 }, { Feedback, .25f }, { DelayDamping, .6f }, { Diffusion, .8f }, { DelayMix, .15f },
          { Character, 0 }, { Space, .45f }, { Decay, 1.6f }, { Damping, .7f }, { ReverbMix, .18f } },
        {
          { "Old Path", { { Cutoff, 7000 }, { Darkness, .3f }, { FilterMovement, .2f }, { FilterRate, .07f }, { Wind, .25f },
                          { WraithMix, .4f }, { Blur, .72f }, { Ghost, .5f }, { WraithDetune, 30 }, { Drift, .55f },
                          { DelayTime, 560 }, { Feedback, .35f }, { DelayDamping, .75f }, { Diffusion, .7f }, { Reverse, 1 }, { DelayMix, .18f },
                          { Space, .5f }, { Decay, 2 }, { Damping, .75f }, { PreDelay, 20 } } },
          { "Witch's Hut", { { Cutoff, 9000 }, { Wind, 0 }, { Drive, 5 }, { BloodMix, .25f }, { Texture, .4f }, { Age, .25f }, { Flicker, .3f },
                             { Tomb, .12f }, { TombOctave, 2 }, { WraithMix, .1f }, { Ghost, .4f },
                             { DelayTime, 70 }, { Feedback, .2f }, { Diffusion, .3f }, { DelayMix, .1f },
                             { Space, .12f }, { Decay, .6f }, { Damping, .55f }, { PreDelay, 3 }, { ReverbMix, .35f } } },
          { "Ancient Grove", { { Cutoff, 14000 }, { Darkness, .05f }, { FilterMovement, .15f }, { FilterRate, .05f }, { Wind, .1f },
                               { WraithMix, .25f }, { Blur, .8f }, { Ghost, .2f }, { Drift, .3f },
                               { Voices, 2 }, { Interval, 4 }, { ChoirBlend, .1f }, { Formant, .7f },
                               { Character, 3 }, { Space, .7f }, { Decay, 6 }, { Damping, .3f }, { PreDelay, 40 }, { Shimmer, .45f }, { ReverbMix, .42f } } },
        },
        {
          { "Mist", { { WraithMix, -.12f, .3f }, { Blur, -.3f, .2f }, { Drift, -.2f, .3f } } },
          { "Whispers", { { Ghost, -.2f, .4f }, { DelayMix, -.1f, .15f }, { Feedback, -.15f, .2f } } },
          { "Wind", { { Wind, -.3f, .4f }, { FilterMovement, -.15f, .3f }, { HighPass, 0, 1.5f } } },
          { "Shadows", { { Cutoff, 1, -1.5f }, { Darkness, -.1f, .35f }, { Decay, -.5f, .8f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Caverns", "Cave Mouth",
        // Rock and long, dripping echoes.
        { { HighPass, 60 }, { Cutoff, 9000 }, { Drips, .35f }, { Wind, .1f },
          { DelayTime, 290 }, { Feedback, .4f }, { Diffusion, .3f }, { DelayDamping, .35f }, { Spread, .85f }, { DelayMix, .2f },
          { Character, 1 }, { Space, .85f }, { Decay, 5 }, { Damping, .3f }, { PreDelay, 55 }, { ReverbMix, .4f } },
        {
          { "Crystal Cavern", { { Cutoff, 20000 }, { HighPass, 120 }, { Wind, 0 }, { Drips, .4f },
                                { Metal, .35f }, { MetalKind, 2 }, { MetalOctave, 3 }, { MetalDecay, 2.5f },
                                { DelayTime, 210 }, { DelayDamping, .1f }, { Spread, 1 }, { DelayMix, .14f },
                                { Space, .75f }, { Decay, 5.5f }, { Damping, .1f }, { Shimmer, .3f }, { ReverbMix, .42f } } },
          { "Underground Lake", { { Cutoff, 2400 }, { Resonance, .25f }, { FilterMovement, .35f }, { FilterRate, .12f }, { Darkness, .3f }, { Wind, 0 },
                                  { Instability, .35f }, { BloodMix, .25f }, { Drips, .5f },
                                  { WraithMix, .2f }, { Blur, .6f }, { Drift, .6f },
                                  { DelayTime, 650 }, { Feedback, .55f }, { DelayDamping, .8f }, { Diffusion, .85f }, { DelayMix, .25f },
                                  { Character, 3 }, { Space, .8f }, { Decay, 9 }, { Damping, .7f }, { PreDelay, 80 }, { ReverbMix, .5f } } },
          { "Dragon's Hoard", { { Cutoff, 2600 }, { Darkness, .6f }, { HighPass, 25 }, { Wind, 0 }, { Drips, 0 },
                                { Drive, 12 }, { BloodMix, .45f }, { Texture, .6f },
                                { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .25f }, { Formant, .15f }, { Drone, .35f }, { DroneHold, 8 },
                                { Tomb, .25f }, { TombOctave, 0 },
                                { DelayTime, 900 }, { Feedback, .3f }, { DelayMix, .1f },
                                { Character, 3 }, { Space, 1 }, { Decay, 10 }, { Damping, .65f }, { PreDelay, 70 }, { ReverbMix, .48f } } },
        },
        {
          { "Depth", { { Space, -.35f, .15f }, { Decay, -1, 1 }, { PreDelay, -25, 40 } } },
          { "Drips", { { Drips, -.35f, .45f } } },
          { "Echo", { { DelayMix, -.15f, .2f }, { Feedback, -.25f, .25f }, { DelayTime, -.6f, .6f } } },
          { "Rumble", { { Drone, 0, .35f }, { Drive, -4, 10 }, { BloodMix, -.15f, .25f }, { Tomb, 0, .25f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Village", "Village Green",
        { { HighPass, 50 }, { Wind, .12f },
          { DelayTime, 140 }, { Feedback, .1f }, { Diffusion, .3f }, { DelayMix, .14f },
          { Character, 0 }, { Space, .35f }, { Decay, 1 }, { Damping, .5f }, { ReverbMix, .15f } },
        {
          { "Tavern", { { Cutoff, 12000 }, { Darkness, .1f }, { Wind, 0 }, { Drive, 4 }, { Texture, .2f }, { Age, .08f }, { BloodMix, .3f }, { Flicker, .15f },
                        { DelayTime, 105 }, { Feedback, .08f }, { DelayDamping, .55f }, { DelayMix, .08f },
                        { Space, .22f }, { Decay, .9f }, { Damping, .65f }, { PreDelay, 6 }, { ReverbMix, .24f } } },
          { "Market Square", { { HighPass, 70 }, { DelayTime, 110 }, { Feedback, .12f }, { Spread, .8f }, { Diffusion, .2f }, { DelayMix, .2f },
                               { Space, .45f }, { Decay, 1.3f }, { PreDelay, 15 }, { ReverbMix, .2f } } },
          { "Blacksmith", { { Cutoff, 11000 }, { Wind, 0 }, { Drive, 12 }, { BloodMix, .5f }, { Texture, .7f }, { Dust, .2f }, { Flicker, .35f },
                            { Metal, .3f }, { MetalKind, 0 }, { MetalOctave, 3 }, { MetalDecay, .9f },
                            { Space, .25f }, { Decay, .9f }, { ReverbMix, .25f } } },
        },
        {
          { "Warmth", { { Drive, -3, 6 }, { Texture, -.15f, .3f }, { Cutoff, 1, -1 }, { Damping, -.2f, .25f } } },
          { "Bustle", { { DelayMix, -.1f, .15f }, { Space, -.15f, .25f }, { ReverbMix, -.1f, .15f } } },
          { "Grit", { { Dust, 0, .4f }, { Age, 0, .4f }, { Metal, 0, .25f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Swamp", "Mire",
        { { Cutoff, 2400 }, { Resonance, .2f }, { Darkness, .25f }, { FilterMovement, .25f }, { FilterRate, .2f },
          { Instability, .3f }, { BloodMix, .25f }, { Drips, .2f },
          { DelayTime, 380 }, { Feedback, .3f }, { DelayDamping, .75f }, { Diffusion, .5f }, { DelayMix, .18f },
          { Character, 2 }, { Space, .45f }, { Decay, 2 }, { Damping, .75f }, { ReverbMix, .22f } },
        {
          { "Bog", { { Cutoff, 1500 }, { Resonance, .32f }, { Slope, 1 }, { Darkness, .3f }, { FilterMovement, .4f }, { FilterRate, .3f },
                     { Drive, 4 }, { Instability, .5f }, { Age, .22f }, { BloodMix, .4f }, { Drips, .3f },
                     { DelayTime, 430 }, { Feedback, .38f }, { DelayDamping, .8f }, { DelayMix, .22f },
                     { Space, .5f }, { Decay, 2.4f }, { Damping, .8f }, { ReverbMix, .26f } } },
          { "Sunken Ruins", { { Cutoff, 900 }, { Resonance, .35f }, { Slope, 1 }, { FilterMovement, .3f },
                              { WraithMix, .2f }, { Blur, .7f },
                              { DelayTime, 520 }, { Feedback, .5f }, { DelayDamping, .9f }, { Diffusion, .8f }, { DelayMix, .3f },
                              { Space, .7f }, { Decay, 3.5f }, { Damping, .9f }, { ReverbMix, .4f } } },
        },
        {
          { "Murk", { { Cutoff, 1.5f, -1.5f }, { Resonance, -.15f, .2f }, { Darkness, -.2f, .3f } } },
          { "Wobble", { { Instability, -.25f, .4f }, { FilterMovement, -.2f, .4f } } },
          { "Damp", { { Damping, -.3f, .15f }, { DelayMix, -.12f, .15f }, { Drips, -.2f, .4f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Mountains", "Foothills",
        { { HighPass, 80 }, { Wind, .2f },
          { DelayTime, 420 }, { Feedback, .3f }, { Diffusion, .3f }, { DelayDamping, .45f }, { Spread, .8f }, { DelayMix, .2f },
          { Character, 0 }, { Space, .6f }, { Decay, 2.2f }, { Damping, .35f }, { PreDelay, 40 }, { ReverbMix, .2f } },
        {
          { "Mountain Pass", { { HighPass, 110 }, { DelayTime, 640 }, { Feedback, .42f }, { Diffusion, .2f }, { Spread, .9f }, { DelayMix, .3f },
                               { Character, 3 }, { Space, .65f }, { Decay, 4.5f }, { Damping, .25f }, { PreDelay, 80 }, { ReverbLowCut, 220 }, { ReverbMix, .22f } } },
          { "Frozen Peak", { { HighPass, 350 }, { Cutoff, 15000 }, { Wind, .4f },
                             { WraithMix, .2f }, { Blur, .4f }, { Ghost, .2f },
                             { DelayTime, 900 }, { Feedback, .3f }, { DelayMix, .15f },
                             { Character, 3 }, { Space, .5f }, { Decay, 3 }, { Damping, .1f }, { PreDelay, 100 }, { Shimmer, .3f }, { ReverbLowCut, 400 }, { ReverbMix, .3f } } },
          { "Dwarven Hall", { { HighPass, 30 }, { Cutoff, 6000 }, { Darkness, .3f }, { Wind, 0 }, { Drive, 5 }, { BloodMix, .2f }, { Flicker, .2f },
                              { Tomb, .3f }, { TombOctave, 1 }, { Metal, .1f }, { MetalKind, 0 }, { MetalOctave, 1 },
                              { Voices, 1 }, { Interval, 0 }, { ChoirBlend, .12f }, { DelayMix, .05f },
                              { Character, 1 }, { Space, .9f }, { Decay, 4.5f }, { Damping, .5f }, { PreDelay, 40 }, { ReverbMix, .45f } } },
        },
        {
          { "Altitude", { { HighPass, -.8f, 1.5f }, { Wind, -.2f, .3f }, { Darkness, .1f, 0 } } },
          { "Echo", { { DelayMix, -.15f, .2f }, { Feedback, -.2f, .25f }, { DelayTime, -.6f, .5f } } },
          { "Frost", { { Shimmer, 0, .35f }, { Damping, .2f, -.2f }, { Blur, 0, .3f }, { WraithMix, 0, .15f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Necropolis", "Gates",
        { { HighPass, 100 }, { Darkness, .1f }, { Wind, .15f },
          { WraithMix, .2f }, { Blur, .5f }, { Ghost, .35f },
          { DelayTime, 520 }, { Feedback, .25f }, { DelayMix, .12f },
          { Character, 0 }, { Space, .55f }, { Decay, 2.6f }, { Damping, .3f }, { ReverbMix, .26f } },
        {
          { "Graveyard", { { HighPass, 140 }, { WraithMix, .38f }, { Blur, .55f }, { Ghost, .55f }, { WraithDetune, 30 }, { Drift, .6f },
                           { DelayTime, 700 }, { Feedback, .3f }, { Reverse, 1 }, { DelayMix, .14f },
                           { Space, .6f }, { Decay, 3.8f }, { Damping, .2f }, { ReverbMix, .3f } } },
          { "Mausoleum", { { HighPass, 50 }, { Cutoff, 3500 }, { Darkness, .4f }, { Wind, 0 }, { Drips, .15f },
                           { Tomb, .2f }, { TombOctave, 2 }, { WraithMix, .15f }, { Ghost, .5f }, { DelayMix, .05f },
                           { Character, 2 }, { Space, .25f }, { Decay, 1.5f }, { Damping, .6f }, { PreDelay, 5 }, { ReverbMix, .45f } } },
        },
        {
          { "Chill", { { HighPass, -.6f, 1 }, { Damping, .15f, -.2f }, { Shimmer, 0, .2f } } },
          { "Ghosts", { { WraithMix, -.15f, .3f }, { Ghost, -.25f, .35f }, { Drift, -.2f, .3f } } },
          { "Silence", { { ReverbMix, .1f, -.15f }, { DelayMix, .08f, -.1f }, { Space, .15f, -.25f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Wizard's Tower", "Stairwell",
        { { Cutoff, 9000 }, { Flicker, .2f },
          { DelayTime, 95 }, { Feedback, .35f }, { Diffusion, .7f }, { DelayMix, .15f },
          { Character, 0 }, { Space, .5f }, { Decay, 2.2f }, { Damping, .45f }, { ReverbMix, .35f } },
        {
          { "Library", { { Cutoff, 5000 }, { Darkness, .35f }, { HighPass, 80 }, { Flicker, .1f },
                         { BloodMix, .15f }, { Age, .2f }, { Dust, .15f }, { DelayMix, 0 },
                         { Space, .3f }, { Decay, .8f }, { Damping, .8f }, { ReverbMix, .2f } } },
          { "Alchemy Lab", { { Cutoff, 3000 }, { Resonance, .45f }, { FilterMovement, .5f }, { FilterRate, 1.5f },
                             { Instability, .5f }, { BloodMix, .3f }, { Drips, .3f },
                             { Voices, 2 }, { Interval, 3 }, { ChoirBlend, .25f }, { ChoirDetune, 35 }, { ChoirMovement, .8f },
                             { DelayTime, 190 }, { Feedback, .4f }, { Descent, 2 }, { DelayMix, .18f },
                             { Space, .35f }, { Decay, 1.4f }, { ReverbMix, .25f } } },
          { "Observatory", { { Cutoff, 20000 }, { Flicker, 0 }, { Voices, 2 }, { Interval, 4 }, { ChoirBlend, .18f }, { ChoirDetune, 18 }, { ChoirMovement, .5f },
                             { WraithMix, .18f }, { Ghost, .3f }, { Blur, .4f },
                             { Metal, .12f }, { MetalKind, 2 }, { MetalOctave, 4 }, { MetalDecay, 4 },
                             { DelayTime, 375 }, { DelayMix, .16f },
                             { Character, 3 }, { Space, 1 }, { Decay, 12 }, { Shimmer, .55f }, { Damping, .3f }, { ReverbMix, .5f } } },
        },
        {
          { "Arcana", { { Shimmer, -.1f, .35f }, { ChoirBlend, -.1f, .2f }, { Ghost, 0, .3f } } },
          { "Alchemy", { { FilterMovement, -.2f, .4f }, { Instability, -.2f, .4f }, { ChoirDetune, -10, 20 } } },
          { "Starlight", { { Space, -.3f, .2f }, { Decay, -1, 1 }, { Metal, 0, .2f } } },
        } });
    return w;
}

const std::vector<LocationDesign> world = buildWorld();

float clampToSpec(P id, float v) noexcept { return std::clamp(v, moduleSpecs[id].min, moduleSpecs[id].max); }
}

const LocationDesign& design(int location) noexcept {
    return world[static_cast<std::size_t>(std::clamp(location, 0, static_cast<int>(LocationCount) - 1))];
}

int subLocationCount(int location) noexcept { return static_cast<int>(design(location).subs.size()); }
int clampSubLocation(int location, int sub) noexcept { return std::clamp(sub, 0, subLocationCount(location)); }

const char* subLocationName(int location, int sub) noexcept {
    const auto& d = design(location);
    sub = clampSubLocation(location, sub);
    return sub == 0 ? d.scene : d.subs[static_cast<std::size_t>(sub - 1)].name;
}

int macroCountFor(int location) noexcept { return static_cast<int>(design(location).macros.size()); }
const char* macroName(int location, int macro) noexcept {
    const auto& d = design(location);
    return macro >= 0 && macro < static_cast<int>(d.macros.size()) ? d.macros[static_cast<std::size_t>(macro)].name : "";
}

Frame placeFrame(int location, int sub) noexcept {
    const auto& d = design(location);
    auto f = neutralFrame();
    for (const auto& c : d.base) f[c.id] = clampToSpec(c.id, c.value);
    sub = clampSubLocation(location, sub);
    if (sub > 0)
        for (const auto& c : d.subs[static_cast<std::size_t>(sub - 1)].changes) f[c.id] = clampToSpec(c.id, c.value);
    return f;
}

void applyMacros(Frame& f, int location, const std::array<float, macroCount>& values) noexcept {
    const auto& d = design(location);
    for (std::size_t m = 0; m < d.macros.size() && m < values.size(); ++m) {
        const float v = std::clamp(values[m], 0.0f, 1.0f);
        if (std::abs(v - 0.5f) < 1.0e-6f) continue;
        const float down = v < 0.5f ? (0.5f - v) * 2.0f : 0.0f, up = v > 0.5f ? (v - 0.5f) * 2.0f : 0.0f;
        for (const auto& t : d.macros[m].targets) {
            const float offset = t.down * down + t.up * up;
            const auto& spec = moduleSpecs[t.id];
            float value = f[t.id];
            if (spec.glide == Glide::Log) value *= std::exp2(offset);
            else if (spec.glide == Glide::Step) value = std::round(value + offset);
            else value += offset;
            f[t.id] = clampToSpec(t.id, value);
        }
    }
}

float placeTrimDb(int location, int sub) noexcept {
    location = std::clamp(location, 0, static_cast<int>(LocationCount) - 1);
    sub = clampSubLocation(location, sub);
    return levelTrimsDb[static_cast<std::size_t>(location)][static_cast<std::size_t>(sub)];
}
}
