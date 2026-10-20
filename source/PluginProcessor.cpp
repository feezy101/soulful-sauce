#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kChordChannel = 1;
    constexpr int kBassChannel = 2;   // internal only; MIDI out sends everything on channel 1
    constexpr int kMaxHistory = 50;

    juce::String locksToString (const std::map<int, sauce::Chord>& locks)
    {
        juce::StringArray parts;
        for (const auto& [i, c] : locks)
            parts.add (juce::String (i) + "," + juce::String (c.root) + "," + juce::String ((int) c.fam) + ","
                       + juce::String (c.bass) + "," + juce::String (c.rich));
        return parts.joinIntoString (";");
    }

    std::map<int, sauce::Chord> locksFromString (const juce::String& s)
    {
        std::map<int, sauce::Chord> locks;
        for (const auto& part : juce::StringArray::fromTokens (s, ";", ""))
        {
            const auto f = juce::StringArray::fromTokens (part, ",", "");
            if (f.size() != 5)
                continue;
            const char fam = (char) f[2].getIntValue();
            if (juce::String ("MmDXSho6").indexOfChar (fam) < 0)
                continue;
            locks[f[0].getIntValue()] = { juce::jlimit (0, 11, f[1].getIntValue()), fam,
                                          juce::jlimit (-1, 11, f[3].getIntValue()), juce::jlimit (0, 1, f[4].getIntValue()) };
        }
        return locks;
    }
}

//==============================================================================
SoulfulSauceProcessor::SoulfulSauceProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SoulfulSauce", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (ranged->getParameterID(), this);

    volumeParam   = apvts.getRawParameterValue (ParamIDs::volume);
    midiOutParam  = apvts.getRawParameterValue (ParamIDs::midiOut);
    soundParam    = apvts.getRawParameterValue (ParamIDs::sound);
    velocityParam = apvts.getRawParameterValue (ParamIDs::velocity);

    for (int i = 0; i < 24; ++i)
        synth.addVoice (new sauce::SauceVoice (soundType));
    synth.addSound (new sauce::SauceSound());

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.62f;
    rp.damping = 0.45f;
    rp.wetLevel = 0.2f;
    rp.dryLevel = 0.85f;
    rp.width = 0.9f;
    reverb.setParameters (rp);

    pending.reserve (4096);
    auditionNotes.reserve (16);

    syncEntryFromParameters();
    regenerate();
    startTimer (1000);
}

SoulfulSauceProcessor::~SoulfulSauceProcessor()
{
    cancelPendingUpdate();
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (ranged->getParameterID(), this);
}

//==============================================================================
int SoulfulSauceProcessor::choiceIndex (const char* id) const
{
    return juce::roundToInt (apvts.getRawParameterValue (id)->load());
}

float SoulfulSauceProcessor::rawValue (const char* id) const
{
    return apvts.getRawParameterValue (id)->load();
}

sauce::Settings SoulfulSauceProcessor::currentSettings() const
{
    sauce::Settings s;
    {
        const juce::ScopedLock sl (historyLock);
        s.seed = history[(size_t) historyPos].seed;
    }
    s.key = choiceIndex (ParamIDs::key);
    s.style = choiceIndex (ParamIDs::style);
    s.mood = choiceIndex (ParamIDs::mood);
    s.bars = ParamChoices::barsFromIndex (choiceIndex (ParamIDs::bars));
    s.length = ParamChoices::engineLengthFromIndex (choiceIndex (ParamIDs::length));
    s.voicing = choiceIndex (ParamIDs::voicing);
    s.richness = choiceIndex (ParamIDs::richness);
    s.spice = juce::roundToInt (rawValue (ParamIDs::spice) * 100.0f) / 100.0;
    s.octave = choiceIndex (ParamIDs::octave);
    s.bassOn = choiceIndex (ParamIDs::bassMode) != sauce::BassOff;
    return s;
}

