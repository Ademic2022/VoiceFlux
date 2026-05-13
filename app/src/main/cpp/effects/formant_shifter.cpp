#include "formant_shifter.h"
#include "../fft_util.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace voiceflux {

// Formant shifting via spectral envelope re-sampling.
// We stretch/compress the magnitude spectrum in frequency by formantFactor_
// while preserving the original phases. This moves vocal tract resonances
// (formants) up or down without changing the fundamental pitch.

FormantShifter::FormantShifter() {
    window_      = makeHannWindow(kFrameSize);
    inputFifo_   .assign(kFrameSize, 0.f);
    outputAccum_ .assign(kFrameSize, 0.f);
    outputQueue_ .assign(kFrameSize * 2, 0.f);
    fftBuf_      .assign(kFrameSize, {0.f, 0.f});
}

void FormantShifter::setFormantFactor(float factor) {
    formantFactor_ = std::max(0.25f, std::min(factor, 4.0f));
}

void FormantShifter::reset() {
    std::fill(inputFifo_.begin(),   inputFifo_.end(),   0.f);
    std::fill(outputAccum_.begin(), outputAccum_.end(), 0.f);
    std::fill(outputQueue_.begin(), outputQueue_.end(), 0.f);
    inputWritePos_      = 0;
    samplesSinceProcess_ = 0;
    outQHead_           = 0;
    outQSize_           = 0;
}

void FormantShifter::process(const float* input, float* output, int numSamples) {
    const int qCap = static_cast<int>(outputQueue_.size());
    for (int i = 0; i < numSamples; ++i) {
        inputFifo_[inputWritePos_] = input[i];
        inputWritePos_ = (inputWritePos_ + 1) % kFrameSize;
        ++samplesSinceProcess_;

        if (samplesSinceProcess_ >= kHopSize) {
            samplesSinceProcess_ = 0;
            processFrame();
        }

        if (outQSize_ > 0) {
            output[i] = outputQueue_[outQHead_];
            outQHead_ = (outQHead_ + 1) % qCap;
            --outQSize_;
        } else {
            output[i] = 0.f;
        }
    }
}

void FormantShifter::processFrame() {
    const int N     = kFrameSize;
    const int halfN = N / 2;
    const int qCap  = static_cast<int>(outputQueue_.size());

    // Window the input frame
    for (int i = 0; i < N; ++i) {
        const int pos = (inputWritePos_ - N + i + N) % N;
        fftBuf_[i] = { inputFifo_[pos] * window_[i], 0.f };
    }

    fft(fftBuf_, false);

    // Extract magnitude and phase
    static thread_local std::vector<float> mag(N / 2 + 1);
    static thread_local std::vector<float> phase(N / 2 + 1);
    for (int k = 0; k <= halfN; ++k) {
        mag[k]   = std::abs(fftBuf_[k]);
        phase[k] = std::arg(fftBuf_[k]);
    }

    // Re-sample the magnitude envelope by formantFactor_
    // (linear interpolation between adjacent bins)
    static thread_local std::vector<float> newMag(N / 2 + 1, 0.f);
    std::fill(newMag.begin(), newMag.end(), 0.f);

    for (int k = 0; k <= halfN; ++k) {
        const float srcK = static_cast<float>(k) / formantFactor_;
        const int   lo   = static_cast<int>(srcK);
        const float frac = srcK - static_cast<float>(lo);
        if (lo >= 0 && lo <= halfN) {
            const float mHi = (lo + 1 <= halfN) ? mag[lo + 1] : 0.f;
            newMag[k] = mag[lo] * (1.f - frac) + mHi * frac;
        }
    }

    // Reconstruct spectrum (keep original phases, new magnitudes)
    for (int k = 0; k <= halfN; ++k)
        fftBuf_[k] = std::polar(newMag[k], phase[k]);
    for (int k = 1; k < halfN; ++k)
        fftBuf_[N - k] = std::conj(fftBuf_[k]);
    fftBuf_[halfN] = { newMag[halfN], 0.f };

    fft(fftBuf_, true);

    // OLA normalization
    float windowSum = 0.f;
    for (float w : window_) windowSum += w * w;
    const float scale = static_cast<float>(kHopSize) / windowSum;

    for (int i = 0; i < N; ++i)
        outputAccum_[i] += fftBuf_[i].real() * window_[i] * scale;

    // Enqueue one hop's worth of output
    for (int i = 0; i < kHopSize; ++i) {
        if (outQSize_ < qCap) {
            const int idx = (outQHead_ + outQSize_) % qCap;
            outputQueue_[idx] = outputAccum_[i];
            ++outQSize_;
        }
    }

    const int remaining = N - kHopSize;
    if (remaining > 0)
        std::memmove(outputAccum_.data(), outputAccum_.data() + kHopSize,
                     remaining * sizeof(float));
    std::fill(outputAccum_.begin() + remaining, outputAccum_.end(), 0.f);
}

} // namespace voiceflux
