#pragma once
#include <complex>
#include <vector>

// Beacon-based frequency calibration. A QO-100 CW beacon is a stable narrow
// carrier at a known offset; the LNB local oscillator drifts, so the beacon
// shows up shifted. Measuring that shift gives the correction to apply to RX
// tuning (and, coupled by the fixed LO offset, to TX).
namespace qo100 {

using cf32 = std::complex<float>;

struct CalResult {
    bool found = false;
    double measuredHz = 0.0; // where the beacon actually is (offset in the band)
    double errorHz = 0.0;    // measured - expected; the LNB drift to correct out
    double snrDb = 0.0;      // peak vs. window median
};

class BeaconCalibrator {
public:
    BeaconCalibrator(double fs, int fftSize = 16384, double minSnrDb = 6.0);
    // Find a CW beacon near expectedHz within +/- searchHz. Averages power over
    // as many FFT frames as `iq` provides, then refines to sub-bin accuracy.
    CalResult find(const std::vector<cf32>& iq, double expectedHz, double searchHz) const;

private:
    double fs_;
    int fftSize_;
    double minSnrDb_;
    std::vector<float> window_;
};

} // namespace qo100