sauce::Feel SoulfulSauceProcessor::currentFeel() const
{
    sauce::Feel f;
    f.rhythm = choiceIndex (ParamIDs::rhythm);
    f.humanize = rawValue (ParamIDs::humanize);
    f.velocity = rawValue (ParamIDs::velocity);
    f.strum = rawValue (ParamIDs::strum);
    f.bassMode = choiceIndex (ParamIDs::bassMode);
    return f;
}

void SoulfulSauceProcessor::syncEntryFromParameters()
{
    const juce::ScopedLock sl (historyLock);
    auto& e = history[(size_t) historyPos];
    e.style = choiceIndex (ParamIDs::style);
    e.mood = choiceIndex (ParamIDs::mood);
    e.bars = choiceIndex (ParamIDs::bars);
    e.length = choiceIndex (ParamIDs::length);
    e.richness = choiceIndex (ParamIDs::richness);
    e.spice = rawValue (ParamIDs::spice);
}

void SoulfulSauceProcessor::applyEntryToParameters (const Entry& e)
{
    auto set = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    };
    set (ParamIDs::style, (float) e.style);
    set (ParamIDs::mood, (float) e.mood);
    set (ParamIDs::bars, (float) e.bars);
    set (ParamIDs::length, (float) e.length);
    set (ParamIDs::richness, (float) e.richness);
    set (ParamIDs::spice, e.spice);
}

void SoulfulSauceProcessor::regenerate()
{
    const auto settings = currentSettings();
    std::map<int, sauce::Chord> locks;
    {
        const juce::ScopedLock sl (historyLock);
        locks = history[(size_t) historyPos].locks;
    }
    auto chords = sauce::generate (settings, locks);
    auto pattern = sauce::bakePattern (std::move (chords), settings, currentFeel(), barQuarters.load());

    std::shared_ptr<const sauce::Pattern> old;
    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        old = std::move (sharedPattern);
        sharedPattern = pattern;
    }
    if (old != nullptr)
        retired.push_back (std::move (old));   // freed later on the message thread, never on the audio thread

    messagePattern = pattern;
    ++version;
    sendChangeMessage();
}

void SoulfulSauceProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();   // may be called from any thread
}

void SoulfulSauceProcessor::handleAsyncUpdate()
{
    syncEntryFromParameters();
    regenerate();
}

void SoulfulSauceProcessor::timerCallback()
{
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [] (const auto& p) { return p.use_count() <= 1; }),
                   retired.end());
}

//==============================================================================
int SoulfulSauceProcessor::getHistoryPosition() const
{
    const juce::ScopedLock sl (historyLock);
    return historyPos;
}

int SoulfulSauceProcessor::getHistorySize() const
{
    const juce::ScopedLock sl (historyLock);
    return (int) history.size();
}

void SoulfulSauceProcessor::reroll()
{
    syncEntryFromParameters();
    {
        const juce::ScopedLock sl (historyLock);
        Entry next = history[(size_t) historyPos];
        juce::Random rnd;
        while (next.seed == history[(size_t) historyPos].seed)
            next.seed = 1 + rnd.nextInt (9999);
        history.push_back (next);
        if ((int) history.size() > kMaxHistory)
            history.erase (history.begin());
        historyPos = (int) history.size() - 1;
    }
    regenerate();
}

void SoulfulSauceProcessor::historyBack()
{
    syncEntryFromParameters();
    Entry e;
    {
        const juce::ScopedLock sl (historyLock);
        if (historyPos <= 0)
            return;
        e = history[(size_t) --historyPos];
    }
    applyEntryToParameters (e);
    regenerate();
}

void SoulfulSauceProcessor::historyForward()
{
    syncEntryFromParameters();
    Entry e;
    {
        const juce::ScopedLock sl (historyLock);
        if (historyPos >= (int) history.size() - 1)
            return;
        e = history[(size_t) ++historyPos];
    }
    applyEntryToParameters (e);
    regenerate();
}

