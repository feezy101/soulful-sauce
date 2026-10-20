#include "Widgets.h"

namespace ui
{

namespace
{
    void drawChevron (juce::Graphics& g, juce::Rectangle<float> r, bool down, bool up, juce::Colour c, float thickness)
    {
        juce::Path p;
        const auto cx = r.getCentreX(), cy = r.getCentreY();
        const float w = r.getWidth() * 0.5f, h = r.getHeight() * 0.25f;
        if (down)
        {
            p.startNewSubPath (cx - w, cy - h);
            p.lineTo (cx, cy + h);
            p.lineTo (cx + w, cy - h);
        }
        else if (up)
        {
            p.startNewSubPath (cx - w, cy + h);
            p.lineTo (cx, cy - h);
            p.lineTo (cx + w, cy + h);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
Dropdown::Dropdown (const AccentSource& a, juce::String c, juce::StringArray i)
    : accent (a), caption (std::move (c)), items (std::move (i))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (caption);
}

void Dropdown::setIndex (int i)
{
    i = juce::jlimit (0, juce::jmax (0, items.size() - 1), i);
    if (i != index)
    {
        index = i;
        repaint();
    }
}

void Dropdown::setItems (juce::StringArray newItems)
{
    items = std::move (newItems);
    repaint();
}

void Dropdown::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (open ? accent.getAccent() : isMouseOver() ? colours::menuBorder : colours::controlBorder);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    auto inner = r.reduced (12.0f, 0.0f);
    inner.removeFromRight (16.0f);
    const float captionH = 10.0f, valueH = 16.0f, gap = 4.0f;
    const float top = r.getCentreY() - (captionH + gap + valueH) * 0.5f;
    drawCaption (g, caption, { inner.getX(), top, inner.getWidth() + 16.0f, captionH });

    auto valueArea = juce::Rectangle<float> (inner.getX(), top + captionH + gap, inner.getWidth(), valueH);
    if (index < dots.size())
    {
        g.setColour (dots[index]);
        g.fillEllipse (valueArea.getX(), valueArea.getCentreY() - 4.0f, 8.0f, 8.0f);
        valueArea.removeFromLeft (15.0f);
    }
    g.setColour (colours::text);
    g.setFont (Fonts::sans (14.0f, 700));
    g.drawText (valueText ? valueText (index) : items[index], valueArea, juce::Justification::centredLeft, true);

    drawChevron (g, { r.getRight() - 22.0f, r.getCentreY() - 3.5f, 9.0f, 7.0f }, ! upward, upward, colours::textMuted, 2.0f);
}

void Dropdown::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;

    juce::PopupMenu menu;
    for (int i = 0; i < items.size(); ++i)
    {
        juce::PopupMenu::Item item (items[i]);
        item.itemID = i + 1;
        item.isTicked = i == index;
        if (i < dots.size())
            item.colour = dots[i];
        menu.addItem (item);
    }

    open = true;
    repaint();
    auto options = juce::PopupMenu::Options()
                       .withTargetComponent (this)
                       .withMinimumWidth (getWidth())
                       .withItemThatMustBeVisible (index + 1)
                       .withPreferredPopupDirection (upward ? juce::PopupMenu::Options::PopupDirection::upwards
                                                            : juce::PopupMenu::Options::PopupDirection::downwards);
    if (numColumns > 1)
        options = options.withMinimumNumColumns (numColumns).withMaximumNumColumns (numColumns);

    juce::Component::SafePointer<Dropdown> safe (this);
    menu.showMenuAsync (options, [safe] (int result)
    {
        if (safe == nullptr)
            return;
        safe->open = false;
        safe->repaint();
        if (result > 0)
        {
            safe->setIndex (result - 1);
            if (safe->onChange)
                safe->onChange (result - 1);
        }
    });
}

//==============================================================================
Segmented::Segmented (const AccentSource& a, juce::StringArray i, bool monospace)
    : accent (a), items (std::move (i)), mono (monospace)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Segmented::setIndex (int i)
{
    i = juce::jlimit (0, items.size() - 1, i);
    if (i != index)
    {
        index = i;
        repaint();
    }
}

juce::Rectangle<float> Segmented::itemBounds (int i) const
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float w = r.getWidth() / (float) items.size();
    return { r.getX() + w * (float) i, r.getY(), w, r.getHeight() };
}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (colours::controlBorder);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    for (int i = 0; i < items.size(); ++i)
    {
        auto b = itemBounds (i).reduced (1.0f, 0.0f);
        const bool selected = i == index;
        if (selected)
        {
            g.setColour (accent.getAccent());
            g.fillRoundedRectangle (b, 7.0f);
        }
        g.setColour (selected ? colours::onAccent : colours::textSoft);
        g.setFont (mono ? Fonts::mono (12.0f, true) : Fonts::sans (12.0f, 700));
        g.drawText (items[i], b, juce::Justification::centred, false);
    }
}

void Segmented::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < items.size(); ++i)
    {
        if (itemBounds (i).contains (e.position))
        {
            if (i != index)
            {
                setIndex (i);
                if (onChange)
                    onChange (i);
            }
            return;
        }
    }
}

