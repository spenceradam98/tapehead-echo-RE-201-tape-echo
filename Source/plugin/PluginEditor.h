#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class TapeheadLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TapeheadLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

class TapeheadEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TapeheadEditor (TapeheadProcessor&);
    ~TapeheadEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::String label;
        int kind = 0;   // 0 = 0..10 scale, 1 = -5..+5 tone scale, 2 = mode selector, 3 = small service knob
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    Knob& addKnob (const juce::String& id, const juce::String& label, int kind);
    void drawScale (juce::Graphics&, const Knob&);
    void drawVu (juce::Graphics&, juce::Rectangle<float>);
    void drawTapeWindow (juce::Graphics&, juce::Rectangle<float>);
    void drawHeadChart (juce::Graphics&, juce::Rectangle<float>);

    TapeheadProcessor& proc;
    TapeheadLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *input = nullptr, *mode = nullptr, *rate = nullptr, *intensity = nullptr, *echoVol = nullptr, *reverbVol = nullptr,
         *bass = nullptr, *treble = nullptr, *wow = nullptr, *age = nullptr, *noise = nullptr, *output = nullptr;

    juce::ToggleButton echoOn { "ECHO" }, splice { "SPLICE" }, dry { "DRY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> echoOnAttach, spliceAttach, dryAttach;
    juce::TextButton power { "POWER" };
    juce::ComboBox presetBox;

    juce::Image caseTexture;
    float vuNeedle = 0.0f;
    bool peakLit = false;
    int powerFlash = 0;
    double tapePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeheadEditor)
};
