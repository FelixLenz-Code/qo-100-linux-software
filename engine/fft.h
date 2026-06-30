#pragma once
#include <complex>
#include <vector>

// Dependency-free iterative radix-2 FFT. Size must be a power of two.
namespace qo100 {

using cf32 = std::complex<float>;

void fft(std::vector<cf32>& a, bool inverse = false);

inline bool isPowerOfTwo(size_t n) { return n && ((n & (n - 1)) == 0); }

} // namespace qo100