//==============================================================================
IconButton::IconButton (const juce::String& name, Icon i) : juce::Button (name), icon (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float alpha = isEnabled() ? 1.0f : 0.4f;
    g.setColour (colours::panel.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour ((highlighted && isEnabled() ? colours::menuBorder : colours::controlBorder).withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    const auto c = (down ? colours::text : colours::textSoft).withMultipliedAlpha (alpha);
    const auto centre = r.getCentre();
    juce::Path p;
    if (icon == Icon::ChevronLeft || icon == Icon::ChevronRight)
    {
        const float s = icon == Icon::ChevronLeft ? 1.0f : -1.0f;
        p.startNewSubPath (centre.x + 3.0f * s, centre.y - 6.0f);
        p.lineTo (centre.x - 3.0f * s, centre.y);
        p.lineTo (centre.x + 3.0f * s, centre.y + 6.0f);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (c);
        g.drawLine (centre.x - 8.0f, centre.y - 4.0f, centre.x + 8.0f, centre.y - 4.0f, 1.8f);
        g.drawLine (centre.x - 8.0f, centre.y + 4.0f, centre.x + 8.0f, centre.y + 4.0f, 1.8f);
        for (auto [dx, dy] : { std::pair<float, float> { -3.0f, -4.0f }, { 3.0f, 4.0f } })
        {
            g.setColour (colours::panel);
            g.fillEllipse (centre.x + dx - 2.6f, centre.y + dy - 2.6f, 5.2f, 5.2f);
            g.setColour (c);
            g.drawEllipse (centre.x + dx - 2.6f, centre.y + dy - 2.6f, 5.2f, 5.2f, 1.8f);
        }
    }
}

//==============================================================================
void RerollButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    auto a = accent.getAccent();
    if (down)
        a = a.darker (0.15f);
    else if (highlighted)
        a = a.brighter (0.08f);
    g.setColour (a);
    g.fillRoundedRectangle (r, 10.0f);

    // dice
    const juce::Rectangle<float> die (r.getX() + 16.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f);
    g.setColour (colours::onAccent);
    g.drawRoundedRectangle (die, 3.0f, 1.8f);
    for (auto [dx, dy] : { std::pair<float, float> { 5.0f, 5.0f }, { 11.0f, 5.0f }, { 5.0f, 11.0f }, { 11.0f, 11.0f } })
        g.fillEllipse (die.getX() + dx - 1.3f, die.getY() + dy - 1.3f, 2.6f, 2.6f);

    g.setFont (Fonts::sans (14.0f, 800));
    g.drawText ("Re-roll", r.withTrimmedLeft (41.0f), juce::Justification::centredLeft, false);
}

void PlayButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    auto a = accent.getAccent();
    if (down)
        a = a.darker (0.15f);
    else if (highlighted)
        a = a.brighter (0.08f);
    g.setColour (a);
    g.fillEllipse (r);
    g.setColour (colours::onAccent);
    const auto c = r.getCentre();
    if (playing)
    {
        g.fillRoundedRectangle (c.x - 6.0f, c.y - 6.0f, 12.0f, 12.0f, 2.0f);
    }
    else
    {
        juce::Path tri;
        tri.addTriangle (c.x - 4.5f, c.y - 7.0f, c.x + 7.5f, c.y, c.x - 4.5f, c.y + 7.0f);
        g.fillPath (tri);
    }
}

//==============================================================================
void HistoryBox::setPosition (int pos, int size)
{
    if (pos != position || size != total)
    {
        position = pos;
        total = size;
        repaint();
    }
}

void HistoryBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::inset);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (colours::controlBorder);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);
    drawCaption (g, "HISTORY", { r.getX(), r.getCentreY() - 15.0f, r.getWidth(), 10.0f }, juce::Justification::centred);
    g.setColour (accent.getAccent());
    g.setFont (Fonts::mono (14.0f, true));
    g.drawText (juce::String (position + 1) + " / " + juce::String (total),
                juce::Rectangle<float> (r.getX(), r.getCentreY() - 1.0f, r.getWidth(), 16.0f), juce::Justification::centred, false);
}

//==============================================================================
Knob::Knob (juce::RangedAudioParameter& param, const juce::String& l)
    : attachment (param, slider), label (l)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (180);
    slider.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
    slider.setTitle (label);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withSizeKeepingCentre (60, 60).withY (0));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::textSoft);
    g.setFont (Fonts::sans (11.5f, 700));
    g.drawText (label, juce::Rectangle<int> (0, 66, getWidth(), 14), juce::Justification::centred, false);
    g.setColour (colours::label);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), juce::Rectangle<int> (0, 84, getWidth(), 14),
                juce::Justification::centred, false);
}

//==============================================================================
void DragMidiButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto a = accent.getAccent();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 11.0f);

    juce::Path outline;
    outline.addRoundedRectangle (r, 11.0f);
    juce::Path dashed;
    const float dashes[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (a);
    g.fillPath (dashed);

    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (r.getX() + 15.0f + col * 6.0f, r.getCentreY() - 7.8f + row * 6.0f, 3.6f, 3.6f);

    g.setColour (colours::text);
    g.setFont (Fonts::sans (14.0f, 800));
    g.drawText ("Drag MIDI", r.withTrimmedLeft (40.0f), juce::Justification::centredLeft, false);
}

void DragMidiButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5 || ! createFile)
        return;
    dragging = true;
    const auto f = createFile();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

void DragMidiButton::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDragging = dragging;
    dragging = false;
    if (wasDragging || ! getLocalBounds().contains (e.getPosition()) || ! createFile)
        return;

    // Click: save the MIDI file somewhere instead
    const auto f = createFile();
    if (! f.existsAsFile())
        return;
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI", juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                                    .getChildFile (f.getFileName()), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [f] (const juce::FileChooser& fc)
                          {
                              const auto target = fc.getResult();
                              if (target != juce::File())
                                  f.copyFileTo (target.withFileExtension ("mid"));
                          });
}

} // namespace ui
