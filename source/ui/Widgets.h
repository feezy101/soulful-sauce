#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SauceLookAndFeel.h"

namespace ui
{

/** Gives widgets the current mood colour. */
struct AccentSource
{
    virtual ~AccentSource() = default;
    virtual juce::Colour getAccent() const = 0;
    virtual juce::Colour getDeep() const = 0;
};

//==============================================================================
/** Caption-over-value dropdown that opens a styled menu. */
class Dropdown : public juce::Component
{
public:
    Dropdown (const AccentSource&, juce::String caption, juce::StringArray items);

    void setIndex (int index);
    int getIndex() const { return index; }
    void setItems (juce::StringArray newItems);
    void setDotColours (juce::Array<juce::Colour> colours) { dots = std::move (colours); repaint(); }
    void setNumColumns (int columns) { numColumns = columns; }
    void setOpensUpward (bool up) { upward = up; }

    std::function<void (int)> onChange;
    std::function<juce::String (int)> valueText;   // optional custom display text

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    const AccentSource& accent;
    juce::String caption;
    juce::StringArray items;
    juce::Array<juce::Colour> dots;
    int index = 0, numColumns = 1;
    bool upward = false, open = false;
};

//==============================================================================
/** A row of mutually exclusive options. */
class Segmented : public juce::Component
{
public:
    Segmented (const AccentSource&, juce::StringArray items, bool monospace = false);

    void setIndex (int index);
    int getIndex() const { return index; }
    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> itemBounds (int i) const;

    const AccentSource& accent;
    juce::StringArray items;
    int index = 0;
    bool mono = false;
};

//==============================================================================
/** Binds a Dropdown or Segmented control to a choice/int parameter (index = value - offset). */
class IndexAttachment
{
public:
    template <typename Control>
    IndexAttachment (juce::RangedAudioParameter& param, Control& control, int offset = 0)
        : attachment (param, [&control, offset] (float v) { control.setIndex (juce::roundToInt (v) - offset); })
    {
        control.onChange = [this, offset] (int i) { attachment.setValueAsCompleteGesture ((float) (i + offset)); };
        attachment.sendInitialUpdate();
    }

private:
    juce::ParameterAttachment attachment;
};

//==============================================================================
class IconButton : public juce::Button
{
public:
    enum class Icon { ChevronLeft, ChevronRight, Settings };

    IconButton (const juce::String& name, Icon icon);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
};

class RerollButton : public juce::Button
{
public:
    explicit RerollButton (const AccentSource& a) : juce::Button ("Re-roll"), accent (a) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    const AccentSource& accent;
};

class PlayButton : public juce::Button
{
public:
    explicit PlayButton (const AccentSource& a) : juce::Button ("Play"), accent (a) {}
    void setPlaying (bool p) { if (p != playing) { playing = p; repaint(); } }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    const AccentSource& accent;
    bool playing = false;
};

//==============================================================================
class HistoryBox : public juce::Component
{
public:
    explicit HistoryBox (const AccentSource& a) : accent (a) {}
    void setPosition (int pos, int size);
    void paint (juce::Graphics&) override;

private:
    const AccentSource& accent;
    int position = 0, total = 1;
};

//==============================================================================
/** A rotary knob with its label and value underneath. */
class Knob : public juce::Component
{
public:
    Knob (juce::RangedAudioParameter& param, const juce::String& label);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    juce::SliderParameterAttachment attachment;
    juce::String label;
};

//==============================================================================
/** Drag this into FL Studio's piano roll or playlist; click to save the MIDI file. */
class DragMidiButton : public juce::Component
{
public:
    explicit DragMidiButton (const AccentSource& a) : accent (a) {}

    std::function<juce::File()> createFile;

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

private:
    const AccentSource& accent;
    bool dragging = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace ui
