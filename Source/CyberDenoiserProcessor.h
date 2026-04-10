#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#if JUCE_MAC
  #include <Accelerate/Accelerate.h>
#endif
#include "ChipDetector.h"

/**
 * @class CyberDenoiserAudioProcessor
 * @brief Port of the Cyber-Denoiser PRO JSFX to C++ for the Audio Unit format.
 * 
 * Optimized for Apple Silicon (M1-M5) using Accelerate.framework (vDSP),
 * 128-byte alignment, and runtime chip detection for peak forensic denoising.
 */
class CyberDenoiserAudioProcessor : public juce::AudioProcessor
{
public:
    CyberDenoiserAudioProcessor();
    ~CyberDenoiserAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cyber-Denoiser PRO"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Added for Learn Mode and Visualization ---
    float getReduction (int band) const { return lastReductions[band]; }
    float getLevel (int band) const { return lastLevels[band]; }
    void startLearn() { 
        learnActive.store (true); 
        learnElapsed.store (0.0); 
        for(auto& p : peakLevels) p.store (1e-6f); 
    }
    bool isLearning() const { return learnActive.load(); }
    float getLearnProgress() const { return (float)(learnElapsed.load() / parameters.getRawParameterValue ("learnTime")->load()); }
    juce::AudioProcessorValueTreeState& getVTS() { return parameters; }
    
    std::atomic<bool> learnJustFinished { false };
    std::atomic<float> peakLevels[10]; // Now atomic for safe UI reading

    
private:
    // ... Parameters ...
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* thresholds[10];
    std::atomic<float>* attack;
    std::atomic<float>* release;
    std::atomic<float>* floor;
    std::atomic<float>* gradient;
    std::atomic<float>* rmsMode;
    std::atomic<float>* lowCut;
    std::atomic<float>* highCut;
    std::atomic<float>* listenNoise;
    std::atomic<float>* hysteresis;
    std::atomic<float>* masterThreshold;

    // Filter states (10 bands + low-cut + high-cut)
    // Using 128-byte aligned memory for M-series cache efficiency
    // Layout changed to [Channel][Band] for contiguous vDSP access
    float lpStates[2][9]     __attribute__((aligned(128))); 
    float hpCutStat[2]       __attribute__((aligned(128)));   
    float lpCutStat[2]       __attribute__((aligned(128)));   
    
    // Level detection [Channel][Band]
    float levelStates[2][10] __attribute__((aligned(128)));
    float targetGains[2][10] __attribute__((aligned(128)));
    float currentGains[2][10] __attribute__((aligned(128)));
    int hystStates[2][10]    __attribute__((aligned(128)));

    // For visualization and local sync (thread-safe for UI)
    std::atomic<float> lastLevels[10];
    std::atomic<float> lastReductions[10];
    std::atomic<bool> learnActive { false };
    std::atomic<double> learnElapsed { 0.0 }; // Now atomic
    float learnDuration = 20.0;


    // Master Threshold sync helper
    float prevMaster = 0.0f;

    void updateParams();
    float calcGhostG (float rIn, float tDb, float hDb, float gradient, float floorLevel, int& st);

    // Diagnostics
    std::unique_ptr<juce::FileLogger> fileLogger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyberDenoiserAudioProcessor)
};
