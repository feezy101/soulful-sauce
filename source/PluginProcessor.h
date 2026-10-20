#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "SauceSynth.h"
#include "engine/ChordEngine.h"
#include "engine/Pattern.h"

#include <array>
#include <map>

class SoulfulSauceProcessor : public juce::AudioProcessor,
                              public juce::ChangeBroadcaster,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater,
                              private juce::Timer
{
public:
    SoulfulSauceProcessor();
    ~SoulfulSauceProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // Editor API (message thread)
    std::shared_ptr<const sauce::Pattern> getPattern() const { return messagePattern; }
    int getVersion() const { return version.load(); }
    int getHistoryPosition() const;
    int getHistorySize() const;
    void historyBack();
    void historyForward();
    void reroll();
    void toggleLock (int chordIndex);
    void auditionChord (int chordIndex) { auditionRequest.store (chordIndex); }
    void setPreviewPlaying (bool shouldPlay);
    bool isPreviewPlaying() const { return previewPlaying.load(); }
    bool isHostPlaying() const { return hostPlaying.load(); }
    double getHostBpm() const { return hostBpm.load(); }
    int getPlayingChord() const { return playingChord.load(); }
    int getMoodIndex() const { return choiceIndex (ParamIDs::mood); }
    juce::File writeMidiFile() const;

private:
    struct Entry
    {
        int seed = 1;
        int style = 0, mood = 0, bars = 1, length = 1, richness = 2;
        float spice = 0.3f;
        std::map<int, sauce::Chord> locks;
    };

    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;

    int choiceIndex (const char* id) const;
    float rawValue (const char* id) const;
    sauce::Settings currentSettings() const;
    sauce::Feel currentFeel() const;
    void syncEntryFromParameters();
    void applyEntryToParameters (const Entry&);
    void regenerate();

    // Audio thread
    struct PendingEvent { int sample; bool on; int note; bool bass; float velocity; };
    void noteOn (juce::MidiBuffer&, int note, bool bass, float velocity, int sample);
    void noteOff (juce::MidiBuffer&, int note, bool bass, int sample);
    void allNotesOff (juce::MidiBuffer&, int sample);
    void collectRange (const sauce::Pattern&, double q0, double q1, double quartersPerSample, int numSamples);
    void endAudition (juce::MidiBuffer&, int sample);

    // History (message thread, guarded for hosts that save state from another thread)
    juce::CriticalSection historyLock;
    std::vector<Entry> history { Entry {} };
    int historyPos = 0;

    // Pattern hand-off between threads
    juce::SpinLock patternLock;
    std::shared_ptr<const sauce::Pattern> sharedPattern;    // guarded by patternLock
    std::shared_ptr<const sauce::Pattern> messagePattern;   // message thread
    std::vector<std::shared_ptr<const sauce::Pattern>> retired;
    std::shared_ptr<const sauce::Pattern> audioPattern;     // audio thread
    std::atomic<int> version { 0 };

    // Sound
    juce::Synthesiser synth;
    std::atomic<int> soundType { 0 };
    juce::Reverb reverb;
    bool soundWasOff = false;

    // Playback state (audio thread)
    std::array<uint8_t, 256> active {};
    std::vector<PendingEvent> pending;
    std::vector<std::pair<int, bool>> auditionNotes;
    int auditionSamplesLeft = 0;
    bool wasRunning = false;
    double expectedQ = 0.0, previewQ = 0.0;

    std::atomic<bool> previewPlaying { false }, previewRestart { false }, hostPlaying { false };
    std::atomic<double> hostBpm { 120.0 }, barQuarters { 4.0 };
    std::atomic<int> playingChord { -1 }, auditionRequest { -1 };

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* midiOutParam = nullptr;
    std::atomic<float>* soundParam = nullptr;
    std::atomic<float>* velocityParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulfulSauceProcessor)
};
