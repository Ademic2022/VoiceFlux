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
    inputWritePos_       = 0;
    samplesSinceProcess_ = 0;
    outQueueHead_        = 0;
    outQueueSize_        = 0;
}

void PitchShifter::process(const float* input, float* output, int numSamples) {
    const int qCapacity = static_cast<int>(outputQueue_.size());

    for (int i = 0; i < numSamples; ++i) {
        inputFifo_[inputWritePos_] = input[i];
        inputWritePos_ = (inputWritePos_ + 1) % kFrameSize;
        ++samplesSinceProcess_;

        if (samplesSinceProcess_ >= kHopSize) {
            samplesSinceProcess_ = 0;
            processFrame();
        }

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

    // 1. Window the input frame
    for (int i = 0; i < N; ++i) {
        const int pos = (inputWritePos_ - N + i + N) % N;
        fftBuf_[i] = { inputFifo_[pos] * window_[i], 0.f };
    }
    fft(fftBuf_, false);

    // 2. Compute instantaneous frequency for each bin
    const float freqPerBin = twoPi / static_cast<float>(N);
    const float hopPhase   = freqPerBin * static_cast<float>(kHopSize);

    for (int k = 0; k <= halfN; ++k) {
        const float mag   = std::abs(fftBuf_[k]);
        const float phase = std::arg(fftBuf_[k]);

        float dp = phase - lastAnalysisPhase_[k] - static_cast<float>(k) * hopPhase;
        dp = wrapPhase(dp);

        analysisMag_[k]       = mag;
        analysisFreq_[k]      = static_cast<float>(k) * freqPerBin
                                + dp / static_cast<float>(kHopSize);
        lastAnalysisPhase_[k] = phase;
    }

    // 3. Phase locking: assign every bin to its nearest spectral peak.
    //    Bins in the same partial group share the peak's instantaneous frequency,
    //    preventing independent phase drift that causes the metallic phasiness
    //    of a standard phase vocoder.
    static thread_local std::vector<int> peakOf(N / 2 + 1, 0);

    // Left-to-right pass: assign each bin to the most recent peak
    {
        int lp = 0;
        for (int k = 0; k <= halfN; ++k) {
            const bool isPeak = (k > 0 && k < halfN)
                && (analysisMag_[k] >= analysisMag_[k - 1])
                && (analysisMag_[k] >= analysisMag_[k + 1]);
            if (isPeak) lp = k;
            peakOf[k] = lp;
        }
    }
    // Right-to-left pass: replace with the right-side peak if it's closer
    {
        int rp = -1;
        for (int k = halfN; k >= 0; --k) {
            const bool isPeak = (k > 0 && k < halfN)
                && (analysisMag_[k] >= analysisMag_[k - 1])
                && (analysisMag_[k] >= analysisMag_[k + 1]);
            if (isPeak) rp = k;
            if (rp >= 0 && std::abs(k - rp) < std::abs(k - peakOf[k]))
                peakOf[k] = rp;
        }
    }

    // 4. Pitch-shift: map analysis bin k → synthesis bin round(k * pitchFactor)
    //    Take the loudest contributor when bins collide (max, not sum).
    static thread_local std::vector<float> outMag (N / 2 + 1, 0.f);
    static thread_local std::vector<float> outFreq(N / 2 + 1, 0.f);
    std::fill(outMag .begin(), outMag .begin() + halfN + 1, 0.f);
    std::fill(outFreq.begin(), outFreq.begin() + halfN + 1, 0.f);

    for (int k = 0; k <= halfN; ++k) {
        const int outK = static_cast<int>(
            std::round(static_cast<float>(k) * pitchFactor_));
        if (outK >= 0 && outK <= halfN && analysisMag_[k] > outMag[outK]) {
            outMag [outK] = analysisMag_[k];
            // Use the peak-group's true instantaneous frequency, scaled to pitch
            outFreq[outK] = analysisFreq_[peakOf[k]] * pitchFactor_;
        }
    }

    // 5. Accumulate synthesis phases and reconstruct the complex spectrum.
    //    synthHop == kHopSize: output produces exactly as many samples as are
    //    consumed from input, keeping the real-time buffer balanced.
    for (int k = 0; k <= halfN; ++k) {
        synthPhase_[k] += outFreq[k] * static_cast<float>(kHopSize);
        fftBuf_[k]      = std::polar(outMag[k], synthPhase_[k]);
    }
    for (int k = 1; k < halfN; ++k)
        fftBuf_[N - k] = std::conj(fftBuf_[k]);
    fftBuf_[halfN] = { outMag[halfN], 0.f };

    fft(fftBuf_, true);

    // 6. Overlap-add into the accumulation buffer
    for (int i = 0; i < N; ++i)
        outputAccum_[i] += fftBuf_[i].real() * window_[i] * outputScale_;

    // 7. Enqueue exactly kHopSize samples — matches the analysis hop so the
    //    queue stays at a constant depth (no growing latency, no sample drops)
    for (int i = 0; i < kHopSize; ++i) {
        if (outQueueSize_ < qCap) {
            const int idx = (outQueueHead_ + outQueueSize_) % qCap;
            outputQueue_[idx] = outputAccum_[i];
            ++outQueueSize_;
        }
    }

    // 8. Slide accumulation buffer left by one hop
    const int remaining = N - kHopSize;
    std::memmove(outputAccum_.data(), outputAccum_.data() + kHopSize,
                 remaining * sizeof(float));
    std::fill(outputAccum_.begin() + remaining, outputAccum_.end(), 0.f);
}

} // namespace voiceflux
