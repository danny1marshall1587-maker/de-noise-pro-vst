#include "CyberDenoiserEditor.h"
#include "ChipDetector.h"

CyberDenoiserAudioProcessorEditor::CyberDenoiserAudioProcessorEditor (CyberDenoiserAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Configure Sliders
    for (int i = 0; i < 10; ++i)
    {
        thresholdSliders[i].setSliderStyle (juce::Slider::LinearVertical);
        thresholdSliders[i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (thresholdSliders[i]);
        thresholdAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.getVTS(), "t" + juce::String (i + 1), thresholdSliders[i]);
        
        levelsSnapshot[i] = 0.0f;
        reductionsSnapshot[i] = 1.0f;
    }

    masterSlider.setSliderStyle (juce::Slider::LinearVertical);
    masterSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (masterSlider);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getVTS(), "master", masterSlider);

    learnButton.setToggleable (true);
    learnButton.onClick = [this] {
        processor.startLearn();
    };
    addAndMakeVisible (learnButton);

    setSize (880, 700);
    juce::Logger::writeToLog ("Editor Created");
    startTimerHz (60);
}

CyberDenoiserAudioProcessorEditor::~CyberDenoiserAudioProcessorEditor()
{
    isShuttingDown.store (true);
    stopTimer();

    // Explicitly reset attachments to ensure they stop hitting the host immediately
    masterAttachment.reset();
    for (auto& att : thresholdAttachments) att.reset();
}


void CyberDenoiserAudioProcessorEditor::timerCallback()
{
    if (isShuttingDown.load()) return;

    // 1. Decoupled Snapshotting (Take data from processor once)
    for (int i = 0; i < 10; ++i)
    {
        levelsSnapshot[i] = processor.getLevel (i);
        reductionsSnapshot[i] = processor.getReduction (i);
        thresholdsSnapshot[i] = processor.getVTS().getRawParameterValue ("t" + juce::String (i + 1))->load();
    }

    if (isShuttingDown.load()) return;

    // 2. Handle Learn Results
    if (processor.learnJustFinished.exchange (false))
    {
        for (int i = 0; i < 10; ++i)
        {
            float peak = processor.peakLevels[i].load();
            float db = 20.0f * std::log10 (peak + 1e-6f) + 3.0f;
            
            auto* param = processor.getVTS().getParameter ("t" + juce::String (i + 1));
            float newValue = juce::jlimit (0.0f, 1.0f, (db + 100.0f) / 100.0f);
            
            // Only update if changed significantly to avoid host notification spam
            if (std::abs (param->getValue() - newValue) > 0.001f)
                param->setValueNotifyingHost (newValue);
        }
    }

    if (isShuttingDown.load()) return;

    // 3. Handle Master Threshold Sync (using class member prevMaster)
    float currentMaster = processor.getVTS().getRawParameterValue ("master")->load();
    if (std::abs (currentMaster - prevMaster) > 0.05f) // Increased deadzone to prevent jitter
    {
        float diff = currentMaster - prevMaster;
        for (int i = 0; i < 10; ++i)
        {
            auto* param = processor.getVTS().getParameter ("t" + juce::String (i + 1));
            float t = processor.getVTS().getRawParameterValue ("t" + juce::String (i + 1))->load();
            float newValue = juce::jlimit (0.0f, 1.0f, (t + diff + 100.0f) / 100.0f);
            
            if (std::abs (param->getValue() - newValue) > 0.001f)
                param->setValueNotifyingHost (newValue);
        }
        prevMaster = currentMaster;
    }

    // UI Updates
    if (processor.isLearning())
    {
        float progress = processor.getLearnProgress();
        float total = processor.getVTS().getRawParameterValue ("learnTime")->load();
        int remaining = (int)std::ceil (total * (1.0f - juce::jlimit(0.0f, 1.0f, progress)));
        
        if (learnButton.getButtonText() != "LEARNING " + juce::String (remaining) + "s")
            learnButton.setButtonText ("LEARNING " + juce::String (remaining) + "s");
        
        learnButton.setToggleState (true, juce::dontSendNotification);
    }
    else
    {
        if (learnButton.getButtonText() != "LEARN")
            learnButton.setButtonText ("LEARN");
        learnButton.setToggleState (false, juce::dontSendNotification);
    }
    
    if (!isShuttingDown.load())
        repaint();
}


void CyberDenoiserAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    double time = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;

    // 1. Deep Silk Base (Image-matched Purple)
    g.setColour (juce::Colour (0xff2a1640)); // Deep Purple from image
    g.fillAll();

    // 2. Procedural "Liquid Silk" Animated Folds
    auto drawSilkFold = [&](float phaseOffset, float freq, float amp, juce::Colour col, float yBase) {
        juce::Path p;
        juce::Path shadowP;
        float w = area.getWidth();
        float h = area.getHeight();
        
        // Shadow path (slightly lower and offset)
        shadowP.startNewSubPath (0, h);
        p.startNewSubPath (0, h); 
        
        const float shadowOffset = 15.0f;
        
        for (float x = 0; x <= w + 10.0f; x += 10.0f)
        {
            // Compound wave logic (Lava Lamp / Organic Liquid feel)
            float t = (float)time;
            float wave = std::sin (t * 0.5f + x * freq + phaseOffset) * amp 
                       + std::sin (t * 0.82f - x * freq * 1.8f + phaseOffset * 0.5f) * (amp * 0.4f)
                       + std::sin (t * 1.3f + x * freq * 0.4f) * (amp * 0.2f);

            float y = yBase - (x * 0.55f) + wave; // Diagonal slope (~30 deg)
            p.lineTo (x, y);
            shadowP.lineTo (x, y + shadowOffset);
        }
        
        p.lineTo (w, h); p.closeSubPath();
        shadowP.lineTo (w, h); shadowP.closeSubPath();

        // 1. Draw Depth Shadow
        g.setColour (juce::Colours::black.withAlpha (0.15f));
        g.fillPath (shadowP);

        // 2. Draw Main Silk Layer with Highlight Sheen
        juce::Colour sheenCol = col.brighter (0.15f);
        juce::ColourGradient grad (col.darker (0.3f), 0, h, sheenCol, w * 0.4f, yBase * 0.4f, false);
        grad.addColour (0.5, col);
        grad.addColour (0.8, sheenCol.withAlpha (0.8f));
        
        g.setGradientFill (grad);
        g.fillPath (p);
    };

    // Draw multiple overlapping layers of liquid silk
    drawSilkFold (0.0f, 0.004f, 22.0f, juce::Colour (0xff2b1640), area.getHeight() * 0.95f);
    drawSilkFold (2.5f, 0.006f, 28.0f, juce::Colour (0xff3d255c), area.getHeight() * 0.75f);
    drawSilkFold (5.1f, 0.003f, 18.0f, juce::Colour (0xff553a80), area.getHeight() * 0.55f);
    drawSilkFold (1.8f, 0.005f, 12.0f, juce::Colour (0xff7a5aa0), area.getHeight() * 0.35f);

    // 3. Ultra-Fine Fabric Grain (Velvet texture feel)
    g.setColour (juce::Colours::white.withAlpha (0.008f));
    for (float x = 0; x < area.getWidth(); x += 3.0f)
        g.drawVerticalLine ((int)x, 0, area.getHeight());

    // 4. Glossy Overlay (Glass top)
    juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.04f), 0, 0, 
                               juce::Colours::transparentWhite, 0, area.getHeight() * 0.4f, false);
    g.setGradientFill (gloss);
    g.fillRect (area.withHeight (area.getHeight() * 0.4f));

    // 5. Meters and Bands (Centered and Clean)
    float centerX = area.getWidth() * 0.5f;
    float totalW = area.getWidth() * 0.85f;
    float startX = centerX - (totalW * 0.5f);
    float mW = (totalW - 60.0f) / 10.0f;
    float vH = area.getHeight() * 0.45f;
    float pad = mW * 0.25f;

    for (int i = 0; i < 10; ++i)
    {
        float level = levelsSnapshot[i];
        float reduction = reductionsSnapshot[i];
        float levelDb = 20.0f * std::log10 (level + 1e-6f);
        float thresholdDb = thresholdsSnapshot[i];

        // Clean UI: Only show activity if above threshold
        if (levelDb > thresholdDb)
        {
            float hM = ((levelDb + 100.0f) / 100.0f) * vH;
            float dH = (1.0f - reduction) * hM; 

            // Cyan-to-Magenta Band coloring
            juce::Colour bandCol = juce::Colour::fromHSV (0.5f + (float)i * 0.04f, 0.8f, 0.9f, 0.6f);
            
            // Signal bar
            g.setColour (bandCol);
            g.fillRoundedRectangle (startX + i * mW + pad, vH + 50 - hM + dH, mW - (pad * 2.0f), hM - dH, 2.0f);

            // Reduction overlay (Red)
            g.setColour (juce::Colours::red.withAlpha (0.3f));
            g.fillRect (startX + i * mW + pad, vH + 50 - hM, mW - (pad * 2.0f), dH);
        }

        const char* labels[] = {"60", "150", "400", "800", "1.5k", "3k", "5k", "8k", "12k", "16k"};
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (12.0f);
        g.drawText (labels[i], startX + i * mW, vH + 65, mW, 20.0f, juce::Justification::centred);
    }

    // Master Label
    g.setColour (juce::Colours::white);
    g.drawText ("MASTER", startX + (totalW - 40), vH + 65, 60.0f, 20.0f, juce::Justification::centred);

    // Build/Opt Status
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.setFont (11.0f);
    g.drawText (ChipDetector::getOptimizationStatus() + " | Forensic Core v2.1", 
                area.removeFromBottom (30).removeFromRight (300).translated (-20, -10), 
                juce::Justification::bottomRight, true);
}

void CyberDenoiserAudioProcessorEditor::resized()
{
    if (isShuttingDown) return;
    auto area = getLocalBounds();

    float totalW = area.getWidth() * 0.85f;
    float startX = (area.getWidth() - totalW) * 0.5f;
    float mW = (totalW - 60.0f) / 10.0f;
    float vH = area.getHeight() * 0.45f;
    
    float sliderY = 50;

    for (int i = 0; i < 10; ++i)
    {
        thresholdSliders[i].setBounds (startX + i * mW, (int)sliderY, (int)mW, (int)vH);
    }

    masterSlider.setBounds (startX + (int)totalW - 40, (int)sliderY, 30, (int)vH);

    // Learn button centered at bottom
    learnButton.setBounds (area.getCentreX() - 100, area.getHeight() - 100, 200, 60);
}
