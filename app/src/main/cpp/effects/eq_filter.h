#pragma once
#include <array>

namespace voiceflux {

// Second-order (biquad) IIR filter.
// Supports low-pass, high-pass, band-pass, notch, and peak/shelf EQ types.
class BiquadFilter {
public:
    enum class Type {
        LOW_PASS,
        HIGH_PASS,
        BAND_PASS,
        NOTCH,
        PEAK_EQ,
        LOW_SHELF,
        HIGH_SHELF
    };

    BiquadFilter() = default;

    void setLowPass (float freq, float q,          int sampleRate);
    void setHighPass(float freq, float q,          int sampleRate);
    void setBandPass(float freq, float bw,         int sampleRate);
    void setNotch   (float freq, float bw,         int sampleRate);
    void setPeakEQ  (float freq, float q, float dBgain, int sampleRate);
    void setLowShelf(float freq, float dBgain,     int sampleRate);
    void setHighShelf(float freq, float dBgain,    int sampleRate);

    float tick(float in);
    void  reset();

private:
    float b0_{1.f}, b1_{0.f}, b2_{0.f};
    float      a1_{0.f}, a2_{0.f};
    float x1_{0.f}, x2_{0.f};
    float y1_{0.f}, y2_{0.f};

    void setCoeffs(float b0, float b1, float b2, float a0, float a1, float a2);
};

// 3-band parametric EQ (low-shelf, mid-peak, high-shelf).
class ThreeBandEQ {
public:
    explicit ThreeBandEQ(int sampleRate = 48000);

    void setLowGain (float dB);
    void setMidGain (float dB);
    void setHighGain(float dB);

    void process(const float* in, float* out, int numSamples);
    void reset();

private:
    int        sampleRate_;
    BiquadFilter lowShelf_, midPeak_, highShelf_;
    void rebuild();
    float lowGain_{0.f}, midGain_{0.f}, highGain_{0.f};
};

} // namespace voiceflux
