#pragma once
#include "effects/pitch_shifter.h"
#include "effects/formant_shifter.h"
#include "effects/reverb.h"
#include "effects/distortion.h"
#include "effects/eq_filter.h"
#include "effects/robot_effect.h"
#include <atomic>

namespace voiceflux {

// Parameters exposed to Kotlin via JNI.
struct ProcessorParams {
    std::atomic<float> pitchSemitones{0.f};   // -12 to +12
    std::atomic<float> formantSemitones{0.f}; // -12 to +12
    std::atomic<float> reverbMix{0.f};        // 0-1
    std::atomic<float> distortionAmount{0.f}; // 0-1
    std::atomic<float> echoAmount{0.f};        // 0-1
    std::atomic<float> gain{1.f};             // 0-2
    std::atomic<int>   effectMode{0};         // 0=normal,1=robot,2=radio
    std::atomic<float> eqLow{0.f};           // dB, low shelf (~200 Hz)
    std::atomic<float> eqMid{0.f};           // dB, peak     (~2 kHz)
    std::atomic<float> eqHigh{0.f};          // dB, high shelf (~8 kHz)
};

// Preset identifiers — must match VoicePreset.kt ordinals.
enum class PresetId : int {
    PASSTHROUGH = 0,
    CHILD       = 1,
    DEEP_MALE   = 2,
    FEMALE      = 3,
    ROBOT       = 4,
    MONSTER     = 5,
    GRANDMA     = 6,
    RADIO       = 7,
};

class VoiceProcessor {
public:
    explicit VoiceProcessor(int sampleRate = 48000);

    void applyPreset(PresetId id);
    ProcessorParams& params() { return params_; }

    // Called on the audio thread — must be lock-free.
    void process(const float* input, float* output, int numSamples);

    void reset();

private:
    [[maybe_unused]] int sampleRate_;
    ProcessorParams params_;

    PitchShifter    pitchShifter_;
    FormantShifter  formantShifter_;
    Reverb          reverb_;
    Distortion      distortion_;
    ThreeBandEQ     eq_;
    RobotEffect     robot_;

    // Simple echo line
    std::vector<float> echoBuffer_;
    int                echoHead_{0};
    int                echoDelaySamples_;

    // Per-frame working buffers (avoid allocation on audio thread)
    std::vector<float> stageBuf_;

    float lastPitch_{0.f};
    float lastFormant_{0.f};

    void updateEffectParams();
};

} // namespace voiceflux
