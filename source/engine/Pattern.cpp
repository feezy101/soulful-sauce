#include "Pattern.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace sauce
{

int Pattern::chordIndexAt (double quarters) const
{
    if (chords.empty() || loopLength <= 0)
        return -1;
    double q = std::fmod (quarters, loopLength);
    if (q < 0)
        q += loopLength;
    const int i = (int) std::floor (q / chordSpan + 1e-9);
    return std::min ((int) chords.size() - 1, std::max (0, i));
}

std::shared_ptr<const Pattern> bakePattern (std::vector<GeneratedChord> chords, const Settings& s,
                                            const Feel& feelIn, double barQuarters)
{
    auto p = std::make_shared<Pattern>();
    p->barQuarters = barQuarters;
    p->chordSpan = chordSpanQuarters (s, barQuarters);
    p->loopLength = p->chordSpan * (double) chords.size();

    const double step = 0.25;   // a 16th note
    const int spanSteps = std::max (1, (int) std::floor (p->chordSpan / step + 1e-9));
    const int barSteps = std::max (1, (int) std::floor (barQuarters / step + 1e-9));

    // Mood feel: mellow = softer and more rolled, happy = brighter and tighter
    double baseVel = feelIn.velocity;
    double strum = feelIn.strum;
    if (s.mood == Mellow)
    {
        baseVel *= 0.85;
        strum = std::min (1.0, strum + 0.15);
    }
    else if (s.mood == Happy)
    {
        baseVel = std::min (1.0, baseVel * 1.08);
        strum *= 0.6;
    }
    const double hum = feelIn.humanize;
    const double push = feelIn.rhythm == Pushed ? 0.5 : 0.0;

    // 16th-step patterns per bar: (start step, length in steps)
    static const std::vector<std::pair<int, int>> stabs = { { 0, 2 }, { 3, 2 }, { 6, 2 }, { 10, 2 }, { 12, 3 } };
    static const std::vector<std::pair<int, int>> organ = { { 2, 2 }, { 6, 2 }, { 10, 2 }, { 14, 2 } };
    const std::vector<std::pair<int, int>>* pattern = feelIn.rhythm == Stabs ? &stabs
                                                    : feelIn.rhythm == OffbeatOrgan ? &organ : nullptr;

    Rng feel (s.seed * 7919 + 17);
    auto jitter = [&feel] { return feel.rand() - 0.5; };

    struct Ev { double off, len; bool accent; };
    const int n = (int) chords.size();

    for (int i = 0; i < n; ++i)
    {
        const auto& c = chords[(size_t) i];
        double t0 = i * p->chordSpan;
        double t1 = t0 + p->chordSpan;
        if (push > 0)
        {
            if (i > 0) t0 -= push;
            if (i < n - 1) t1 -= push;
        }
        const double chordLen = t1 - t0;

        std::vector<Ev> events;
        if (pattern != nullptr)
        {
            for (int barOff = 0; barOff < spanSteps; barOff += barSteps)
                for (const auto& [ps, pl] : *pattern)
                {
                    const int st = barOff + ps;
                    if (st < spanSteps)
                        events.push_back ({ st * step, std::min (pl, spanSteps - st) * step, ps == 0 });
                }
        }
        else
        {
            events.push_back ({ 0.0, chordLen, true });
        }

        if (c.bassNote >= 0 && feelIn.bassMode != BassOff)
        {
            const std::vector<Ev> bassEvents = feelIn.bassMode == BassSustain ? std::vector<Ev> { { 0.0, chordLen, true } } : events;
            for (const auto& e : bassEvents)
            {
                const double jit = jitter() * 2.0 * hum * step * 0.08;
                const double vel = baseVel * 0.95 * (1.0 + jitter() * 2.0 * hum * 0.12);
                p->notes.push_back ({ std::max (0.0, t0 + e.off + jit), t0 + e.off + e.len, c.bassNote,
                                      (float) std::clamp (vel, 0.02, 1.0), true, i });
            }
        }

        const int voices = (int) c.upper.size();
        for (const auto& e : events)
        {
            const double evJit = jitter() * 2.0 * hum * step * 0.12;
            for (int k = 0; k < voices; ++k)
            {
                const double strumOff = strum * step * 0.4 * k;
                const double jit = evJit + jitter() * hum * step * 0.06;
                const double start = t0 + e.off + strumOff + jit;
                const double length = std::max (step / 4.0, e.len - strumOff - jit);
                double vel = baseVel * (1.0 + jitter() * 2.0 * hum * 0.18);
                if (k == voices - 1)
                    vel += 0.05;   // let the top voice sing
                else if (k > 0)
                    vel -= 0.03;   // softer inner voices
                if (e.accent && pattern != nullptr)
                    vel += 0.04;
                const double st = std::max (0.0, start);
                p->notes.push_back ({ st, st + length, c.upper[(size_t) k], (float) std::clamp (vel, 0.02, 1.0), false, i });
            }
        }
    }

    std::stable_sort (p->notes.begin(), p->notes.end(), [] (const NoteEvent& a, const NoteEvent& b) { return a.start < b.start; });

    // A pitch must never overlap itself (including across the loop point), otherwise
    // the earlier note's note-off would cut the later one short.
    std::map<std::pair<bool, int>, std::vector<size_t>> byPitch;
    for (size_t i = 0; i < p->notes.size(); ++i)
        byPitch[{ p->notes[i].bass, p->notes[i].note }].push_back (i);

    for (auto& [pitch, idx] : byPitch)
    {
        for (size_t j = 0; j + 1 < idx.size(); ++j)
        {
            auto& a = p->notes[idx[j]];
            const auto& b = p->notes[idx[j + 1]];
            if (a.end > b.start - 0.001)
                a.end = std::max (a.start + 0.01, b.start - 0.001);
        }
        auto& last = p->notes[idx.back()];
        const auto& first = p->notes[idx.front()];
        if (last.end > first.start + p->loopLength - 0.001)
            last.end = std::max (last.start + 0.01, first.start + p->loopLength - 0.001);
    }

    p->chords = std::move (chords);
    return p;
}

} // namespace sauce
