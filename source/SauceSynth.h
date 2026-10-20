#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// The built-in sound: a small polyphonic synth with four characters
// (Rhodes-style FM electric piano, warm pad, soft piano, drawbar organ) and a round bass.
// Bass notes arrive on MIDI channel 2; chord notes on channel 1.

namespace sauce
{

enum SoundType { Rhodes = 0, PadSound, Piano, Organ };

class SauceSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SauceVoice : public juce::SynthesiserVoice
{
public:
    explicit SauceVoice (const std::atomic<int>& soundType);

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SauceSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    struct Envelope
    {
        enum Stage { Idle, Attack, Decay, Release };
        Stage stage = Idle;
        double level = 0, peak = 0, sustain = 0, attackInc = 0, decayCoef = 0, releaseCoef = 0;

        void start (double peakLevel, double attackSeconds, double sustainLevel, double decayTau, double sr);
        void release (double tau, double sr);
        double next();
        bool finished() const { return stage == Release && level < 1.0e-4; }
    };

    float renderSample();

    const std::atomic<int>& soundTypeRef;
    int sound = Rhodes;
    bool isBass = false;
    double sr = 44100.0;
    double freq = 440.0, vel = 0.7;
    double phase[4] {}, modPhase = 0;
    double modIndex = 0, modIndexTarget = 0, modCoef = 0;
    double svfIc1 = 0, svfIc2 = 0, svfA1 = 0, svfA2 = 0, svfA3 = 0;
    Envelope env;
};

} // namespace sauce
