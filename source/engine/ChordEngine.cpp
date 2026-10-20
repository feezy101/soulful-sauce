#include "ChordEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>

// Keep the floating point maths exactly as the reference script does it:
// the build disables fused multiply-add contraction for this file (see CMakeLists.txt).

namespace sauce
{

const char* const kNoteNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* const kStyleNames[NumStyles] = { "Deep House", "Soulful House", "Neo-Soul", "Gospel", "Jazzy Lounge", "Afro Soul" };
const char* const kMoodNames[NumMoods] = { "Balanced", "Mellow", "Happy" };

namespace
{
    // Families: M = major 7th type, m = minor 7th type, D = dominant, X = altered dominant,
    // S = sus dominant, h = half-diminished, o = diminished 7th, 6 = major 6/9.
    // Mood tags: m = fits Mellow, h = fits Happy, * = only used by a mood.
    struct Template { const char* tag; const char* chords[4]; };

    const std::vector<std::vector<Template>> kTemplates = {
        { // Deep House (minor)
            { "mh", { "1:m", "4:m", "b7:S", "b3:M" } },
            { "m",  { "1:m", "b6:M", "4:m", "5:m" } },
            { "m",  { "4:m", "5:m", "b3:M", "1:m" } },
            { "m",  { "1:m", "1:m", "4:m", "b7:S" } },
            { "h",  { "b6:M", "b7:S", "1:m", "5:X" } },
            { "mh", { "1:m", "b3:M", "4:m", "b6:M" } },
            { "",   { "2:h", "5:X", "1:m", "1:m" } },
            { "m",  { "1:m", "b7:M", "b6:M", "5:m" } },
            { "h*", { "b3:M", "b6:M", "b7:S", "b3:M" } },
            { "h*", { "b6:M", "b7:S", "b3:M", "b3:M" } },
            { "h*", { "4:m", "b7:S", "b3:M", "b6:M" } },
        },
        { // Soulful House (major)
            { "mh", { "4:M", "3:m", "2:m", "1:M" } },
            { "h",  { "2:m", "3:m", "4:M", "5:S" } },
            { "h",  { "1:M", "6:m", "2:m", "5:S" } },
            { "mh", { "4:M", "5:S", "3:m", "6:m" } },
            { "h",  { "1:M", "3:D", "6:m", "4:M" } },
            { "m",  { "4:M", "4:m", "1:M", "6:D" } },
            { "m",  { "6:m", "2:m", "4:M", "5:S" } },
            { "h",  { "1:M", "2:m", "3:m", "4:M" } },
            { "m*", { "1:M", "4:M", "1:M", "4:M" } },
            { "m*", { "6:m", "4:M", "6:m", "4:M" } },
            { "m*", { "2:m", "3:m", "2:m", "1:M" } },
            { "h*", { "4:M", "5:D", "3:m", "6:D" } },
            { "h*", { "1:M", "4:M", "5:S", "1:M" } },
        },
        { // Neo-Soul
            { "m",  { "4:M", "3:m", "b3:M", "2:m" } },
            { "m",  { "1:M", "b7:D", "4:M", "4:m" } },
            { "",   { "2:m", "b2:D", "1:M", "6:X" } },
            { "m",  { "6:m", "2:m", "b6:M", "5:S" } },
            { "h",  { "4:M", "b7:D", "3:m", "6:D" } },
            { "h",  { "3:m", "6:D", "2:m", "5:D" } },
            { "m",  { "3:m", "b3:o", "2:m", "5:S" } },
            { "m*", { "2:m", "5:S", "2:m", "5:S" } },
            { "m*", { "1:M", "3:m", "4:M", "4:m" } },
            { "h*", { "1:M", "6:D", "2:m", "5:D" } },
            { "h*", { "4:M", "5:D", "3:D", "6:m" } },
            { "h*", { "1:M", "2:m", "3:m", "6:D" } },
        },
        { // Gospel
            { "h",  { "1:M", "4:M", "#4:o", "1:M/5" } },
            { "h",  { "1:M/5", "6:X", "2:m", "5:S" } },
            { "mh", { "1:M", "1:D/3", "4:M", "4:m" } },
            { "h",  { "3:m", "6:X", "2:m", "5:D" } },
            { "h",  { "4:M", "#4:o", "1:M/5", "6:X" } },
            { "h",  { "2:m", "5:S", "1:M", "1:D" } },
            { "m",  { "4:M", "4:m", "3:m", "6:X" } },
            { "m*", { "4:M", "3:m", "6:m", "2:m" } },
            { "m*", { "1:M", "4:M/1", "1:M", "4:M/1" } },
            { "m*", { "6:m", "5:m", "4:M", "4:m" } },
        },
        { // Jazzy Lounge
            { "h",  { "2:m", "5:D", "1:M", "6:X" } },
            { "h",  { "1:M", "6:m", "2:m", "5:D" } },
            { "m",  { "3:m", "b3:D", "2:m", "b2:D" } },
            { "m",  { "4:m", "b7:D", "1:M", "6:X" } },
            { "h",  { "1:6", "6:X", "2:m", "5:D" } },
            { "",   { "3:h", "6:X", "2:m", "5:X" } },
            { "m*", { "1:M", "4:M", "3:m", "6:m" } },
            { "m*", { "2:m", "5:S", "1:M", "1:6" } },
            { "h*", { "1:M", "3:D", "4:M", "5:D" } },
        },
        { // Afro Soul (dorian)
            { "mh", { "1:m", "4:D", "1:m", "4:D" } },
            { "m",  { "1:m", "2:m", "b3:M", "2:m" } },
            { "m",  { "1:m", "b7:M", "b6:M", "b7:M" } },
            { "h",  { "1:m", "4:D", "b7:M", "b3:M" } },
            { "mh", { "b3:M", "4:D", "1:m", "1:m" } },
            { "m",  { "1:m", "5:m", "4:D", "1:m" } },
            { "h*", { "b3:M", "4:D", "b7:M", "b3:M" } },
            { "h*", { "b7:M", "b3:M", "4:D", "1:m" } },
        },
    };

