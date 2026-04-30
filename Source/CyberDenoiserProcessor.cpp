#include "CyberDenoiserProcessor.h"
#include "CyberDenoiserEditor.h"
#include <cmath>

CyberDenoiserAudioProcessor::CyberDenoiserAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", {
          std::make_unique<juce::AudioParameterFloat> ("t1", "Threshold 1", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t2", "Threshold 2", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t3", "Threshold 3", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t4", "Threshold 4", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t5", "Threshold 5", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t6", "Threshold 6", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t7", "Threshold 7", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t8", "Threshold 8", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t9", "Threshold 9", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("t10", "Threshold 10", -100.0f, 0.0f, -100.0f),
          std::make_unique<juce::AudioParameterFloat> ("attack", "Attack", 0.1f, 100.0f, 0.1f),
          std::make_unique<juce::AudioParameterFloat> ("release", "Release", 0.0f, 1000.0f, 0.0f),
          std::make_unique<juce::AudioParameterFloat> ("floor", "Floor", 0.0f, 1.0f, 0.0f),
          std::make_unique<juce::AudioParameterFloat> ("gradient", "Gradient", 1.0f, 24.0f, 6.0f),
          std::make_unique<juce::AudioParameterBool> ("rmsMode", "RMS Mode", false),
          std::make_unique<juce::AudioParameterBool> ("lowCut", "Low Cut", true),
          std::make_unique<juce::AudioParameterBool> ("highCut", "High Cut", true),
          std::make_unique<juce::AudioParameterBool> ("listenNoise", "Listen Noise", false),
          std::make_unique<juce::AudioParameterFloat> ("hysteresis", "Hysteresis", 0.0f, 12.0f, 3.0f),
          std::make_unique<juce::AudioParameterFloat> ("master", "Master Threshold", -100.0f, 0.0f, -50.0f),
          std::make_unique<juce::AudioParameterFloat> ("learnTime", "Learn Duration (s)", 1.0f, 60.0f, 20.0f)
      })
{
    for (int i = 0; i < 10; ++i)
        thresholds[i] = (std::atomic<float>*) parameters.getRawParameterValue ("t" + juce::String (i + 1));
    
    attack = (std::atomic<float>*) parameters.getRawParameterValue ("attack");
    release = (std::atomic<float>*) parameters.getRawParameterValue ("release");
    floor = (std::atomic<float>*) parameters.getRawParameterValue ("floor");
    gradient = (std::atomic<float>*) parameters.getRawParameterValue ("gradient");
    rmsMode = (std::atomic<float>*) parameters.getRawParameterValue ("rmsMode");
    lowCut = (std::atomic<float>*) parameters.getRawParameterValue ("lowCut");
    highCut = (std::atomic<float>*) parameters.getRawParameterValue ("highCut");
    listenNoise = (std::atomic<float>*) parameters.getRawParameterValue ("listenNoise");
    hysteresis = (std::atomic<float>*) parameters.getRawParameterValue ("hysteresis");
    masterThreshold = (std::atomic<float>*) parameters.getRawParameterValue ("master");

    for (int i = 0; i < 10; ++i) {
        lastLevels[i].store (0.0f);
        lastReductions[i].store (1.0f);
        peakLevels[i].store (1e-6f);
    }

    // SAFE LOGGING: Use instance-unique filename and avoid global setCurrentLogger
    auto logDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    juce::String instID = juce::String::toHexString ((juce::int64)this).substring (0, 4);
    auto logFile = logDir.getChildFile ("CyberDenoiser_Log_" + instID + ".txt");
    
    fileLogger = std::make_unique<juce::FileLogger> (logFile, "Cyber-Denoiser PRO [" + instID + "]", 128 * 1024);
    fileLogger->logMessage ("--- Instance Started ---");
}

CyberDenoiserAudioProcessor::~CyberDenoiserAudioProcessor() 
{
    if (fileLogger)
        fileLogger->logMessage ("--- Instance Ended ---");
}

void CyberDenoiserAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int ch = 0; ch < 2; ++ch) { 
        for (int i = 0; i < 9; ++i) lpStates[ch][i] = 0;
        for (int i = 0; i < 10; ++i) {
            levelStates[ch][i] = 0; 
            targetGains[ch][i] = 1;
            currentGains[ch][i] = 1;
            hystStates[ch][i] = 0;
        }
    }
    hpCutStat[0] = hpCutStat[1] = 0;
    lpCutStat[0] = lpCutStat[1] = 0;

    for (int i = 0; i < 10; ++i) { 
        peakLevels[i].store (1e-6f);
        lastLevels[i].store (0.0f);
        lastReductions[i].store (1.0f);
    }

    setLatencySamples (0); 
}


void CyberDenoiserAudioProcessor::releaseResources() {}

bool CyberDenoiserAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

float CyberDenoiserAudioProcessor::calcGhostG (float rIn, float tDb, float tLinear, float hDb, float grad, float floorL, int& st)
{
    // Pure spectral subtraction: noise estimate (tLinear) is continuously
    // cancelled from the signal. No binary gate state, no jitter.
    // gain = (signal - noise) / signal = 1 - noise/signal
    (void)tDb; (void)hDb; (void)grad; (void)st;
    float gain = 1.0f - tLinear / (rIn + 1e-6f);
    return std::max (floorL, std::min (1.0f, gain));
}

void CyberDenoiserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sRate = getSampleRate();

    // Master Threshold sync removed from audio thread to avoid setValueNotifyingHost
    // This will be handled by the Editor on the Message Thread

    // Gain envelope: dual-rate smoothing with enforced stability
    float attCoeff = 1.0f - std::exp (-1.0f / (std::max (0.1f, attack->load()) * (float)sRate / 1000.0f));
    float baseRelCoeff = 1.0f - std::exp (-1.0f / (std::max (100.0f, release->load()) * (float)sRate / 1000.0f));

    // Level detector: 2ms attack, 60ms decay for stability with non-static noise
    float levelAtt = 1.0f - std::exp (-1.0f / (2.0f * (float)sRate / 1000.0f));
    float levelRel = 1.0f - std::exp (-1.0f / (60.0f * (float)sRate / 1000.0f));
    float rmsF = 1.0f - std::exp (-1.0f / (0.050f * (float)sRate));

    auto getCoeff = [sRate](float freq) {
        return 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * freq / (float)std::max(1.0, sRate));
    };

    float hpF = getCoeff (40.0f);
    float lpF_cut = getCoeff (18000.0f);
    
    float multi[] = { 60.0f, 150.0f, 400.0f, 800.0f, 1500.0f, 3000.0f, 5000.0f, 8000.0f, 12000.0f };
    float bCoeffs[9];
    for (int i = 0; i < 9; ++i) {
        bCoeffs[i] = std::min (0.99f, getCoeff (multi[i]));
    }

    bool isRMS = (bool)rmsMode->load();
    float floorV = floor->load();
    bool isLowCut = lowCut->load();
    bool isHighCut = highCut->load();
    bool isListenNoise = listenNoise->load();

    // Precompute per-band noise estimates (linear) outside sample loop
    float tLinear[10];
    for (int i = 0; i < 10; ++i) {
        tLinear[i] = std::pow (10.0f, thresholds[i]->load() * 0.05f);
    }

    const int activeChannels = std::min (2, numChannels);

    for (int ch = 0; ch < activeChannels; ++ch) {
        auto* data = buffer.getWritePointer (ch);
        
        for (int s = 0; s < numSamples; ++s) {
            float s0 = data[s];
            float outSum = 0;

            // 1. Low Cut / High Cut filters (1st Order)
            if (isLowCut) {
                hpCutStat[ch] += (s0 - hpCutStat[ch]) * hpF;
                s0 -= hpCutStat[ch];
            }
            if (isHighCut) {
                lpCutStat[ch] += (s0 - lpCutStat[ch]) * lpF_cut;
                s0 = lpCutStat[ch];
            }

            // 2. 10-Band Filter Bank (1st Order Parallel Subtraction)
            float b[10];
            float currentIn = s0;
            for (int i = 0; i < 9; ++i) {
                lpStates[ch][i] += (currentIn - lpStates[ch][i]) * bCoeffs[i];
                b[i] = (i == 0) ? lpStates[ch][0] : lpStates[ch][i] - lpStates[ch][i-1];
            }
            b[9] = currentIn - lpStates[ch][8];

            // 3. Level Detection and Spectral Subtraction
            for (int i = 0; i < 10; ++i) {
                float r;
                if (isRMS) {
                    levelStates[ch][i] += (b[i] * b[i] - levelStates[ch][i]) * rmsF;
                    r = std::sqrt (std::max (0.0f, levelStates[ch][i]));
                } else {
                    // Dual-rate peak follower: fast attack (2ms), steady decay (60ms)
                    // This bridges the "bumps" in transient noise (traffic) to prevent flutter
                    float absVal = std::abs (b[i]);
                    float coeff = (absVal > levelStates[ch][i]) ? levelAtt : levelRel;
                    levelStates[ch][i] += (absVal - levelStates[ch][i]) * coeff;
                    r = levelStates[ch][i];
                }

                if (learnActive.load()) {
                    float currentPeak = peakLevels[i].load();
                    if (r > currentPeak) peakLevels[i].store (r);
                }

                // Aggressive Subtractive Gain Calculation (Pure Phase Cancellation Concept)
                // gain = (signal - noise) / signal = 1 - noise/signal
                float targetG = 1.0f - (tLinear[i] / (r + 1e-9f));
                targetG = std::max (floorV, std::min (1.0f, targetG));

                // Adaptive Release: Stabilize the tail without reducing suppression depth
                float currentCoeff;
                if (targetG > currentGains[ch][i]) {
                    currentCoeff = attCoeff; // Fast Attack
                } else {
                    // Slow down release as we approach the noise floor to prevent "chirping" artifacts
                    float tailDist = (currentGains[ch][i] - floorV) / (1.01f - floorV);
                    float tailStab = 1.0f + (1.0f - std::min(1.0f, tailDist)) * 8.0f; // up to 9x slower at tail
                    currentCoeff = 1.0f - std::exp (-1.0f / (std::max(10.0f, release->load() * tailStab) * (float)sRate / 1000.0f));
                }
                currentGains[ch][i] += (targetG - currentGains[ch][i]) * currentCoeff;
            }

            // 4. Reconstruction via Phase Cancellation
            // We explicitly calculate the noise component (below threshold) and cancel it.
            outSum = 0;
            for (int i = 0; i < 10; ++i) {
                float noiseComponent = b[i] * (1.0f - currentGains[ch][i]);
                outSum += b[i] - noiseComponent; // Subtractive Phase Cancellation
            }

            data[s] = isListenNoise ? (s0 - outSum) : outSum;
        }

        // 5. Update Atomically for UI (Channel 0 for visualization)
        if (ch == 0) {
            for (int i = 0; i < 10; ++i) {
                lastLevels[i].store (levelStates[0][i]);
                lastReductions[i].store (currentGains[0][i]);
            }
        }
    }

    // Handle Learn finish
    if (learnActive.load()) {
        learnElapsed.store (learnElapsed.load() + (double)numSamples / sRate);
        if (learnElapsed.load() >= parameters.getRawParameterValue ("learnTime")->load()) { 

            learnActive.store (false);
            learnJustFinished.store (true); // Notify UI to update parameters
        }
    }
}


void CyberDenoiserAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CyberDenoiserAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* CyberDenoiserAudioProcessor::createEditor()
{
    return new CyberDenoiserAudioProcessorEditor (*this);
}

// JUCE Entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() 
{ 
    return new CyberDenoiserAudioProcessor(); 
}


