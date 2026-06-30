#include "audiofilter.h"
#include <algorithm>
#include <cmath>

namespace qo100 {

namespace {
constexpr double kTwoPi = 6.283185307179586476925287;
}

BandpassFIR::BandpassFIR(double sampleRate, double lowHz, double highHz, int numTaps) {
    if (numTaps % 2 == 0) ++numTaps; // odd, symmetric
    taps_.resize(numTaps);
    buf_.assign(numTaps, 0.0f);
    const int M = numTaps / 2;
    const double fl = lowHz / sampleRate;  // normalised 0..0.5
    const double fh = highHz / sampleRate;

    for (int k = 0; k < numTaps; ++k) {
        const int m = k - M;
        // Ideal band-pass = high-pass(fl) ... low-pass(fh) as a difference of sincs.
        double ideal;
        if (m == 0) ideal = 2.0 * (fh - fl);
        else ideal = (std::sin(kTwoPi * fh * m) - std::sin(kTwoPi * fl * m)) / (M_PI * m);
        const double w = 0.54 - 0.46 * std::cos(kTwoPi * k / (numTaps - 1)); // Hamming
        taps_[k] = static_cast<float>(ideal * w);
    }

    // Normalise to unity gain at the band centre.
    const double fc = 0.5 * (fl + fh);
    double re = 0.0, im = 0.0;
    for (int k = 0; k < numTaps; ++k) {
        const double ang = kTwoPi * fc * (k - M);
        re += taps_[k] * std::cos(ang);
        im += taps_[k] * std::sin(ang);
    }
    const double gain = std::sqrt(re * re + im * im);
    if (gain > 1e-9)
        for (auto& t : taps_) t = static_cast<float>(t / gain);
}

void BandpassFIR::reset() {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    pos_ = 0;
}

float BandpassFIR::process(float x) {
    const int N = static_cast<int>(taps_.size());
    buf_[pos_] = x;
    pos_ = (pos_ + 1) % N;
    float acc = 0.0f;
    for (int k = 0; k < N; ++k) {
        int idx = pos_ - 1 - k;
        if (idx < 0) idx += N;
        acc += taps_[k] * buf_[idx];
    }
    return acc;
}

} // namespace qo100
