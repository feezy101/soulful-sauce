#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{

namespace colours
{
    const juce::Colour background    { 0xff15120f };
    const juce::Colour panel         { 0xff1e1a16 };
    const juce::Colour panelBorder   { 0xff2e2721 };
    const juce::Colour controlBorder { 0xff3a322a };
    const juce::Colour card          { 0xff1f1a16 };
    const juce::Colour cardSelected  { 0xff2b241d };
    const juce::Colour inset         { 0xff100e0c };
    const juce::Colour menu          { 0xff241f1a };
    const juce::Colour menuBorder    { 0xff4a4036 };
    const juce::Colour menuHighlight { 0xff332b24 };
    const juce::Colour menuSelected  { 0xff3a3027 };
    const juce::Colour text          { 0xfff1e7d8 };
    const juce::Colour textSoft      { 0xffd9ccba };
    const juce::Colour textMuted     { 0xffb3a594 };
    const juce::Colour label         { 0xff9a8b79 };
    const juce::Colour optionText    { 0xffe7dccb };
    const juce::Colour whiteKey      { 0xffede3d3 };
    const juce::Colour blackKey      { 0xff15120f };
    const juce::Colour knobFace      { 0xff2a241e };
    const juce::Colour onAccent      { 0xff15120f };
    const juce::Colour logoRing      { 0xff5e5245 };
}

juce::Colour moodAccent (int mood);
juce::Colour moodDeep (int mood);

/** Fonts embedded in the plugin (Manrope for UI text, JetBrains Mono for numbers). */
struct Fonts
{
    static juce::Font sans (float size, int weight = 700, float tracking = 0.0f);
    static juce::Font mono (float size, bool bold = false);
};

/** Draws a caption in small tracked capitals, as used above every control. */
void drawCaption (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                  juce::Justification = juce::Justification::centredLeft, float size = 9.5f);

class SauceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SauceLookAndFeel();

    juce::Colour accent = moodAccent (0);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;
    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override { return 4; }

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 6; }

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
};

} // namespace ui
