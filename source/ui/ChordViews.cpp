#include "ChordViews.h"

namespace ui
{

void ChordCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto a = accent.getAccent();
    g.setColour (selected ? colours::cardSelected : colours::card);
    g.fillRoundedRectangle (r.reduced (0.5f), 12.0f);
    if (selected)
    {
        g.setColour (a);
        g.drawRoundedRectangle (r.reduced (1.0f), 12.0f, 2.0f);
    }
    else
    {
        g.setColour (colours::controlBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    }

    if (playing)
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r.reduced (1.0f), 11.0f);
        g.reduceClipRegion (clip);
        g.setColour (a);
        g.fillRect (r.withTop (r.getBottom() - 4.0f));
    }

    auto content = r.reduced (13.0f, 12.0f);
    g.setColour (colours::label);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (data.bar, content.removeFromTop (14.0f), juce::Justification::centredLeft, false);

    auto romanArea = content.removeFromBottom (15.0f);
    g.setColour (colours::textMuted);
    g.setFont (Fonts::sans (12.0f, 700));
    g.drawText (data.roman, romanArea, juce::Justification::centredLeft, false);

    content.removeFromBottom (6.0f);
    g.setColour (colours::text);
    auto font = Fonts::sans (22.0f, 800, -0.01f);
    // shrink long names so they always fit the card
    const float available = content.getWidth();
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, data.name, 0, 0);
    const float w = ga.getBoundingBox (0, -1, true).getWidth();
    if (w > available && w > 0)
        font = Fonts::sans (juce::jmax (13.0f, 22.0f * available / w), 800);
    g.setFont (font);
    g.drawText (data.name, content.removeFromBottom (24.0f), juce::Justification::bottomLeft, false);

    // lock
    const auto lr = lockArea();
    const auto lc = lr.getCentre();
    g.setColour (data.locked ? a : colours::label);
    const juce::Rectangle<float> body (lc.x - 7.0f, lc.y - 1.5f, 14.0f, 10.0f);
    g.drawRoundedRectangle (body, 2.0f, 2.0f);
    juce::Path shackle;
    if (data.locked)
    {
        shackle.startNewSubPath (lc.x - 4.0f, body.getY());
        shackle.lineTo (lc.x - 4.0f, body.getY() - 3.0f);
        shackle.addCentredArc (lc.x, body.getY() - 3.0f, 4.0f, 4.0f, 0.0f, -juce::MathConstants<float>::halfPi,
                               juce::MathConstants<float>::halfPi, false);
        shackle.lineTo (lc.x + 4.0f, body.getY());
    }
    else
    {
        shackle.startNewSubPath (lc.x - 4.0f, body.getY());
        shackle.lineTo (lc.x - 4.0f, body.getY() - 3.0f);
        shackle.addCentredArc (lc.x, body.getY() - 3.0f, 4.0f, 4.0f, 0.0f, -juce::MathConstants<float>::halfPi,
                               juce::MathConstants<float>::halfPi * 0.55f, false);
    }
    g.strokePath (shackle, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ChordCard::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    if (lockArea().contains (e.position))
    {
        if (onToggleLock)
            onToggleLock();
    }
    else if (onSelect)
    {
        onSelect();
    }
}

//==============================================================================
ChordStrip::ChordStrip (const AccentSource& a) : accent (a)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true, false, true);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
}

void ChordStrip::setChords (const std::vector<CardData>& data)
{
    while (cards.size() > data.size())
        cards.pop_back();
    while (cards.size() < data.size())
    {
        const int i = (int) cards.size();
        auto card = std::make_unique<ChordCard> (accent);
        card->onSelect = [this, i] { if (onSelect) onSelect (i); };
        card->onToggleLock = [this, i] { if (onToggleLock) onToggleLock (i); };
        content.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }
    for (size_t i = 0; i < data.size(); ++i)
    {
        cards[i]->setData (data[i]);
        cards[i]->setSelected ((int) i == selected);
        cards[i]->setPlaying ((int) i == playing);
    }
    layoutCards();
}

void ChordStrip::setSelected (int index)
{
    selected = index;
    for (size_t i = 0; i < cards.size(); ++i)
        cards[i]->setSelected ((int) i == index);
}

void ChordStrip::setPlaying (int index)
{
    if (index == playing)
        return;
    playing = index;
    for (size_t i = 0; i < cards.size(); ++i)
        cards[i]->setPlaying ((int) i == index);

    // keep the playing chord in view when the strip scrolls
    if (index >= 0 && index < (int) cards.size())
    {
        const auto b = cards[(size_t) index]->getBounds();
        const int x = viewport.getViewPositionX();
        if (b.getX() < x || b.getRight() > x + viewport.getViewWidth())
            viewport.setViewPosition (juce::jmax (0, b.getX() - 10), 0);
    }
}

void ChordStrip::resized()
{
    viewport.setBounds (getLocalBounds());
    layoutCards();
}

void ChordStrip::layoutCards()
{
    const int n = (int) cards.size();
    if (n == 0)
        return;
    const int gap = 10, height = 112;
    const int width = getWidth();
    const bool scroll = n > 8;
    const int cardW = scroll ? 124 : (width - gap * (n - 1)) / n;
    const int contentW = scroll ? n * cardW + (n - 1) * gap : width;
    content.setSize (contentW, height);
    for (int i = 0; i < n; ++i)
        cards[(size_t) i]->setBounds (i * (cardW + gap), 0, cardW, height);
}

//==============================================================================
void KeyboardView::setNotes (std::vector<int> u, int b)
{
    if (u != upper || b != bass)
    {
        upper = std::move (u);
        bass = b;
        repaint();
    }
}

void KeyboardView::paint (juce::Graphics& g)
{
    static const bool isWhite[12] = { true, false, true, false, true, true, false, true, false, true, false, true };
    const auto a = accent.getAccent(), deep = accent.getDeep();
    auto contains = [this] (int n) { return std::find (upper.begin(), upper.end(), n) != upper.end(); };

    int wi = 0;
    struct Black { float x; juce::Colour c; };
    std::vector<Black> blacks;
    for (int n = lowest; n <= highest; ++n)
    {
        const auto colour = n == bass ? deep : contains (n) ? a : juce::Colour();
        if (isWhite[n % 12])
        {
            const float x = (float) (wi * keyStep);
            g.setColour (colour.isTransparent() ? colours::whiteKey : colour);
            g.fillRoundedRectangle ({ x, -4.0f, (float) keyWidth, (float) whiteHeight + 4.0f }, 3.0f);
            if (n % 12 == 0)
            {
                g.setColour (colours::label);
                g.setFont (Fonts::mono (9.0f));
                g.drawText ("C" + juce::String (n / 12 - 1), juce::Rectangle<float> (x + 1.0f, (float) whiteHeight + 4.0f, 20.0f, 10.0f),
                            juce::Justification::centredLeft, false);
            }
            ++wi;
        }
        else
        {
            blacks.push_back ({ (float) (wi * keyStep) - 4.5f, colour.isTransparent() ? colours::blackKey : colour });
        }
    }
    for (const auto& b : blacks)
    {
        const juce::Rectangle<float> r (b.x, -3.0f, 8.0f, (float) blackHeight + 3.0f);
        g.setColour (b.c);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colour (0xff0c0a08));
        g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
    }
}

} // namespace ui
