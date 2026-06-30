#pragma once
#include <complex>
#include <vector>

// Turns a block of complex IQ into one power-spectrum row for the waterfall.
// Output is fftshifted (index 0 = -fs/2 ... centre = DC ... end = +fs/2) and
// in dBFS, so a full-scale complex tone reads about 0 dB.
namespace qo100 {

using cf32 = std::complex<float>;

class Spectrum {
public:
    explicit Spectrum(int fftSize); // power of two
    void compute(const cf32* in, std::vector<float>& dbOut);
    int size() const { return size_; }

private:
    int size_;
    std::vector<float> window_;
    std::vector<cf32> scratch_;
    float scale_; // coherent-gain normalisation (1 / sum(window))
};

} // namespace qo100
