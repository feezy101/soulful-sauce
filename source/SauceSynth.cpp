#include "SauceSynth.h"

#include <cmath>

namespace sauce
{

namespace
{
    constexpr double twoPi = 6.283185307179586;

    inline double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    inline double saw (double ph, double dt) { return 2.0 * ph - 1.0 - polyBlep (ph, dt); }
    inline double tri (double ph) { return 1.0 - 4.0 * std::abs (ph - 0.5); }
    inline double coefFor (double tau, double sr) { return std::exp (-1.0 / (std::max (1.0e-4, tau) * sr)); }

    inline void advance (double& ph, double inc)
    {
        ph += inc;
        if (ph >= 1.0)
            ph -= std::floor (ph);
    }
}

void SauceVoice::Envelope::start (double peakLevel, double attackSeconds, double sustainLevel, double decayTau, double rate)
{
    peak = peakLevel;
    sustain = sustainLevel;
    attackInc = (peak - level) / std::max (1.0, attackSeconds * rate);
    decayCoef = coefFor (decayTau, rate);
    stage = Attack;
}

void SauceVoice::Envelope::release (double tau, double rate)
{
    releaseCoef = coefFor (tau, rate);
    stage = Release;
}

double SauceVoice::Envelope::next()
{
    switch (stage)
    {
        case Attack:
            level += attackInc;
            if ((attackInc >= 0 && level >= peak) || (attackInc < 0 && level <= peak))
            {
                level = peak;
                stage = Decay;
            }
            break;
        case Decay:   level = sustain + (level - sustain) * decayCoef; break;
        case Release: level *= releaseCoef; break;
        case Idle:    level = 0; break;
    }
    return level;
}

SauceVoice::SauceVoice (const std::atomic<int>& soundType) : soundTypeRef (soundType) {}

void SauceVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    freq = juce::MidiMessage::getMidiNoteInHertz (midiNote);
    vel = juce::jlimit (0.02, 1.0, (double) velocity);
    isBass = isPlayingChannel (2);
    sound = soundTypeRef.load();
    for (auto& p : phase)
        p = 0;
    modPhase = 0;
    svfIc1 = svfIc2 = 0;
    env.level = 0;

    if (isBass)
    {
        env.start (vel * 0.5, 0.01, vel * 0.32, 0.4, sr);
        return;
    }

    switch (sound)
    {
        case Rhodes:
            env.start (vel * 0.34, 0.006, vel * 0.12, 0.9, sr);
            modIndex = 1.6 * vel;          // bright "tine" attack that mellows out
            modIndexTarget = 0.25;
            modCoef = coefFor (0.35, sr);
            break;
        case PadSound:
        {
            env.start (vel * 0.23, 0.45, vel * 0.23, 1.0, sr);
            const double cutoff = 900.0 + 1400.0 * vel;
            const double g = std::tan (juce::MathConstants<double>::pi * std::min (cutoff, sr * 0.45) / sr);
            const double k = 1.0 / 0.6;
            svfA1 = 1.0 / (1.0 + g * (g + k));
            svfA2 = g * svfA1;
            svfA3 = g * svfA2;
            break;
        }
        case Piano:
            env.start (vel * 0.36, 0.004, 0.0001, 0.75, sr);
            break;
        case Organ:
        default:
            env.start (vel * 0.2, 0.012, vel * 0.2, 1.0, sr);
            break;
    }
}

void SauceVoice::stopNote (float, bool allowTailOff)
{
    if (! allowTailOff)
    {
        env.stage = Envelope::Idle;
        env.level = 0;
        clearCurrentNote();
        return;
    }

    double tau = 0.1;
    if (isBass)                  tau = 0.06;
    else if (sound == Rhodes)    tau = 0.12;
    else if (sound == PadSound)  tau = 0.35;
    else if (sound == Piano)     tau = 0.08;
    else                         tau = 0.05;
    env.release (tau, sr);
}

float SauceVoice::renderSample()
{
    const double inc = freq / sr;
    double out = 0.0;

    if (isBass)
    {
        out = std::sin (twoPi * phase[0]) + 0.25 * tri (phase[0]);
        advance (phase[0], inc);
    }
    else
    {
        switch (sound)
        {
            case Rhodes:
            {
                modIndex = modIndexTarget + (modIndex - modIndexTarget) * modCoef;
                const double mod = std::sin (twoPi * modPhase);
                out = std::sin (twoPi * phase[0] + modIndex * mod) + 0.12 * std::sin (twoPi * phase[1]);
                advance (modPhase, inc);
                advance (phase[0], inc);
                advance (phase[1], inc * 2.0);
                break;
            }
            case PadSound:
            {
                const double d = std::pow (2.0, 7.0 / 1200.0);
                const double x = 0.5 * saw (phase[0], inc / d) + 0.5 * saw (phase[1], inc * d) + 0.3 * tri (phase[2]);
                advance (phase[0], inc / d);
                advance (phase[1], inc * d);
                advance (phase[2], inc * 0.5);
                const double v3 = x - svfIc2;
                const double v1 = svfA1 * svfIc1 + svfA2 * v3;
                const double v2 = svfIc2 + svfA2 * svfIc1 + svfA3 * v3;
                svfIc1 = 2.0 * v1 - svfIc1;
                svfIc2 = 2.0 * v2 - svfIc2;
                out = v2;
                break;
            }
            case Piano:
                out = tri (phase[0]) + 0.35 * std::sin (twoPi * phase[1]) + 0.08 * std::sin (twoPi * phase[2]);
                advance (phase[0], inc);
                advance (phase[1], inc * 2.0);
                advance (phase[2], inc * 3.0);
                break;
            case Organ:
            default:
                out = 0.6 * std::sin (twoPi * phase[0]) + 0.35 * std::sin (twoPi * phase[1])
                    + 0.18 * std::sin (twoPi * phase[2]) + 0.08 * std::sin (twoPi * phase[3]);
                advance (phase[0], inc);
                advance (phase[1], inc * 2.0);
                advance (phase[2], inc * 3.0);
                advance (phase[3], inc * 4.0);
                break;
        }
    }
    return (float) (out * env.next());
}

void SauceVoice::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const int channels = buffer.getNumChannels();
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = renderSample();
        for (int ch = 0; ch < channels; ++ch)
            buffer.addSample (ch, startSample + i, s);

        if (env.finished())
        {
            env.stage = Envelope::Idle;
            env.level = 0;
            clearCurrentNote();
            break;
        }
    }
}

} // namespace sauce
