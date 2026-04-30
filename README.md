# Cyber-Denoiser PRO (Audio Unit)

Premium 10-band forensic denoiser for macOS, optimized for Apple Silicon (M2). Ported from the original JSFX version and hardened for professional Logic Pro stability.

## 🚀 Key Features
- **10-Band Parallel Filter Bank**: Uses 1st-order filters for zero-latency, forensic-grade noise reduction.
- **Learn Mode**: Automatically samples environment noise and sets band thresholds.
- **Silk-Wave GUI**: A procedural, high-performance software-rendered interface (no OpenGL required).
- **M2 Optimized**: Custom build flags for Apple Silicon performance.

## 🛠 Architecture & Tech Stack
- **Framework**: JUCE 8.0.12 (standard C++).
- **Format**: AU (Audio Unit v2).
- **Core Engine**: `Source/CyberDenoiserProcessor.cpp` handles the DSP loops.
- **User Interface**: `Source/CyberDenoiserEditor.cpp` handles the "Silk-Wave" graphics and metering.
- **Build System**: CMake (requires the custom path: `/Users/dan/.gemini/antigravity/scratch/homebrew/bin/cmake`).

## ⚠️ Stability & Performance Notes (AI READ THIS)
This codebase includes several critical "Hardening" measures implemented for Logic Pro compatibility:
1. **Channel Hardening**: The DSP engine is strictly limited to 2 channels. If the host provides more, the plugin caps processing at 2 to avoid memory overruns during **Live Monitoring**.
2. **Software Rendering**: All OpenGL code has been removed to ensure stable window closure.
3. **Asynchronous Parameter Sync**: Real-time safe—parameter updates from "Learn" results are queued and processed on the Message Thread, not the High-Priority Audio Thread.
4. **Optimized Atomics**: UI metering updates only happen once per block to minimize thread overhead.

## 🔨 How to Build
To build and install the AU component:
```bash
cd build
/Users/dan/.gemini/antigravity/scratch/homebrew/bin/cmake ..
make -j8
cp -R "CyberDenoiserPro_artefacts/AU/Cyber-Denoiser PRO.component" ~/Library/Audio/Plug-Ins/Components/
```

## 🧪 Diagnostics
- Logs are written to `~/Documents/CyberDenoiser_Log.txt`.
- Includes "Heartbeat" traces for the Editor and Processor to track teardown crashes.
