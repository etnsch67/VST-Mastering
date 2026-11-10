#pragma once
#include "PluginProcessor.h"

class MonsterEditor : public juce::AudioProcessorEditor
{
public:
    explicit MonsterEditor (MonsterAudioProcessor&);
    ~MonsterEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::array<Knob, 7> knobs;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonsterEditor)
};
