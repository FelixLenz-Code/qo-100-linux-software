#include "calib.h"
#include "fft.h"
#include <algorithm>
#include <cmath>

namespace qo100 {

BeaconCalibrator::BeaconCalibrator(double fs, int fftSize, double minSnrDb)
    : fs_(fs), fftSize_(fftSize), minSnrDb_(minSnrDb), window_(fftSize) {
    for (int i = 0; i < fftSize_; ++i)
        window_[i] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * M_PI * i / (fftSize_ - 1)));
}

CalResult BeaconCalibrator::find(const std::vector<cf32>& iq, double expectedHz,
                                 double searchHz) const {
    const int N = fftSize_;
    if ((int)iq.size() < N) return {};

    // Welch-style averaged power spectrum (a CW line stays put, noise averages down).
    std::vector<double> psd(N, 0.0);
    std::vector<cf32> buf(N);
    int frames = 0;
    for (size_t off = 0; off + N <= iq.size(); off += N) {
        for (int i = 0; i < N; ++i) buf[i] = iq[off + i] * window_[i];
        fft(buf, false);
        for (int k = 0; k < N; ++k) psd[k] += (double)std::norm(buf[k]);
        ++frames;
    }
    if (frames == 0) return {};

    const double binHz = fs_ / N;
    auto freqOf = [&](int k) { return (k <= N / 2) ? k * binHz : (k - N) * binHz; };

    // Peak within the search window, plus the window powers for an SNR estimate.
    int peak = -1;
    double peakP = -1.0;
    std::vector<double> winPow;
    for (int k = 0; k < N; ++k) {
        const double f = freqOf(k);
        if (f < expectedHz - searchHz || f > expectedHz + searchHz) continue;
        winPow.push_back(psd[k]);
        if (psd[k] > peakP) { peakP = psd[k]; peak = k; }
    }
    if (peak < 0 || winPow.size() < 3) return {};

    // Parabolic interpolation (log domain) for sub-bin frequency accuracy.
    const int km = (peak - 1 + N) % N, kp = (peak + 1) % N;
    const double a = 10.0 * std::log10(psd[km] + 1e-30);
    const double b = 10.0 * std::log10(psd[peak] + 1e-30);
    const double c = 10.0 * std::log10(psd[kp] + 1e-30);
    const double denom = a - 2.0 * b + c;
    double delta = (denom != 0.0) ? 0.5 * (a - c) / denom : 0.0;
    delta = std::clamp(delta, -0.5, 0.5);

    const double measured = freqOf(peak) + delta * binHz;

    std::sort(winPow.begin(), winPow.end());
    const double median = winPow[winPow.size() / 2];
    const double snrDb = 10.0 * std::log10((peakP + 1e-30) / (median + 1e-30));

    CalResult r;
    r.found = snrDb >= minSnrDb_;
    r.measuredHz = measured;
    r.errorHz = measured - expectedHz;
    r.snrDb = snrDb;
    return r;
}

} // namespace qo100
