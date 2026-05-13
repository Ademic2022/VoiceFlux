#pragma once
#include <cmath>

namespace voiceflux {

// Robot voice via ring modulation: y = x * sin(2π f t)
// The carrier produces a metallic/robotic timbre.
class RobotEffect {
public:
    explicit RobotEffect(int sampleRate = 48000);

    void setCarrierFreq(float hz);  // default 50 Hz gives classic robot
    void setDepth(float depth);     // 0.0-1.0: blend between dry and ring-mod
    void process(const float* input, float* output, int numSamples);
    void reset();

private:
    int   sampleRate_;
    float carrierFreq_{50.0f};
    float depth_{1.0f};
    float phase_{0.0f};
};

} // namespace voiceflux
