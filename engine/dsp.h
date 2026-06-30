#pragma once
#include <complex>
#include <vector>
#include <cmath>

// Small DSP helpers for tests and signal generation.
namespace qo100 {

using cf32 = std::complex<float>;

// Generate a real sine tone.
inline std::vector<float> tone(double freqHz, double fs, int n, float amp = 1.0f) {
    std::vector<float> out(n);
    const double w = 2.0 * M_PI * freqHz / fs;
    for (int i = 0; i < n; ++i) out[i] = amp * static_cast<float>(std::sin(w * i));
    return out;
}

// Generate a complex exponential (single tone in the IQ band).
inline std::vector<cf32> complexTone(double freqHz, double fs, int n, float amp = 1.0f) {
    std::vector<cf32> out(n);
    const double w = 2.0 * M_PI * freqHz / fs;
    for (int i = 0; i < n; ++i)
        out[i] = cf32(amp * static_cast<float>(std::cos(w * i)),
                      amp * static_cast<float>(std::sin(w * i)));
    return out;
}

// Power (magnitude^2) at a single frequency via a direct DFT bin.
// Works on a complex IQ stream; skip a transient window with `start`.
inline double binPower(const std::vector<cf32>& x, double freqHz, double fs, int start = 0) {
    const double w = 2.0 * M_PI * freqHz / fs;
    double re = 0.0, im = 0.0;
    for (int i = start; i < static_cast<int>(x.size()); ++i) {
        const double c = std::cos(w * i), s = std::sin(w * i);
        re += x[i].real() * c + x[i].imag() * s;
        im += x[i].imag() * c - x[i].real() * s;
    }
    const int n = static_cast<int>(x.size()) - start;
    return (re * re + im * im) / (double(n) * n);
}

// Same for a real-valued (audio) stream.
inline double binPower(const std::vector<float>& x, double freqHz, double fs, int start = 0) {
    std::vector<cf32> c(x.size());
    for (size_t i = 0; i < x.size(); ++i) c[i] = cf32(x[i], 0.0f);
    return binPower(c, freqHz, fs, start);
}

inline double toDb(double ratio) { return 10.0 * std::log10(ratio); }

} // namespace qo100