    struct Quality { const char* suffix; std::vector<int> intervals; };

    const std::vector<Quality>& qualities (char fam)
    {
        static const std::map<char, std::vector<Quality>> table = {
            { 'M', { { "maj7", { 0, 4, 7, 11 } }, { "maj9", { 0, 4, 7, 11, 14 } }, { "maj13", { 0, 4, 7, 11, 14, 21 } } } },
            { 'm', { { "m7", { 0, 3, 7, 10 } }, { "m9", { 0, 3, 7, 10, 14 } }, { "m11", { 0, 3, 7, 10, 14, 17 } } } },
            { 'D', { { "7", { 0, 4, 7, 10 } }, { "9", { 0, 4, 7, 10, 14 } }, { "13", { 0, 4, 7, 10, 14, 21 } } } },
            { 'X', { { "7", { 0, 4, 7, 10 } }, { "7b9", { 0, 4, 7, 10, 13 } }, { "7#9b13", { 0, 4, 10, 15, 20 } } } },
            { 'S', { { "7sus4", { 0, 5, 7, 10 } }, { "9sus4", { 0, 5, 7, 10, 14 } }, { "13sus4", { 0, 5, 7, 10, 14, 21 } } } },
            { 'h', { { "m7b5", { 0, 3, 6, 10 } }, { "m9b5", { 0, 3, 6, 10, 14 } }, { "m11b5", { 0, 3, 6, 10, 14, 17 } } } },
            { 'o', { { "dim7", { 0, 3, 6, 9 } }, { "dim7", { 0, 3, 6, 9 } }, { "dim7", { 0, 3, 6, 9 } } } },
            { '6', { { "6", { 0, 4, 7, 9 } }, { "6/9", { 0, 4, 7, 9, 14 } }, { "6/9", { 0, 4, 7, 9, 14 } } } },
        };
        return table.at (fam);
    }

    const char* const kRoman[12] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };

    inline int pmod (int a, int n) { return ((a % n) + n) % n; }

    bool isMajorStyle (int style) { return style >= SoulfulHouse && style <= JazzyLounge; }

