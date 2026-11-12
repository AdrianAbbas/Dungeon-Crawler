#include "Search.h"
#include "Locations.h"
#include "Presets.h"
#include "../Instrument/Presets.h"
#include "../Instrument/Synth.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <set>

namespace dc::search {
namespace {
// "·" (UTF-8), as the rest of the window writes "Cathedral · Crypt".
constexpr const char* dot = " \xc2\xb7 ";

struct Info {
    const char* tags;          // comma separated
    const char* recordings;    // comma separated (may be "")
    const char* description;
};

// The ten locations (their landmark on the map; choosing one lands at its own scene).
constexpr std::array<Info, LocationCount> locationInfo { {
    { "dark, stone, medieval, dungeon, sinister, torches, reverb", "", "Torchlit stone passages under the castle: damp, dark and echoing." },
    { "medieval, royal, castle, stone, epic, regal, halls", "", "The royal court: open courts, grand halls, a chapel and the battlements." },
    { "sacred, church, choir, bells, reverb, huge, holy, stone", "", "Arcades, the endless nave, the crypt below and the bell tower." },
    { "haunted, eerie, forest, wood, dark, ghostly, wind", "", "A wood you should not enter: shadows, crooked charms and ancient whispers." },
    { "cave, caverns, stone, echo, watery, crystal, huge", "", "Rock, crystal and still water, and a dragon asleep on its gold." },
    { "village, folk, weary, medieval, warm, intimate", "", "A tired village in the dark realm: the green, the tavern, the square, the smithy." },
    { "swamp, murky, watery, dark, damp, mud", "", "Thick damp air over mud and drowned stone." },
    { "mountains, open, echo, cold, epic, stone, wind", "", "Hills that answer, a canyon pass, a frozen summit and the dwarves' hall." },
    { "necropolis, dead, haunted, ghostly, dark, sinister, tomb", "", "The city of the dead: its iron gates, the graveyard and a sealed tomb." },
    { "wizard, magical, arcane, tower, strange, stars", "", "A spiral stair, a hushed library, the alchemist's bench and the stars." },
} };

// Every place in place order (location by location, its scene first): what it sounds like
// in the FX (the instrument's places sound the same around the instrument).
constexpr std::array<Info, 40> placeInfo { {
    // Dungeon
    { "dark, stone, damp, dripping, torches, echo, slapback, reverb, medieval", "",
      "Long stone corridors: a dark, damp stone room, a short slap down the passage, torches flickering, water dripping." },
    { "vaulted, stone, reverb, echo, huge, torches, medieval, hall", "",
      "A big vaulted stone entrance: bright torches, a wide slap from the far door, the stone fluttering." },
    { "cramped, muffled, metallic, iron, intimate, dark, sinister, small, bars", "",
      "A cramped cell: boxy and muffled, iron bars ringing, grime on everything." },
    { "bright, gold, shimmer, glitter, metallic, lush, chorus, reverb, sparkle", "",
      "Gold everywhere: a bright golden space full of glints, a lush ensemble, glitter on every attack." },
    { "huge, sinister, dark, epic, choir, voice, drive, distortion, echo, evil, demonic, reverb", "voice",
      "Huge and sinister: a colossal dark space, a sub-octave choir, menacing drive, ominous echoes falling a fourth." },
    // Castle
    { "open, stone, flutter, shimmer, mystical, airy, medieval, wind", "",
      "An open stone court between high walls: fluttering reflections, a gentle shimmer, a breeze." },
    { "huge, grand, epic, reverb, warm, hall, medieval, fire", "",
      "A massive, long and grand hall: a slow-blooming tail, timber warmth, a fire's flicker." },
    { "regal, royal, warm, lush, chorus, rich, medieval, epic, brass", "",
      "A regal, warm hall: a lush ensemble, a slight harmonic richness, an organ-like fifth above." },
    { "intimate, sacred, choir, voice, warm, soft, small, church, quiet", "voice",
      "A quiet chapel, a sermon in progress: close and warm, a gentle choir blooming an octave below." },
    { "open, airy, echo, delay, wind, bright, distant, epic, sky", "",
      "On the battlements: a wide open sky, long distant echoes, bright, the wind." },
    // Cathedral
    { "sacred, stone, reverb, flutter, choir, voice, bright, holy, arcade", "voice",
      "Arcades around a garden: a flutter of reflections feeding a long stone reverb, a faint choir an octave above." },
    { "huge, sacred, epic, majestic, reverb, cathedral, choir, long, holy", "",
      "The longest, most majestic space: huge smooth stone, a long pre-delay and a slow bloom, a breath of choir." },
    { "dark, sinister, eerie, reverse, whispers, low, close, haunted, sacred, tomb", "whispers",
      "Below the floor: very dark and close, a murmur an octave down, a reversed swell rising out of the stone, faint whispers." },
    { "bells, metallic, ringing, phaser, sacred, bright, swirl, echo, tower", "",
      "The belfry: tuned bell partials ring with every attack, fed back through a tape loop, a slow metallic swirl." },
    // Haunted Forest
    { "eerie, tense, dark, detune, haunted, unstable, wood, forest, shadow", "",
      "The edge of the wood: a shadow a semitone below, an unstable detune, tense filtering that never settles." },
    { "sinister, dark, haunted, eerie, wind, forest, creepy, path", "",
      "The old path: sinister, dark and close, the wind moving in the trees." },
    { "alluring, crooked, wobble, warm, eerie, choir, shimmer, witch, sweet, haunted", "",
      "A small wooden hut, sweet and inviting on the surface, wrong underneath: a crooked wobble, a crooked choir a fifth below." },
    { "magical, shimmer, ethereal, whispers, breathing, choir, mystical, ancient, halo", "whispers",
      "Ancient and breathing: a soft halo that blooms slowly, a whispering spectral blur, a choir an octave and a fifth above." },
    // Caverns
    { "cave, rock, warm, echo, reverb, fire, enclosed", "",
      "The mouth of the cave: rocky reflections close around you, a medium cave tail, firelight flickering." },
    { "crystal, shimmer, glassy, sparkle, magical, bright, bells, ringing", "",
      "A room full of crystals: an octave shimmer climbing, crystal resonators ringing, glassy sparkle." },
    { "watery, water, lake, dripping, cave, still, reverb, underground", "",
      "A still lake under the rock: dark water, slow ripples, drips falling into it." },
    { "huge, vast, dark, echo, rumble, epic, dragon, metallic, gold", "",
      "The vast cavern where the dragon sleeps: long distinct far echoes, a sheen of coins, a deep rumble." },
    // Village
    { "calm, soft, chorus, folk, gentle, open, weary, medieval, green", "",
      "The village green: a calming slow modulation, a gentle folk chorus, softened highs, open air." },
    { "warm, wooden, intimate, cosy, small, saturation, medieval, fire, tavern", "",
      "A calm, almost empty tavern: a small warm wooden room, a little dull, gentle saturation, the fire." },
    { "dark, dead, damped, slapback, slow, weary, medieval, square", "",
      "The market square, slow and dead: dark and damped, sparse slaps off the empty stalls, a dragging modulation." },
    { "metallic, anvil, iron, loud, gritty, drive, bright, forge, ringing", "",
      "The smithy, the only loud place in the village: anvils ringing, hard bright metal reflections, gritty drive." },
    // Swamp
    { "murky, watery, damp, dark, mud, drips, slow, wet", "",
      "The mire: thick damp air over mud, attacks dragged, a murky filter, dark wet reflections." },
    { "murky, wobble, watery, bubbles, low, thick, dark, muffled", "",
      "The bog: deep and thick, everything low-passed and wobbling, bubbles of drips." },
    { "watery, underwater, echo, drowned, haze, ruins, eerie, stone", "",
      "Ruins half under water: echoes off drowned stone, a slow haze." },
    // Mountains
    { "echo, open, hills, tape, delay, airy, wind, pastoral, stereo", "",
      "Open hills: echoes answering from hill to hill across the stereo field, a light tape wow, the wind." },
    { "huge, canyon, echo, ping pong, tape, epic, delay, stone", "",
      "A massive canyon: a tape echo bouncing between the walls, darker with every repeat." },
    { "icy, frozen, cold, shimmer, crystal, bright, thin, glassy, ice", "",
      "Sharp, frozen, jagged: almost no lows, glassy short reflections, an icy shimmer two octaves up." },
    { "stone, deep, warm, hall, huge, dwarven, reverb, forge, low", "",
      "Deep stone carved into the mountain, warm with the forge: a big deep hall with a resonant low-mid body." },
    // Necropolis
    { "dark, sinister, ominous, huge, iron, metallic, haunted, death, gates", "",
      "The gates of the dead city: a wide dark vault, rusted iron groaning, a slow ominous sway, a shadow an octave below." },
    { "ghostly, haunted, eerie, sinister, reverse, whispers, cold, dead, graves", "whispers",
      "Sinister and uneasy: notes drawn in backwards, a dissonant shimmer, wavering echoes, whispers among the graves." },
    { "tomb, hollow, muffled, small, intimate, stone, dead, dark, sealed", "",
      "A tight, sealed stone tomb: hollow and muffled." },
    // Wizard's Tower
    { "rising, phaser, echo, magical, arcane, swirl, stone, spiral", "",
      "The spiral stair: a phaser that climbs forever, echoes rising a semitone as they circle around you." },
    { "dry, hushed, intimate, small, quiet, soft, books, close", "",
      "Hushed like a library: a small dry room, paper-soft damping of highs and attacks." },
    { "bubbles, flanger, wobble, magical, arcane, strange, watery, glassy", "",
      "The alchemist's bench: a slow flanging wobble, bubbles rising, a strange resonance in a small glassy room." },
    { "huge, shimmer, stars, celestial, bright, magical, ethereal, epic, night", "",
      "Open to the night sky: a vast starry shimmer, long and bright, twinkling starlight, a slow-rising halo." },
} };

// The forty instruments in place order: their family, tags, the developer's recordings
// they play (Session 8's table) and a description.
struct InstInfo { const char* family; Info info; };
constexpr std::array<InstInfo, 40> instrumentInfo { {
    // Dungeon
    { "Strings", { "pluck, strings, bass, low, dark, pizzicato, orchestral", "", "A contrabass plucked by the side of a finger, the bass's body and a finger's thump; Snap: the string slapping the board." } },
    { "Strings", { "strings, bowed, cello, drone, lead, dark, low", "", "One bowed cello; Drone: the same note an octave below on a bowed resonator." } },
    { "Metal percussion", { "metallic, chains, iron, struck, bowed, percussion, sinister, dark", "", "Iron chains: a struck link (soft: a heavy scrape), a bowed iron hum, the links rattling after." } },
    { "Tuned percussion", { "bells, celesta, bright, sparkle, metallic, magical, glitter, gold", "", "A golden celesta, steel bars over resonators, with coins ringing high above each note." } },
    { "Brass", { "brass, choir, voice, epic, huge, sinister, doom, horns, dark", "voice, piano", "Colossal doom brass, low horns and an octave below, driven; a choir shouting the note; Weight: a piano struck an octave below." } },
    // Castle
    { "Plucked strings", { "pluck, lute, strings, medieval, warm, intimate", "guitar", "A lute made from the developer's classical guitar: paired courses a few cents apart, a smaller, brighter body." } },
    { "Brass", { "brass, fanfare, trumpet, trombone, epic, royal, medieval", "", "A fanfare section: two trumpets a few cents apart and a trombone an octave below." } },
    { "Strings", { "strings, ensemble, violins, orchestral, regal, lush, epic, bowed", "", "A regal string ensemble: violins, and violas and cellos an octave below." } },
    { "Organ", { "organ, keys, sacred, soft, intimate, church, medieval", "", "A small chamber organ, flue stops 8', 4', 2'; Stops: adds the nazard and tierce." } },
    { "Brass", { "brass, horn, heroic, epic, call, medieval", "", "Heroic horn calls: a dark-belled horn scooping onto the note and a second horn doubling." } },
    // Cathedral
    { "Voices", { "choir, voice, soft, sacred, pad, ooh, aah, angelic, vocal", "voice", "A soft choir of the developer's voice, two sections a little apart; Open: ooh to aah." } },
    { "Organ", { "organ, sacred, huge, epic, church, majestic, keys", "", "A grand pipe organ, the full principal chorus from 16' to a mixture; Reeds: a trumpet rank joins." } },
    { "Voices", { "chant, choir, voice, monks, low, drone, sacred, dark, medieval, vocal", "voice", "A low monk chant an octave below the key, three monks of the developer's voice; Open: hum to chant." } },
    { "Bells", { "bells, church, sacred, metallic, toll, ringing, huge", "", "Tuned church bells, the lowest partials beating; Strike: the clapper's clang." } },
    // Haunted Forest
    { "Winds", { "flute, wind, wooden, detuned, eerie, breathy, haunted", "", "Two wooden flutes not quite agreeing; Detune: how far apart." } },
    { "Strings", { "strings, cello, harmonics, bowed, creaking, eerie, ghostly, thin", "", "Thin cello harmonics bowed near the bridge, the wood creaking now and then under the bow." } },
    { "Tuned percussion", { "music box, bells, lullaby, eerie, sweet, haunted, pluck, voice", "voice", "An alluring music box, each tooth paired with a sweeter one a little sharp, the developer humming softly under it." } },
    { "Plucked strings", { "harp, pluck, strings, magical, whispers, ancient, ethereal", "whispers, breaths", "An ancient harp; Whispers: a breath or a whisper of the developer's voice with each pluck." } },
    // Caverns
    { "Tuned percussion", { "marimba, mallets, stone, percussion, warm, fire, struck", "", "A firelit stone marimba: rough stone bars over resonators, the firelight flickering on it." } },
    { "Bells", { "bells, crystal, glass, shimmer, sparkle, magical, bright", "", "Glass and crystal bells, beating, with a shimmer an octave above that swells after the strike." } },
    { "Tuned percussion", { "kalimba, pluck, watery, ripple, gentle, magical, tines", "kalimba", "The developer's kalimba, its pitch rippling as if heard across water." } },
    { "Percussion", { "gong, metallic, huge, brass, growl, dragon, epic, dark", "", "A huge tuned gong; played hard, a growling low brass rises out of it." } },
    // Village
    { "Winds", { "whistle, recorder, flute, wind, folk, medieval, weary, breathy", "", "A tin whistle, a little tired, its pitch wandering; Breath: how much air." } },
    { "Plucked strings", { "guitar, pluck, strings, warm, folk, weary, intimate", "guitar", "The developer's classical guitar, weary: warm, dull, slower to speak, dying a little sooner." } },
    { "Strings", { "fiddle, violin, strings, folk, bowed, medieval, dance", "violin", "A fiddle (the developer's violin), with a raspy folk bow under it." } },
    { "Metal percussion", { "anvil, metallic, iron, hammer, percussion, struck, bright, forge", "", "A tuned anvil struck with a hammer; Clank: the hammer's iron." } },
    // Swamp
    { "Plucked strings", { "banjo, pluck, strings, detuned, twangy, murky, folk", "banjo", "The developer's banjo, its paired strings badly out of tune with each other." } },
    { "Winds", { "reed, clarinet, woodwind, low, murky, dark, breathy", "", "A murky low reed like a bass clarinet, with mud in its breath." } },
    { "Bells", { "bells, watery, underwater, drifting, pad, eerie, glassy", "", "Bells under water, drifting in pitch, with a glassy pad below." } },
    // Mountains
    { "Winds", { "flute, wind, breathy, pastoral, folk, low, open, shepherd", "", "A low end-blown shepherd's flute, breathy, with the shepherd's breath around it." } },
    { "Drums", { "drums, war drums, percussion, huge, epic, hand drums, tribal, low", "djembe", "War drums from the developer's two djembes, pitched down and huge, every stroke on its own keys." } },
    { "Tuned percussion", { "glockenspiel, bells, icy, bright, metallic, crystal, frozen", "", "An ice glockenspiel: hard bright bars, ice crackling after the strike." } },
    { "Voices", { "chant, voice, choir, deep, low, brass, epic, dwarven, drone, vocal", "voice", "A dwarven chant of the developer's deep voice an octave below the key, over low brass." } },
    // Necropolis
    { "Voices", { "choir, pad, voice, dark, slow, sinister, haunted, huge, vocal", "voice", "A dark choir pad: slow dark voices, more an octave below, a faint breath of an analog pad." } },
    { "Strings", { "violin, strings, ghostly, haunted, eerie, bowed, unstable", "violin", "The developer's violin as a ghost, its pitch never settling, the bow's wood knocking on the strings." } },
    { "Bells", { "bells, organ, toll, funeral, dark, sacred, sinister", "piano", "A tolling bell with a low organ under it while the key is held, and a piano struck an octave below." } },
    // Wizard's Tower
    { "Keys", { "harpsichord, arpeggio, pluck, keys, baroque, arcane, rising, medieval", "", "A harpsichord whose held key climbs on its own, at the host's tempo." } },
    { "Keys", { "clavichord, keys, intimate, soft, quiet, pluck, medieval", "", "A soft clavichord, a brass tangent pressing the string; it stops the moment the key lets go." } },
    { "Tuned percussion", { "marimba, glass, bubbles, synth, magical, arcane, watery", "", "A bubbling glass marimba synth, bubbles rising off each note." } },
    { "Synth", { "pad, synth, celestial, stars, shimmer, ethereal, dreamy, huge", "soft pad", "A celestial pad (the developer's soft pad recording), with starlight an octave above." } },
} };

std::vector<std::string> splitList(const char* text) {
    std::vector<std::string> out;
    std::string current;
    for (const char* c = text; ; ++c) {
        if (*c == ',' || *c == 0) {
            const auto a = current.find_first_not_of(' '), b = current.find_last_not_of(' ');
            if (a != std::string::npos) out.push_back(current.substr(a, b - a + 1));
            current.clear();
            if (*c == 0) break;
        } else current += *c;
    }
    return out;
}

void addUnique(std::vector<std::string>& to, const std::vector<std::string>& from) {
    for (const auto& t : from)
        if (std::find(to.begin(), to.end(), t) == to.end()) to.push_back(t);
}

int placeOrder(int location, int sub) {
    int k = 0;
    for (int l = 0; l < location; ++l) k += subLocationCount(l) + 1;
    return k + sub;
}

std::string placeWhere(int location, int sub) { return std::string(locationNames[std::size_t(location)]) + dot + subLocationName(location, sub); }

bool isStopWord(const std::string& w) {
    static const std::set<std::string> stop { "the", "of", "a", "an", "in", "and", "at", "by", "for", "to", "on" };
    return stop.count(w) > 0;
}

void prepare(Entry& e) {
    e.normName = normalise(e.name);
    e.nameWords = words(e.name);
    e.whereWords = words(e.where);
    e.tagWords.clear();
    for (const auto& t : e.tags) addUnique(e.tagWords, words(t));
    for (const auto& r : e.recordings) addUnique(e.tagWords, words(r));
    addUnique(e.tagWords, words(e.family));
    e.extraWords = words(e.extra);
}
} // namespace

std::string normalise(const std::string& text) {
    std::string out;
    bool space = true;
    for (unsigned char c : text) {
        if (c == '\'') continue;   // "Witch's" -> "witchs"
        if (std::isalnum(c) && c < 0x80) {
            out += static_cast<char>(std::tolower(c));
            space = false;
        } else if (!space) {
            out += ' ';
            space = true;
        }
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::vector<std::string> words(const std::string& text) {
    std::vector<std::string> out;
    const auto n = normalise(text);
    std::size_t start = 0;
    while (start < n.size()) {
        auto end = n.find(' ', start);
        if (end == std::string::npos) end = n.size();
        if (end > start) out.push_back(n.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

int editDistance(const std::string& a, const std::string& b, int limit) {
    const int n = int(a.size()), m = int(b.size());
    if (std::abs(n - m) > limit) return limit + 1;
    // Optimal string alignment (Damerau-Levenshtein with adjacent transpositions).
    std::vector<int> prev2(std::size_t(m + 1)), prev(std::size_t(m + 1)), cur(std::size_t(m + 1));
    for (int j = 0; j <= m; ++j) prev[std::size_t(j)] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        int rowBest = cur[0];
        for (int j = 1; j <= m; ++j) {
            const int cost = a[std::size_t(i - 1)] == b[std::size_t(j - 1)] ? 0 : 1;
            int v = std::min({ prev[std::size_t(j)] + 1, cur[std::size_t(j - 1)] + 1, prev[std::size_t(j - 1)] + cost });
            if (i > 1 && j > 1 && a[std::size_t(i - 1)] == b[std::size_t(j - 2)] && a[std::size_t(i - 2)] == b[std::size_t(j - 1)])
                v = std::min(v, prev2[std::size_t(j - 2)] + 1);
            cur[std::size_t(j)] = v;
            rowBest = std::min(rowBest, v);
        }
        if (rowBest > limit) return limit + 1;
        std::swap(prev2, prev);
        std::swap(prev, cur);
    }
    return std::min(prev[std::size_t(m)], limit + 1);
}

float wordMatch(const std::string& q, const std::string& w) {
    if (q.empty() || w.empty()) return 0.0f;
    if (q == w) return 1.0f;
    if (w.size() > q.size() && w.compare(0, q.size(), q) == 0) return 0.82f + 0.13f * float(q.size()) / float(w.size());
    if (q.size() >= 3 && w.find(q) != std::string::npos) return 0.62f;
    if (q.size() >= 4) {
        const int allowed = q.size() >= 7 ? 2 : 1;
        const int d = editDistance(q, w, allowed);
        if (d <= allowed) return 0.72f - 0.12f * float(d - 1);
        // A partial word typed with a typo: "dwarv" or "crys" with a slip, against the
        // word's start.
        if (w.size() > q.size()) {
            const int dp = std::min(editDistance(q, w.substr(0, q.size()), 1), editDistance(q, w.substr(0, q.size() + 1), 1));
            if (dp <= 1) return 0.55f;
        }
    }
    return 0.0f;
}

bool Entry::hasTag(const std::string& tag) const { return std::find(tags.begin(), tags.end(), tag) != tags.end(); }

bool Filter::admits(const Entry& e) const {
    if (instruments || places || presets) {
        const bool ok = (instruments && e.kind == Kind::Instrument) || (places && e.kind == Kind::Place) || (presets && e.kind == Kind::Preset);
        if (!ok) return false;
    }
    if (!moods.empty()) {
        bool any = false;
        for (const auto& m : moods) any = any || e.hasTag(m);
        if (!any) return false;
    }
    return true;
}

const char* kindName(Kind k) {
    switch (k) {
        case Kind::Instrument: return "Instruments";
        case Kind::Place: return "Places";
        default: return "Presets";
    }
}

Index::Index(bool instrumentPlugin) : forInstrument(instrumentPlugin) {
    // Instruments (the instrument plug-in only), in place order.
    std::array<std::vector<std::string>, 40> instTags, instRecs;
    for (int i = 0; i < inst::instrumentCount && i < 40; ++i) {
        instTags[std::size_t(i)] = splitList(instrumentInfo[std::size_t(i)].info.tags);
        instRecs[std::size_t(i)] = splitList(instrumentInfo[std::size_t(i)].info.recordings);
    }
    if (forInstrument)
        for (int i = 0; i < inst::instrumentCount; ++i) {
            Entry e;
            e.kind = Kind::Instrument;
            e.key = "i:" + std::to_string(i);
            e.name = inst::design(i).name;
            e.location = inst::locationOf(i);
            e.sub = inst::subOf(i);
            e.instrument = i;
            e.where = placeWhere(e.location, e.sub);
            e.family = instrumentInfo[std::size_t(i)].family;
            e.tags = instTags[std::size_t(i)];
            e.recordings = instRecs[std::size_t(i)];
            e.description = instrumentInfo[std::size_t(i)].info.description;
            prepare(e);
            all.push_back(std::move(e));
        }
    // The locations (their landmark) and every place.
    for (int l = 0; l < LocationCount; ++l) {
        Entry e;
        e.kind = Kind::Place;
        e.key = "l:" + std::to_string(l);
        e.name = locationNames[std::size_t(l)];
        e.where = std::string("Location") + dot + subLocationName(l, 0);
        e.family = "Location";
        e.location = l;
        e.sub = 0;
        e.tags = splitList(locationInfo[std::size_t(l)].tags);
        e.description = locationInfo[std::size_t(l)].description;
        for (int k = 0; k <= subLocationCount(l); ++k) e.extra += std::string(subLocationName(l, k)) + " ";
        prepare(e);
        all.push_back(std::move(e));
        for (int k = 0; k <= subLocationCount(l); ++k) {
            const int order = placeOrder(l, k);
            const auto& info = placeInfo[std::size_t(order)];
            Entry p;
            p.kind = Kind::Place;
            p.key = "p:" + std::to_string(l) + ":" + std::to_string(k);
            p.name = subLocationName(l, k);
            p.where = placeWhere(l, k);
            p.family = locationNames[std::size_t(l)];
            p.location = l;
            p.sub = k;
            p.tags = splitList(info.tags);
            p.recordings = splitList(info.recordings);
            p.description = info.description;
            if (forInstrument) {
                // The place's own instrument plays there (unless one is locked).
                const int i = inst::designFor(l, k);
                p.extra = std::string(inst::design(i).name) + " " + instrumentInfo[std::size_t(i)].family;
                p.description += std::string(" Its instrument: ") + inst::design(i).name + ".";
                addUnique(p.recordings, instRecs[std::size_t(i)]);
            }
            prepare(p);
            all.push_back(std::move(p));
        }
    }
    // The factory presets, in the plug-in's own list.
    const auto& presets = forInstrument ? inst::instrumentPresets() : factoryPresets();
    for (int r = 0; r < int(presets.size()); ++r) {
        const auto& f = presets[std::size_t(r)];
        Entry e;
        e.kind = Kind::Preset;
        e.key = "r:" + std::to_string(r);
        e.name = f.name;
        e.family = f.category;
        e.preset = r;
        std::map<std::string, float> v;
        for (const auto& [id, value] : f.values) v[id] = value;
        const auto get = [&v](const char* id, float fallback) { const auto it = v.find(id); return it == v.end() ? fallback : it->second; };
        const int l = std::clamp(int(get("location", 0)), 0, LocationCount - 1);
        const int k = clampSubLocation(l, int(get("subLocation", 0)));
        const bool roam = get("roamOn", 0) >= 0.5f;
        e.journey = !f.route.empty() || roam;
        e.location = l;
        e.sub = k;
        if (!f.route.empty()) {
            e.where = std::string("Journey") + dot + locationNames[std::size_t(f.route.front().location)];
            std::string stops;
            std::vector<std::string> seen;
            for (const auto& st : f.route) {
                const std::string n = subLocationName(st.location, st.sub);
                if (!stops.empty()) stops += ", ";
                stops += n;
                e.extra += std::string(locationNames[std::size_t(st.location)]) + " " + n + " ";
                // The journey's tags: each stop's first few (its character), not all of them.
                const auto t = splitList(placeInfo[std::size_t(placeOrder(st.location, st.sub))].tags);
                addUnique(e.tags, std::vector<std::string>(t.begin(), t.begin() + std::min<std::ptrdiff_t>(3, std::ptrdiff_t(t.size()))));
            }
            const int mode = int(get("journeyMode", 0));
            const char* modes[] { "a loop", "one way", "there and back" };
            e.description = "A journey through " + stops + " (" + modes[std::clamp(mode, 0, 2)] + ", " + std::to_string(int(get("journeyBars", 4))) + " bars each).";
            addUnique(e.tags, { "journey", "travel" });
        } else if (roam) {
            e.where = std::string("Roam") + dot + locationNames[std::size_t(l)];
            e.description = std::string("Roams between the places of the ") + locationNames[std::size_t(l)] + " every " + std::to_string(int(get("roamBars", 2)))
                            + " bars, " + std::to_string(int(std::lround(get("roamRandomness", 0.5f) * 100))) + " % at random.";
            e.extra = std::string(locationNames[std::size_t(l)]);
            for (int s = 0; s <= subLocationCount(l); ++s) e.extra += std::string(" ") + subLocationName(l, s);
            addUnique(e.tags, splitList(locationInfo[std::size_t(l)].tags));
            addUnique(e.tags, { "journey", "roam", "wander" });
        } else {
            e.where = placeWhere(l, k);
            const auto& place = placeInfo[std::size_t(placeOrder(l, k))];
            const bool locked = get("instLock", 0) >= 0.5f;
            const int instrument = forInstrument ? (locked ? std::clamp(int(get("instrument", 0)), 0, inst::instrumentCount - 1) : inst::designFor(l, k)) : -1;
            if (instrument >= 0) {
                e.instrument = instrument;
                const auto& ii = instrumentInfo[std::size_t(instrument)];
                addUnique(e.tags, instTags[std::size_t(instrument)]);
                addUnique(e.recordings, instRecs[std::size_t(instrument)]);
                e.extra = std::string(inst::design(instrument).name) + " " + ii.family + " " + locationNames[std::size_t(l)];
                e.description = std::string(inst::design(instrument).name) + (locked ? " played in the " : " in its place, the ") + subLocationName(l, k) + ". "
                                + ii.info.description;
                if (get("intensity", 1.0f) <= 0.01f) { e.description = std::string(inst::design(instrument).name) + " alone, with no place around it."; addUnique(e.tags, { "dry" }); }
            } else {
                e.extra = std::string(subLocationName(l, k)) + " " + locationNames[std::size_t(l)];
                e.description = place.description;
                if (get("intensity", 1.0f) < 0.99f) e.description += " At " + std::to_string(int(std::lround(get("intensity", 1.0f) * 100))) + " % intensity.";
            }
            addUnique(e.tags, splitList(place.tags));
            addUnique(e.recordings, splitList(place.recordings));
        }
        // Session 15: presets built around DUNGEON SYNTH (the roller up): the genre's words.
        if (const float ds = get("dungeonSynth", 0.0f); ds > 0.0f) {
            const bool modern = get("dungeonSynthEra", 0.0f) >= 0.5f;
            addUnique(e.tags, { "dungeon synth", "lo-fi", "lofi", "retro" });
            addUnique(e.tags, modern ? std::vector<std::string> { "tape", "warm", "chorus", "wide" } : std::vector<std::string> { "cassette", "tape", "90s", "bitcrush", "mono" });
            e.description += std::string(" Dungeon synth, ") + (modern ? "Modern (a lush, warm vintage sampler, warm tape, a deep ensemble chorus, wide, a long dark hall that blooms)"
                                                                         : "Classic (the 90s cassette demo: a crushed sampler, wow, hiss, nearly mono, a big grainy hall)")
                             + ", " + std::to_string(int(std::lround(ds * 100))) + " %.";
        }
        prepare(e);
        all.push_back(std::move(e));
    }
}

const Entry* Index::find(const std::string& key) const {
    const int i = indexOf(key);
    return i < 0 ? nullptr : &all[std::size_t(i)];
}

int Index::indexOf(const std::string& key) const {
    for (std::size_t i = 0; i < all.size(); ++i)
        if (all[i].key == key) return int(i);
    return -1;
}

float Index::score(const Entry& e, const std::vector<std::string>& q, const std::string& normQuery) const {
    if (q.empty()) return 0.0f;
    float total = 0.0f;
    const auto best = [](const std::string& w, const std::vector<std::string>& ws) {
        float b = 0.0f;
        for (const auto& x : ws) b = std::max(b, wordMatch(w, x));
        return b;
    };
    for (const auto& w : q) {
        const float s = std::max({ 3.0f * best(w, e.nameWords), 2.0f * best(w, e.tagWords), 1.5f * best(w, e.whereWords), 1.0f * best(w, e.extraWords) });
        if (s < 0.5f) return 0.0f;   // every word must match something
        total += s;
    }
    // The whole query in the name.
    if (e.normName == normQuery) total += 4.0f;
    else if (e.normName.compare(0, normQuery.size(), normQuery) == 0) total += 1.2f;
    else if (normQuery.size() >= 3 && e.normName.find(normQuery) != std::string::npos) total += 1.0f;
    // A tag phrase typed in full ("war drums", "music box").
    if (q.size() > 1)
        for (const auto& t : e.tags)
            if (normalise(t) == normQuery) { total += 1.5f; break; }
    return total;
}

std::vector<Result> Index::search(const std::string& query, const Filter& filter, int limit) const {
    std::vector<Result> out;
    auto q = words(query);
    // "the", "of"... are dropped when the query has other words ("the crypt").
    std::vector<std::string> kept;
    for (const auto& w : q) if (!isStopWord(w)) kept.push_back(w);
    if (!kept.empty()) q = kept;
    std::string normQuery;
    for (const auto& w : q) normQuery += (normQuery.empty() ? "" : " ") + w;
    for (int i = 0; i < int(all.size()); ++i) {
        const auto& e = all[std::size_t(i)];
        if (!filter.admits(e)) continue;
        if (q.empty()) { out.push_back({ i, 0.0f }); continue; }
        const float s = score(e, q, normQuery);
        if (s > 0.0f) out.push_back({ i, s });
    }
    if (q.empty()) {
        // Everything the filter admits, grouped in the fixed order.
        std::stable_sort(out.begin(), out.end(), [this](const Result& a, const Result& b) { return int(all[std::size_t(a.entry)].kind) < int(all[std::size_t(b.entry)].kind); });
    } else {
        // Groups ordered by their best result, best first within each.
        // A slight lead for instruments, then places, over presets named after them
        // ("choir": the choirs before "Choir in the Observatory").
        std::array<float, 3> groupBest { 0, 0, 0 };
        constexpr std::array<float, 3> lead { 0.3f, 0.2f, 0.0f };
        for (const auto& r : out) {
            const auto k = std::size_t(all[std::size_t(r.entry)].kind);
            groupBest[k] = std::max(groupBest[k], r.score + lead[k]);
        }
        std::stable_sort(out.begin(), out.end(), [&](const Result& a, const Result& b) {
            const int ka = int(all[std::size_t(a.entry)].kind), kb = int(all[std::size_t(b.entry)].kind);
            if (ka != kb) return groupBest[std::size_t(ka)] != groupBest[std::size_t(kb)] ? groupBest[std::size_t(ka)] > groupBest[std::size_t(kb)] : ka < kb;
            return a.score > b.score;
        });
    }
    if (int(out.size()) > limit) {
        // Keep the groups' order but no more than `limit` in all.
        out.resize(std::size_t(limit));
    }
    return out;
}

const Index& index(bool instrumentPlugin) {
    static const Index instrument(true), effect(false);
    return instrumentPlugin ? instrument : effect;
}
}
