#include "Parameters.h"
#include "engine/ChordEngine.h"

namespace ParamChoices
{
    juce::StringArray keys()
    {
        juce::StringArray a;
        for (auto* n : sauce::kNoteNames)
            a.add (n);
        return a;
    }

    juce::StringArray styles()
    {
        juce::StringArray a;
        for (auto* n : sauce::kStyleNames)
            a.add (n);
        return a;
    }

    juce::StringArray moods()     { return { "Balanced", "Mellow", "Happy" }; }
    juce::StringArray bars()      { return { "4", "8", "16" }; }
    juce::StringArray lengths()
    {
        juce::StringArray a;
        a.add (juce::String::fromUTF8 ("\xc2\xbd bar"));
        a.add ("1 bar");
        a.add ("2 bars");
        return a;
    }
    juce::StringArray voicings()  { return { "Close", "Drop 2", "Wide" }; }
    juce::StringArray rhythms()   { return { "Pads", "Pushed", "Stabs", "Organ" }; }
    juce::StringArray sounds()    { return { "Rhodes", "Pad", "Piano", "Organ", "Off" }; }
    juce::StringArray bassModes() { return { "Sustain", "Follow rhythm", "Off" }; }

    int barsFromIndex (int index)
    {
        static const int values[] = { 4, 8, 16 };
        return values[juce::jlimit (0, 2, index)];
    }

    int engineLengthFromIndex (int index)
    {
        static const int values[] = { sauce::HalfBar, sauce::OneBar, sauce::TwoBars };
        return values[juce::jlimit (0, 2, index)];
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto choice = [&] (const char* id, const char* name, StringArray options, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, options, def));
    };

    auto percent = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };

    choice (ParamIDs::key,     "Key",          ParamChoices::keys(), 9);
    choice (ParamIDs::style,   "Style",        ParamChoices::styles(), 0);
    choice (ParamIDs::mood,    "Mood",         ParamChoices::moods(), 0);
    choice (ParamIDs::bars,    "Bars",         ParamChoices::bars(), 1);
    choice (ParamIDs::length,  "Chord Length", ParamChoices::lengths(), 1);
    choice (ParamIDs::voicing, "Voicing",      ParamChoices::voicings(), 0);

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::octave, 1 }, "Octave", -2, 2, 0,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v > 0 ? "+" + String (v) : String (v); })));

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::richness, 1 }, "Richness", 1, 3, 2,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
        {
            static const char* names[] = { "7ths", "9ths", "Lush" };
            return String (names[jlimit (1, 3, v) - 1]);
        })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::spice, 1 }, "Spice",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f, AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::humanize, 1 }, "Humanize",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f, AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::velocity, 1 }, "Velocity",
        NormalisableRange<float> (0.2f, 1.0f, 0.01f), 0.72f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::strum, 1 }, "Strum",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.1f, AudioParameterFloatAttributes().withStringFromValueFunction (percent)));

    choice (ParamIDs::rhythm,   "Rhythm",    ParamChoices::rhythms(), 0);
    choice (ParamIDs::sound,    "Sound",     ParamChoices::sounds(), 0);
    choice (ParamIDs::bassMode, "Bass Note", ParamChoices::bassModes(), 0);

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::volume, 1 }, "Volume",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f, AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::midiOut, 1 }, "MIDI Out", true));

    return layout;
}
