#pragma once

#include "PluginProcessor.h"
#include "ui/ChordViews.h"
#include "ui/SauceLookAndFeel.h"
#include "ui/Widgets.h"

class SoulfulSauceEditor : public juce::AudioProcessorEditor,
                           public ui::AccentSource,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit SoulfulSauceEditor (SoulfulSauceProcessor&);
    ~SoulfulSauceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Colour getAccent() const override { return ui::moodAccent (mood); }
    juce::Colour getDeep() const override { return ui::moodDeep (mood); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshFromProcessor();
    void refreshKeyboard();
    void showSettingsMenu();

    juce::RangedAudioParameter& param (const char* id) { return *processor.apvts.getParameter (id); }

    SoulfulSauceProcessor& processor;
    ui::SauceLookAndFeel lookAndFeel;
    int mood = 0;

    // Top row
    ui::Dropdown keyBox, styleBox, moodBox, barsBox, lengthBox, voicingBox;
    ui::IconButton backButton { "Previous generation", ui::IconButton::Icon::ChevronLeft };
    ui::IconButton forwardButton { "Next generation", ui::IconButton::Icon::ChevronRight };
    ui::HistoryBox historyBox { *this };
    ui::RerollButton rerollButton { *this };
    ui::IconButton settingsButton { "Settings", ui::IconButton::Icon::Settings };

    // Chords
    ui::ChordStrip chordStrip { *this };
    ui::KeyboardView keyboard { *this };
    ui::Segmented octaveSwitch;

    // Knobs
    ui::Knob richnessKnob, spiceKnob, humanizeKnob, velocityKnob, strumKnob;

    // Footer
    ui::PlayButton playButton { *this };
    ui::Segmented rhythmSwitch;
    ui::Dropdown soundBox;
    ui::DragMidiButton dragButton { *this };

    std::vector<std::unique_ptr<ui::IndexAttachment>> attachments;

    int selectedChord = 2;
    int lastVersion = -1, lastPlaying = -2;
    bool lastRunning = false;
    juce::String bpmText = "120 BPM";

    juce::Rectangle<int> pianoPanel, knobPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulfulSauceEditor)
};