void SoulfulSauceProcessor::toggleLock (int chordIndex)
{
    {
        const juce::ScopedLock sl (historyLock);
        auto& locks = history[(size_t) historyPos].locks;
        if (locks.count (chordIndex) > 0)
            locks.erase (chordIndex);
        else if (messagePattern != nullptr && chordIndex >= 0 && chordIndex < (int) messagePattern->chords.size())
            locks[chordIndex] = messagePattern->chords[(size_t) chordIndex].chord;
    }
    regenerate();
}

void SoulfulSauceProcessor::setPreviewPlaying (bool shouldPlay)
{
    if (shouldPlay)
        previewRestart.store (true);
    previewPlaying.store (shouldPlay);
}

//==============================================================================
void SoulfulSauceProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    active.fill (0);
    wasRunning = false;
    auditionNotes.clear();
    auditionSamplesLeft = 0;
}

bool SoulfulSauceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && layouts.getMainInputChannelSet().isDisabled();
}

void SoulfulSauceProcessor::noteOn (juce::MidiBuffer& out, int note, bool bass, float velocity, int sample)
{
    const int idx = (bass ? 128 : 0) + note;
    const int ch = bass ? kBassChannel : kChordChannel;
    if (active[(size_t) idx] != 0)
        out.addEvent (juce::MidiMessage::noteOff (ch, note), sample);
    out.addEvent (juce::MidiMessage::noteOn (ch, note, juce::jlimit (0.01f, 1.0f, velocity)), sample);
    active[(size_t) idx] = 1;
}

void SoulfulSauceProcessor::noteOff (juce::MidiBuffer& out, int note, bool bass, int sample)
{
    const int idx = (bass ? 128 : 0) + note;
    if (active[(size_t) idx] == 0)
        return;
    out.addEvent (juce::MidiMessage::noteOff (bass ? kBassChannel : kChordChannel, note), sample);
    active[(size_t) idx] = 0;
}

void SoulfulSauceProcessor::allNotesOff (juce::MidiBuffer& out, int sample)
{
    for (int idx = 0; idx < 256; ++idx)
        if (active[(size_t) idx] != 0)
            noteOff (out, idx % 128, idx >= 128, sample);
}

void SoulfulSauceProcessor::endAudition (juce::MidiBuffer& out, int sample)
{
    for (const auto& [note, bass] : auditionNotes)
        noteOff (out, note, bass, sample);
    auditionNotes.clear();
    auditionSamplesLeft = 0;
}

void SoulfulSauceProcessor::collectRange (const sauce::Pattern& p, double q0, double q1, double qps, int numSamples)
{
    const double loop = p.loopLength;
    if (loop <= 0.0)
        return;

    auto occurrence = [&] (double t) -> int
    {
        const double k = std::ceil ((q0 - t) / loop);
        const double tt = t + k * loop;
        if (tt >= q1)
            return -1;
        return juce::jlimit (0, numSamples - 1, (int) ((tt - q0) / qps));
    };

    for (const auto& e : p.notes)
    {
        const int off = occurrence (e.end);
        if (off >= 0)
            pending.push_back ({ off, false, e.note, e.bass, 0.0f });
        const int on = occurrence (e.start);
        if (on >= 0)
            pending.push_back ({ on, true, e.note, e.bass, e.velocity });
    }
}

void SoulfulSauceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Pick up a new pattern without ever blocking the audio thread
    bool patternChanged = false;
    {
        const juce::SpinLock::ScopedTryLockType tl (patternLock);
        if (tl.isLocked() && sharedPattern.get() != audioPattern.get())
        {
            patternChanged = audioPattern != nullptr;
            audioPattern = sharedPattern;
        }
    }
    const auto* pattern = audioPattern.get();

    // Transport
    double bpm = 120.0, barQ = 4.0;
    bool hostIsPlaying = false, havePpq = false;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = juce::jlimit (20.0, 400.0, *b);
            hostIsPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition())
            {
                havePpq = true;
                ppq = *q;
            }
            if (auto ts = pos->getTimeSignature())
                if (ts->numerator > 0 && ts->denominator > 0)
                    barQ = 4.0 * ts->numerator / ts->denominator;
        }
    }
    hostBpm.store (bpm);
    hostPlaying.store (hostIsPlaying);
    if (std::abs (barQ - barQuarters.load()) > 1.0e-6)
    {
        barQuarters.store (barQ);
        triggerAsyncUpdate();
    }
    if (hostIsPlaying && previewPlaying.load())
        previewPlaying.store (false);   // the host's transport takes over

    if (previewRestart.exchange (false))
    {
        previewQ = 0.0;
        wasRunning = false;
    }

    const double qps = bpm / 60.0 / getSampleRate();
    bool running = false;
    double q0 = 0.0;
    if (hostIsPlaying && havePpq)
    {
        running = true;
        q0 = ppq;
    }
    else if (previewPlaying.load())
    {
        running = true;
        q0 = previewQ;
    }

    juce::MidiBuffer out;
    if (! running || pattern == nullptr)
    {
        if (wasRunning)
            allNotesOff (out, 0);
        wasRunning = false;
        playingChord.store (-1);
    }
    else
    {
        if (! auditionNotes.empty())
            endAudition (out, 0);

        const double q1 = q0 + numSamples * qps;
        const bool jumped = ! wasRunning || patternChanged || std::abs (q0 - expectedQ) > 0.01;
        pending.clear();
        if (jumped)
        {
            allNotesOff (out, 0);
            // Chase: start any note that should already be sounding at this position
            double q = std::fmod (q0, pattern->loopLength);
            if (q < 0)
                q += pattern->loopLength;
            for (const auto& e : pattern->notes)
                if (e.start < q && e.end > q + 1.0e-6)
                    pending.push_back ({ 0, true, e.note, e.bass, e.velocity });
        }
        collectRange (*pattern, q0, q1, qps, numSamples);
        std::stable_sort (pending.begin(), pending.end(), [] (const PendingEvent& a, const PendingEvent& b)
        {
            return a.sample < b.sample || (a.sample == b.sample && ! a.on && b.on);
        });
        for (const auto& ev : pending)
        {
            if (ev.on)
                noteOn (out, ev.note, ev.bass, ev.velocity, ev.sample);
            else
                noteOff (out, ev.note, ev.bass, ev.sample);
        }

        expectedQ = q1;
        wasRunning = true;
        if (! hostIsPlaying)
            previewQ = q1;
        playingChord.store (pattern->chordIndexAt (q0));
    }

    // Click-to-audition a chord while the transport is stopped
    const int request = auditionRequest.exchange (-1);
    if (request >= 0 && ! running && pattern != nullptr && request < (int) pattern->chords.size())
    {
        endAudition (out, 0);
        const auto& c = pattern->chords[(size_t) request];
        const float v = velocityParam->load();
        if (c.bassNote >= 0)
        {
            noteOn (out, c.bassNote, true, v * 0.95f, 0);
            auditionNotes.push_back ({ c.bassNote, true });
        }
        for (int n : c.upper)
        {
            noteOn (out, n, false, v, 0);
            auditionNotes.push_back ({ n, false });
        }
        auditionSamplesLeft = (int) (getSampleRate() * 60.0 / bpm * 2.0);
    }
    else if (auditionSamplesLeft > 0)
    {
        if (auditionSamplesLeft <= numSamples)
            endAudition (out, juce::jmax (0, auditionSamplesLeft - 1));
        else
            auditionSamplesLeft -= numSamples;
    }

    // Built-in sound
    const int sound = juce::roundToInt (soundParam->load());
    if (sound == ParamChoices::soundOff)
    {
        if (! soundWasOff)
            synth.allNotesOff (0, false);
        soundWasOff = true;
    }
    else
    {
        soundWasOff = false;
        soundType.store (sound);
        synth.renderNextBlock (buffer, out, 0, numSamples);

        const float gain = 0.4f * volumeParam->load();
        buffer.applyGain (gain);
        if (buffer.getNumChannels() >= 2)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
        else
            reverb.processMono (buffer.getWritePointer (0), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = std::tanh (d[i]);
        }
    }

    // MIDI out (to drive another instrument). Incoming MIDI is not used yet.
    midi.clear();
    if (midiOutParam->load() > 0.5f)
    {
        for (const auto meta : out)
        {
            auto msg = meta.getMessage();
            if (msg.getChannel() == kBassChannel)
                msg.setChannel (kChordChannel);
            midi.addEvent (msg, meta.samplePosition);
        }
    }
}

