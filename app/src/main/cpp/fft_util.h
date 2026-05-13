#pragma once
#include <complex>
#include <vector>
#include <cmath>
#include <algorithm>

namespace voiceflux {

// Cooley-Tukey in-place FFT — size must be a power of 2.
// inverse=false → forward FFT, inverse=true → unnormalised inverse.
inline void fft(std::vector<std::complex<float>>& a, bool inverse) {
    const int n = static_cast<int>(a.size());

    // Bit-reversal permutation
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    // Butterfly passes
    for (int len = 2; len <= n; len <<= 1) {
        const float ang = 2.0f * static_cast<float>(M_PI) / static_cast<float>(len)
                          * (inverse ? -1.0f : 1.0f);
        const std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                const std::complex<float> u = a[i + j];
                const std::complex<float> v = a[i + j + len / 2] * w;
                a[i + j]           = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse) {
        const float inv = 1.0f / static_cast<float>(n);
        for (auto& x : a) x *= inv;
    }
}

// Build a Hann window of the given length.
inline std::vector<float> makeHannWindow(int size) {
    std::vector<float> w(size);
    const float factor = 2.0f * static_cast<float>(M_PI) / static_cast<float>(size);
    for (int i = 0; i < size; ++i)
        w[i] = 0.5f * (1.0f - std::cos(factor * static_cast<float>(i)));
    return w;
}

// Wrap angle to (-pi, pi].
inline float wrapPhase(float phase) {
    while (phase >  static_cast<float>(M_PI)) phase -= 2.0f * static_cast<float>(M_PI);
    while (phase < -static_cast<float>(M_PI)) phase += 2.0f * static_cast<float>(M_PI);
    return phase;
}

} // namespace voiceflux
