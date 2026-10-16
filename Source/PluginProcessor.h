#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <string>

struct VibePreset
{
    juce::String name;
    float gain;
    float peakReduction;
    int   ratioMode;      // 0: 2:1, 1: 4:1, 2: 8:1, 3: LIMIT
    float attackMs;       // 0.5 to 50.0 ms
    float releaseMs;      // 30.0 to 1500.0 ms
    float hiFreqFlat;     // 0.0 (Max HI FREQ pre-emphasis) to 100.0 ([ FLAT ])
    float tubeDrive;      // 0.0 to 100.0 %
    float parallelMix;    // 0.0 to 100.0 %
    bool  fastT4B;
};

class Vibe2AProAudioProcessor : public juce::AudioProcessor
{
public:
    Vibe2AProAudioProcessor();
    ~Vibe2AProAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Live metering getters for UI (thread-safe atomics)
    float getMeterDisplayDb() const noexcept     { return meterDisplayDb.load (std::memory_order_relaxed); }
    float getLiveGainReductionDb() const noexcept { return liveGainReductionDb.load (std::memory_order_relaxed); }
    float getSweetSpotActivity() const noexcept   { return sweetSpotActivity.load (std::memory_order_relaxed); }
    bool  isOutputMeterMode() const noexcept      { return outputMeterMode.load (std::memory_order_relaxed); }

    const std::vector<VibePreset>& getFactoryPresets() const noexcept { return factoryPresets; }
    void loadFactoryPreset (int index);

private:
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void initFactoryPresets();
    std::vector<VibePreset> factoryPresets;
    int currentPresetIndex { 3 }; // Default to 04. Hoodtrap & Jersey Bounce Vocal or 01

    // DSP State Variables (T4B Electro-Optical Cell + Tube Stage)
    double currentSampleRate { 44100.0 };

    // Hybrid RMS + Peak optical detector states
    float detectorRmsSq { 0.0f };
    float fastEnvDb     { 0.0f }; // Primary optical envelope (dB of GR, positive internally)
    float slowMemEnvDb  { 0.0f }; // CdS phosphor memory tail envelope
    float opticalCharge { 0.0f }; // 0..1 sustained illumination charge memory

    // Sidechain R37 HI FREQ / [FLAT] filter states (biquad high-shelf)
    juce::dsp::IIR::Filter<float> scHighShelfL, scHighShelfR;

    // Sweet-spot warmth & transformer contour filters (subtle body + air sheen)
    juce::dsp::IIR::Filter<float> warmthFilterL, warmthFilterR;
    juce::dsp::IIR::Filter<float> dcBlockerL, dcBlockerR;

    // Parameter smoothers to prevent any zipper noise
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothGainDb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothPeakRed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDrive;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothMix;

    // Ballistic VU meter state
    float vuNeedleDb { 0.0f };
    std::atomic<float> meterDisplayDb      { 0.0f };
    std::atomic<float> liveGainReductionDb { 0.0f };
    std::atomic<float> sweetSpotActivity   { 0.0f };
    std::atomic<bool>  outputMeterMode     { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vibe2AProAudioProcessor)
};
