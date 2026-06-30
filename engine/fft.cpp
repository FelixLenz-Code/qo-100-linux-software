#include "fft.h"
#include <cmath>

namespace qo100 {

void fft(std::vector<cf32>& a, bool inverse) {
    const size_t n = a.size();
    if (!isPowerOfTwo(n)) return;

    // Bit-reversal permutation.
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    // Cooley-Tukey butterflies.
    const double sign = inverse ? 1.0 : -1.0;
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = sign * 2.0 * M_PI / static_cast<double>(len);
        const cf32 wlen(static_cast<float>(std::cos(ang)), static_cast<float>(std::sin(ang)));
        for (size_t i = 0; i < n; i += len) {
            cf32 w(1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; ++k) {
                const cf32 u = a[i + k];
                const cf32 v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse) {
        const float inv = 1.0f / static_cast<float>(n);
        for (auto& x : a) x *= inv;
    }
}

} // namespace qo100
