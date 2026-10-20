#include "PluginEditor.h"

namespace
{
    constexpr int kWidth = 1120, kHeight = 600, kPadX = 22, kPadY = 18;

    juce::Array<juce::Colour> moodDots()
    {
        return { ui::moodAccent (0), ui::moodAccent (1), ui::moodAccent (2) };
    }

    juce::StringArray octaveLabels()
    {
        const auto minus = juce::String::fromUTF8 ("\xe2\x88\x92");
        juce::StringArray a;
        a.add (minus + "2");
        a.add (minus + "1");
        a.add ("0");
        a.add ("+1");
        a.add ("+2");
        return a;
    }

    // The volume slider shown inside the settings menu
    class VolumeItem : public juce::PopupMenu::CustomComponent
    {
    public:
        explicit VolumeItem (juce::RangedAudioParameter& p) : juce::PopupMenu::CustomComponent (false), attachment (p, slider)
        {
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setTitle ("Volume");
            addAndMakeVisible (slider);
        }
        void getIdealSize (int& w, int& h) override { w = 240; h = 46; }
        void resized() override { slider.setBounds (getLocalBounds().reduced (14, 0).withTrimmedTop (18)); }
        void paint (juce::Graphics& g) override
        {
            ui::drawCaption (g, "VOLUME", { 14.0f, 6.0f, 120.0f, 10.0f });
            g.setColour (ui::colours::label);
            g.setFont (ui::Fonts::mono (11.0f));
            g.drawText (slider.getTextFromValue (slider.getValue()), juce::Rectangle<int> (getWidth() - 74, 4, 60, 14),
                        juce::Justification::centredRight, false);
        }

    private:
        juce::Slider slider;
        juce::SliderParameterAttachment attachment;
    };
}

SoulfulSauceEditor::SoulfulSauceEditor (SoulfulSauceProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      keyBox (*this, "KEY", ParamChoices::keys()),
      styleBox (*this, "STYLE", ParamChoices::styles()),
      moodBox (*this, "MOOD", ParamChoices::moods()),
      barsBox (*this, "BARS", ParamChoices::bars()),
      lengthBox (*this, "CHORD LENGTH", ParamChoices::lengths()),
      voicingBox (*this, "VOICING", ParamChoices::voicings()),
      octaveSwitch (*this, octaveLabels(), true),
      richnessKnob (param (ParamIDs::richness), "Richness"),
      spiceKnob (param (ParamIDs::spice), "Spice"),
      humanizeKnob (param (ParamIDs::humanize), "Humanize"),
      velocityKnob (param (ParamIDs::velocity), "Velocity"),
      strumKnob (param (ParamIDs::strum), "Strum"),
      rhythmSwitch (*this, ParamChoices::rhythms()),
      soundBox (*this, "SOUND", ParamChoices::sounds())
{
    setLookAndFeel (&lookAndFeel);
    mood = processor.getMoodIndex();
    lookAndFeel.accent = getAccent();

    keyBox.setNumColumns (4);
    keyBox.valueText = [this] (int i)
    {
        const int style = juce::roundToInt (processor.apvts.getRawParameterValue (ParamIDs::style)->load());
        const bool minor = style == sauce::DeepHouse || style == sauce::AfroSoul;
        return juce::String (sauce::kNoteNames[juce::jlimit (0, 11, i)]) + (minor ? " minor" : " major");
    };
    moodBox.setDotColours (moodDots());
    soundBox.setOpensUpward (true);

    for (auto* c : std::initializer_list<juce::Component*> { &keyBox, &styleBox, &moodBox, &barsBox, &lengthBox, &voicingBox,
                                                             &backButton, &historyBox, &forwardButton, &rerollButton, &settingsButton,
                                                             &chordStrip, &keyboard, &octaveSwitch,
                                                             &richnessKnob, &spiceKnob, &humanizeKnob, &velocityKnob, &strumKnob,
                                                             &playButton, &rhythmSwitch, &soundBox, &dragButton })
        addAndMakeVisible (c);

    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::key), keyBox));
    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::style), styleBox));
    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::mood), moodBox));
    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::bars), barsBox));
    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::length), lengthBox));
    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::voicing), voicingBox));
    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::octave), octaveSwitch, -2));
    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::rhythm), rhythmSwitch));
    attachments.push_back (std::make_unique<ui::IndexAttachment> (param (ParamIDs::sound), soundBox));

    backButton.onClick = [this] { processor.historyBack(); };
    forwardButton.onClick = [this] { processor.historyForward(); };
    rerollButton.onClick = [this] { processor.reroll(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };

    chordStrip.onSelect = [this] (int i)
    {
        selectedChord = i;
        chordStrip.setSelected (i);
        refreshKeyboard();
        processor.auditionChord (i);
    };
    chordStrip.onToggleLock = [this] (int i) { processor.toggleLock (i); };

    playButton.onClick = [this]
    {
        if (! processor.isHostPlaying())
            processor.setPreviewPlaying (! processor.isPreviewPlaying());
    };
    dragButton.createFile = [this] { return processor.writeMidiFile(); };

    processor.addChangeListener (this);
    setSize (kWidth, kHeight);
    refreshFromProcessor();
    startTimerHz (30);
}

