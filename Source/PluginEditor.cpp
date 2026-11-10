#include "PluginEditor.h"

MonsterEditor::MonsterEditor (MonsterAudioProcessor& p) : AudioProcessorEditor (&p)
{
    static const char* ids[]   = { "input", "drive", "tone", "width", "ceiling", "mix", "output" };
    static const char* names[] = { "INPUT", "DRIVE", "TONE", "WIDTH", "CEILING", "MIX", "OUTPUT" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe03030));
        k.slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        k.slider.setDoubleClickReturnValue (true, p.apvts.getParameter (ids[i])->convertFrom0to1 (
                                                  p.apvts.getParameter (ids[i])->getDefaultValue()));
        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, juce::Colours::white);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                            p.apvts, ids[i], k.slider);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
    }
    setSize (740, 300);
}

void MonsterEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1a1a), 0, 0,
                                             juce::Colour (0xff050505), 0, (float) getHeight(), false));
    g.fillAll();
    g.setColour (juce::Colour (0xffe03030));
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("MONSTER CLONE", getLocalBounds().removeFromTop (60), juce::Justification::centred);
}

void MonsterEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (55);
    const int w = area.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = area.removeFromLeft (w);
        k.label.setBounds (col.removeFromTop (24));
        k.slider.setBounds (col);
    }
}
