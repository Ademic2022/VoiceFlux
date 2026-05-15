#pragma once
#include <vector>
#include <complex>
#include <cstdint>

namespace voiceflux {

// Phase-vocoder pitch shifter.
// Latency = kFrameSize / sampleRate  (~42 ms at 48 kHz, frame 2048).
class PitchShifter {
public:
    static constexpr int kFrameSize = 1024;
    static constexpr int kOverlap   = 4;
    static constexpr int kHopSize   = kFrameSize / kOverlap;

    PitchShifter();

    // pitchFactor: 1.0 = unchanged, 2.0 = one octave up, 0.5 = one octave down.
    // Use semitoneToFactor(n) to convert semitone offset.
    void setPitchFactor(float factor);

    static float semitoneToFactor(float semitones) {
        return std::pow(2.0f, semitones / 12.0f);
    }

    // Process numSamples from input → output. May be called with any buffer size.
    void process(const float* input, float* output, int numSamples);

    void reset();

private:
    float pitchFactor_{1.0f};

    std::vector<float>                  window_;
    std::vector<float>                  inputFifo_;   // size kFrameSize
    std::vector<float>                  outputAccum_; // size kFrameSize
    std::vector<float>                  outputQueue_; // ring buffer for output samples
    int                                 outQueueHead_{0};
    int                                 outQueueSize_{0};

    std::vector<std::complex<float>>    fftBuf_;      // size kFrameSize

    std::vector<float>                  lastAnalysisPhase_;
    std::vector<float>                  synthPhase_;
    std::vector<float>                  analysisMag_;
    std::vector<float>                  analysisFreq_;

    int  inputWritePos_{0};
    int  samplesSinceProcess_{0};
    float outputScale_{1.0f};

    void processFrame();
};

} // namespace voiceflux
