#pragma once
#include <vector>
#include <complex>

namespace voiceflux {

// Formant shifter using spectral envelope re-sampling.
// Works best combined with PitchShifter (apply formant AFTER pitch).
class FormantShifter {
public:
    static constexpr int kFrameSize = 2048;
    static constexpr int kHopSize   = kFrameSize / 4;

    FormantShifter();

    // formantFactor: 1.0 = unchanged, >1 = higher formants (brighter/child-like),
    //               <1 = lower formants (deeper/masculine).
    void setFormantFactor(float factor);

    static float semitoneToFactor(float semitones) {
        return std::pow(2.0f, semitones / 12.0f);
    }

    void process(const float* input, float* output, int numSamples);
    void reset();

private:
    float formantFactor_{1.0f};

    std::vector<float>                window_;
    std::vector<float>                inputFifo_;
    std::vector<float>                outputAccum_;
    std::vector<float>                outputQueue_;
    int                               outQHead_{0};
    int                               outQSize_{0};
    std::vector<std::complex<float>>  fftBuf_;
    int                               inputWritePos_{0};
    int                               samplesSinceProcess_{0};

    void processFrame();
};

} // namespace voiceflux
