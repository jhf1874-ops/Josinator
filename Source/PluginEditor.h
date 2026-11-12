#pragma once
#include "UI/Components.h"

class JosinatorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JosinatorEditor (JosinatorProcessor&);
    ~JosinatorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct FxPanel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        std::unique_ptr<juce::ToggleButton> power;
        std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
        std::vector<juce::Component*> cells; // knobs (and the half-time switch) in grid order
    };

    void timerCallback() override;
    void addFx (const juce::String& title, const juce::String& onId,
                std::vector<std::pair<juce::String, juce::String>> knobs, int toggleAt = -1);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, bool arched = false) const;

    GothicLookAndFeel lnf; // declared first so it outlives every child
    JosinatorProcessor& proc;

    TomeList tomes;
    StepGrid grid;
    juce::ComboBox kitBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> kitAttachment;
    juce::TextButton slotButtons[JosinatorProcessor::kSlots];
    juce::TextButton clearButton { "Clear" }, summonButton { "SUMMON" };
    juce::OwnedArray<GothicKnob> knobs;
    juce::ToggleButton halfTime { "Half-Time" };
    std::unique_ptr<APVTS::ButtonAttachment> halfTimeAttachment;
    std::vector<GothicKnob*> fleshKnobs;
    std::vector<FxPanel> fx;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> lcdArea, tomePanel, kitPanel, ritualPanel, fleshPanel;
    int lastStep = -2, lastSlot = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JosinatorEditor)
};