    std::vector<const Template*> templatePool (int style, int mood)
    {
        std::vector<const Template*> out;
        for (const auto& t : kTemplates[(size_t) style])
        {
            const std::string tag = t.tag;
            const bool use = mood == Mellow ? tag.find ('m') != std::string::npos
                           : mood == Happy  ? tag.find ('h') != std::string::npos
                                            : tag.find ('*') == std::string::npos;
            if (use)
                out.push_back (&t);
        }
        return out;
    }

    int parseDegree (std::string tok)
    {
        static const std::map<char, int> degrees = { { '1', 0 }, { '2', 2 }, { '3', 4 }, { '4', 5 }, { '5', 7 }, { '6', 9 }, { '7', 11 } };
        int shift = 0;
        while (! tok.empty() && (tok[0] == 'b' || tok[0] == '#'))
        {
            shift += tok[0] == 'b' ? -1 : 1;
            tok.erase (0, 1);
        }
        return pmod (degrees.at (tok[0]) + shift, 12);
    }

    Chord parseChord (const std::string& token)
    {
        Chord c;
        std::string body = token;
        const auto slash = body.find ('/');
        if (slash != std::string::npos)
        {
            c.bass = parseDegree (body.substr (slash + 1));
            body = body.substr (0, slash);
        }
        const auto colon = body.find (':');
        c.root = parseDegree (body.substr (0, colon));
        c.fam = body[colon + 1];
        return c;
    }

    std::vector<Chord> buildProgression (int style, int nChords, double spice, Rng& rng, int mood)
    {
        const auto templates = templatePool (style, mood);
        std::vector<Chord> chords;
        const Template* last = nullptr;

        while ((int) chords.size() < nChords)
        {
            const Template* t;
            if (last != nullptr && rng.rand() < 0.5)
                t = last;
            else
                t = templates[(size_t) (rng.rand() * (double) templates.size()) % templates.size()];
            last = t;
            for (const char* tok : t->chords)
                chords.push_back (parseChord (tok));
        }
        chords.resize ((size_t) nChords);

        const Chord first = chords[0];
        for (size_t i = 0; i < chords.size(); ++i)
        {
            auto& c = chords[i];
            const Chord* nxt = i + 1 < chords.size() ? &chords[i + 1] : nullptr;
            double r[6];
            for (double& x : r)
                x = rng.rand();
            if (i == 0)
                continue;

            if ((c.fam == 'D' || c.fam == 'X') && r[0] < spice * 0.45)
            {
                c.root = (c.root + 6) % 12;   // tritone substitution
                c.fam = 'D';
                c.bass = -1;
            }
            else if (c.fam == 'M' && c.root == 5 && mood != Happy && r[1] < spice * 0.4)
            {
                c.fam = 'm';                  // borrowed minor iv
            }
            else if (c.fam == 'M' && r[2] < spice * 0.2)
            {
                c.root = (c.root + 9) % 12;   // relative minor swap
                c.fam = 'm';
                c.bass = -1;
            }
            else if (c.fam == 'm' && nxt != nullptr && pmod (nxt->root - c.root, 12) == 5 && r[3] < spice * 0.35)
            {
                c.fam = (nxt->fam == 'm' || nxt->fam == 'h') ? 'X' : 'D';   // secondary dominant
                c.bass = -1;
            }
            else if (c.fam == 'D' && r[4] < spice * 0.3)
            {
                c.fam = 'S';
            }

            const double lush = mood == Mellow ? 0.3 : 0.0;
            if (r[5] < spice * 0.35 + lush)
                c.rich = 1;
        }

        // Turnaround back to the top of the loop
        if (chords.size() >= 4)
        {
            const double r1 = rng.rand(), r2 = rng.rand();
            if (r1 < spice * 0.6)
            {
                const int rich = chords.back().rich;
                if (r2 < 0.3)
                {
                    chords.back() = { (first.root + 1) % 12, 'D', -1, rich };
                }
                else
                {
                    const bool minorHome = first.fam == 'm' || first.fam == 'h';
                    char fam = minorHome ? 'X' : 'S';
                    if (mood == Happy && ! minorHome)
                        fam = 'D';
                    chords.back() = { (first.root + 7) % 12, fam, -1, rich };
                }
            }
        }

        if (mood == Mellow)
        {
            for (auto& c : chords)
                if (c.fam == 'X')
                    c.fam = 'S';
        }
        else if (mood == Happy)
        {
            for (auto& c : chords)
            {
                const double r = rng.rand();
                if (c.fam == 'S' && r < 0.5)
                    c.fam = 'D';
                else if (isMajorStyle (style) && c.fam == 'M' && c.root == 0 && c.bass == -1 && r < 0.5)
                    c.fam = '6';
            }
        }
        return chords;
    }

