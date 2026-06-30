#include "../engine/dsp.h"
#include "../engine/rx.h"
#include "../engine/tx.h"

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
    const double fsWide = 384000.0; // TX/RX wide rate (stands in for the Pluto)
    const int factor = 8;           // audio <-> wide ratio
    const double audioFs = fsWide / factor;
    const double tune = 50000.0;    // carrier offset in the band
    const double fa = 1000.0;       // audio test tone
    const int audioN = (int)audioFs; // 1 second
    const int skipAudio = 4000;
    const int skipIq = 40000;

    std::printf("QO-100 TX chain + TX->RX loopback (wide=%.0f, factor=%d)\n", fsWide, factor);

    auto audio = tone(fa, audioFs, audioN);

    // Transmit.
    TxChain tx(fsWide, factor);
    tx.setTune(tune);
    std::vector<cf32> iq;
    tx.process(audio, iq);

    // --- TX must emit USB (carrier+audio), not LSB (carrier-audio) ---
    {
        const double usb = binPower(iq, tune + fa, fsWide, skipIq);
        const double lsb = binPower(iq, tune - fa, fsWide, skipIq);
        const double rej = toDb(usb / lsb);
        std::printf("tx spectrum:\n");
        check(rej > 35.0, "USB/LSB sideband rejection", rej, "dB");
    }

    // --- Loopback: receive what we just transmitted ---
    {
        RxChain rx(fsWide, factor);
        rx.setTune(tune);
        std::vector<float> back;
        rx.process(iq, back);

        const double sig = binPower(back, fa, audioFs, skipAudio);
        const double spur = binPower(back, fa + 1700.0, audioFs, skipAudio); // empty bin
        const double sinad = toDb(sig / spur);
        std::printf("loopback (tx -> rx):\n");
        check(sinad > 40.0, "recovered tone / spur ratio", sinad, "dB");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
