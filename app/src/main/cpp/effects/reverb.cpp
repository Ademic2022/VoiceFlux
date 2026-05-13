#include "reverb.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace voiceflux {

// Comb filter delay lengths tuned to be mutually prime (avoids resonance clustering).
// Values from Schroeder & Moorer, scaled for 48 kHz.
static const int kCombDelays[4]    = { 1557, 1617, 1491, 1422 };
static const int kAllPassDelays[2] = {  225,   556 };

Reverb::Reverb(int sampleRate) {
    const float scale = static_cast<float>(sampleRate) / 44100.f;
    for (int i = 0; i < kNumCombs; ++i)
        combs_[i].buffer.assign(static_cast<int>(kCombDelays[i] * scale) + 1, 0.f);
    for (int i = 0; i < kNumAllPasses; ++i)
        allPasses_[i].buffer.assign(static_cast<int>(kAllPassDelays[i] * scale) + 1, 0.f);
    updateFeedbacks();
}

void Reverb::setMix(float mix) {
    mix_ = std::max(0.f, std::min(mix, 1.f));
}

void Reverb::setRoomSize(float size) {
    roomSize_ = std::max(0.f, std::min(size, 1.f));
    updateFeedbacks();
}

void Reverb::updateFeedbacks() {
    // Map roomSize 0-1 to feedback 0.70-0.93
    const float fb = 0.70f + roomSize_ * 0.23f;
    for (auto& c : combs_) c.feedback = fb;
}

void Reverb::reset() {
    for (auto& c : combs_)    { std::fill(c.buffer.begin(), c.buffer.end(), 0.f); c.pos = 0; c.store = 0.f; }
    for (auto& a : allPasses_){ std::fill(a.buffer.begin(), a.buffer.end(), 0.f); a.pos = 0; }
}

float Reverb::CombFilter::tick(float in) {
    const float out   = buffer[pos];
    store             = out * (1.f - damp) + store * damp;
    buffer[pos]       = in + store * feedback;
    pos               = (pos + 1) % static_cast<int>(buffer.size());
    return out;
}

float Reverb::AllPassFilter::tick(float in) {
    const float buffered = buffer[pos];
    buffer[pos]          = in + buffered * feedback;
    pos                  = (pos + 1) % static_cast<int>(buffer.size());
    return buffered - in;
}

void Reverb::process(const float* input, float* output, int numSamples) {
    const float wet = mix_;
    const float dry = 1.f - mix_;

    for (int i = 0; i < numSamples; ++i) {
        const float in  = input[i] * 0.015f; // Pre-gain to avoid saturation in comb filters
        float        rev = 0.f;

        for (auto& c : combs_)   rev += c.tick(in);
        for (auto& a : allPasses_) rev = a.tick(rev);

        output[i] = input[i] * dry + rev * wet;
    }
}

} // namespace voiceflux
