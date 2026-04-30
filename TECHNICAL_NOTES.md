# Cyber-Denoiser PRO: Technical Deep Dive

This document captures the DSP and UI logic to assist future maintenance and AI-driven enhancements.

## 🎵 DSP Core (CyberDenoiserProcessor.cpp)

### 10-Band Reconstruction
Unlike an FFT-based denoiser, this plugin uses **Parallel 1st-Order Filters**. 
- **Bands**: 9 low-pass filters create 10 frequency slots (Low, High, and 8 band-pass slots in between).
- **Subtraction**: Each band `i` is derived by subtracting the previous LP state from the current LP state (`lp[i] - lp[i-1]`). This ensures perfect phase reconstruction at zero latency. 
- **Frequency Map**: 60Hz, 150Hz, 400Hz, 800Hz, 1.5k, 3k, 5k, 8k, 12k, 16k.

### Lookahead & Ghost Gains
The plugin maintains **Zero Latency** (important for Live Monitoring). 
- Dynamic gains are calculated based on the signal and the thresholds.
- **RMS Mode**: Optional smoothing filter for levels.
- **Hysteresis**: Prevents "chatter" at the gate thresholds by requiring a wider swing to change the state.

### Channel Hardening
- **Critical Fix**: The engine is strictly hard-coded for a stereo footprint (`2 channels`). 
- **Stability**: All processing loops are capped at `std::min(2, numChannels)` to prevent memory overruns if the host (e.g., Logic Pro) provides wider-than-expected buffers in Live Monitoring.

## 🎨 UI & Visualization (CyberDenoiserEditor.cpp)

### Silk-Wave Graphics
The GUI background is **procedural** rather than image-based.
- **Gradients**: Uses `juce::ColourGradient` with diagonal soft-white "folds" to simulate the silk fabric provided by the user.
- **Optimization**: To avoid CPU spikes, the texture is drawn directly as simple vectors. 
- **Metering**: The editor takes a **decoupled snapshot** of the engine's levels in `timerCallback` instead of reading directly from the audio thread.

### Teardown Safety
A persistent `isShuttingDown` flag in the Editor ensures that `timerCallback` and `resized` events are aborted as soon as the Host begins tearing down the plugin. This prevents Logic Pro from crashing due to asynchronous UI callbacks hitting a partially destroyed engine.

## 🛠 Compilation Quirks
- **CMake**: JUCE 8.0.12 is fetched automatically but must be built with standard system toolchains. Ensure `/Users/dan/.gemini/antigravity/scratch/homebrew/bin/cmake` is used as the primary binary for all generation tasks.
- **Optimization**: Binary is built with `-mcpu=apple-m2 -O3`. 

---
*Technical Notes Updated: 2026-04-08*