    struct Info { std::string name; int rootPc; int bassPc; std::vector<int> intervals; };

    Info chordInfo (int key, const Chord& c, int richness, int mood)
    {
        int level = std::max (1, std::min (3, richness + c.rich)) - 1;
        if (mood == Happy && (c.fam == 'm' || c.fam == 'h'))
            level = std::min (level, 1);   // happy: 9ths, not darker 11ths
        const auto& q = qualities (c.fam)[(size_t) level];
        Info info;
        info.rootPc = (key + c.root) % 12;
        info.bassPc = (key + (c.bass >= 0 ? c.bass : c.root)) % 12;
        info.name = std::string (kNoteNames[info.rootPc]) + q.suffix;
        if (info.bassPc != info.rootPc)
            info.name += std::string ("/") + kNoteNames[info.bassPc];
        info.intervals = q.intervals;
        return info;
    }

    std::vector<int> upperPcs (int rootPc, int bassPc, const std::vector<int>& intervals, bool bassOn)
    {
        std::vector<int> pcs;
        for (int iv : intervals)
        {
            const int pc = (rootPc + iv) % 12;
            if (std::find (pcs.begin(), pcs.end(), pc) == pcs.end())
                pcs.push_back (pc);
        }
        auto it = std::find (pcs.begin(), pcs.end(), bassPc);
        if (bassOn && it != pcs.end() && pcs.size() > 3)
            pcs.erase (it);                                   // rootless voicing over the bass
        const int fifth = (rootPc + 7) % 12;
        it = std::find (pcs.begin(), pcs.end(), fifth);
        if (pcs.size() > 4 && it != pcs.end())
            pcs.erase (it);                                   // thin out the 5th on big chords
        return pcs;
    }

    std::vector<int> spreadVoicing (std::vector<int> v, int mode)
    {
        std::sort (v.begin(), v.end());
        if (mode >= 1 && v.size() >= 4)
        {
            v[v.size() - 2] -= 12;                            // drop 2
            if (mode == 2 && v.size() >= 5)
                v[v.size() - 4] -= 12;                        // drop 2 & 4
            std::sort (v.begin(), v.end());
        }
        return v;
    }

    std::vector<std::vector<int>> candidateVoicings (const std::vector<int>& pcs, int mode, int low, int high)
    {
        std::vector<std::vector<int>> cands;
        std::set<std::vector<int>> seen;
        std::vector<int> ordered = pcs;
        std::sort (ordered.begin(), ordered.end());
        const size_t n = ordered.size();

        for (size_t r = 0; r < n; ++r)
        {
            std::vector<int> order (ordered.begin() + (long) r, ordered.end());
            order.insert (order.end(), ordered.begin(), ordered.begin() + (long) r);

            for (int base = low - 12; base < high; ++base)
            {
                if (pmod (base, 12) != order[0])
                    continue;
                std::vector<int> notes { base };
                for (size_t k = 1; k < order.size(); ++k)
                {
                    int nx = notes.back() + 1;
                    while (pmod (nx, 12) != order[k])
                        ++nx;
                    notes.push_back (nx);
                }
                auto v = spreadVoicing (notes, mode);
                if (v.front() < low || v.back() > high)
                    continue;
                if (v[1] - v[0] < 3 && v[0] < 57)
                    continue;                                 // avoid muddy low clusters
                if (seen.insert (v).second)
                    cands.push_back (v);
            }
        }
        return cands;
    }

