#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

Vibe2AProAudioProcessor::Vibe2AProAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    initFactoryPresets();
}

Vibe2AProAudioProcessor::~Vibe2AProAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout Vibe2AProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 },
        "Gain",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        40.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "peak_reduction", 1 },
        "Peak Reduction",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        62.0f));

    // Selectable Ratios: 2:1, 4:1, 8:1, LIMIT (inf:1)
    juce::StringArray ratioChoices { "2:1", "4:1", "8:1", "LIMIT" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "ratio_mode", 1 },
        "Ratio",
        ratioChoices,
        1)); // Default 4:1 (classic LA-2A / CLA-2A vocal ratio)

    // Fine-tuned Attack (0.5 ms to 50.0 ms, logarithmic feel)
    juce::NormalisableRange<float> attackRange (0.5f, 50.0f, 0.1f, 0.45f);
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack_ms", 1 },
        "Attack",
        attackRange,
        10.0f));

    // Fine-tuned Release (30.0 ms to 1500.0 ms, logarithmic feel)
    juce::NormalisableRange<float> releaseRange (30.0f, 1500.0f, 1.0f, 0.45f);
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release_ms", 1 },
        "Release",
        releaseRange,
        120.0f));

    // CLA-2A style HI FREQ <-> [ FLAT ] sidechain pre-emphasis
    // 100.0 = [ FLAT ] (full clockwise, flat sidechain response)
    // 0.0   = MAX HI FREQ (full counter-clockwise, strong HF sidechain emphasis / opto de-essing)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "hi_freq_flat", 1 },
        "HI FREQ / FLAT",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        75.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tube_drive", 1 },
        "12AX7 Tube Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        45.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "parallel_mix", 1 },
        "Parallel Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        88.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "fast_t4b", 1 },
        "Fast T4B (Rap)",
        true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "vu_mode", 1 },
        "VU: GR / OUT",
        false)); // false = Gain Reduction (GR), true = Output VU

    return { params.begin(), params.end() };
}

void Vibe2AProAudioProcessor::initFactoryPresets()
{
    factoryPresets = {
        // Name, Gain, PeakRed, Ratio(0=2:1,1=4:1,2=8:1,3=LIM), AttackMs, ReleaseMs, HiFreqFlat(100=FLAT,0=HIFREQ), Drive, Mix, FastT4B
        { "01. CLA-2A Classic Golden Vocal (-6dB Sweet Spot)", 40.0f, 62.0f, 1, 10.0f, 120.0f,  78.0f, 42.0f, 100.0f, false },
        { "02. Modern Melodic Trap Lead (Upfront)",            42.5f, 68.0f, 1,  6.5f,  85.0f,  60.0f, 52.0f,  90.0f, true  },
        { "03. Hard Drill & Aggressive Rap Punch",             44.0f, 74.0f, 2,  4.0f,  60.0f,  55.0f, 65.0f,  85.0f, true  },
        { "04. Hoodtrap & Jersey Bounce Vocal",                43.0f, 72.0f, 1,  5.5f,  75.0f,  65.0f, 55.0f,  82.0f, true  },
        { "05. Silky R&B Air & Warm Tube Hug",                 39.0f, 58.0f, 0, 14.0f, 180.0f,  85.0f, 48.0f,  95.0f, false },
        { "06. Heavy 8:1 Vocal Wall (Locked In)",              45.0f, 78.0f, 2,  3.5f,  70.0f,  50.0f, 60.0f,  80.0f, true  },
        { "07. Optical De-Esser & Bright Vocal Tamer",         40.0f, 65.0f, 1,  5.0f,  95.0f,  22.0f, 35.0f, 100.0f, true  },
        { "08. True FLAT Vintage Broadcast Limiter",           41.5f, 70.0f, 3,  8.0f, 140.0f, 100.0f, 58.0f, 100.0f, false },
        { "09. Parallel Ad-Lib & Dub Thickener",               43.5f, 82.0f, 2,  4.5f,  90.0f,  45.0f, 72.0f,  65.0f, true  },
        { "10. Gentle 2:1 Mastering & Vocal Bus Glue",         37.5f, 48.0f, 0, 18.0f, 240.0f, 100.0f, 30.0f,  75.0f, false }
    };
}

