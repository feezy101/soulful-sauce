#pragma once

#include "Widgets.h"
#include "../engine/ChordEngine.h"

namespace ui
{

struct CardData
{
    juce::String name, roman, bar;
    bool locked = false;
};

class ChordCard : public juce::Component
{
public:
    explicit ChordCard (const AccentSource& a) : accent (a) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void setData (const CardData& d) { data = d; repaint(); }
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void setPlaying (bool p) { if (p != playing) { playing = p; repaint(); } }

    std::function<void()> onSelect, onToggleLock;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> lockArea() const { return { (float) getWidth() - 40.0f, 4.0f, 36.0f, 36.0f }; }

    const AccentSource& accent;
    CardData data;
    bool selected = false, playing = false;
};

/** The row of chord cards. Scrolls sideways when there are more than eight. */
class ChordStrip : public juce::Component
{
public:
    explicit ChordStrip (const AccentSource&);

    void setChords (const std::vector<CardData>&);
    void setSelected (int index);
    void setPlaying (int index);

    std::function<void (int)> onSelect, onToggleLock;

    void resized() override;

private:
    void layoutCards();

    const AccentSource& accent;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<ChordCard>> cards;
    int selected = 0, playing = -1;
};

class KeyboardView : public juce::Component
{
public:
    explicit KeyboardView (const AccentSource& a) : accent (a) {}
    void setNotes (std::vector<int> upper, int bass);
    void paint (juce::Graphics&) override;

    static constexpr int lowest = 24, highest = 107;   // C1 .. B7
    static constexpr int keyWidth = 12, keyStep = 13, whiteHeight = 76, blackHeight = 47;

private:
    const AccentSource& accent;
    std::vector<int> upper;
    int bass = -1;
};

} // namespace ui
