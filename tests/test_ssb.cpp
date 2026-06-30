#include "../engine/ssb.h"
#include "../engine/dsp.h"

#include <cstdio>
#include <vector>

using namespace qo100;

namespace {
int g_failures = 0;

void check(bool cond, const char* name, double measured, const char* unit) {
    std::printf("  [%s] %-34s = %8.2f %s\n", cond ? "PASS" : "FAIL", name, measured, unit);
    if (!cond) ++g_failures;
}
} // namespace

int main() {
    const double fs = 48000.0;   // working sample rate
    const double fc = 8000.0;    // suppressed-carrier offset in the IQ band
    const double fa = 1000.0;    // audio test tone
    const int n = 48000;         // 1 second
    const int skip = 2000;       // ignore filter transient when measuring

    std::printf("QO-100 SSB engine self-test (fs=%.0f, carrier=%.0f, audio=%.0f)\n",
                fs, fc, fa);

    // --- Modulator: a USB tone must land at fc+fa, not at fc-fa ---
    {
        SsbModulator mod(fs, fc);
        auto audio = tone(fa, fs, n);
        std::vector<cf32> iq(n);
        for (int i = 0; i < n; ++i) iq[i] = mod.process(audio[i]);

        const double usb = binPower(iq, fc + fa, fs, skip); // wanted
        const double lsb = binPower(iq, fc - fa, fs, skip); // image
        const double rej = toDb(usb / lsb);
        std::printf("modulator:\n");
        check(rej > 40.0, "USB/LSB sideband rejection", rej, "dB");
    }

    // --- Demodulator: recover the audio tone from a clean USB signal ---
    {
        SsbModulator mod(fs, fc);
        SsbDemodulator dem(fs, fc);
        auto audio = tone(fa, fs, n);
        std::vector<float> out(n);
        for (int i = 0; i < n; ++i) out[i] = dem.process(mod.process(audio[i]));

        const double sig = binPower(out, fa, fs, skip);     // recovered tone
        const double img = binPower(out, fa, fs, skip);     // (placeholder)
        const double noise = binPower(out, fa + 1500.0, fs, skip);
        const double sinad = toDb(sig / noise);
        (void)img;
        std::printf("demodulator (clean USB):\n");
        check(sinad > 50.0, "recovered-tone / spur ratio", sinad, "dB");
    }

    // --- Demodulator must reject an LSB signal (opposite sideband) ---
    {
        SsbDemodulator dem(fs, fc);
        // A bare tone at fc+fa is the USB the demod should pass...
        auto usbIq = complexTone(fc + fa, fs, n);
        std::vector<float> usbOut(n);
        for (int i = 0; i < n; ++i) usbOut[i] = dem.process(usbIq[i]);

        dem.reset();
        // ...and a bare tone at fc-fa is the LSB image it must suppress.
        auto lsbIq = complexTone(fc - fa, fs, n);
        std::vector<float> lsbOut(n);
        for (int i = 0; i < n; ++i) lsbOut[i] = dem.process(lsbIq[i]);

        const double pass = binPower(usbOut, fa, fs, skip);
        const double leak = binPower(lsbOut, fa, fs, skip);
        const double rej = toDb(pass / leak);
        std::printf("demodulator (image rejection):\n");
        check(rej > 40.0, "USB pass / LSB leak", rej, "dB");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
