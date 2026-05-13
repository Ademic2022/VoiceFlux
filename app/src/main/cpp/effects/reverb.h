#pragma once
#include <vector>
#include <array>

namespace voiceflux {

// Schroeder reverb: 4 parallel comb filters + 2 series all-pass filters.
class Reverb {
public:
    explicit Reverb(int sampleRate = 48000);

    // mix: 0.0 = dry only, 1.0 = full wet
    void setMix(float mix);

    // roomSize: 0.0-1.0
    void setRoomSize(float size);

    void process(const float* input, float* output, int numSamples);
    void reset();

private:
    static constexpr int kNumCombs    = 4;
    static constexpr int kNumAllPasses = 2;

    struct CombFilter {
        std::vector<float> buffer;
        int   pos{0};
        float feedback{0.84f};
        float damp{0.2f};
        float store{0.0f};
        float tick(float in);
    };

    struct AllPassFilter {
        std::vector<float> buffer;
        int   pos{0};
        float feedback{0.5f};
        float tick(float in);
    };

    std::array<CombFilter,    kNumCombs>     combs_;
    std::array<AllPassFilter, kNumAllPasses> allPasses_;

    float mix_{0.0f};
    float roomSize_{0.7f};

    void updateFeedbacks();
};

} // namespace voiceflux
