#pragma once
// Session 10: the search index. Every place (the ten locations' scenes and their
// sub-locations), every instrument (the instrument plug-in only), every factory preset and
// journey, each with descriptive tags (sound and mood), the developer's recordings it
// uses, where it belongs and a short description. Built once per plug-in kind (at load)
// and only read afterwards; a search is a scan of about 130 entries with a small fuzzy
// matcher (case-insensitive, partial words, typos), well under a millisecond.
//
// Pure data and functions (no JUCE): the panel (UI/SearchPanel) shows the results and
// does what choosing them by hand does.
#include <string>
#include <vector>

namespace dc::search {

enum class Kind { Instrument, Place, Preset };

struct Entry {
    Kind kind = Kind::Place;
    std::string key;           // stable: "i:12", "p:3:2", "r:5" (Recent stores these)
    std::string name;          // "Monk Chant", "Crypt", "Pilgrimage"
    std::string where;         // the location it belongs to: "Cathedral · Crypt"
    std::string family;        // instruments: "Voices"; presets: their category; places: the location
    std::string description;   // one or two short sentences
    std::vector<std::string> tags;         // sound and mood ("choir", "dark", "sacred"...)
    std::vector<std::string> recordings;   // the developer's recordings it uses ("voice", "guitar"...)
    std::string extra;         // more searchable words (a place's instrument, a preset's target)
    bool journey = false;      // a preset that travels (a route or roaming)
    // What choosing it does.
    int location = -1, sub = 0;   // places (and an instrument's own place)
    int instrument = -1;          // instruments
    int preset = -1;              // the factory preset's index in the plug-in's list
    // Precomputed (normalised words).
    std::vector<std::string> nameWords, whereWords, tagWords, extraWords;
    std::string normName;
    bool hasTag(const std::string& tag) const;
};

// What the chips ask for: one kind (or any) and moods (any of them).
struct Filter {
    bool instruments = false, places = false, presets = false;   // none set: every kind
    std::vector<std::string> moods;                              // "dark", "epic"...
    bool admits(const Entry&) const;
};

struct Result { int entry = -1; float score = 0; };

class Index {
public:
    explicit Index(bool instrumentPlugin);
    const std::vector<Entry>& entries() const noexcept { return all; }
    const Entry* find(const std::string& key) const;
    int indexOf(const std::string& key) const;
    // Best first; grouped by kind (the groups ordered by their best result). An empty query
    // lists everything the filter admits (in the index's order).
    std::vector<Result> search(const std::string& query, const Filter& = {}, int limit = 60) const;
    // How well one entry matches (0: not at all). Every word of the query must match.
    float score(const Entry&, const std::vector<std::string>& queryWords, const std::string& normQuery) const;
    bool instrumentPlugin() const noexcept { return forInstrument; }
private:
    std::vector<Entry> all;
    bool forInstrument;
};

// The index for a plug-in kind, built on first use (the processor asks for it at load).
const Index& index(bool instrumentPlugin);

// Helpers (tests).
std::string normalise(const std::string&);                 // lower case, letters and digits, single spaces
std::vector<std::string> words(const std::string&);        // normalised words
int editDistance(const std::string& a, const std::string& b, int limit);   // optimal string alignment, capped at limit + 1
float wordMatch(const std::string& queryWord, const std::string& word);    // 0..1
const char* kindName(Kind);
// The mood chips.
inline const std::vector<std::string>& moodChips() {
    static const std::vector<std::string> m { "dark", "epic", "eerie", "sacred" };
    return m;
}
}