SoulfulSauceEditor::~SoulfulSauceEditor()
{
    processor.removeChangeListener (this);
    attachments.clear();
    setLookAndFeel (nullptr);
}

void SoulfulSauceEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromProcessor();
}

void SoulfulSauceEditor::refreshFromProcessor()
{
    const auto pattern = processor.getPattern();
    lastVersion = processor.getVersion();

    const int newMood = processor.getMoodIndex();
    if (newMood != mood)
    {
        mood = newMood;
        lookAndFeel.accent = getAccent();
        repaint();
    }
    keyBox.repaint();   // "minor"/"major" follows the style

    historyBox.setPosition (processor.getHistoryPosition(), processor.getHistorySize());
    backButton.setEnabled (processor.getHistoryPosition() > 0);
    forwardButton.setEnabled (processor.getHistoryPosition() < processor.getHistorySize() - 1);

    if (pattern == nullptr)
        return;

    std::vector<ui::CardData> cards;
    const double span = pattern->chordSpan, barQ = pattern->barQuarters;
    for (size_t i = 0; i < pattern->chords.size(); ++i)
    {
        const auto& c = pattern->chords[i];
        const double start = (double) i * span;
        const int bar = (int) std::floor (start / barQ + 1e-9) + 1;
        const int beat = (int) std::floor (std::fmod (start, barQ) + 1e-9) + 1;
        ui::CardData d;
        d.name = c.name;
        d.roman = juce::String (c.roman).replace ("b", juce::String::fromUTF8 ("\xe2\x99\xad")).replace ("#", juce::String::fromUTF8 ("\xe2\x99\xaf"));
        d.bar = beat == 1 ? "BAR " + juce::String (bar) : "BAR " + juce::String (bar) + "." + juce::String (beat);
        d.locked = c.locked;
        cards.push_back (d);
    }
    selectedChord = juce::jlimit (0, juce::jmax (0, (int) cards.size() - 1), selectedChord);
    chordStrip.setSelected (selectedChord);
    chordStrip.setChords (cards);
    refreshKeyboard();
    repaint (pianoPanel);
}

void SoulfulSauceEditor::refreshKeyboard()
{
    const auto pattern = processor.getPattern();
    if (pattern == nullptr || pattern->chords.empty())
        return;
    const int playing = processor.getPlayingChord();
    const int shown = juce::jlimit (0, (int) pattern->chords.size() - 1, playing >= 0 ? playing : selectedChord);
    const auto& c = pattern->chords[(size_t) shown];
    keyboard.setNotes (c.upper, c.bassNote);
    repaint (pianoPanel.withHeight (40));
}

void SoulfulSauceEditor::timerCallback()
{
    if (processor.getVersion() != lastVersion)
        refreshFromProcessor();

    const int playing = processor.getPlayingChord();
    if (playing != lastPlaying)
    {
        lastPlaying = playing;
        chordStrip.setPlaying (playing);
        refreshKeyboard();
    }

    const bool running = processor.isHostPlaying() || processor.isPreviewPlaying();
    if (running != lastRunning)
    {
        lastRunning = running;
        playButton.setPlaying (running);
    }

    const auto text = juce::String (juce::roundToInt (processor.getHostBpm())) + " BPM";
    if (text != bpmText)
    {
        bpmText = text;
        repaint (playButton.getBounds().withWidth (130));
    }
}

