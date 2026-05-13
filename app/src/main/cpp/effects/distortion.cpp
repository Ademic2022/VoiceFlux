#include "distortion.h"
#include <cmath>
#include <algorithm>

namespace voiceflux {

void Distortion::setAmount(float amount) {
    amount_ = std::max(0.f, std::min(amount, 1.f));
}

void Distortion::setMode(Mode mode) {
    mode_ = mode;
}

float Distortion::processSample(float x) const {
    if (amount_ < 1e-4f) return x;

    // Drive gain: map 0-1 → 1-50
    const float drive = 1.f + amount_ * 49.f;

    switch (mode_) {
        case Mode::SOFT_CLIP: {
            // tanh waveshaping — smooth, musical clipping
            const float y = std::tanh(x * drive);
            return y / std::tanh(drive); // normalise so max = 1
        }
        case Mode::HARD_CLIP: {
            const float threshold = 1.f / drive;
            return std::max(-threshold, std::min(x, threshold)) * drive;
        }
        case Mode::FUZZ: {
            // Asymmetric soft-clip for transistor fuzz character
            const float driven = x * drive;
            if (driven >= 0.f)
                return 1.f - std::exp(-driven);
            else
                return -(1.f - std::exp(driven)) * 0.7f;
        }
    }
    return x;
}

void Distortion::process(const float* input, float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i)
        output[i] = processSample(input[i]);
}

} // namespace voiceflux
