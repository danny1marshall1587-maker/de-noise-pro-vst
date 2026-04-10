#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_core/juce_core.h>
#include "CyberDenoiserProcessor.h"

/**
 * @class CyberDenoiserAudioProcessorEditor
 * @brief Custom GUI for Cyber-Denoiser PRO with GLSL Silk Background.
 */
class CyberDenoiserAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::Timer
{
public:
    CyberDenoiserAudioProcessorEditor (CyberDenoiserAudioProcessor&);
    ~CyberDenoiserAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    CyberDenoiserAudioProcessor& processor;

    // Attachments for sliders
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachments[10];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    
    // Sliders & Buttons
    juce::Slider thresholdSliders[10];
    juce::Slider masterSlider;
    juce::TextButton learnButton { "LEARN" };

    // UI Snapshots (Decoupled from Engine)
    float levelsSnapshot[10];
    float reductionsSnapshot[10];
    float thresholdsSnapshot[10];

    // Animation & Sub-components
    double startTime = juce::Time::getMillisecondCounterHiRes();
    float prevMaster = -50.0f;
    std::atomic<bool> isShuttingDown { false };
    juce::uint32 lastLogTime = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyberDenoiserAudioProcessorEditor)
};