void SoulfulSauceEditor::showSettingsMenu()
{
    juce::PopupMenu bass;
    const int bassMode = juce::roundToInt (processor.apvts.getRawParameterValue (ParamIDs::bassMode)->load());
    const auto modes = ParamChoices::bassModes();
    for (int i = 0; i < modes.size(); ++i)
        bass.addItem (100 + i, modes[i], true, i == bassMode);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Soulful Sauce " + juce::String (JucePlugin_VersionString));
    menu.addSubMenu ("Bass note", bass);
    const bool midiOut = processor.apvts.getRawParameterValue (ParamIDs::midiOut)->load() > 0.5f;
    menu.addItem (200, "Send MIDI out", true, midiOut);
    menu.addSeparator();
    menu.addCustomItem (300, std::make_unique<VolumeItem> (param (ParamIDs::volume)), nullptr, "Volume");

    juce::Component::SafePointer<SoulfulSauceEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton).withMinimumWidth (240),
                        [safe] (int result)
                        {
                            if (safe == nullptr)
                                return;
                            auto set = [safe] (const char* id, float value)
                            {
                                auto& p = safe->param (id);
                                p.beginChangeGesture();
                                p.setValueNotifyingHost (p.convertTo0to1 (value));
                                p.endChangeGesture();
                            };
                            if (result >= 100 && result < 103)
                                set (ParamIDs::bassMode, (float) (result - 100));
                            else if (result == 200)
                                set (ParamIDs::midiOut, safe->processor.apvts.getRawParameterValue (ParamIDs::midiOut)->load() > 0.5f ? 0.0f : 1.0f);
                        });
}

//==============================================================================
void SoulfulSauceEditor::paint (juce::Graphics& g)
{
    using namespace ui;
    const auto accent = getAccent();
    g.fillAll (colours::background);

    // Header: logo + title
    const juce::Rectangle<float> logo ((float) kPadX, (float) kPadY + 6.0f, 34.0f, 34.0f);
    const auto lc = logo.getCentre();
    auto ring = [&] (float r, juce::Colour c, float w) { g.setColour (c); g.drawEllipse (lc.x - r, lc.y - r, r * 2, r * 2, w); };
    ring (14.9f, accent, 2.1f);
    ring (10.6f, colours::logoRing, 1.6f);
    ring (6.9f, colours::logoRing, 1.6f);
    g.setColour (accent);
    g.fillEllipse (lc.x - 2.8f, lc.y - 2.8f, 5.6f, 5.6f);

    g.setColour (accent);
    g.setFont (Fonts::sans (10.0f, 800, 0.22f));
    g.drawText ("JUST FEEZY", juce::Rectangle<float> (logo.getRight() + 12.0f, logo.getY() + 3.0f, 200.0f, 11.0f),
                juce::Justification::centredLeft, false);
    g.setColour (colours::text);
    g.setFont (Fonts::sans (20.0f, 800, -0.01f));
    g.drawText ("Soulful Sauce", juce::Rectangle<float> (logo.getRight() + 12.0f, logo.getY() + 14.0f, 260.0f, 22.0f),
                juce::Justification::centredLeft, false);

    g.setColour (colours::panelBorder);
    g.fillRect (kPadX, kPadY + 57, kWidth - 2 * kPadX, 1);   // header line
    g.fillRect (kPadX, 523, kWidth - 2 * kPadX, 1);          // footer line

    // Panels
    for (auto r : { pianoPanel, knobPanel })
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r.toFloat().reduced (0.5f), 14.0f);
        g.setColour (colours::panelBorder);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 14.0f, 1.0f);
    }

    // Chord name + notes
    if (auto pattern = processor.getPattern(); pattern != nullptr && ! pattern->chords.empty())
    {
        const int playing = processor.getPlayingChord();
        const int shown = juce::jlimit (0, (int) pattern->chords.size() - 1, playing >= 0 ? playing : selectedChord);
        const auto& c = pattern->chords[(size_t) shown];
        auto line = juce::Rectangle<float> ((float) pianoPanel.getX() + 16.0f, (float) pianoPanel.getY() + 12.0f, 600.0f, 20.0f);
        g.setColour (accent);
        g.setFont (Fonts::sans (16.0f, 800));
        juce::GlyphArrangement ga;
        ga.addLineOfText (g.getCurrentFont(), c.name, 0, 0);
        const float nameW = ga.getBoundingBox (0, -1, true).getWidth();
        g.drawText (c.name, line, juce::Justification::centredLeft, false);

        juce::String tones;
        if (c.bassNote >= 0)
            tones << sauce::kNoteNames[c.bassNote % 12] << "  " << juce::String::fromUTF8 ("\xc2\xb7") << "  ";
        for (size_t i = 0; i < c.upper.size(); ++i)
            tones << (i > 0 ? " " : "") << sauce::kNoteNames[c.upper[i] % 12];
        g.setColour (colours::textSoft);
        g.setFont (Fonts::mono (12.0f));
        g.drawText (tones, line.withTrimmedLeft (nameW + 12.0f), juce::Justification::centredLeft, false);

        if (playing >= 0)
        {
            const double start = playing * pattern->chordSpan;
            const int bar = (int) std::floor (start / pattern->barQuarters + 1e-9) + 1;
            drawCaption (g, "PLAYING " + juce::String::fromUTF8 ("\xc2\xb7") + " BAR " + juce::String (bar),
                         { (float) pianoPanel.getRight() - 216.0f, (float) pianoPanel.getY() + 12.0f, 200.0f, 20.0f },
                         juce::Justification::centredRight, 10.0f);
        }
    }

    // Octave caption + legend
    const auto oct = octaveSwitch.getBounds().toFloat();
    drawCaption (g, "OCTAVE", { (float) pianoPanel.getX() + 16.0f, oct.getY(), 60.0f, oct.getHeight() }, juce::Justification::centredLeft, 10.0f);
    auto legend = [&] (float x, juce::Colour c, const juce::String& text)
    {
        g.setColour (c);
        g.fillRoundedRectangle (x, oct.getCentreY() - 4.5f, 9.0f, 9.0f, 2.0f);
        g.setColour (colours::textMuted);
        g.setFont (Fonts::sans (11.0f, 600));
        g.drawText (text, juce::Rectangle<float> (x + 15.0f, oct.getY(), 50.0f, oct.getHeight()), juce::Justification::centredLeft, false);
    };
    legend ((float) pianoPanel.getRight() - 128.0f, accent, "Chord");
    legend ((float) pianoPanel.getRight() - 64.0f, getDeep(), "Bass");

    // Footer tempo
    g.setColour (colours::label);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (bpmText, juce::Rectangle<int> (playButton.getRight() + 10, playButton.getY(), 70, playButton.getHeight()),
                juce::Justification::centredLeft, false);
}