void Vibe2AProAudioProcessor::loadFactoryPreset (int index)
{
    if (index < 0 || index >= static_cast<int> (factoryPresets.size()))
        return;

    currentPresetIndex = index;
    const auto& p = factoryPresets[static_cast<size_t> (index)];

    auto setParam = [this] (const juce::String& id, float plainVal)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plainVal));
    };

    setParam ("gain",           p.gain);
    setParam ("peak_reduction", p.peakReduction);
    setParam ("ratio_mode",     static_cast<float> (p.ratioMode));
    setParam ("attack_ms",      p.attackMs);
    setParam ("release_ms",     p.releaseMs);
    setParam ("hi_freq_flat",   p.hiFreqFlat);
    setParam ("tube_drive",     p.tubeDrive);
    setParam ("parallel_mix",   p.parallelMix);
    setParam ("fast_t4b",       p.fastT4B ? 1.0f : 0.0f);
}

const juce::String Vibe2AProAudioProcessor::getName() const { return JucePlugin_Name; }
bool Vibe2AProAudioProcessor::acceptsMidi() const { return false; }
bool Vibe2AProAudioProcessor::producesMidi() const { return false; }
bool Vibe2AProAudioProcessor::isMidiEffect() const { return false; }
double Vibe2AProAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int Vibe2AProAudioProcessor::getNumPrograms() { return static_cast<int> (factoryPresets.size()); }
int Vibe2AProAudioProcessor::getCurrentProgram() { return currentPresetIndex; }
void Vibe2AProAudioProcessor::setCurrentProgram (int index) { loadFactoryPreset (index); }
const juce::String Vibe2AProAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < static_cast<int> (factoryPresets.size()))
        return factoryPresets[static_cast<size_t> (index)].name;
    return {};
}
void Vibe2AProAudioProcessor::changeProgramName (int, const juce::String&) {}

void Vibe2AProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 8000.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = 1;

    scHighShelfL.prepare (spec);
    scHighShelfR.prepare (spec);
    warmthFilterL.prepare (spec);
    warmthFilterR.prepare (spec);
    dcBlockerL.prepare (spec);
    dcBlockerR.prepare (spec);

    // Subtle analog warmth body filter (~165 Hz gentle transformer weight)
    auto warmthCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        currentSampleRate, 165.0f, 0.75f, juce::Decibels::decibelsToGain (0.85f));
    *warmthFilterL.coefficients = *warmthCoeffs;
    *warmthFilterR.coefficients = *warmthCoeffs;

    // 18 Hz High-pass DC blocker after asymmetric tube saturation
    auto dcCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 18.0f, 0.707f);
    *dcBlockerL.coefficients = *dcCoeffs;
    *dcBlockerR.coefficients = *dcCoeffs;

    smoothGainDb.reset (currentSampleRate, 0.015);
    smoothPeakRed.reset (currentSampleRate, 0.015);
    smoothDrive.reset (currentSampleRate, 0.015);
    smoothMix.reset (currentSampleRate, 0.015);

    detectorRmsSq = 0.0f;
    fastEnvDb     = 0.0f;
    slowMemEnvDb  = 0.0f;
    opticalCharge = 0.0f;
    vuNeedleDb    = 0.0f;
}

void Vibe2AProAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Vibe2AProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
}
#endif

