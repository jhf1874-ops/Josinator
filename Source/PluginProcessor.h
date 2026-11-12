#pragma once
#include <JuceHeader.h>
#include "DSP/Sequencer.h"
#include "DSP/Veil.h"

class JosinatorProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kSlots = 4;

    JosinatorProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Josinator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Josinator"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Pattern editing (message thread); the audio thread only reads.
    int currentSlot() const { return (int) apvts.getRawParameterValue ("slot")->load(); }
    int cell (int slot, int lane, int step) const { return cells[(size_t) index (slot, lane, step)].load(); }
    void setCell (int slot, int lane, int step, int vel) { cells[(size_t) index (slot, lane, step)] = (uint8_t) juce::jlimit (0, 3, vel); }
    void setPattern (int slot, const jz::Pattern&);
    void loadTome (int tome);   // into the current slot, also sets its swing / half-time
    void summonBreak();         // fresh variation of the current tome into the current slot
    void clearSlot();
    void setParam (const juce::String& id, float plainValue);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> playStep { -1 }, tome { 0 };
    std::atomic<float> shownBpm { 88.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static int index (int slot, int lane, int step) { return (slot * jz::kNumLanes + lane) * jz::kSteps + step; }
    float v (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    std::array<std::atomic<uint8_t>, kSlots * jz::kNumLanes * jz::kSteps> cells {};
    double freePpq = 0.0;
    jz::Rng uiRng;

    jz::Sequencer seq;
    jz::DrumKit kit;
    jz::Crusher crusher;
    jz::Biquad lowpass;
    jz::Smear smear;
    jz::ReverseBloom bloom;
    jz::OctaveUp shimmer;
    jz::TapeHaze haze;
    juce::Reverb reverb;
    juce::AudioBuffer<float> dry, wet, bloomBuf, revBuf, shimBuf;
    float breathEnv = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JosinatorProcessor)
};
