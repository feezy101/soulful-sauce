#include "SauceLookAndFeel.h"
#include "BinaryData.h"

namespace ui
{

juce::Colour moodAccent (int mood)
{
    static const juce::Colour c[] = { juce::Colour (0xffe8a13a), juce::Colour (0xff6cb8b2), juce::Colour (0xffff8a5c) };
    return c[juce::jlimit (0, 2, mood)];
}

juce::Colour moodDeep (int mood)
{
    static const juce::Colour c[] = { juce::Colour (0xff8a5a17), juce::Colour (0xff2f6e6a), juce::Colour (0xffa8482a) };
    return c[juce::jlimit (0, 2, mood)];
}

namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    struct Typefaces
    {
        juce::Typeface::Ptr semi  = load (BinaryData::ManropeSemiBold_ttf, BinaryData::ManropeSemiBold_ttfSize);
        juce::Typeface::Ptr bold  = load (BinaryData::ManropeBold_ttf, BinaryData::ManropeBold_ttfSize);
        juce::Typeface::Ptr extra = load (BinaryData::ManropeExtraBold_ttf, BinaryData::ManropeExtraBold_ttfSize);
        juce::Typeface::Ptr monoMedium = load (BinaryData::JetBrainsMonoMedium_ttf, BinaryData::JetBrainsMonoMedium_ttfSize);
        juce::Typeface::Ptr monoBold   = load (BinaryData::JetBrainsMonoBold_ttf, BinaryData::JetBrainsMonoBold_ttfSize);
    };

    const Typefaces& typefaces()
    {
        static Typefaces t;
        return t;
    }
}

juce::Font Fonts::sans (float size, int weight, float tracking)
{
    const auto& t = typefaces();
    auto tf = weight >= 800 ? t.extra : weight >= 700 ? t.bold : t.semi;
    return juce::Font (juce::FontOptions (tf).withPointHeight (size).withKerningFactor (tracking));
}

juce::Font Fonts::mono (float size, bool bold)
{
    const auto& t = typefaces();
    return juce::Font (juce::FontOptions (bold ? t.monoBold : t.monoMedium).withPointHeight (size));
}

void drawCaption (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                  juce::Justification just, float size)
{
    g.setColour (colours::label);
    g.setFont (Fonts::sans (size, 800, 0.1f));
    g.drawText (text, area, just, false);
}

//==============================================================================
SauceLookAndFeel::SauceLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colours::menu);
    setColour (juce::PopupMenu::textColourId, colours::optionText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::menuHighlight);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::ScrollBar::thumbColourId, colours::menuBorder);
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::thumbColourId, colours::text);
    setDefaultSansSerifTypeface (Fonts::sans (14.0f, 700).getTypefacePtr());
}

void SauceLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = size * 0.5f * (22.0f / 28.0f);
    const float stroke = size * (4.0f / 56.0f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::controlBorder);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = startAngle + pos * (endAngle - startAngle);
    if (pos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float faceR = size * 0.5f * (15.0f / 28.0f);
    g.setColour (colours::knobFace);
    g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2, faceR * 2);
    g.setColour (colours::controlBorder);
    g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2, faceR * 2, 1.0f);

    const float pointerLen = size * 0.5f * (11.0f / 28.0f);
    const juce::Point<float> tip (centre.x + pointerLen * std::sin (angle), centre.y - pointerLen * std::cos (angle));
    g.setColour (colours::text);
    g.drawLine ({ centre, tip }, size * (2.4f / 56.0f));
}

void SauceLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<int> (width, height).toFloat();
    g.fillAll (colours::menu);
    g.setColour (colours::menuBorder);
    g.drawRect (r, 1.0f);
}

void SauceLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                          bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                          const juce::String& text, const juce::String&, const juce::Drawable*,
                                          const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colours::controlBorder);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.toFloat().reduced (2.0f, 1.0f);
    if (isTicked)
    {
        g.setColour (colours::menuSelected);
        g.fillRoundedRectangle (r, 7.0f);
    }
    else if (isHighlighted && isActive)
    {
        g.setColour (colours::menuHighlight);
        g.fillRoundedRectangle (r, 7.0f);
    }

    auto content = r.reduced (10.0f, 0.0f);
    if (textColour != nullptr)
    {
        // Coloured items (moods) show a dot in their colour
        g.setColour (*textColour);
        g.fillEllipse (content.getX(), content.getCentreY() - 4.0f, 8.0f, 8.0f);
        content.removeFromLeft (16.0f);
    }

    g.setColour (! isActive ? colours::label : isTicked ? accent : colours::optionText);
    g.setFont (getPopupMenuFont());
    g.drawText (text, content, juce::Justification::centredLeft, true);

    if (hasSubMenu)
    {
        juce::Path arrow;
        const float cx = r.getRight() - 12.0f, cy = r.getCentreY();
        arrow.startNewSubPath (cx - 2.0f, cy - 4.0f);
        arrow.lineTo (cx + 2.0f, cy);
        arrow.lineTo (cx - 2.0f, cy + 4.0f);
        g.setColour (colours::textMuted);
        g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void SauceLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int,
                                                  int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 50;
        idealHeight = 9;
        return;
    }
    juce::GlyphArrangement ga;
    ga.addLineOfText (getPopupMenuFont(), text, 0.0f, 0.0f);
    idealWidth = (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + 48;
    idealHeight = 34;
}

juce::Font SauceLookAndFeel::getPopupMenuFont()
{
    return Fonts::sans (13.0f, 700);
}

void SauceLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                      bool isVertical, int thumbStart, int thumbSize, bool, bool)
{
    auto thumb = isVertical ? juce::Rectangle<int> (x, thumbStart, width, thumbSize)
                            : juce::Rectangle<int> (thumbStart, y, thumbSize, height);
    g.setColour (colours::menuBorder);
    g.fillRoundedRectangle (thumb.toFloat().reduced (1.0f), 3.0f);
}

void SauceLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) height * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) width, 4.0f);
    g.setColour (colours::controlBorder);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);
    g.setColour (colours::text);
    g.fillEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
}

} // namespace ui
