#pragma once

// Turns a generated progression into timed notes (rhythm, humanize, strum, velocity),
// following the same rules as the FL Studio script's apply() function.
// Times are in quarter notes from the start of the loop.

#include "ChordEngine.h"

#include <memory>
#include <vector>

namespace sauce
{

enum Rhythm { Pads = 0, Pushed, Stabs, OffbeatOrgan };
enum BassMode { BassSustain = 0, BassFollow, BassOff };

struct Feel
{
    int rhythm = Pads;
    double humanize = 0.35;
    double velocity = 0.72;
    double strum = 0.1;
    int bassMode = BassSustain;
};

struct NoteEvent
{
    double start = 0, end = 0;   // quarter notes, start within [0, loopLength)
    int note = 60;
    float velocity = 0.7f;       // 0..1
    bool bass = false;
    int chord = 0;               // index of the chord it belongs to
};

struct Pattern
{
    std::vector<GeneratedChord> chords;
    std::vector<NoteEvent> notes;  // sorted by start
    double loopLength = 32.0;      // quarter notes
    double chordSpan = 4.0;        // quarter notes per chord
    double barQuarters = 4.0;

    int chordIndexAt (double quarters) const;
};

std::shared_ptr<const Pattern> bakePattern (std::vector<GeneratedChord> chords, const Settings& settings,
                                            const Feel& feel, double barQuarters);

} // namespace sauce
