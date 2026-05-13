#pragma once

namespace voiceflux {

// Soft-clip / hard-clip waveshaping distortion.
class Distortion {
public:
    enum class Mode { SOFT_CLIP, HARD_CLIP, FUZZ };

    void setAmount(float amount); // 0.0-1.0
    void setMode(Mode mode);
    void process(const float* input, float* output, int numSamples);

private:
    float amount_{0.0f};
    Mode  mode_{Mode::SOFT_CLIP};

    float processSample(float x) const;
};

} // namespace voiceflux
