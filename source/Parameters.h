#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    inline constexpr const char* key       = "key";
    inline constexpr const char* style     = "style";
    inline constexpr const char* mood      = "mood";
    inline constexpr const char* bars      = "bars";
    inline constexpr const char* length    = "chordLength";
    inline constexpr const char* voicing   = "voicing";
    inline constexpr const char* octave    = "octave";
    inline constexpr const char* richness  = "richness";
    inline constexpr const char* spice     = "spice";
    inline constexpr const char* humanize  = "humanize";
    inline constexpr const char* velocity  = "velocity";
    inline constexpr const char* strum     = "strum";
    inline constexpr const char* rhythm    = "rhythm";
    inline constexpr const char* sound     = "sound";
    inline constexpr const char* bassMode  = "bassMode";
    inline constexpr const char* volume    = "volume";
    inline constexpr const char* midiOut   = "midiOut";
}

namespace ParamChoices
{
    juce::StringArray keys();
    juce::StringArray styles();
    juce::StringArray moods();
    juce::StringArray bars();        // "4", "8", "16"
    juce::StringArray lengths();     // "1/2 bar", "1 bar", "2 bars"
    juce::StringArray voicings();
    juce::StringArray rhythms();
    juce::StringArray sounds();      // ... plus "Off"
    juce::StringArray bassModes();

    int barsFromIndex (int index);           // 4, 8, 16
    int engineLengthFromIndex (int index);   // sauce::ChordLength
    constexpr int soundOff = 4;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
