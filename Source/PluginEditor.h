#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class Vibe2ALookAndFeel : public juce::LookAndFeel_V4
{
public:
    Vibe2ALookAndFeel();
    ~Vibe2ALookAndFeel() override = default;

    // Style IDs stored in Slider::getProperties()["knobStyle"]
    // 0 = Large Scalloped Black Bakelite Knob (GAIN, PEAK REDUCTION)
    // 1 = Brushed Silver Aluminum Knob (ATTACK, RELEASE, DRIVE, MIX)
    // 2 = CLA-2A Slotted Metallic Trimmer Screw with Curved Arrow (HI FREQ / [FLAT])
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
};

class Vibe2AProAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit Vibe2AProAudioProcessorEditor (Vibe2AProAudioProcessor&);
    ~Vibe2AProAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void generateWeatheredChassisTexture (int width, int height);
    void drawCalibratedVUMeter (juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawValueReadoutBox (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text, bool highlight = false);

    // Piecewise calibrated VU scale mapping (-20 dB to +3 dB with zero awkward gaps)
    static float dbToNormalizedMeterPos (float db) noexcept;
    static float normalizedMeterPosToAngle (float normPos) noexcept;

    Vibe2AProAudioProcessor& audioProcessor;
    Vibe2ALookAndFeel customLookAndFeel;

    // Pre-rendered procedural 3D weathered/scratched hardware chassis cache
    juce::Image weatheredChassisImage;

    // Top Preset Bar Controls
    juce::TextButton prevPresetBtn { "<" };
    juce::TextButton nextPresetBtn { ">" };
    juce::ComboBox   presetComboBox;

    // Upper Panel Main Controls (Large Scalloped Bakelite Knobs)
    juce::Slider gainSlider;
    juce::Slider peakReductionSlider;

    // Center VU & Ratio Bank Controls
    juce::ToggleButton vuModeToggle { "VU: GR / OUT" };
    juce::TextButton   ratio2Btn    { "2:1" };
    juce::TextButton   ratio4Btn    { "4:1" };
    juce::TextButton   ratio8Btn    { "8:1" };
    juce::TextButton   ratioLimBtn  { "LIMIT" };

    // Lower Panel Precision Controls
    juce::ToggleButton fastT4BToggle { "FAST T4B (RAP)" };
    juce::Slider       attackSlider;
    juce::Slider       releaseSlider;
    juce::Slider       hiFreqFlatSlider;
    juce::Slider       tubeDriveSlider;
    juce::Slider       parallelMixSlider;

    // Clickable [ FLAT ] hit-box next to HI FREQ knob (CLA-2A style)
    juce::Rectangle<float> flatLabelClickArea;

    // APVTS Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> peakReductionAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> hiFreqFlatAttachment;
    std::unique_ptr<SliderAttachment> tubeDriveAttachment;
    std::unique_ptr<SliderAttachment> parallelMixAttachment;
    std::unique_ptr<ButtonAttachment> fastT4BAttachment;
    std::unique_ptr<ButtonAttachment> vuModeAttachment;

    float displayedMeterDb   { 0.0f };
    float displayedGrDb      { 0.0f };
    float displayedSweetSpot { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vibe2AProAudioProcessorEditor)
};
