#include "eq_filter.h"
#include <cmath>
#include <algorithm>

namespace voiceflux {

// ----- BiquadFilter -----

void BiquadFilter::setCoeffs(float b0, float b1, float b2,
                              float a0, float a1, float a2) {
    b0_ = b0 / a0;
    b1_ = b1 / a0;
    b2_ = b2 / a0;
    a1_ = a1 / a0;
    a2_ = a2 / a0;
}

void BiquadFilter::setLowPass(float freq, float q, int sampleRate) {
    const float w0    = 2.f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate);
    const float cosW  = std::cos(w0);
    const float alpha = std::sin(w0) / (2.f * q);
    setCoeffs((1.f - cosW) / 2.f, 1.f - cosW, (1.f - cosW) / 2.f,
               1.f + alpha, -2.f * cosW, 1.f - alpha);
}

void BiquadFilter::setHighPass(float freq, float q, int sampleRate) {
    const float w0    = 2.f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate);
    const float cosW  = std::cos(w0);
    const float alpha = std::sin(w0) / (2.f * q);
    setCoeffs((1.f + cosW) / 2.f, -(1.f + cosW), (1.f + cosW) / 2.f,
               1.f + alpha, -2.f * cosW, 1.f - alpha);
}

void BiquadFilter::setBandPass(float freq, float bw, int sampleRate) {
    const float w0    = 2.f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate);
    const float alpha = std::sin(w0) * std::sinh(std::log(2.f) / 2.f * bw * w0 / std::sin(w0));
    const float cosW  = std::cos(w0);
    setCoeffs(alpha, 0.f, -alpha, 1.f + alpha, -2.f * cosW, 1.f - alpha);
}

void BiquadFilter::setNotch(float freq, float bw, int sampleRate) {
    const float w0    = 2.f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate);
    const float alpha = std::sin(w0) * std::sinh(std::log(2.f) / 2.f * bw * w0 / std::sin(w0));
    const float cosW  = std::cos(w0);
    setCoeffs(1.f, -2.f * cosW, 1.f, 1.f + alpha, -2.f * cosW, 1.f - alpha);
}

void BiquadFilter::setPeakEQ(float freq, float q, float dBgain, int sampleRate) {
    const float A     = std::pow(10.f, dBgain / 40.f);
    const float w0    = 2.f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate);
    const float alpha = std::sin(w0) / (2.f * q);
    const float cosW  = std::cos(w0);
    setCoeffs(1.f + alpha * A,  -2.f * cosW, 1.f - alpha * A,
               1.f + alpha / A, -2.f * cosW, 1.f - alpha / A);
}

void BiquadFilter::setLowShelf(float freq, float dBgain, int sampleRate) {
    const float A    = std::pow(10.f, dBgain / 40.f);
    const float w0   = 2.f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate);
    const float cosW = std::cos(w0);
    const float sinW = std::sin(w0);
    const float beta = std::sqrt(A) / 1.0f; // slope = 1
    setCoeffs(
        A * ((A + 1.f) - (A - 1.f) * cosW + beta * sinW),
        2.f * A * ((A - 1.f) - (A + 1.f) * cosW),
        A * ((A + 1.f) - (A - 1.f) * cosW - beta * sinW),
        (A + 1.f) + (A - 1.f) * cosW + beta * sinW,
        -2.f * ((A - 1.f) + (A + 1.f) * cosW),
        (A + 1.f) + (A - 1.f) * cosW - beta * sinW);
}

void BiquadFilter::setHighShelf(float freq, float dBgain, int sampleRate) {
    const float A    = std::pow(10.f, dBgain / 40.f);
    const float w0   = 2.f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate);
    const float cosW = std::cos(w0);
    const float sinW = std::sin(w0);
    const float beta = std::sqrt(A);
    setCoeffs(
        A * ((A + 1.f) + (A - 1.f) * cosW + beta * sinW),
        -2.f * A * ((A - 1.f) + (A + 1.f) * cosW),
        A * ((A + 1.f) + (A - 1.f) * cosW - beta * sinW),
        (A + 1.f) - (A - 1.f) * cosW + beta * sinW,
        2.f * ((A - 1.f) - (A + 1.f) * cosW),
        (A + 1.f) - (A - 1.f) * cosW - beta * sinW);
}

float BiquadFilter::tick(float in) {
    const float out = b0_ * in + b1_ * x1_ + b2_ * x2_
                               - a1_ * y1_ - a2_ * y2_;
    x2_ = x1_; x1_ = in;
    y2_ = y1_; y1_ = out;
    return out;
}

void BiquadFilter::reset() {
    x1_ = x2_ = y1_ = y2_ = 0.f;
}

// ----- ThreeBandEQ -----

ThreeBandEQ::ThreeBandEQ(int sampleRate) : sampleRate_(sampleRate) {
    rebuild();
}

void ThreeBandEQ::setLowGain(float dB)  { lowGain_  = dB; rebuild(); }
void ThreeBandEQ::setMidGain(float dB)  { midGain_  = dB; rebuild(); }
void ThreeBandEQ::setHighGain(float dB) { highGain_ = dB; rebuild(); }

void ThreeBandEQ::rebuild() {
    lowShelf_ .setLowShelf (200.f,  lowGain_,  sampleRate_);
    midPeak_  .setPeakEQ   (2000.f, 0.7f, midGain_, sampleRate_);
    highShelf_.setHighShelf(8000.f, highGain_, sampleRate_);
}

void ThreeBandEQ::process(const float* in, float* out, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        float s = lowShelf_.tick(in[i]);
        s = midPeak_.tick(s);
        out[i] = highShelf_.tick(s);
    }
}

void ThreeBandEQ::reset() {
    lowShelf_.reset();
    midPeak_.reset();
    highShelf_.reset();
}

} // namespace voiceflux