void SoulfulSauceEditor::resized()
{
    // Header
    settingsButton.setBounds (kWidth - kPadX - 38, kPadY + 4, 38, 38);

    // Control row
    int x = kPadX;
    const int rowY = 90, rowH = 48;
    for (auto [box, w] : { std::pair<ui::Dropdown*, int> { &keyBox, 112 }, { &styleBox, 142 }, { &moodBox, 120 },
                           { &barsBox, 76 }, { &lengthBox, 122 }, { &voicingBox, 102 } })
    {
        box->setBounds (x, rowY, w, rowH);
        x += w + 8;
    }
    int right = kWidth - kPadX;
    rerollButton.setBounds (right - 128, rowY, 128, rowH);
    right -= 128 + 10;
    forwardButton.setBounds (right - 42, rowY, 42, rowH);
    right -= 42 + 4;
    historyBox.setBounds (right - 78, rowY, 78, rowH);
    right -= 78 + 4;
    backButton.setBounds (right - 42, rowY, 42, rowH);

    // Chord cards
    chordStrip.setBounds (kPadX, 152, kWidth - 2 * kPadX, 120);

    // Piano + knobs
    pianoPanel = { kPadX, 286, 680, 194 };
    knobPanel = { pianoPanel.getRight() + 12, 286, kWidth - kPadX - pianoPanel.getRight() - 12, 194 };
    keyboard.setBounds (pianoPanel.getX() + (pianoPanel.getWidth() - (49 * ui::KeyboardView::keyStep - 1)) / 2,
                        pianoPanel.getY() + 40, 49 * ui::KeyboardView::keyStep - 1, 90);
    octaveSwitch.setBounds (pianoPanel.getX() + 82, pianoPanel.getY() + 144, 214, 34);

    const int colW = (knobPanel.getWidth() - 20) / 5;
    int kx = knobPanel.getX() + 10;
    for (auto* k : { &richnessKnob, &spiceKnob, &humanizeKnob, &velocityKnob, &strumKnob })
    {
        k->setBounds (kx, knobPanel.getY() + 46, colW, 100);
        kx += colW;
    }

    // Footer
    const int footY = 536;
    playButton.setBounds (kPadX, footY, 46, 46);
    rhythmSwitch.setBounds (kPadX + 46 + 88, footY + 2, 300, 42);
    soundBox.setBounds (rhythmSwitch.getRight() + 14, footY, 132, 46);
    dragButton.setBounds (kWidth - kPadX - 150, footY, 150, 46);
}
