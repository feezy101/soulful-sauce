#pragma once

// Soulful Sauce chord engine.
// A line-for-line port of the "Soulful Chords" FL Studio piano roll script, so the
// plugin, the script and the design preview all generate identical progressions.
// Pure C++17 with no JUCE dependency, so it can be unit tested on its own.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace sauce
{

enum Style { DeepHouse = 0, SoulfulHouse, NeoSoul, Gospel, JazzyLounge, AfroSoul, NumStyles };
enum Mood { Balanced = 0, Mellow, Happy, NumMoods };
enum ChordLength { OneBar = 0, TwoBars = 1, HalfBar = 2 };
enum Voicing { Close = 0, Drop2 = 1, Wide = 2 };

extern const char* const kNoteNames[12];
extern const char* const kStyleNames[NumStyles];
extern const char* const kMoodNames[NumMoods];

/** A chord in key-relative terms. fam: M m D X S h o 6 (see ChordEngine.cpp). bass = -1 for none. */
struct Chord
{
    int root = 0;
    char fam = 'M';
    int bass = -1;
    int rich = 0;

    bool operator== (const Chord& o) const { return root == o.root && fam == o.fam && bass == o.bass && rich == o.rich; }
};

struct Settings
{
    int key = 9;              // pitch class of the key, C = 0
    int style = DeepHouse;
    int mood = Balanced;
    int seed = 1;             // "Variation"
    int bars = 8;             // 4, 8 or 16
    int length = OneBar;      // ChordLength
    int voicing = Close;
    int richness = 2;         // 1..3
    double spice = 0.3;       // 0..1
    int octave = 0;           // -2..2
    bool bassOn = true;
};

struct GeneratedChord
{
    std::string name;         // e.g. "Am9", "Cmaj9/G"
    std::string roman;        // e.g. "i", "bVI"
    int bassNote = -1;        // MIDI note, -1 when the bass is off
    std::vector<int> upper;   // MIDI notes of the voicing, ascending
    Chord chord;              // key-relative chord, used for locking
    bool locked = false;
};

/** Number of chords a setting produces. */
int chordCount (const Settings& s);

/** Length of one chord in quarter notes, for a bar of barQuarters quarter notes. */
double chordSpanQuarters (const Settings& s, double barQuarters = 4.0);

/** Generates the progression. Locked chords (by index) replace the generated ones. */
std::vector<GeneratedChord> generate (const Settings& s, const std::map<int, Chord>& locks = {});

/** Small deterministic random generator, identical to the script's. */
class Rng
{
public:
    explicit Rng (int seed);
    double rand();

private:
    uint32_t state;
};

} // namespace sauce
