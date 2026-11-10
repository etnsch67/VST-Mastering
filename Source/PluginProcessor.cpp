#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MonsterAudioProcessor::createLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                    NormalisableRange<float> (lo, hi, 0.01f), def, Attr().withLabel (unit)));
    };

    add ("input",   "Input",   -12.f, 12.f,  0.f,   "dB");
    add ("drive",   "Drive",     0.f, 24.f,  6.f,   "dB");
    add ("tone",    "Tone",     -6.f,  6.f,  0.f,   "dB");
    add ("width",   "Width",     0.f, 200.f, 100.f, "%");
    add ("ceiling", "Ceiling",  -6.f,  0.f, -0.3f,  "dB");
    add ("mix",     "Mix",       0.f, 100.f, 100.f, "%");
    add ("output",  "Output",  -12.f,  6.f,  0.f,   "dB");
    return layout;
}

MonsterAudioProcessor::MonsterAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pInput   = apvts.getRawParameterValue ("input");
    pDrive   = apvts.getRawParameterValue ("drive");
    pTone    = apvts.getRawParameterValue ("tone");
    pWidth   = apvts.getRawParameterValue ("width");
    pCeiling = apvts.getRawParameterValue ("ceiling");
    pMix     = apvts.getRawParameterValue ("mix");
    pOutput  = apvts.getRawParameterValue ("output");
}

bool MonsterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == layouts.getMainOutputChannelSet();
}

void MonsterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto factor = oversampling.getOversamplingFactor();
    osRate = sampleRate * (double) factor;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    mixer.prepare (spec);
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    mixer.setWetLatency (oversampling.getLatencyInSamples());
    setLatencySamples ((int) oversampling.getLatencyInSamples());

    juce::dsp::ProcessSpec osSpec { osRate, (juce::uint32) ((size_t) samplesPerBlock * factor), 2 };
    tone.prepare (osSpec);
    lastToneDb = 0.0f;
    *tone.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (osRate, 3000.0f, 0.7f, 1.0f);
    tone.reset();

    for (auto* s : { &inS, &driveS, &widthS, &ceilS })
        s->reset (osRate, 0.02);

    inS.setCurrentAndTargetValue    (juce::Decibels::decibelsToGain (pInput->load()));
    driveS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    widthS.setCurrentAndTargetValue (pWidth->load() * 0.01f);
    ceilS.setCurrentAndTargetValue  (juce::Decibels::decibelsToGain (pCeiling->load()));
    lastOutGain = juce::Decibels::decibelsToGain (pOutput->load());
}

void MonsterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    juce::dsp::AudioBlock<float> block (buffer);

    mixer.setWetMixProportion (pMix->load() * 0.01f);
    mixer.pushDrySamples (block);

    auto osBlock = oversampling.processSamplesUp (block);
    const size_t n = osBlock.getNumSamples();
    float* l = osBlock.getChannelPointer (0);
    float* r = osBlock.getChannelPointer (1);

    inS.setTargetValue    (juce::Decibels::decibelsToGain (pInput->load()));
    driveS.setTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    widthS.setTargetValue (pWidth->load() * 0.01f);
    ceilS.setTargetValue  (juce::Decibels::decibelsToGain (pCeiling->load()));

    // 1) Input + saturation (tanh), compensated by 1/sqrt(drive)
    for (size_t i = 0; i < n; ++i)
    {
        const float in = inS.getNextValue();
        const float k  = driveS.getNextValue();
        const float comp = 1.0f / std::sqrt (k);
        l[i] = std::tanh (l[i] * in * k) * comp;
        r[i] = std::tanh (r[i] * in * k) * comp;
    }

    // 2) Tone: high shelf at 3 kHz, +/- 6 dB
    const float toneDb = pTone->load();
    if (toneDb != lastToneDb)
    {
        lastToneDb = toneDb;
        *tone.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            osRate, 3000.0f, 0.7f, juce::Decibels::decibelsToGain (toneDb));
    }
    tone.process (juce::dsp::ProcessContextReplacing<float> (osBlock));

    // 3) Stereo width (mid/side) + final clipper
    for (size_t i = 0; i < n; ++i)
    {
        const float w = widthS.getNextValue();
        const float c = ceilS.getNextValue();
        const float m = 0.5f * (l[i] + r[i]);
        const float s = 0.5f * (l[i] - r[i]) * w;
        l[i] = juce::jlimit (-c, c, m + s);
        r[i] = juce::jlimit (-c, c, m - s);
    }

    oversampling.processSamplesDown (block);
    mixer.mixWetSamples (block);

    const float outGain = juce::Decibels::decibelsToGain (pOutput->load());
    buffer.applyGainRamp (0, numSamples, lastOutGain, outGain);
    lastOutGain = outGain;
}

juce::AudioProcessorEditor* MonsterAudioProcessor::createEditor()
{
    return new MonsterEditor (*this);
}

void MonsterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MonsterAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonsterAudioProcessor();
}
