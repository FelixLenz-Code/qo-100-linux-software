#include "spectrum.h"
#include "fft.h"
#include <cmath>

namespace qo100 {

Spectrum::Spectrum(int fftSize) : size_(fftSize), window_(fftSize), scratch_(fftSize) {
    // Hann window.
    double sum = 0.0;
    for (int i = 0; i < size_; ++i) {
        window_[i] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * M_PI * i / (size_ - 1)));
        sum += window_[i];
    }
    scale_ = static_cast<float>(1.0 / sum);
}

void Spectrum::compute(const cf32* in, std::vector<float>& dbOut) {
    for (int i = 0; i < size_; ++i) scratch_[i] = in[i] * window_[i];
    fft(scratch_, false);

    dbOut.resize(size_);
    const int half = size_ / 2;
    for (int j = 0; j < size_; ++j) {
        const int src = (j + half) % size_; // fftshift: centre = DC
        const float mag = std::abs(scratch_[src]) * scale_;
        dbOut[j] = 20.0f * std::log10(mag + 1e-12f);
    }
}

} // namespace qo100
