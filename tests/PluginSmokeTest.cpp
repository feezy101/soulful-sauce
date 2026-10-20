// Runs the real plugin processor without a DAW: checks MIDI output, history, locks,
// state save/restore and the MIDI file, renders audio demos and a picture of the window.
// Build with -DSAUCE_SMOKE_TEST=ON. Usage: PluginSmokeTest <output folder>

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! ok)
            ++failures;
    }

    void pump (int ms = 60) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    struct FakePlayHead : juce::AudioPlayHead
    {
        double bpm = 122.0, ppq = 0.0;
        bool playing = true;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm);
            p.setPpqPosition (ppq);
            p.setIsPlaying (playing);
            p.setTimeSignature (TimeSignature { 4, 4 });
            return p;
        }
    };

    struct Rendered
    {
        juce::AudioBuffer<float> audio;
        std::vector<std::pair<int64_t, juce::MidiMessage>> midi;
    };

    constexpr double kRate = 48000.0;
    constexpr int kBlock = 512;

    Rendered render (SoulfulSauceProcessor& p, FakePlayHead& ph, double quarters, bool keepAudio = true)
    {
        Rendered r;
        const int blocks = (int) std::ceil (quarters * 60.0 / ph.bpm * kRate / kBlock);
        if (keepAudio)
            r.audio.setSize (2, blocks * kBlock);
        juce::AudioBuffer<float> buf (2, kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            p.processBlock (buf, midi);
            for (const auto m : midi)
                r.midi.push_back ({ (int64_t) b * kBlock + m.samplePosition, m.getMessage() });
            if (keepAudio)
                for (int ch = 0; ch < 2; ++ch)
                    r.audio.copyFrom (ch, b * kBlock, buf, ch, 0, kBlock);
            if (ph.playing)
                ph.ppq += kBlock * ph.bpm / 60.0 / kRate;
        }
        return r;
    }

    juce::StringArray names (const SoulfulSauceProcessor& p)
    {
        juce::StringArray a;
        if (auto pat = p.getPattern())
            for (const auto& c : pat->chords)
                a.add (c.name);
        return a;
    }

    void setParam (SoulfulSauceProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        pump();
    }

    void writeWav (const juce::AudioBuffer<float>& audio, const juce::File& file)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (file.createOutputStream().release());
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), kRate, 2, 24, {}, 0)))
        {
            os.release();
            writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    out.createDirectory();

    auto proc = std::make_unique<SoulfulSauceProcessor>();
    FakePlayHead ph;
    proc->setPlayHead (&ph);
    proc->setPlayConfigDetails (0, 2, kRate, kBlock);
    proc->prepareToPlay (kRate, kBlock);
    pump();

    std::cout << "Defaults\n";
    const auto initial = names (*proc);
    std::cout << "  chords: " << initial.joinIntoString (" | ") << "\n";
    {
        sauce::Settings s;
        juce::StringArray expected;
        for (const auto& c : sauce::generate (s))
            expected.add (c.name);
        check (initial == expected, "plugin chords match the engine / FL script (A minor, Deep House, seed 1)");
    }

    std::cout << "Playback in time with the host\n";
    auto pat = proc->getPattern();
    {
        ph.ppq = 0.0;
        ph.playing = true;
        auto r = render (*proc, ph, pat->loopLength + 0.01);
        int onsInLoop = 0;
        const double samplesPerQuarter = 60.0 / ph.bpm * kRate;
        std::map<int, int> firstOns;
        for (const auto& [t, m] : r.midi)
            if (m.isNoteOn() && (double) t < pat->loopLength * samplesPerQuarter - 1.0)
                ++onsInLoop;
        int atZero = 0;
        for (const auto& [t, m] : r.midi)
            if (m.isNoteOn() && t == 0)
                ++atZero;
        const auto& c0 = pat->chords[0];
        check (onsInLoop == (int) pat->notes.size(), "one loop sends every note once (" + juce::String (onsInLoop) + " of " + juce::String ((int) pat->notes.size()) + ")");
        check (atZero >= 1 && atZero <= (int) c0.upper.size() + 1, "first chord starts on the downbeat");
        bool allChannel1 = true;
        for (const auto& [t, m] : r.midi)
            allChannel1 = allChannel1 && m.getChannel() == 1;
        check (allChannel1, "MIDI out is all on channel 1");

        float peak = r.audio.getMagnitude (0, r.audio.getNumSamples());
        float rms = r.audio.getRMSLevel (0, 0, r.audio.getNumSamples());
        check (rms > 0.005f && peak <= 1.0f, "built-in sound plays (rms " + juce::String (rms, 3) + ", peak " + juce::String (peak, 3) + ")");

        // Stop: every note must be released
        ph.playing = false;
        auto stop = render (*proc, ph, 0.25, false);
        std::map<int, int> balance;
        for (auto* list : { &r.midi, &stop.midi })
            for (const auto& [t, m] : *list)
            {
                if (m.isNoteOn()) balance[m.getNoteNumber()]++;
                else if (m.isNoteOff()) balance[m.getNoteNumber()]--;
            }
        bool clean = true;
        for (const auto& [n, b] : balance)
            clean = clean && b == 0;
        check (clean, "stopping releases every note (no stuck notes)");
    }

    std::cout << "Jumping and looping in the song\n";
    {
        ph.playing = true;
        ph.ppq = 6.0;   // middle of bar 2
        auto r = render (*proc, ph, 0.05, false);
        int chased = 0;
        for (const auto& [t, m] : r.midi)
            if (m.isNoteOn() && t == 0)
                ++chased;
        check (chased == (int) pat->chords[1].upper.size() + 1, "starting mid-chord plays the chord that should be sounding");
        ph.playing = false;
        render (*proc, ph, 0.25, false);
    }

    std::cout << "History and locks\n";
    {
        proc->reroll();
        pump();
        const auto second = names (*proc);
        check (second != initial && proc->getHistorySize() == 2, "Re-roll makes a new progression: " + second.joinIntoString (" | "));
        proc->historyBack();
        pump();
        check (names (*proc) == initial && proc->getHistoryPosition() == 0, "back arrow returns the first progression");
        proc->historyForward();
        pump();
        check (names (*proc) == second, "forward arrow returns the second progression");

        const auto locked = proc->getPattern()->chords[3].name;
        proc->toggleLock (3);
        proc->reroll();
        pump();
        const auto third = proc->getPattern();
        check (third->chords[3].name == locked && third->chords[3].locked, "a locked chord survives Re-roll (" + juce::String (locked) + ")");
    }

    std::cout << "Settings change the chords\n";
    {
        setParam (*proc, ParamIDs::mood, 1.0f);
        sauce::Settings s;
        s.mood = sauce::Mellow;
        bool moodOk = proc->getPattern()->chords.size() == 8;
        check (moodOk, "Mellow mood regenerates: " + names (*proc).joinIntoString (" | "));
        setParam (*proc, ParamIDs::bars, 2.0f);
        check (proc->getPattern()->chords.size() == 16, "16 bars gives 16 chords");
        setParam (*proc, ParamIDs::length, 0.0f);
        check (proc->getPattern()->chords.size() == 32, "half-bar chords over 16 bars gives 32 chords");
        setParam (*proc, ParamIDs::bars, 1.0f);
        setParam (*proc, ParamIDs::length, 1.0f);
        setParam (*proc, ParamIDs::mood, 0.0f);
    }

    std::cout << "Saving with the project\n";
    {
        juce::MemoryBlock state;
        proc->getStateInformation (state);
        auto copy = std::make_unique<SoulfulSauceProcessor>();
        copy->setStateInformation (state.getData(), (int) state.getSize());
        pump();
        check (names (*copy) == names (*proc), "reopening the project restores the same chords");
        check (copy->getHistorySize() == proc->getHistorySize() && copy->getHistoryPosition() == proc->getHistoryPosition(),
               "reopening restores the history (" + juce::String (copy->getHistorySize()) + " progressions)");
        check (copy->getPattern()->chords[3].locked, "reopening keeps locked chords");
    }

    std::cout << "Drag MIDI file\n";
    {
        const auto f = proc->writeMidiFile();
        juce::MidiFile mf;
        juce::FileInputStream in (f);
        const bool read = in.openedOk() && mf.readFrom (in);
        int ons = 0;
        if (read)
            for (int t = 0; t < mf.getNumTracks(); ++t)
                for (const auto* e : *mf.getTrack (t))
                    if (e->message.isNoteOn())
                        ++ons;
        check (read && ons == (int) proc->getPattern()->notes.size(), "MIDI file has every note: " + f.getFileName());
        f.copyFileTo (out.getChildFile ("Soulful Sauce example.mid"));
    }

    std::cout << "Clicking a chord / preview button\n";
    {
        ph.playing = false;
        render (*proc, ph, 0.5, false);
        proc->auditionChord (2);
        auto r = render (*proc, ph, 3.0, false);
        int ons = 0, offs = 0;
        for (const auto& [t, m] : r.midi)
        {
            ons += m.isNoteOn() ? 1 : 0;
            offs += m.isNoteOff() ? 1 : 0;
        }
        const auto& c2 = proc->getPattern()->chords[2];
        check (ons == (int) c2.upper.size() + 1 && offs == ons, "clicking a card plays that chord, then releases it");

        proc->setPreviewPlaying (true);
        auto pr = render (*proc, ph, 4.0, false);
        int previewOns = 0;
        for (const auto& [t, m] : pr.midi)
            previewOns += m.isNoteOn() ? 1 : 0;
        check (previewOns > 0 && proc->getPlayingChord() == 0, "plugin Play button previews with FL stopped");
        proc->setPreviewPlaying (false);
        render (*proc, ph, 0.25, false);
    }

    std::cout << "Audio demos\n";
    {
        struct Demo { const char* file; int mood, sound, rhythm, style; };
        const Demo demos[] = {
            { "Balanced - Deep House - Rhodes.wav", 0, 0, 0, 0 },
            { "Mellow - Deep House - Pad.wav", 1, 1, 0, 0 },
            { "Happy - Soulful House - Rhodes stabs.wav", 2, 0, 2, 1 },
            { "Balanced - Neo-Soul - Piano pushed.wav", 0, 2, 1, 2 },
        };
        for (const auto& d : demos)
        {
            auto p = std::make_unique<SoulfulSauceProcessor>();
            FakePlayHead h;
            p->setPlayHead (&h);
            p->setPlayConfigDetails (0, 2, kRate, kBlock);
            p->prepareToPlay (kRate, kBlock);
            setParam (*p, ParamIDs::mood, (float) d.mood);
            setParam (*p, ParamIDs::sound, (float) d.sound);
            setParam (*p, ParamIDs::rhythm, (float) d.rhythm);
            setParam (*p, ParamIDs::style, (float) d.style);
            if (d.style != 0)
                setParam (*p, ParamIDs::key, 5.0f);   // F major for the major styles
            h.playing = true;
            auto r = render (*p, h, 32.0);
            h.playing = false;
            auto tail = render (*p, h, 4.0);
            juce::AudioBuffer<float> all (2, r.audio.getNumSamples() + tail.audio.getNumSamples());
            for (int ch = 0; ch < 2; ++ch)
            {
                all.copyFrom (ch, 0, r.audio, ch, 0, r.audio.getNumSamples());
                all.copyFrom (ch, r.audio.getNumSamples(), tail.audio, ch, 0, tail.audio.getNumSamples());
            }
            writeWav (all, out.getChildFile (d.file));
            std::cout << "  wrote " << d.file << "  (" << names (*p).joinIntoString (" | ") << ", peak "
                      << juce::String (all.getMagnitude (0, all.getNumSamples()), 2) << ")\n";
            p->setPlayHead (nullptr);
        }
    }

    std::cout << "Plugin window\n";
    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr)
    {
        for (int m = 0; m < 3; ++m)
        {
            setParam (*proc, ParamIDs::mood, (float) m);
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
            pump (120);
            auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            const auto file = out.getChildFile ("window-" + juce::String (m) + ".png");
            file.deleteFile();
            juce::FileOutputStream os (file);
            juce::PNGImageFormat().writeImageToStream (img, os);
            check (img.getWidth() == 2240, "window renders (" + file.getFileName() + ")");
            editor.reset();
            pump();
        }
    }
    else
    {
        std::cout << "  (no display - window snapshot skipped)\n";
    }

    proc->setPlayHead (nullptr);
    proc.reset();
    std::cout << (failures == 0 ? "\nALL CHECKS PASSED\n" : "\nSOME CHECKS FAILED\n");
    return failures == 0 ? 0 : 1;
}
