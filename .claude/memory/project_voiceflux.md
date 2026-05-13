---
name: project-voiceflux
description: VoiceFlux Android voice changer project — architecture, tech stack, and build notes
metadata:
  type: project
---

Full Android voice changer app built at /Users/ademic/Downloads/VoiceFlux.

**Why:** User requested a real-time, DSP-only (no AI/cloud) voice changer app for Android.

**Tech stack:**
- Language: Kotlin (UI/JNI) + C++17 (DSP)
- Audio: Oboe 1.8.0 (via Prefab AAR) — full-duplex, EXCLUSIVE/LowLatency mode
- DSP: Custom phase vocoder (pitch), spectral re-sampling (formant), Schroeder reverb, biquad EQ, tanh distortion, ring modulation — all in C++
- FFT: Cooley-Tukey implemented in fft_util.h (no external dependencies)
- UI: Material3, ViewBinding, MVVM (LiveData + ViewModel), custom WaveformView (Canvas)
- Persistence: SharedPreferences + Gson for preset storage

**Key files:**
- `app/src/main/cpp/` — all DSP engine (C++)
- `app/src/main/cpp/jni_bridge.cpp` — JNI surface; functions named `Java_com_voiceflux_app_AudioEngine_native*`
- `app/src/main/java/com/voiceflux/app/AudioEngine.kt` — Kotlin JNI bridge
- `app/src/main/java/com/voiceflux/app/ui/MainViewModel.kt` — engine lifecycle + polling

**Build notes:**
- minSdk 27 (AAudio requirement)
- `buildFeatures { prefab = true }` required for Oboe AAR
- `ANDROID_STL=c++_shared` in CMake args
- ABI: arm64-v8a + x86_64
- No gradlew scripts — user must run `gradle wrapper` or use Android Studio

**How to apply:** Reference this for any future feature additions, latency tuning, or preset changes.
