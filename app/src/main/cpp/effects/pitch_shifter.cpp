#include "pitch_shifter.h"
#include "../fft_util.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace voiceflux {

static constexpr float kPi = static_cast<float>(M_PI);

PitchShifter::PitchShifter() {
    window_            = makeHannWindow(kFrameSize);
    inputFifo_         .assign(kFrameSize, 0.f);
    outputAccum_       .assign(kFrameSize, 0.f);
    outputQueue_       .assign(kFrameSize * 2, 0.f);
    fftBuf_            .assign(kFrameSize, {0.f, 0.f});
    lastAnalysisPhase_ .assign(kFrameSize / 2 + 1, 0.f);
    synthPhase_        .assign(kFrameSize / 2 + 1, 0.f);
    analysisMag_       .assign(kFrameSize / 2 + 1, 0.f);
    analysisFreq_      .assign(kFrameSize / 2 + 1, 0.f);

    // OLA scale: compensates for Hann-window overlap-add amplitude reduction.
    float windowSum = 0.f;
    for (float w : window_) windowSum += w * w;
    outputScale_ = static_cast<float>(kHopSize) / windowSum;
}

void PitchShifter::setPitchFactor(float factor) {
    pitchFactor_ = std::max(0.25f, std::min(factor, 4.0f));
}

void PitchShifter::reset() {
    std::fill(inputFifo_.begin(),          inputFifo_.end(),          0.f);
    std::fill(outputAccum_.begin(),        outputAccum_.end(),        0.f);
    std::fill(outputQueue_.begin(),        outputQueue_.end(),        0.f);
    std::fill(lastAnalysisPhase_.begin(), lastAnalysisPhase_.end(),  0.f);
    std::fill(synthPhase_.begin(),         synthPhase_.end(),         0.f);
    inputWritePos_      = 0;
    samplesSinceProcess_ = 0;
    outQueueHead_       = 0;
    outQueueSize_       = 0;
}

void PitchShifter::process(const float* input, float* output, int numSamples) {
    const int qCapacity = static_cast<int>(outputQueue_.size());

    for (int i = 0; i < numSamples; ++i) {
        // Feed input FIFO (newest sample at inputWritePos_-1)
        inputFifo_[inputWritePos_] = input[i];
        inputWritePos_ = (inputWritePos_ + 1) % kFrameSize;
        ++samplesSinceProcess_;

        if (samplesSinceProcess_ >= kHopSize) {
            samplesSinceProcess_ = 0;
            processFrame();
        }

        // Drain output queue
        if (outQueueSize_ > 0) {
            output[i] = outputQueue_[outQueueHead_];
            outQueueHead_ = (outQueueHead_ + 1) % qCapacity;
            --outQueueSize_;
        } else {
            output[i] = 0.f;
        }
    }
}

void PitchShifter::processFrame() {
    const int N       = kFrameSize;
    const int halfN   = N / 2;
    const int qCap    = static_cast<int>(outputQueue_.size());
    const float twoPi = 2.0f * kPi;

    // Copy input frame into FFT buffer with Hann window
    for (int i = 0; i < N; ++i) {
        const int pos = (inputWritePos_ - N + i + N) % N;
        fftBuf_[i] = { inputFifo_[pos] * window_[i], 0.f };
    }

    fft(fftBuf_, false);

    // Compute magnitudes and instantaneous frequencies
    const float expectedPhaseDelta = twoPi * static_cast<float>(kHopSize) / static_cast<float>(N);

    for (int k = 0; k <= halfN; ++k) {
        const float mag   = std::abs(fftBuf_[k]);
        const float phase = std::arg(fftBuf_[k]);

        float dp = phase - lastAnalysisPhase_[k]
                   - static_cast<float>(k) * expectedPhaseDelta;
        dp = wrapPhase(dp);

        analysisMag_[k]         = mag;
        analysisFreq_[k]        = (twoPi * static_cast<float>(k) / static_cast<float>(N))
                                  + dp / static_cast<float>(kHopSize);
        lastAnalysisPhase_[k]   = phase;
    }

    // Pitch-shift: map each analysis bin to a synthesis bin
    // synthHop = kHopSize * pitchFactor_
    const int synthHop = static_cast<int>(static_cast<float>(kHopSize) * pitchFactor_ + 0.5f);

    static thread_local std::vector<float> outMag(N / 2 + 1, 0.f);
    static thread_local std::vector<float> outFreq(N / 2 + 1, 0.f);
    std::fill(outMag.begin(),  outMag.end(),  0.f);
    std::fill(outFreq.begin(), outFreq.end(), 0.f);

    for (int k = 0; k <= halfN; ++k) {
        const int outK = static_cast<int>(static_cast<float>(k) * pitchFactor_);
        if (outK >= 0 && outK <= halfN) {
            outMag[outK]  += analysisMag_[k];
            outFreq[outK]  = analysisFreq_[k] * pitchFactor_;
        }
    }

    // Accumulate synthesis phases and rebuild complex spectrum
    for (int k = 0; k <= halfN; ++k) {
        synthPhase_[k] += outFreq[k] * static_cast<float>(synthHop);
        fftBuf_[k]      = std::polar(outMag[k], synthPhase_[k]);
    }
    // Conjugate symmetry for real IFFT
    for (int k = 1; k < halfN; ++k)
        fftBuf_[N - k] = std::conj(fftBuf_[k]);
    fftBuf_[halfN] = { std::abs(fftBuf_[halfN]), 0.f };

    fft(fftBuf_, true);

    // Overlap-add into accumulation buffer
    for (int i = 0; i < N; ++i)
        outputAccum_[i] += fftBuf_[i].real() * window_[i] * outputScale_;

    // Push synthHop samples into the output queue
    for (int i = 0; i < synthHop && i < N; ++i) {
        if (outQueueSize_ < qCap) {
            const int idx = (outQueueHead_ + outQueueSize_) % qCap;
            outputQueue_[idx] = outputAccum_[i];
            ++outQueueSize_;
        }
    }

    // Slide accumulation buffer left by synthHop
    const int remaining = N - synthHop;
    if (remaining > 0)
        std::memmove(outputAccum_.data(), outputAccum_.data() + synthHop,
                     remaining * sizeof(float));
    std::fill(outputAccum_.begin() + remaining, outputAccum_.end(), 0.f);
}

} // namespace voiceflux
