#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    const juce::String input = "input", mode = "mode", rate = "rate", intensity = "intensity", echo = "echoVol",
                       reverb = "reverbVol", bass = "bass", treble = "treble", echoOn = "echoOn",
                       wow = "wowFlutter", age = "tapeAge", noise = "noise", splice = "splice", dry = "dry", output = "output";
}

juce::AudioProcessorValueTreeState::ParameterLayout FerroProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto knob = [&] (const String& id, const String& name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def * 10.0f));
    };
    auto tone = [&] (const String& id, const String& name)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (-5.0f, 5.0f, 0.01f), 0.0f));
    };
    knob (ids::input, "Input Volume", 0.5f);
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ids::mode, 1 }, "Mode Selector", 1, 12, 4));
    knob (ids::rate, "Repeat Rate", 0.5f);
    knob (ids::intensity, "Intensity", 0.35f);
    knob (ids::echo, "Echo Volume", 0.5f);
    knob (ids::reverb, "Reverb Volume", 0.35f);
    tone (ids::bass, "Bass");
    tone (ids::treble, "Treble");
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::echoOn, 1 }, "Echo/Normal", true));
    knob (ids::wow, "Wow & Flutter", 0.5f);
    knob (ids::age, "Tape Age", 0.2f);
    knob (ids::noise, "Tape Noise", 0.35f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::splice, 1 }, "Tape Splice", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::dry, 1 }, "Dry Through", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Output",
                                                        NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f));
    return { p.begin(), p.end() };
}

FerroProcessor::FerroProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FerroEcho", createLayout())
{
}

bool FerroProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    const bool okIn = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return okOut && okIn;
}

void FerroProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    mono.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
}

fe::Params FerroProcessor::readParams() const
{
    auto v = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    fe::Params p;
    p.input = v (ids::input) / 10.0f;
    p.mode = (int) std::lround (v (ids::mode));
    p.repeatRate = v (ids::rate) / 10.0f;
    p.intensity = v (ids::intensity) / 10.0f;
    p.echoVolume = v (ids::echo) / 10.0f;
    p.reverbVolume = v (ids::reverb) / 10.0f;
    p.bass = v (ids::bass) / 5.0f;
    p.treble = v (ids::treble) / 5.0f;
    p.echoOn = v (ids::echoOn) > 0.5f;
    p.wowFlutter = v (ids::wow) / 10.0f;
    p.tapeAge = v (ids::age) / 10.0f;
    p.noise = v (ids::noise) / 10.0f;
    p.splice = v (ids::splice) > 0.5f;
    p.dry = v (ids::dry) > 0.5f;
    p.outputDb = v (ids::output);
    return p;
}

void FerroProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if ((int) mono.size() < n) mono.resize ((size_t) n);   // hosts may exceed the announced block size

    // the unit is mono: sum the inputs
    for (int i = 0; i < n; ++i)
    {
        float s = 0;
        for (int c = 0; c < inCh; ++c) s += buffer.getReadPointer (c)[i];
        mono[(size_t) i] = inCh > 0 ? s / (float) inCh : 0.0f;
    }

    if (powerCycleRequested.exchange (false)) engine.powerCycle();
    engine.setParams (readParams());
    engine.process (mono.data(), mono.data(), n);

    for (int c = 0; c < outCh; ++c) buffer.copyFrom (c, 0, mono.data(), n);
}

int FerroProcessor::getNumPrograms() { int n = 0; fe::factoryPresets (n); return n; }

const juce::String FerroProcessor::getProgramName (int index)
{
    int n = 0; auto* ps = fe::factoryPresets (n);
    return juce::isPositiveAndBelow (index, n) ? juce::String (ps[index].name) : juce::String();
}

void FerroProcessor::setCurrentProgram (int index)
{
    int n = 0; auto* ps = fe::factoryPresets (n);
    if (! juce::isPositiveAndBelow (index, n)) return;
    currentProgram = index;
    const auto& p = ps[index].p;
    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    set (ids::input, p.input * 10.0f);
    set (ids::mode, (float) p.mode);
    set (ids::rate, p.repeatRate * 10.0f);
    set (ids::intensity, p.intensity * 10.0f);
    set (ids::echo, p.echoVolume * 10.0f);
    set (ids::reverb, p.reverbVolume * 10.0f);
    set (ids::bass, p.bass * 5.0f);
    set (ids::treble, p.treble * 5.0f);
    set (ids::echoOn, p.echoOn ? 1.0f : 0.0f);
    set (ids::wow, p.wowFlutter * 10.0f);
    set (ids::age, p.tapeAge * 10.0f);
    set (ids::noise, p.noise * 10.0f);
    set (ids::splice, p.splice ? 1.0f : 0.0f);
    set (ids::dry, p.dry ? 1.0f : 0.0f);
    set (ids::output, p.outputDb);
}

void FerroProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void FerroProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* FerroProcessor::createEditor() { return new FerroEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FerroProcessor(); }
