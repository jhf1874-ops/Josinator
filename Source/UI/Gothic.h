#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

// Gothic rhythm-cathedral palette and fonts (all three fonts are SIL Open Font Licence, embedded).
namespace gothic
{
const juce::Colour ground   { 0xff09070b };
const juce::Colour panel    { 0xff15111a };
const juce::Colour plate    { 0xff120e17 };
const juce::Colour edge     { 0xff3a2f45 };
const juce::Colour keyTop   { 0xff2a2330 };
const juce::Colour keyEdge  { 0xff4a3e56 };
const juce::Colour shadow   { 0xff050406 };
const juce::Colour stepOff  { 0xff241d2c };
const juce::Colour stepBeat { 0xff2e2636 };
const juce::Colour text     { 0xffebe5f2 };
const juce::Colour muted    { 0xffa99fb6 };
const juce::Colour gold     { 0xffc9a75a };
const juce::Colour blood    { 0xffd2382c };
const juce::Colour amber    { 0xffffb347 };
const juce::Colour amberDim { 0xffd9953a };
const juce::Colour lcd      { 0xff140d05 };

inline juce::Typeface::Ptr face (const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}

inline juce::Font blackletter (float h)
{
    static auto tf = face (BinaryData::UnifrakturMaguntia_ttf, BinaryData::UnifrakturMaguntia_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}
inline juce::Font cinzel (float h)
{
    static auto tf = face (BinaryData::Cinzel_ttf, BinaryData::Cinzel_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}
inline juce::Font pixel (float h)
{
    static auto tf = face (BinaryData::VT323_ttf, BinaryData::VT323_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

// Chunky raised hardware key.
inline void key (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, bool down = false)
{
    g.setColour (shadow);
    g.fillRoundedRectangle (r, 4);
    auto top = r.withTrimmedBottom (down ? 1.5f : 3.5f).withTrimmedTop (down ? 2.0f : 0.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (top, 4);
    g.setColour (fill.brighter (0.25f).withAlpha (0.5f));
    g.drawRoundedRectangle (top.reduced (0.5f), 4, 1.0f);
}

inline void led (juce::Graphics& g, juce::Point<float> c, float d, juce::Colour col, bool lit)
{
    if (lit)
    {
        g.setColour (col.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (d * 2.4f, d * 2.4f).withCentre (c));
    }
    g.setColour (lit ? col : edge);
    g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c));
}

inline void screw (juce::Graphics& g, juce::Point<float> c)
{
    g.setColour (juce::Colour (0xff3a3340));
    g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre (c));
    g.setColour (shadow);
    g.drawEllipse (juce::Rectangle<float> (9, 9).withCentre (c), 1);
    g.drawLine (c.x - 2.5f, c.y, c.x + 2.5f, c.y, 1.2f);
}
} // namespace gothic

class GothicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GothicLookAndFeel()
    {
        using namespace gothic;
        setColour (juce::Slider::rotarySliderFillColourId, gold);
        setColour (juce::ComboBox::backgroundColourId, lcd);
        setColour (juce::ComboBox::textColourId, amber);
        setColour (juce::ComboBox::arrowColourId, amber);
        setColour (juce::ComboBox::outlineColourId, shadow);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, blood);
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::TooltipWindow::backgroundColourId, panel);
        setColour (juce::TooltipWindow::textColourId, text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider& s) override
    {
        using namespace gothic;
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = start + pos * (end - start);
        const juce::PathStrokeType stroke (r * 0.09f + 1.0f);

        juce::Path track, value;
        track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, start, end, true);
        value.addCentredArc (c.x, c.y, r - 2, r - 2, 0, start, ang, true);
        g.setColour (juce::Colour (0xff2b2434));
        g.strokePath (track, stroke);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, stroke);

        const float body = r * 0.72f;
        g.setColour (shadow);
        g.fillEllipse (juce::Rectangle<float> (body * 2 + 3, body * 2 + 3).withCentre (c));
        g.setColour (juce::Colour (0xff221b29));
        g.fillEllipse (juce::Rectangle<float> (body * 2, body * 2).withCentre (c));
        g.setColour (juce::Colour (0xff2e2636));
        g.fillEllipse (juce::Rectangle<float> (body * 1.45f, body * 1.45f).withCentre (c));

        const auto tip = c.getPointOnCircumference (body * 0.82f, ang);
        g.setColour (text);
        g.drawLine ({ c, tip }, 2.5f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& fill, bool over, bool down) override
    {
        auto col = fill;
        if (over) col = col.brighter (0.12f);
        gothic::key (g, b.getLocalBounds().toFloat().reduced (0.5f), col, down);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        g.setFont (gothic::cinzel (13.0f));
        g.setColour (b.findColour (juce::TextButton::textColourOffId));
        g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedBottom (down ? 0 : 3).withTrimmedTop (down ? 2 : 0),
                    juce::Justification::centred);
    }

    // Power / half-time switches: a key with a glowing LED.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override
    {
        using namespace gothic;
        auto r = b.getLocalBounds().toFloat();
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        if (b.getButtonText().isEmpty())
        {
            key (g, r.reduced (0.5f), over ? keyTop.brighter (0.1f) : keyTop, down);
            led (g, r.withTrimmedBottom (3).getCentre(), 8, accent, b.getToggleState());
            return;
        }
        auto k = r.removeFromTop (r.getHeight() - 18).withSizeKeepingCentre (44, 28);
        key (g, k, over ? keyTop.brighter (0.1f) : keyTop, down);
        led (g, k.withTrimmedBottom (3).getCentre(), 8, accent, b.getToggleState());
        g.setFont (cinzel (11.0f));
        g.setColour (text);
        g.drawText (b.getButtonText(), r.removeFromTop (14), juce::Justification::centred);
        g.setFont (pixel (15.0f));
        g.setColour (amber);
        g.drawText (b.getToggleState() ? "ON" : "OFF", r, juce::Justification::centredTop);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (gothic::lcd);
        g.fillRoundedRectangle (r, 4);
        g.setColour (gothic::shadow);
        g.drawRoundedRectangle (r.reduced (1), 4, 2);
        juce::Path arrow;
        arrow.addTriangle ((float) w - 20, (float) h * 0.42f, (float) w - 10, (float) h * 0.42f, (float) w - 15, (float) h * 0.6f);
        g.setColour (gothic::amber);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return gothic::pixel (20.0f); }
    juce::Font getPopupMenuFont() override { return gothic::pixel (20.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 30, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }
};
