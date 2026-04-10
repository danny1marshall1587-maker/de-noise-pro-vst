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

float CyberDenoiserAudioProcessor::calcGhostG (float rIn, float tDb, float hDb, float grad, float floorL, int& st)
{
    float rDb = 20.0f * std::log10 (rIn + 1e-6f);
    if (rDb > tDb) st = 1;
    else if (rDb < (tDb - hDb)) st = 0;

    if (st == 1) return 1.0f;
    if (rDb <= (tDb - grad)) return floorL;
    return floorL + (1.0f - floorL) * ((rDb - (tDb - grad)) / grad);
}

void CyberDenoiserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sRate = getSampleRate();

    // Master Threshold sync removed from audio thread to avoid setValueNotifyingHost
    // This will be handled by the Editor on the Message Thread

    // Smoothers constants
    float attF = std::exp (-1.0f / (attack->load() * (float)sRate / 1000.0f));
    float relVal = release->load();
    float relF = (relVal == 0.0f) ? 0.0f : std::exp (-1.0f / (relVal * (float)sRate / 1000.0f));
    float smF = 1.0f - std::exp (-1.0f / (15.0f * (float)sRate / 1000.0f));
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
    float grad = gradient->load();
    float floorV = floor->load();
    float hys = hysteresis->load();
    bool isLowCut = lowCut->load();
    bool isHighCut = highCut->load();
    bool isListenNoise = listenNoise->load();

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

            // 3. Level Detection and Gain Update
            for (int i = 0; i < 10; ++i) {
                float r;
                if (isRMS) {
                    levelStates[ch][i] += (b[i] * b[i] - levelStates[ch][i]) * rmsF;
                    r = std::sqrt (std::max (0.0f, levelStates[ch][i]));
                } else {
                    levelStates[ch][i] = levelStates[ch][i] * relF + std::abs (b[i]) * (1.0f - relF);
                    r = levelStates[ch][i];
                }

                if (learnActive.load()) {
                    float currentPeak = peakLevels[i].load();
                    if (r > currentPeak) peakLevels[i].store (r);
                }

                float tg = calcGhostG (r, thresholds[i]->load(), hys, grad, floorV, hystStates[ch][i]);
                targetGains[ch][i] = targetGains[ch][i] * attF + tg * (1.0f - attF);
                currentGains[ch][i] += (targetGains[ch][i] - currentGains[ch][i]) * smF;
            }

            // 4. Reconstruction (Cross-Platform)
            float* bandPointer = b;
            float* gainPointer = currentGains[ch];
            
#if JUCE_MAC
            // High-speed M-series hardware acceleration
            vDSP_dotpr (bandPointer, 1, gainPointer, 1, &outSum, 10);
#else
            // Standard high-precision fallback for Windows/Linux
            for (int i = 0; i < 10; ++i)
                outSum += bandPointer[i] * gainPointer[i];
#endif

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


