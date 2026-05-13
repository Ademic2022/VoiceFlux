# VoiceFlux — Real-Time Android Voice Changer

A production-ready, low-latency Android voice changer powered entirely by on-device DSP. No AI, no cloud, no internet required.

---

## Architecture

```
Microphone → [Oboe Input Stream] → AudioFifo (lock-free ring buffer)
                                         ↓
              [Oboe Output Stream callback] → VoiceProcessor DSP chain → Speaker/Earphone

VoiceProcessor chain (in order):
  1. PitchShifter    — Phase Vocoder (FFT-based)
  2. FormantShifter  — Spectral envelope re-sampling
  3. RobotEffect     — Ring modulation (for Robot preset)
  4. Distortion      — Tanh / hard-clip waveshaping
  5. ThreeBandEQ     — Low-shelf + mid-peak + high-shelf biquad
  6. Reverb          — Schroeder (4 comb + 2 all-pass filters)
  7. Echo            — Single-tap delay line (~250 ms)
  8. Gain + Limiter  — Scaled output with tanh soft-limiter
```

### Why processing happens in the output callback

Oboe's output callback is driven by the hardware DAC timer — it fires at exact intervals with the highest OS priority. By pulling from the input FIFO inside the output callback, all DSP stays on one real-time thread, avoiding any priority inversion or cross-thread synchronisation.

---

## DSP Algorithms

### Phase Vocoder Pitch Shifter

The pitch shifter uses an **overlap-add phase vocoder**:

1. Collect input in a circular FIFO; trigger processing every `kHopSize = 512` samples.
2. Apply a **Hann window** to the latest 2048-sample frame.
3. **FFT** (Cooley-Tukey, in-place, complex float).
4. For each bin `k`, compute the **instantaneous frequency** from the phase difference between successive frames:
   ```
   Δφ = phase[k] - prevPhase[k] - k·2π·HOP/N
   Δφ = wrap(Δφ)   // to (-π, π]
   trueFreq[k] = k·2π/N + Δφ/HOP
   ```
5. **Bin mapping**: map analysis bin `k` → synthesis bin `round(k * pitchFactor)`.
6. Accumulate synthesis **phases** using the synthesis hop `= HOP × pitchFactor`.
7. **IFFT** + Hann-weighted **overlap-add** into the output accumulation buffer.
8. Push `synthHop` samples to the output queue each frame.

Quality/latency tradeoff:
- Frame 2048 / Hop 512 → ~42 ms algorithmic latency at 48 kHz
- 4× overlap → smooth reconstruction, good phase coherence

### Formant Shifter

Formant shifting moves the **vocal tract resonances** (F1, F2, F3) while preserving the fundamental pitch. This is implemented via **spectral envelope re-sampling**:

1. FFT the input frame.
2. For each output bin `k`, source bin = `k / formantFactor` (linear interpolation).
3. Keep the original phases; replace magnitudes with the re-sampled envelope.
4. IFFT + overlap-add.

This moves formants without changing pitch — the complement to pitch shifting.

### Schroeder Reverb

4 parallel **comb filters** feed 2 series **all-pass filters**:
- Comb delays: 1557, 1617, 1491, 1422 samples @ 44.1 kHz (scaled to 48 kHz)
- Comb feedback: 0.70–0.93 (controlled by Room Size slider)
- All-pass delays: 225, 556 samples; feedback 0.5

### Biquad Filters (EQ)

Standard **Audio EQ Cookbook** biquad coefficients for:
- Low-shelf (200 Hz), mid-peak (2 kHz), high-shelf (8 kHz)
- Direct form II transposed, two delay elements per filter
- Stable up to Nyquist; no instability at extreme gain settings

### Robot Effect

**Ring modulation**: `y = x · sin(2π · f_carrier · t)`, `f_carrier = 50 Hz`

Multiplying the signal by a sine wave creates sidebands at `f ± f_carrier` for every frequency component, producing the characteristic metallic/robotic timbre.

### Distortion

**Soft-clip (tanh waveshaping)**:
```
y = tanh(drive · x) / tanh(drive)
```
Drive is mapped from the slider (0→1) to (1→50). The `tanh` normalisation keeps peak output at 1.0.

---

## Voice Presets

| Preset | Pitch | Formant | Reverb | Distortion | Echo | Effect |
|--------|-------|---------|--------|------------|------|--------|
| Original | 0 | 0 | 0% | 0% | 0% | — |
| Child | +8st | +5st | 10% | 0% | 5% | — |
| Deep Male | −5st | −4st | 20% | 0% | 0% | — |
| Female | +5st | +4st | 15% | 0% | 0% | — |
| Robot | 0 | 0 | 25% | 20% | 15% | Ring mod |
| Monster | −8st | −6st | 40% | 45% | 30% | — |
| Grandma | +3st | +2st | 30% | 8% | 20% | — |
| Radio | 0 | 0 | 5% | 35% | 8% | Bandpass EQ |

---

## Setup Instructions

### Prerequisites

- Android Studio Hedgehog (2023.1.1) or newer
- NDK 25 or newer (install via SDK Manager → SDK Tools → NDK)
- CMake 3.22.1 or newer (install via SDK Manager → SDK Tools → CMake)
- Android device or emulator with API 27+ (Oreo MR1)
- A physical device is **strongly recommended** — emulators have poor audio latency

### Steps

1. **Clone / open the project**
   ```
   File → Open → select the VoiceFlux/ folder
   ```

