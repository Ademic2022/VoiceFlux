#include "robot_effect.h"
#include <cmath>
#include <algorithm>

namespace voiceflux {

RobotEffect::RobotEffect(int sampleRate) : sampleRate_(sampleRate) {}

void RobotEffect::setCarrierFreq(float hz) {
    carrierFreq_ = std::max(10.f, std::min(hz, 2000.f));
}

void RobotEffect::setDepth(float depth) {
    depth_ = std::max(0.f, std::min(depth, 1.f));
}

void RobotEffect::process(const float* input, float* output, int numSamples) {
    const float phaseInc = 2.f * static_cast<float>(M_PI) * carrierFreq_
                           / static_cast<float>(sampleRate_);
    for (int i = 0; i < numSamples; ++i) {
        const float carrier = std::sin(phase_);
        output[i] = input[i] * (1.f - depth_) + input[i] * carrier * depth_;
        phase_ += phaseInc;
        if (phase_ > 2.f * static_cast<float>(M_PI))
            phase_ -= 2.f * static_cast<float>(M_PI);
    }
}

void RobotEffect::reset() {
    phase_ = 0.f;
}

} // namespace voiceflux
