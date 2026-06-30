#include "../engine/calib.h"
#include "../engine/dsp.h"
#include "../engine/qo100.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace qo100;

namespace {
int g_failures = 0;

void check(bool cond, const char* name, double measured, const char* unit) {
    std::printf("  [%s] %-34s = %10.3f %s\n", cond ? "PASS" : "FAIL", name, measured, unit);
    if (!cond) ++g_failures;
}

void addNoise(std::vector<cf32>& x, float amp, unsigned seed) {
    unsigned s = seed;
    auto rnd = [&] { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f - 0.5f; };
    for (auto& v : x) v += cf32(amp * rnd(), amp * rnd());
}
} // namespace

int main() {
    const double fs = 384000.0;
    const int n = 16384 * 24; // ~24 averaging frames
    std::printf("QO-100 beacon calibration self-test (fs=%.0f)\n", fs);

    // Frequency-plan sanity: downlink/uplink coupling.
    {
        const double dl = plan::kBeaconMiddlePSK;
        const double ul = plan::uplinkForDownlink(dl);
        const double back = plan::downlinkForUplink(ul);
        std::printf("frequency plan:\n");
        check(std::fabs(back - dl) < 1.0, "uplink<->downlink round-trip err", back - dl, "Hz");
        check(std::fabs(ul - 2400.250e6) < 1.0, "PSK beacon uplink", ul, "Hz");
    }

    // The calibrator must recover a known LNB drift from a CW beacon.
    {
        const double expected = 20000.0; // nominal beacon offset in the band
        const double drift = 1234.5;     // actual LNB drift to be measured
        auto iq = complexTone(expected + drift, fs, n, 1.0f);
        addNoise(iq, 0.05f, 7);

        BeaconCalibrator cal(fs, 16384);
        const CalResult r = cal.find(iq, expected, 8000.0);
        std::printf("beacon (drift = +%.1f Hz):\n", drift);
        check(r.found, "beacon detected (snr ok)", r.snrDb, "dB");
        check(std::fabs(r.errorHz - drift) < 5.0, "measured drift error", r.errorHz - drift, "Hz");
    }

    // A negative drift, smaller carrier, more noise.
    {
        const double expected = -15000.0;
        const double drift = -742.0;
        auto iq = complexTone(expected + drift, fs, n, 0.5f);
        addNoise(iq, 0.08f, 99);

        BeaconCalibrator cal(fs, 16384);
        const CalResult r = cal.find(iq, expected, 8000.0);
        std::printf("beacon (drift = %.1f Hz):\n", drift);
        check(r.found, "beacon detected (snr ok)", r.snrDb, "dB");
        check(std::fabs(r.errorHz - drift) < 5.0, "measured drift error", r.errorHz - drift, "Hz");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
