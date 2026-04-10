#pragma once

#include <juce_core/juce_core.h>
#if JUCE_MAC
  #include <sys/types.h>
  #include <sys/sysctl.h>
#endif
#include <string>

/**
 * Utility to detect the specific Apple Silicon chip model at runtime.
 */
class ChipDetector {
public:
    static juce::String getChipName() {
#if JUCE_MAC
        char buffer[256];
        size_t size = sizeof(buffer);
        if (sysctlbyname("machdep.cpu.brand_string", &buffer, &size, NULL, 0) == 0) {
            juce::String model(buffer);
            return model.replace("Apple ", ""); // Return "M1", "M2 Pro", etc.
        }
#endif
        return "Unknown";
    }

    enum class ChipSeries {
        M1,
        M2,
        M3,
        M4,
        M5,
        Generic
    };

    static ChipSeries getSeries() {
        juce::String name = getChipName();
        if (name.contains("M5")) return ChipSeries::M5;
        if (name.contains("M4")) return ChipSeries::M4;
        if (name.contains("M3")) return ChipSeries::M3;
        if (name.contains("M2")) return ChipSeries::M2;
        if (name.contains("M1")) return ChipSeries::M1;
        return ChipSeries::Generic;
    }

    static bool isHighEnd() {
        juce::String name = getChipName();
        return name.contains("Pro") || name.contains("Max") || name.contains("Ultra");
    }

    static juce::String getOptimizationStatus() {
        return "Optimized for " + getChipName();
    }
};