2. **Sync Gradle**
   Android Studio will prompt to sync. Click "Sync Now". This downloads:
   - Oboe 1.8.0 AAR (via Prefab — no source download needed)
   - All other Kotlin dependencies

3. **Connect NDK**
   If the NDK is not auto-detected:
   ```
   File → Project Structure → SDK Location → Android NDK location
   ```
   Point it to your NDK install (usually `~/Library/Android/sdk/ndk/<version>`).

4. **Build**
   - `Build → Make Project` (Ctrl+F9)
   - First build compiles C++ — allow 2–4 minutes

5. **Install on device**
   - Enable Developer Mode + USB Debugging on your Android device
   - `Run → Run 'app'` (Shift+F10)

6. **Grant microphone permission**
   The app requests it on first tap of the Start button.

7. **For wired earphones (best latency)**
   Plug in before launching the app. Android's `VoiceCommunication` input preset and `LowLatency` output mode are selected automatically.

### Generating the Gradle wrapper scripts

If `gradlew` / `gradlew.bat` are missing:
```bash
cd VoiceFlux
gradle wrapper
```
Or open the project in Android Studio — it will offer to generate them automatically.

---

## Performance & Latency

### Recommended settings for minimum latency

| Setting | Value |
|---------|-------|
| Sample rate | 48 000 Hz (let Oboe choose; it matches hardware) |
| Buffer size | 2× input burst (set in `AudioEngine::openStreams`) |
| Sharing mode | EXCLUSIVE |
| Performance mode | LOW_LATENCY |
| Input preset | VOICE_COMMUNICATION |

### Expected latency breakdown

| Stage | Time |
|-------|------|
| Hardware I/O buffers | 5–20 ms |
| Phase vocoder frame | ~42 ms (1 frame at 48 kHz) |
| Formant shifter frame | ~42 ms |
| All other effects | < 1 ms |
| **Total** | **~50–100 ms** |

Latency is shown live in the app's top-right chip.

### Tips for lower latency

1. **Use a wired headset.** Bluetooth adds 100–200 ms of its own latency (codec buffering). With wired audio, round-trip latency is typically 50–80 ms.
2. **Reduce the FFT frame size** to 1024 (edit `kFrameSize` in `pitch_shifter.h`). Quality decreases slightly but latency drops to ~21 ms per frame.
3. **Disable pitch/formant shifting** when not needed (sliders at 0). The code skips both phase vocoder stages entirely.
4. **Use a device with AAudio support** (Android 8.0+, API 26+). The app enforces minSdk 27 for this reason.

---

## Common Pitfalls

| Problem | Cause | Fix |
|---------|-------|-----|
| Audio crackling | Buffer underrun | Increase output buffer size; reduce DSP load |
| Crackling on pitch change | Phase discontinuity | Smooth pitchFactor changes over several frames |
| No audio on emulator | Poor emulator audio | Use a physical device |
| High latency on BT | Bluetooth codec | Switch to wired headphones |
| App crashes on start | Missing RECORD_AUDIO | Ensure permission is granted |
| Silent output | Input stream failed to open | Check logcat for Oboe error codes |

---

## Project Structure

```
VoiceFlux/
├── app/src/main/
│   ├── cpp/
│   │   ├── CMakeLists.txt          — CMake build for the .so
│   │   ├── fft_util.h              — Cooley-Tukey FFT + Hann window
│   │   ├── audio_engine.{h,cpp}    — Oboe full-duplex engine + FIFO
│   │   ├── voice_processor.{h,cpp} — DSP chain, preset application
│   │   ├── jni_bridge.cpp          — JNI ↔ Kotlin surface
│   │   └── effects/
│   │       ├── pitch_shifter       — Phase vocoder
│   │       ├── formant_shifter     — Spectral envelope re-sampling
│   │       ├── reverb              — Schroeder reverb
│   │       ├── distortion          — Waveshaping
│   │       ├── eq_filter           — Biquad EQ
│   │       └── robot_effect        — Ring modulation
│   ├── java/com/voiceflux/app/
│   │   ├── AudioEngine.kt          — JNI bridge (Kotlin)
│   │   ├── MainActivity.kt         — UI entry point
│   │   ├── AudioService.kt         — Foreground service (background mode)
│   │   ├── data/
│   │   │   ├── VoicePreset.kt      — Data model + default presets
│   │   │   └── PresetRepository.kt — SharedPreferences persistence
│   │   └── ui/
│   │       ├── MainViewModel.kt    — State + engine lifecycle
│   │       ├── PresetAdapter.kt    — RecyclerView adapter
│   │       └── WaveformView.kt     — Custom Canvas waveform visualiser
│   └── res/                        — Layouts, colors, strings, drawables
└── build.gradle.kts                — Kotlin DSL build scripts
```

---

## Future Expansion

- **Phase 3**: The `AudioService` foreground service is already wired up. Overlay controls can be added via `WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY`.
- **Phase 4**: System-wide audio routing is restricted on Android (requires MODIFY_AUDIO_SETTINGS + a VirtualAudioDevice or VoIP hack). The most practical approach is to use the `Accessibility Service` API combined with `AudioPlaybackCapture` (API 29+) for app-specific capture.
- **Better pitch quality**: Replace the simple bin-mapping with a **phase-locked vocoder** or integrate **Rubber Band Library** (LGPL) for best quality at the cost of higher latency.
- **MIDI control**: Wire pitch slider to MIDI input for real-time control during live performance.
- **Preset cloud sync**: Add a Room database + optional backend to share custom presets.
