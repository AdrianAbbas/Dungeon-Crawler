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

    // Milestone 2a sound design (Dungeon, Castle, Cathedral, Haunted Forest, Caverns):
    // every place has a signature of its own (see docs/DEVLOG.md, Milestone 2a).
    //--------------------------------------------------------------------------
    w.push_back({ "Dungeon", "Corridors",
        // Long stone corridors: a dark, damp, mid-sized stone room, a short slap down
        // the passage, torches flickering, water dripping somewhere.
        { { Cutoff, 4500 }, { Darkness, .4f }, { HighPass, 55 }, { Resonance, .1f },
          { Drive, 3 }, { BloodMix, .2f }, { Age, .08f },
          { Flicker, .2f }, { FlickerRate, 3.5f }, { Drips, .22f },
          { DelayTime, 140 }, { Feedback, .2f }, { DelayDamping, .75f }, { Diffusion, .75f }, { Spread, .5f }, { DelayMix, .12f },
          { Character, 2 }, { Space, .55f }, { Decay, 2.5f }, { Damping, .62f }, { PreDelay, 14 }, { ReverbMix, .4f } },
        {
          // A vaulted entrance: bigger and lighter, the torches brighter, no slap.
          { "Entrance Hall", { { Cutoff, 6000 }, { Darkness, .3f }, { HighPass, 45 }, { Flicker, .45f }, { FlickerRate, 2.6f }, { Drips, .06f },
                               { DelayTime, 310 }, { Feedback, .1f }, { Diffusion, .2f }, { DelayDamping, .6f }, { Spread, .9f }, { DelayMix, .1f },
                               { Character, 2 }, { Space, .9f }, { Decay, 3.5f }, { Damping, .55f }, { PreDelay, 38 }, { ReverbMix, .45f } } },
          // A cramped cell: boxy and muffled, iron bars ringing, a flutter between the
          // walls, grime on everything.
          { "Prison Cells", { { Cutoff, 1900 }, { Resonance, .25f }, { Darkness, .45f }, { HighPass, 130 },
                              { Drive, 5 }, { Age, .35f }, { Dust, .25f }, { BloodMix, .35f },
                              { Metal, .42f }, { MetalKind, 0 }, { MetalNote, 2 }, { MetalOctave, 2 }, { MetalDecay, .7f }, { Drips, .3f }, { Flicker, .12f },
                              { DelayTime, 55 }, { Feedback, .42f }, { Diffusion, .15f }, { DelayDamping, .5f }, { Spread, .3f }, { DelayMix, .16f },
                              { Character, 0 }, { Space, .08f }, { Decay, .7f }, { Damping, .7f }, { PreDelay, 2 }, { ReverbMix, .38f } } },
          // Gold everywhere: bright glittering reflections, crystal glints, an octave of
          // sparkle above, shimmer, ping-pong echoes on the eighth.
          { "Treasure Room", { { Cutoff, 20000 }, { Darkness, 0 }, { HighPass, 90 }, { Resonance, 0 }, { BloodMix, 0 }, { Drive, 0 }, { Age, 0 },
                               { Metal, .22f }, { MetalKind, 2 }, { MetalNote, 9 }, { MetalOctave, 4 }, { MetalDecay, 1.4f }, { Flicker, .1f }, { Drips, 0 },
                               { Voices, 1 }, { Interval, 4 }, { ChoirBlend, .12f }, { Formant, .85f }, { ChoirDetune, 6 },
                               { DelayTime, 250 }, { DelaySync, 2 }, { Feedback, .38f }, { Spread, 1 }, { DelayDamping, .12f }, { Diffusion, .2f }, { DelayMix, .16f },
                               { Character, 0 }, { Space, .55f }, { Decay, 3 }, { Damping, .12f }, { PreDelay, 18 }, { Shimmer, .4f }, { ReverbMix, .38f } } },
          // The lair: vast and menacing. A distorted, darkened instrument, a choir a
          // fifth below with a held drone, a low resonance, slow echoes that sink a
          // semitone each time, an endless void of a room.
          { "Final Boss Lair", { { Cutoff, 3000 }, { Darkness, .55f }, { HighPass, 30 }, { Drive, 18 }, { BloodMix, .55f }, { Texture, .6f },
                                 { Voices, 3 }, { Interval, 1 }, { ChoirBlend, .3f }, { Formant, .2f }, { ChoirDetune, 22 }, { Drone, .22f },
                                 { Tomb, .25f }, { TombNote, 2 }, { TombOctave, 1 }, { Flicker, .3f }, { FlickerRate, 1.6f }, { Drips, 0 },
                                 { WraithMix, .1f }, { Blur, .5f },
                                 { DelayTime, 760 }, { Feedback, .45f }, { DelayDamping, .65f }, { Descent, 1 }, { Diffusion, .5f }, { DelayMix, .2f },
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
        // An open courtyard between high walls: a crisp slap echo off the far wall,
        // a short open reverb, a breeze.
        { { HighPass, 70 }, { Darkness, .05f }, { Wind, .22f },
          { DelayTime, 210 }, { Feedback, .1f }, { Diffusion, .1f }, { DelayDamping, .3f }, { Spread, 1 }, { DelayMix, .22f },
          { Character, 0 }, { Space, .6f }, { Decay, 1.5f }, { Damping, .35f }, { PreDelay, 22 }, { ReverbMix, .24f } },
        {
          // Timber roof over stone: warm, wide, medium-large, a fire in the hearth.
          { "Great Hall", { { HighPass, 35 }, { Cutoff, 10000 }, { Darkness, .12f }, { Drive, 3 }, { Texture, .2f }, { BloodMix, .15f },
                            { Flicker, .1f }, { FlickerRate, 2.2f }, { Wind, 0 },
                            { DelayMix, 0 },
                            { Character, 1 }, { Space, .7f }, { Decay, 2.8f }, { Damping, .62f }, { PreDelay, 20 }, { ReverbMix, .4f } } },
          // Regal: a grand, bright, very wide hall, an organ-like fifth above, a halo.
          { "Throne Room", { { HighPass, 40 }, { Wind, 0 },
                             { Voices, 2 }, { Interval, 3 }, { ChoirBlend, .11f }, { Formant, .5f }, { ChoirDetune, 8 }, { ChoirMovement, .2f },
                             { DelayTime, 260 }, { Feedback, .18f }, { Diffusion, .6f }, { DelayMix, .07f },
                             { Character, 1 }, { Space, .95f }, { Decay, 4.2f }, { Damping, .3f }, { PreDelay, 50 }, { Shimmer, .14f }, { ReverbWidth, 1 }, { ReverbMix, .45f } } },
          // Sacred: a long reverb and a soft choir an octave below with a drone.
          { "Chapel", { { Cutoff, 8500 }, { Darkness, .1f }, { Wind, 0 },
                        { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .22f }, { Formant, .55f }, { ChoirMovement, .4f }, { Drone, .12f }, { DroneHold, 6 },
                        { DelayMix, 0 },
                        { Character, 1 }, { Space, .85f }, { Decay, 6.5f }, { Damping, .5f }, { PreDelay, 45 }, { Shimmer, .08f }, { ReverbMix, .5f } } },
          // On the battlements: thin, windy, open, distant echoes across the valley.
          { "Tower Top", { { HighPass, 230 }, { Cutoff, 6000 }, { Darkness, .2f }, { Wind, .42f },
                           { WraithMix, .12f }, { Blur, .6f },
                           { DelayTime, 520 }, { Feedback, .35f }, { DelayDamping, .6f }, { Diffusion, .5f }, { Spread, 1 }, { DelayMix, .26f },
                           { Character, 3 }, { Space, .35f }, { Decay, 2 }, { Damping, .5f }, { PreDelay, 100 }, { ReverbLowCut, 320 }, { ReverbMix, .2f } } },
        },
        {
          { "Hall Size", { { Space, -.35f, .3f }, { Decay, -1, 1 }, { PreDelay, -15, 30 } } },
          { "Grandeur", { { Shimmer, -.1f, .3f }, { ChoirBlend, -.1f, .2f }, { ReverbWidth, -.4f, 0 }, { Drone, 0, .15f } } },
          { "Distance", { { ReverbMix, -.2f, .25f }, { Cutoff, 1, -1.5f }, { DelayMix, -.08f, .12f }, { HighPass, -.5f, 1 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Cathedral", "Cloister",
        // Arcades around a garden: a flutter of reflections between the arches,
        // feeding a long stone reverb.
        { { Cutoff, 12000 }, { Darkness, .05f }, { Wind, .1f },
          { DelayTime, 92 }, { Feedback, .45f }, { Diffusion, .7f }, { DelayDamping, .4f }, { Spread, .6f }, { DelayMix, .16f },
          { Character, 1 }, { Space, .6f }, { Decay, 4.5f }, { Damping, .5f }, { PreDelay, 28 }, { ReverbMix, .38f } },
        {
          // The nave: the longest, wettest place in the world; a breath of choir.
          { "Nave", { { Cutoff, 10000 }, { Wind, 0 }, { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .06f }, { Formant, .55f }, { DelayMix, 0 },
                      { Character, 1 }, { Space, 1 }, { Decay, 12 }, { Damping, .45f }, { PreDelay, 80 }, { Shimmer, .05f }, { ReverbMix, .6f } } },
          // Below the floor: very dark and close, a low resonance, a murmur an octave down.
          { "Crypt", { { Cutoff, 1500 }, { Darkness, .62f }, { HighPass, 28 }, { Wind, 0 }, { Drips, .14f },
                       { Tomb, .32f }, { TombNote, 2 }, { TombOctave, 1 }, { Drive, 3 }, { Age, .15f }, { BloodMix, .2f },
                       { Voices, 1 }, { Interval, 0 }, { ChoirBlend, .15f }, { Formant, .1f }, { DelayMix, 0 },
                       { Character, 2 }, { Space, .3f }, { Decay, 2 }, { Damping, .85f }, { PreDelay, 6 }, { ReverbMix, .45f } } },
          // In the belfry: the instrument strikes bells, wind through the louvres.
          { "Bell Tower", { { HighPass, 100 }, { Cutoff, 14000 }, { Darkness, .05f }, { Wind, .25f },
                            { Metal, .6f }, { MetalKind, 1 }, { MetalNote, 2 }, { MetalOctave, 1 }, { MetalDecay, 5 },
                            { DelayTime, 420 }, { Feedback, .25f }, { Diffusion, .4f }, { DelayMix, .12f },
                            { Character, 0 }, { Space, .55f }, { Decay, 3 }, { Damping, .35f }, { PreDelay, 40 }, { Shimmer, .08f }, { ReverbMix, .3f } } },
        },
        {
          { "Vastness", { { Space, -.4f, 0 }, { Decay, -1.3f, .8f }, { PreDelay, -20, 40 }, { ReverbMix, -.15f, .12f } } },
          { "Choir", { { ChoirBlend, -.15f, .25f }, { Drone, 0, .2f }, { Voices, 0, 1 } } },
          { "Bells", { { Metal, -.3f, .4f }, { MetalDecay, -1, 1 }, { Shimmer, -.08f, .2f } } },
          { "Gloom", { { Cutoff, 1, -2 }, { Darkness, -.15f, .4f }, { Damping, -.2f, .3f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Haunted Forest", "Forest Edge",
        // Open air under the first trees: soft, damped, airy, wind in the leaves, a
        // faint haze, diffuse echoes from the trunks.
        { { Cutoff, 9000 }, { Darkness, .15f }, { HighPass, 40 }, { Wind, .36f },
          { WraithMix, .12f }, { Blur, .5f }, { Ghost, .15f },
          { DelayTime, 380 }, { Feedback, .25f }, { DelayDamping, .7f }, { Diffusion, .85f }, { Spread, .8f }, { DelayMix, .16f },
          { Character, 0 }, { Space, .4f }, { Decay, 1.4f }, { Damping, .8f }, { ReverbMix, .15f } },
        {
          // The old path: spectral ghosts of every note, whispers played backwards.
          { "Old Path", { { Cutoff, 5000 }, { Darkness, .4f }, { FilterMovement, .25f }, { FilterRate, .07f }, { Wind, .2f },
                          { WraithMix, .45f }, { Blur, .75f }, { Ghost, .55f }, { WraithDetune, 30 }, { Drift, .6f },
                          { DelayTime, 560 }, { Feedback, .35f }, { DelayDamping, .75f }, { Diffusion, .7f }, { Reverse, 1 }, { DelayMix, .2f },
                          { Space, .5f }, { Decay, 2 }, { Damping, .75f }, { PreDelay, 20 }, { ReverbMix, .2f } } },
          // A small wooden hut: boxy, woody, a candle, a crooked choir a fifth below.
          { "Witch's Hut", { { Cutoff, 8000 }, { Darkness, .2f }, { HighPass, 70 }, { Wind, 0 },
                             { Drive, 5 }, { BloodMix, .3f }, { Texture, .45f }, { Age, .25f }, { Instability, .2f }, { Flicker, .3f }, { FlickerRate, 4.5f },
                             { Tomb, .14f }, { TombNote, 9 }, { TombOctave, 2 },
                             { Voices, 1 }, { Interval, 1 }, { ChoirBlend, .12f }, { ChoirDetune, 30 }, { ChoirMovement, .7f }, { Formant, .3f },
                             { WraithMix, 0 },
                             { DelayTime, 48 }, { Feedback, .25f }, { Diffusion, .3f }, { DelayDamping, .6f }, { DelayMix, .1f },
                             { Space, .08f }, { Decay, .5f }, { Damping, .55f }, { PreDelay, 2 }, { ReverbMix, .35f } } },
          // The grove: mystical; a vast soft halo, an octave of air above, slow blur.
          { "Ancient Grove", { { Cutoff, 15000 }, { Darkness, 0 }, { FilterMovement, .2f }, { FilterRate, .05f }, { Wind, .2f },
                               { WraithMix, .3f }, { Blur, .85f }, { Ghost, .25f }, { Drift, .3f },
                               { Voices, 2 }, { Interval, 5 }, { ChoirBlend, .14f }, { Formant, .7f }, { ChoirMovement, .6f },
                               { DelayMix, .08f },
                               { Character, 3 }, { Space, .65f }, { Decay, 4.5f }, { Damping, .45f }, { PreDelay, 40 }, { Shimmer, .35f }, { ReverbMix, .42f } } },
        },
        {
          { "Mist", { { WraithMix, -.12f, .3f }, { Blur, -.3f, .2f }, { Drift, -.2f, .3f } } },
          { "Whispers", { { Ghost, -.2f, .4f }, { DelayMix, -.1f, .15f }, { Feedback, -.15f, .2f } } },
          { "Wind", { { Wind, -.3f, .4f }, { FilterMovement, -.15f, .3f }, { HighPass, 0, 1.5f } } },
          { "Shadows", { { Cutoff, 1, -1.5f }, { Darkness, -.1f, .35f }, { Decay, -.5f, .8f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Caverns", "Cave Mouth",
        // The mouth of the cave: big rock, discrete echoes from deep inside, drips.
        { { HighPass, 60 }, { Cutoff, 9000 }, { Darkness, .1f }, { Drips, .35f }, { Wind, .12f },
          { DelayTime, 290 }, { Feedback, .42f }, { Diffusion, .25f }, { DelayDamping, .35f }, { Spread, .85f }, { DelayMix, .22f },
          { Character, 1 }, { Space, .8f }, { Decay, 4.5f }, { Damping, .3f }, { PreDelay, 55 }, { ReverbMix, .38f } },
        {
          // Crystal: everything rings, cold and bright; long glassy echoes.
          { "Crystal Cavern", { { Cutoff, 20000 }, { HighPass, 120 }, { Darkness, 0 }, { Wind, 0 },
                                { Metal, .7f }, { MetalKind, 2 }, { MetalNote, 4 }, { MetalOctave, 3 }, { MetalDecay, 4.5f }, { Drips, .5f },
                                { DelayTime, 230 }, { Feedback, .45f }, { DelayDamping, .08f }, { Diffusion, .1f }, { Spread, 1 }, { DelayMix, .16f },
                                { Character, 1 }, { Space, .6f }, { Decay, 5 }, { Damping, .08f }, { PreDelay, 30 }, { Shimmer, .12f }, { ReverbMix, .36f } } },
          // A still lake under the rock: watery, filtered, wavering, very long.
          { "Underground Lake", { { Cutoff, 2200 }, { Resonance, .35f }, { FilterMovement, .45f }, { FilterRate, .12f }, { Darkness, .3f }, { Wind, 0 },
                                  { Instability, .45f }, { BloodMix, .25f }, { Drips, .6f },
                                  { WraithMix, .2f }, { Blur, .6f }, { Drift, .6f },
                                  { DelayTime, 650 }, { Feedback, .55f }, { DelayDamping, .8f }, { Diffusion, .85f }, { DelayMix, .25f },
                                  { Character, 3 }, { Space, .8f }, { Decay, 8 }, { Damping, .75f }, { PreDelay, 80 }, { ReverbMix, .48f } } },
          // The hoard: massive and dark, a rumbling sub-octave drone and resonance,
          // the glint of gold above it.
          { "Dragon's Hoard", { { Cutoff, 2400 }, { Darkness, .6f }, { HighPass, 25 }, { Wind, 0 }, { Drips, 0 },
                                { Drive, 8 }, { BloodMix, .35f }, { Texture, .5f },
                                { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .22f }, { Formant, .12f }, { Drone, .42f }, { DroneHold, 10 },
                                { Tomb, .3f }, { TombNote, 2 }, { TombOctave, 0 },
                                { Metal, .12f }, { MetalKind, 2 }, { MetalNote, 2 }, { MetalOctave, 4 }, { MetalDecay, 1.5f },
                                { DelayMix, 0 },
                                { Character, 1 }, { Space, 1 }, { Decay, 7 }, { Damping, .7f }, { PreDelay, 70 }, { ReverbMix, .46f } } },
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
        // Open hills (2b sketch): almost dry, wind, one long echo from the slopes.
        { { HighPass, 70 }, { Wind, .3f },
          { DelayTime, 600 }, { Feedback, .2f }, { Diffusion, .1f }, { DelayDamping, .6f }, { Spread, .6f }, { DelayMix, .2f },
          { Character, 0 }, { Space, .3f }, { Decay, 1 }, { Damping, .5f }, { PreDelay, 10 }, { ReverbMix, .1f } },
        {
          { "Mountain Pass", { { HighPass, 110 }, { DelayTime, 640 }, { Feedback, .5f }, { Diffusion, .2f }, { Spread, .9f }, { DelayMix, .35f },
                               { Character, 3 }, { Space, .65f }, { Decay, 3 }, { Damping, .25f }, { PreDelay, 80 }, { ReverbLowCut, 220 }, { ReverbMix, .22f } } },
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
          { "Observatory", { { Cutoff, 20000 }, { Flicker, 0 }, { CryptMode, 1 }, { RiseTime, 1.5f }, { Voices, 2 }, { Interval, 4 }, { ChoirBlend, .18f }, { ChoirDetune, 18 }, { ChoirMovement, .5f },
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