//==============================================================================
juce::File SoulfulSauceProcessor::writeMidiFile() const
{
    const auto pattern = messagePattern;
    if (pattern == nullptr || pattern->chords.empty())
        return {};

    constexpr int ticksPerQuarter = 960;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Soulful Sauce"), 0.0);
    for (const auto& e : pattern->notes)
    {
        seq.addEvent (juce::MidiMessage::noteOn (1, e.note, e.velocity).withTimeStamp (e.start * ticksPerQuarter));
        seq.addEvent (juce::MidiMessage::noteOff (1, e.note).withTimeStamp (e.end * ticksPerQuarter));
    }
    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);
    file.addTrack (seq);

    const int key = choiceIndex (ParamIDs::key);
    const int style = choiceIndex (ParamIDs::style);
    const bool minor = style == sauce::DeepHouse || style == sauce::AfroSoul;
    juce::StringArray names;
    for (size_t i = 0; i < std::min<size_t> (4, pattern->chords.size()); ++i)
        names.add (juce::String (pattern->chords[i].name).replace ("/", " over "));
    const auto name = "Soulful Sauce - " + juce::String (sauce::kNoteNames[key]) + (minor ? " minor - " : " major - ")
                    + names.joinIntoString (" ");

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Soulful Sauce");
    dir.createDirectory();
    auto f = dir.getChildFile (juce::File::createLegalFileName (name) + ".mid");
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (! os.openedOk() || ! file.writeTo (os))
        return {};
    os.flush();
    return f;
}

//==============================================================================
void SoulfulSauceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree h ("History");
    {
        const juce::ScopedLock sl (historyLock);
        h.setProperty ("pos", historyPos, nullptr);
        for (const auto& e : history)
        {
            juce::ValueTree t ("Entry");
            t.setProperty ("seed", e.seed, nullptr);
            t.setProperty ("style", e.style, nullptr);
            t.setProperty ("mood", e.mood, nullptr);
            t.setProperty ("bars", e.bars, nullptr);
            t.setProperty ("length", e.length, nullptr);
            t.setProperty ("richness", e.richness, nullptr);
            t.setProperty ("spice", e.spice, nullptr);
            t.setProperty ("locks", locksToString (e.locks), nullptr);
            h.appendChild (t, nullptr);
        }
    }
    state.removeChild (state.getChildWithName ("History"), nullptr);
    state.appendChild (h, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SoulfulSauceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType()))
        return;

    auto h = tree.getChildWithName ("History");
    if (h.isValid())
    {
        const juce::ScopedLock sl (historyLock);
        history.clear();
        for (const auto& t : h)
        {
            Entry e;
            e.seed = juce::jlimit (1, 999999, (int) t.getProperty ("seed", 1));
            e.style = (int) t.getProperty ("style", 0);
            e.mood = (int) t.getProperty ("mood", 0);
            e.bars = (int) t.getProperty ("bars", 1);
            e.length = (int) t.getProperty ("length", 1);
            e.richness = (int) t.getProperty ("richness", 2);
            e.spice = (float) t.getProperty ("spice", 0.3f);
            e.locks = locksFromString (t.getProperty ("locks", "").toString());
            history.push_back (e);
        }
        if (history.empty())
            history.push_back ({});
        historyPos = juce::jlimit (0, (int) history.size() - 1, (int) h.getProperty ("pos", 0));
        tree.removeChild (h, nullptr);
    }

    apvts.replaceState (tree);
    triggerAsyncUpdate();
}

juce::AudioProcessorEditor* SoulfulSauceProcessor::createEditor()
{
    return new SoulfulSauceEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoulfulSauceProcessor();
}