    double voicingCost (const std::vector<int>& v, const std::vector<int>* prev, int center)
    {
        int sum = 0;
        for (int x : v)
            sum += x;
        double cost = std::abs ((double) sum / (double) v.size() - (double) center) * 0.35;
        if (prev != nullptr)
        {
            int d = 0;
            for (int a : v)
            {
                int m = std::numeric_limits<int>::max();
                for (int b : *prev)
                    m = std::min (m, std::abs (a - b));
                d += m;
            }
            for (int b : *prev)
            {
                int m = std::numeric_limits<int>::max();
                for (int a : v)
                    m = std::min (m, std::abs (a - b));
                d += m;
            }
            cost += (double) d * 0.5 + (double) std::abs (v.back() - prev->back()) * 0.6;
        }
        else
        {
            cost += (double) std::abs (v.back() - (center + 8)) * 0.3;
        }
        if (v[v.size() - 1] - v[v.size() - 2] == 1)
            cost += 3;                                        // semitone rub on the top
        if (v[1] - v[0] == 1 && v[0] < 62)
            cost += 2;                                        // semitone rub low in the chord
        return cost;
    }

    std::vector<int> chooseVoicing (const std::vector<int>& pcs, int mode, int octave, const std::vector<int>* prev, int reg)
    {
        const int shift = octave * 12 + reg;
        const int lows[3] = { 52, 47, 44 };
        const int low = lows[mode] + shift;
        const int high = 81 + shift;
        const int center = 63 + shift;

        auto cands = candidateVoicings (pcs, mode, low, high);
        if (cands.empty())
            cands = candidateVoicings (pcs, 0, low - 6, high + 6);
        if (cands.empty())
        {
            const int base = 60 + shift;
            std::vector<int> v;
            for (int pc : pcs)
                v.push_back (base + pmod (pc - base, 12));
            std::sort (v.begin(), v.end());
            return v;
        }

        size_t best = 0;
        double bestCost = voicingCost (cands[0], prev, center);
        for (size_t i = 1; i < cands.size(); ++i)
        {
            const double c = voicingCost (cands[i], prev, center);
            if (c < bestCost)
            {
                best = i;
                bestCost = c;
            }
        }
        return cands[best];
    }
} // namespace

Rng::Rng (int seed)
    : state ((uint32_t) seed * 2654435761u + 1013904223u)
{
}

double Rng::rand()
{
    state += 0x6D2B79F5u;
    uint32_t t = state;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + ((t ^ (t >> 7)) * (t | 61u));
    return (double) (t ^ (t >> 14)) / 4294967296.0;
}

int chordCount (const Settings& s)
{
    if (s.length == TwoBars)
        return std::max (1, s.bars / 2);
    if (s.length == HalfBar)
        return s.bars * 2;
    return s.bars;
}

double chordSpanQuarters (const Settings& s, double barQuarters)
{
    if (s.length == TwoBars)
        return barQuarters * 2.0;
    if (s.length == HalfBar)
        return barQuarters * 0.5;
    return barQuarters;
}

std::vector<GeneratedChord> generate (const Settings& s, const std::map<int, Chord>& locks)
{
    const int n = chordCount (s);
    const int reg = s.mood == Mellow ? -2 : s.mood == Happy ? 3 : 0;
    Rng rng (s.seed * 101 + s.style * 7 + 1);
    auto chords = buildProgression (s.style, n, s.spice, rng, s.mood);

    std::vector<GeneratedChord> out;
    std::vector<int> prev;
    for (int i = 0; i < n; ++i)
    {
        GeneratedChord g;
        const auto lock = locks.find (i);
        g.locked = lock != locks.end();
        g.chord = g.locked ? lock->second : chords[(size_t) i];

        const auto info = chordInfo (s.key, g.chord, s.richness, s.mood);
        const auto pcs = upperPcs (info.rootPc, info.bassPc, info.intervals, s.bassOn);
        g.upper = chooseVoicing (pcs, s.voicing, s.octave, i == 0 ? nullptr : &prev, reg);
        prev = g.upper;

        int bassNote = 36 + info.bassPc + std::max (-1, std::min (1, s.octave)) * 12;
        while (bassNote > g.upper.front() - 5 && bassNote - 12 >= 24)
            bassNote -= 12;
        g.bassNote = s.bassOn ? bassNote : -1;

        g.name = info.name;
        g.roman = kRoman[g.chord.root];
        if (g.chord.fam == 'm' || g.chord.fam == 'h' || g.chord.fam == 'o')
            for (auto& ch : g.roman)
                ch = (char) std::tolower ((unsigned char) ch);
        out.push_back (std::move (g));
    }
    return out;
}

} // namespace sauce
