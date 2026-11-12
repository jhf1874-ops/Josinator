#pragma once
#include "Gothic.h"
#include "../PluginProcessor.h"

using APVTS = juce::AudioProcessorValueTreeState;

// Knob + Cinzel caption + amber pixel readout, bound to one parameter.
struct GothicKnob : juce::Component
{
    GothicKnob (APVTS& s, const juce::String& id, const juce::String& name, juce::Colour arc)
        : caption (name), attachment (s, id, slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, arc);
        slider.setTooltip (s.getParameter (id)->getName (64));
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        auto readout = r.removeFromBottom (16);
        auto label = r.removeFromBottom (15);
        g.setFont (gothic::cinzel (12.0f));
        g.setColour (gothic::text);
        g.drawText (caption, label, juce::Justification::centred);
        g.setFont (gothic::pixel (17.0f));
        g.setColour (gothic::amber);
        g.drawText (slider.getTextFromValue (slider.getValue()).toUpperCase().removeCharacters (" "), readout, juce::Justification::centred);
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (32)); }

    juce::String caption;
    juce::Slider slider;
    APVTS::SliderAttachment attachment;
};

// The Ritual: 7 lanes x 16 steps. Click to place/erase, drag up/down for velocity, right-click cycles velocity.
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (JosinatorProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        using namespace gothic;
        static const char* names[jz::kNumLanes] = { "KICK", "SNARE", "GHOST", "HATS", "WASH", "TOMS", "RIM" };
        static const char* roman[16] = { "I","II","III","IV","V","VI","VII","VIII","IX","X","XI","XII","XIII","XIV","XV","XVI" };
        const int play = proc.playStep.load(), slot = proc.currentSlot();
        const auto accent = findColour (juce::Slider::rotarySliderFillColourId);

        for (int s = 0; s < jz::kSteps; ++s)
        {
            auto c = cell (0, s);
            auto head = juce::Rectangle<float> (c.getX(), 0, c.getWidth(), kHead);
            g.setFont (cinzel (11.0f));
            g.setColour (s == play ? amber : muted);
            g.drawText (roman[s], head.removeFromTop (14), juce::Justification::centred);
            led (g, head.getCentre(), 6, amber, s == play);
        }

        for (int l = 0; l < jz::kNumLanes; ++l)
        {
            auto row = cell (l, 0);
            g.setFont (pixel (19.0f));
            g.setColour (juce::Colour (0xffd8d0e2));
            g.drawText (names[l], juce::Rectangle<float> (0, row.getY(), kLabel - 6, row.getHeight()), juce::Justification::centredLeft);

            for (int s = 0; s < jz::kSteps; ++s)
            {
                auto r = cell (l, s);
                const int vel = proc.cell (slot, l, s);
                if (vel == 0)
                {
                    key (g, r, s % 4 == 0 ? stepBeat : stepOff);
                    g.setColour (edge);
                    g.drawRoundedRectangle (r.withTrimmedBottom (3).reduced (0.5f), 4, 1);
                }
                else
                {
                    static constexpr float glow[4] = { 0, 2, 5, 9 }, alpha[4] = { 0, 0.4f, 0.7f, 1.0f };
                    g.setColour (accent.withAlpha (0.25f * alpha[vel]));
                    g.fillRoundedRectangle (r.expanded (glow[vel] * 0.5f), 5);
                    key (g, r, stepOff.interpolatedWith (accent, alpha[vel]));
                }
                if (s == play)
                {
                    g.setColour (amber.withAlpha (0.8f));
                    g.drawRoundedRectangle (r.expanded (1.5f), 5, 1.2f);
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! hit (e.position, lane, step)) { lane = -1; return; }
        const int slot = proc.currentSlot(), now = proc.cell (slot, lane, step);
        if (e.mods.isPopupMenu())   { proc.setCell (slot, lane, step, now % 3 + 1); lane = -1; }
        else if (now == 0)          { proc.setCell (slot, lane, step, 2); startVel = 2; }
        else                        { erasing = true; startVel = now; }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (lane < 0 || std::abs (e.getDistanceFromDragStartY()) < 4) return;
        erasing = false; // dragging means "set velocity", not "erase"
        const int vel = juce::jlimit (1, 3, startVel - e.getDistanceFromDragStartY() / 18);
        proc.setCell (proc.currentSlot(), lane, step, vel);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (lane >= 0 && erasing) proc.setCell (proc.currentSlot(), lane, step, 0);
        lane = -1; erasing = false;
        repaint();
    }

private:
    static constexpr float kLabel = 74, kHead = 26, kGap = 4;

    juce::Rectangle<float> cell (int l, int s) const
    {
        const float w = ((float) getWidth() - kLabel - 15 * kGap) / 16.0f;
        const float h = ((float) getHeight() - kHead - 6 - 6 * kGap) / (float) jz::kNumLanes;
        return { kLabel + (float) s * (w + kGap), kHead + 6 + (float) l * (h + kGap), w, h };
    }

    bool hit (juce::Point<float> p, int& l, int& s) const
    {
        for (l = 0; l < jz::kNumLanes; ++l)
            for (s = 0; s < jz::kSteps; ++s)
                if (cell (l, s).expanded (kGap * 0.5f).contains (p)) return true;
        return false;
    }

    JosinatorProcessor& proc;
    int lane = -1, step = 0, startVel = 2;
    bool erasing = false;
};

// Tomes of Breaks: click a tome to bind it into the current pattern slot.
class TomeList : public juce::Component
{
public:
    explicit TomeList (JosinatorProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        using namespace gothic;
        const int current = proc.tome.load();
        for (int i = 0; i < jz::kNumTomes; ++i)
        {
            auto r = row (i);
            if (i == current || i == hover)
            {
                g.setColour (juce::Colour (0xff221b29));
                g.fillRoundedRectangle (r, 4);
            }
            if (i == current)
            {
                g.setColour (gold);
                g.drawRoundedRectangle (r.reduced (0.5f), 4, 1);
            }
            auto t = r.reduced (10, 4);
            g.setFont (cinzel (14.0f));
            g.setColour (text);
            g.drawText (jz::kTomes[i].name, t.removeFromTop (t.getHeight() * 0.55f), juce::Justification::bottomLeft);
            g.setFont (pixel (17.0f));
            g.setColour (muted);
            g.drawText (jz::kTomes[i].feel, t, juce::Justification::topLeft);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (indexAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (const int i = indexAt (e.position); i >= 0) { proc.loadTome (i); repaint(); }
    }

private:
    juce::Rectangle<float> row (int i) const
    {
        const float h = juce::jmin (52.0f, (float) getHeight() / jz::kNumTomes);
        return { 0, (float) i * h, (float) getWidth(), h - 4 };
    }
    int indexAt (juce::Point<float> p) const
    {
        for (int i = 0; i < jz::kNumTomes; ++i) if (row (i).contains (p)) return i;
        return -1;
    }
    void setHover (int i) { if (i != hover) { hover = i; repaint(); } }

    JosinatorProcessor& proc;
    int hover = -1;
};