void Vibe2AProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (numSamples == 0 || totalNumInputChannels == 0)
        return;

    // Read current parameters
    const float gainKnob    = apvts.getRawParameterValue ("gain")->load();
    const float peakRedKnob = apvts.getRawParameterValue ("peak_reduction")->load();
    const int   ratioMode   = static_cast<int> (apvts.getRawParameterValue ("ratio_mode")->load());
    const float attackMs    = apvts.getRawParameterValue ("attack_ms")->load();
    const float releaseMs   = apvts.getRawParameterValue ("release_ms")->load();
    const float hiFreqFlat  = apvts.getRawParameterValue ("hi_freq_flat")->load();
    const float driveKnob   = apvts.getRawParameterValue ("tube_drive")->load();
    const float mixKnob     = apvts.getRawParameterValue ("parallel_mix")->load();
    const bool  fastT4B     = apvts.getRawParameterValue ("fast_t4b")->load() > 0.5f;
    const bool  isOutMode   = apvts.getRawParameterValue ("vu_mode")->load() > 0.5f;

    outputMeterMode.store (isOutMode, std::memory_order_relaxed);

    // Calibrate Gain knob: 32.0 = 0 dB unity, 40.0 = +2.8 dB, 100.0 = +28 dB, 0.0 = -18 dB
    float targetGainDb = 0.0f;
    if (gainKnob >= 32.0f)
        targetGainDb = ((gainKnob - 32.0f) / 68.0f) * 26.0f;
    else
        targetGainDb = ((gainKnob - 32.0f) / 32.0f) * 18.0f;

    smoothGainDb.setTargetValue (targetGainDb);
    smoothPeakRed.setTargetValue (peakRedKnob);
    smoothDrive.setTargetValue (driveKnob * 0.01f);
    smoothMix.setTargetValue (mixKnob * 0.01f);

    // 1. Configure CLA-2A R37 HI FREQ <-> [ FLAT ] Sidechain Filter
    // hiFreqFlat = 100.0 -> [ FLAT ] (0.0 dB shelf boost)
    // hiFreqFlat = 0.0   -> HI FREQ  (+14.0 dB high-shelf above 1150 Hz in sidechain)
    const float hfEmphasisNorm = juce::jlimit (0.0f, 1.0f, (100.0f - hiFreqFlat) * 0.01f);
    const float scShelfGainDb  = 0.05f + hfEmphasisNorm * 14.0f;
    auto scCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        currentSampleRate, 1150.0f, 0.65f, juce::Decibels::decibelsToGain (scShelfGainDb));
    *scHighShelfL.coefficients = *scCoeffs;
    *scHighShelfR.coefficients = *scCoeffs;

    // 2. Configure Ratio & Soft-Knee Geometry
    // 0: 2:1   -> slope = 0.500, knee = 9.0 dB (silky gentle leveling)
    // 1: 4:1   -> slope = 0.750, knee = 6.5 dB (classic LA-2A vocal compression)
    // 2: 8:1   -> slope = 0.875, knee = 4.5 dB (dense upfront vocal compression)
    // 3: LIMIT -> slope = 0.965, knee = 2.5 dB (classic Teletronix Limit mode)
    float compSlope = 0.75f;
    float kneeWidthDb = 6.5f;
    switch (ratioMode)
    {
        case 0: compSlope = 0.500f; kneeWidthDb = 9.0f; break;
        case 1: compSlope = 0.750f; kneeWidthDb = 6.5f; break;
        case 2: compSlope = 0.875f; kneeWidthDb = 4.5f; break;
        case 3: compSlope = 0.965f; kneeWidthDb = 2.5f; break;
        default: break;
    }

    // 3. Configure Attack & Dual-Stage Opto Release Ballistics
    const float sr = static_cast<float> (currentSampleRate);
    const float effectiveAttackMs  = std::max (0.3f, fastT4B ? attackMs * 0.68f : attackMs);
    const float effectiveReleaseMs = std::max (20.0f, fastT4B ? releaseMs * 0.62f : releaseMs);

    // Syllabic RMS integration window (~8 ms) prevents bass waveform distortion
    const float rmsAlpha = 1.0f - std::exp (-1.0f / (0.008f * sr));
    const float attAlpha = 1.0f - std::exp (-1.0f / (0.001f * effectiveAttackMs * sr));
    const float relFastAlpha = 1.0f - std::exp (-1.0f / (0.001f * effectiveReleaseMs * sr));

    // Stage-2 Phosphor Memory Cell (6x slower than primary release, authentic T4B dual-slope tail)
    const float relSlowMs = std::min (4000.0f, effectiveReleaseMs * (fastT4B ? 4.2f : 6.8f));
    const float relSlowAlpha = 1.0f - std::exp (-1.0f / (0.001f * relSlowMs * sr));
    const float chargeAttackAlpha  = 1.0f - std::exp (-1.0f / (0.18f * sr));
    const float chargeReleaseAlpha = 1.0f - std::exp (-1.0f / (0.85f * sr));

    float* leftChannel  = buffer.getWritePointer (0);
    float* rightChannel = totalNumInputChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    float blockMaxGrDb = 0.0f;
    float blockOutRmsSq = 0.0f;
    float blockSweetSpotSum = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = leftChannel[i];
        const float inR = rightChannel ? rightChannel[i] : inL;

        const float curPeakRed = smoothPeakRed.getNextValue();
        const float curGainDb  = smoothGainDb.getNextValue();
        const float curDrive   = smoothDrive.getNextValue();
        const float curMix     = smoothMix.getNextValue();

        // Sidechain pre-emphasis (R37 HI FREQ <-> [ FLAT ])
        const float scL = scHighShelfL.processSample (inL);
        const float scR = rightChannel ? scHighShelfR.processSample (inR) : scL;

        // Stereo-linked hybrid Peak + RMS optical excitation
        const float peakAbs = std::max (std::abs (scL), std::abs (scR));
        const float energySq = 0.5f * (scL * scL + scR * scR);
        detectorRmsSq += rmsAlpha * (energySq - detectorRmsSq);
        const float rmsVal = std::sqrt ( std::max (0.0f, detectorRmsSq) );

        // Blend RMS (body) + Peak (transients) for musical optical detection
        const float detectorLin = 0.65f * rmsVal + 0.35f * peakAbs;
        const float sidechainDb = juce::Decibels::gainToDecibels (detectorLin + 1.0e-7f, -96.0f);

        // Calibrate Threshold so Peak Reduction 55..75 places typical vocals right into -5 to -7 dB GR!
        const float prNorm = curPeakRed * 0.01f;
        const float thresholdDb = -2.0f - std::pow (prNorm, 1.08f) * 36.0f;

        // Soft-knee optical gain computer
        float targetGrDb = 0.0f;
        if (curPeakRed > 0.2f)
        {
            const float overDb = sidechainDb - thresholdDb;
            const float halfKnee = 0.5f * kneeWidthDb;

            if (overDb <= -halfKnee)
            {
                targetGrDb = 0.0f;
            }
            else if (overDb < halfKnee)
            {
                const float x = overDb + halfKnee;
                targetGrDb = compSlope * (x * x) / (2.0f * kneeWidthDb);
            }
            else
            {
                targetGrDb = compSlope * overDb;
            }

            // Program-dependent optical cell saturation ceiling ( smoothly tapers around 22 dB GR )
            targetGrDb = 24.0f * std::tanh (targetGrDb / 24.0f);
        }

        // Dual-stage T4B Envelope Ballistics
        if (targetGrDb > fastEnvDb)
        {
            // Optical attack: slightly faster when driven harder (authentic T4B electroluminescent behavior)
            const float dynamicAttBoost = 1.0f + 0.35f * juce::jlimit (0.0f, 1.0f, (targetGrDb - fastEnvDb) / 12.0f);
            const float effAtt = std::min (0.99f, attAlpha * dynamicAttBoost);
            fastEnvDb += effAtt * (targetGrDb - fastEnvDb);
        }
        else
        {
            // Dual-stage release: blend fast recovery with slow CdS phosphor memory cell
            const float memoryWeight = 0.18f + 0.42f * opticalCharge;
            const float blendedReleaseTarget = (1.0f - memoryWeight) * targetGrDb + memoryWeight * slowMemEnvDb;
            fastEnvDb += relFastAlpha * (blendedReleaseTarget - fastEnvDb);
        }

        // Update Stage-2 slow phosphor memory envelope & charge
        if (fastEnvDb > slowMemEnvDb)
            slowMemEnvDb += (attAlpha * 0.35f) * (fastEnvDb - slowMemEnvDb);
        else
            slowMemEnvDb += relSlowAlpha * (targetGrDb - slowMemEnvDb);

        const float instantCharge = juce::jlimit (0.0f, 1.0f, fastEnvDb / 10.0f);
        if (instantCharge > opticalCharge)
            opticalCharge += chargeAttackAlpha * (instantCharge - opticalCharge);
        else
            opticalCharge += chargeReleaseAlpha * (instantCharge - opticalCharge);

        const float currentGrPositiveDb = juce::jlimit (0.0f, 22.0f, fastEnvDb);

        // =========================================================================
        // CLA-2A "-7 dB to -5 dB" SWEET SPOT CHARACTER ("GOLDEN OPTO HUG")
        // =========================================================================
        // Bell curve centered at 6.0 dB of Gain Reduction (spanning ~4.0 to 8.5 dB GR)
        const float distFromSweetSpot = (currentGrPositiveDb - 6.0f) / 2.2f;
        const float sweetSpotBell = (currentGrPositiveDb > 0.5f)
                                        ? std::exp (-0.5f * distFromSweetSpot * distFromSweetSpot)
                                        : 0.0f;
        blockSweetSpotSum += sweetSpotBell;

        // Subtle program-dependent auto-ride compensation in the -5..-7 dB sweet spot
        // Gives that unmistakable "vocal locked right in front of the speakers" feel
        const float sweetSpotHugDb = sweetSpotBell * 0.65f;
        const float netGainDb = -currentGrPositiveDb + curGainDb + sweetSpotHugDb;
        const float linearGain = juce::Decibels::decibelsToGain (netGainDb);

        float wetL = inL * linearGain;
        float wetR = inR * linearGain;

        // 12AX7 / 12BH7 Cathode Follower Tube Stage + Sweet-Spot Harmonic Bloom
        // Even when Drive is moderate, hitting the -5..-7 dB sweet spot blooms warm 2nd-order triode harmonics
        const float effectiveDrive = juce::jlimit (0.0f, 1.35f, curDrive + sweetSpotBell * 0.22f);
        if (effectiveDrive > 0.005f)
        {
            const float driveGain = 1.0f + effectiveDrive * 1.85f;
            const float asymBias  = 0.065f * effectiveDrive; // Asymmetric triode grid conduction -> warm 2nd harmonic
            const float normFactor = 1.0f / std::tanh (driveGain);

            auto processTriode = [driveGain, asymBias, normFactor] (float x) noexcept
            {
                const float biased = x * driveGain + asymBias;
                const float sat = std::tanh (biased) - std::tanh (asymBias);
                return sat * normFactor;
            };

            wetL = processTriode (wetL);
            wetR = processTriode (wetR);

            // Apply subtle transformer body filter & DC blocking
            wetL = dcBlockerL.processSample (warmthFilterL.processSample (wetL));
            wetR = dcBlockerR.processSample (warmthFilterR.processSample (wetR));
        }

        // Phase-coherent Parallel Mix
        const float outL = inL * (1.0f - curMix) + wetL * curMix;
        const float outR = inR * (1.0f - curMix) + wetR * curMix;

        leftChannel[i] = outL;
        if (rightChannel)
            rightChannel[i] = outR;

        blockMaxGrDb = std::max (blockMaxGrDb, currentGrPositiveDb);
        blockOutRmsSq += 0.5f * (outL * outL + outR * outR);
    }

    // Update live gain reduction & sweet spot activity atomics for UI
    const float currentGrSignedDb = -blockMaxGrDb; // e.g., -6.2 dB
    liveGainReductionDb.store (currentGrSignedDb, std::memory_order_relaxed);
    sweetSpotActivity.store (juce::jlimit (0.0f, 1.0f, blockSweetSpotSum / static_cast<float> (numSamples)),
                             std::memory_order_relaxed);

    // Compute target VU needle reading in dB (-20.0 to +3.0 dB)
    float targetMeterDb = 0.0f;
    if (! isOutMode)
    {
        // GR Mode: 0 dB (no compression) down to -20 dB
        targetMeterDb = juce::jlimit (-20.0f, 0.0f, currentGrSignedDb);
    }
    else
    {
        // Output VU Mode: calibrated so -14 dBFS RMS = 0 VU (modern vocal mix reference)
        const float meanOutRms = std::sqrt (blockOutRmsSq / static_cast<float> (numSamples));
        const float outDbfs = juce::Decibels::gainToDecibels (meanOutRms + 1.0e-7f, -60.0f);
        targetMeterDb = juce::jlimit (-20.0f, 3.0f, outDbfs + 14.0f);
    }

    // Smooth physical VU needle ballistics
    const float blockDurationSec = static_cast<float> (numSamples) / sr;
    const float vuTimeConst = (targetMeterDb < vuNeedleDb) ? 0.035f : 0.095f;
    const float vuAlpha = 1.0f - std::exp (-blockDurationSec / vuTimeConst);
    vuNeedleDb += vuAlpha * (targetMeterDb - vuNeedleDb);

    meterDisplayDb.store (vuNeedleDb, std::memory_order_relaxed);
}

bool Vibe2AProAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* Vibe2AProAudioProcessor::createEditor()
{
    return new Vibe2AProAudioProcessorEditor (*this);
}

void Vibe2AProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", currentPresetIndex, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void Vibe2AProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        auto newState = juce::ValueTree::fromXml (*xmlState);
        if (newState.hasProperty ("presetIndex"))
            currentPresetIndex = static_cast<int> (newState.getProperty ("presetIndex"));
        apvts.replaceState (newState);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Vibe2AProAudioProcessor();
}
