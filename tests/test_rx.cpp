#include "../engine/dsp.h"
#include "../engine/iqfile.h"
#include "../engine/rx.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace qo100;

namespace {
int g_failures = 0;

void check(bool cond, const char* name, double measured, const char* unit) {
    std::printf("  [%s] %-34s = %8.2f %s\n", cond ? "PASS" : "FAIL", name, measured, unit);
    if (!cond) ++g_failures;
}

// Add scaled white-ish noise (deterministic LCG) to an IQ scene.
void addNoise(std::vector<cf32>& x, float amp, unsigned seed = 1) {
    unsigned s = seed;
    auto rnd = [&] { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f - 0.5f; };
    for (auto& v : x) v += cf32(amp * rnd(), amp * rnd());
}
} // namespace

int main() {
    const double fsIn = 384000.0; // wideband input (stands in for a Pluto capture)
    const int decim = 8;          // -> 48 kHz audio
    const int n = 384000;         // 1 second
    const double audioFs = fsIn / decim;
    const double fTune = 50000.0; // wanted carrier offset within the band
    const double fAudio = 1200.0; // audio tone carried in USB
    const int skip = 4000;        // audio samples to drop (filter transients)

    std::printf("QO-100 RX chain self-test (fsIn=%.0f, decim=%d, audio=%.0f)\n",
                fsIn, decim, audioFs);

    // A clean USB signal carrying a single audio tone is just one complex tone
    // at (carrier + audio); that is what the receiver must turn back into audio.
    auto wanted = complexTone(fTune + fAudio, fsIn, n, 1.0f);

    // --- IQ file roundtrip ---
    {
        const char* path = "test_scene.cf32";
        std::vector<cf32> back;
        const bool wrote = iqfile::write(path, wanted);
        const bool readOk = iqfile::read(path, back);
        double maxErr = 0.0;
        if (back.size() == wanted.size())
            for (size_t i = 0; i < back.size(); ++i)
                maxErr = std::max(maxErr, (double)std::abs(back[i] - wanted[i]));
        else
            maxErr = 1.0;
        std::printf("iq file (.cf32):\n");
        check(wrote && readOk && maxErr < 1e-6, "write/read max abs error", maxErr, "");
        std::remove(path);
    }

    // --- Receive the wanted signal amid a far interferer and noise ---
    {
        auto scene = wanted;
        auto interferer = complexTone(130000.0, fsIn, n, 0.5f); // far out of passband
        for (int i = 0; i < n; ++i) scene[i] += interferer[i];
        addNoise(scene, 0.003f);

        RxChain rx(fsIn, decim);
        rx.setTune(fTune);
        std::vector<float> audio;
        rx.process(scene, audio);

        const double sig = binPower(audio, fAudio, audioFs, skip);
        const double noise = binPower(audio, fAudio + 2100.0, audioFs, skip); // empty bin
        const double sinad = toDb(sig / noise);
        std::printf("rx (wanted + interferer + noise):\n");
        check(sinad > 40.0, "recovered tone / noise floor", sinad, "dB");
    }

    // --- Opposite-sideband (image) rejection through the whole chain ---
    // Both sidebands carried in one stream at different audio tones, so the
    // single AGC gain cancels in the ratio: a 1200 Hz tone on the USB side
    // (must pass) and an 1800 Hz tone on the LSB side (must be rejected).
    {
        const double fLeak = 1800.0;
        auto scene = wanted; // USB tone -> 1200 Hz audio
        auto image = complexTone(fTune - fLeak, fsIn, n, 1.0f); // LSB side
        for (int i = 0; i < n; ++i) scene[i] += image[i];

        RxChain rx(fsIn, decim);
        rx.setTune(fTune);
        std::vector<float> audio;
        rx.process(scene, audio);

        const double pass = binPower(audio, fAudio, audioFs, skip); // USB, kept
        const double leak = binPower(audio, fLeak, audioFs, skip);  // LSB, rejected
        const double rej = toDb(pass / leak);
        std::printf("rx (image rejection):\n");
        check(rej > 35.0, "USB pass / LSB leak", rej, "dB");
    }

    // --- SSB channel filter: a signal outside 2.7 kHz must be rejected ---
    // Wanted USB tone -> 1500 Hz audio (in band); an adjacent USB signal 5 kHz
    // higher -> 5000 Hz audio, which the 300-2700 Hz channel filter must cut.
    {
        const double fWanted = 1500.0, fAdjacent = 5000.0;
        auto scene = complexTone(fTune + fWanted, fsIn, n, 1.0f);
        auto adj = complexTone(fTune + fAdjacent, fsIn, n, 1.0f);
        for (int i = 0; i < n; ++i) scene[i] += adj[i];

        RxChain rx(fsIn, decim);
        rx.setTune(fTune);
        std::vector<float> audio;
        rx.process(scene, audio);

        const double pass = binPower(audio, fWanted, audioFs, skip);
        const double rej = binPower(audio, fAdjacent, audioFs, skip);
        const double ratio = toDb(pass / rej);
        std::printf("rx (channel selectivity):\n");
        check(ratio > 35.0, "in-band / out-of-band (5 kHz)", ratio, "dB");
    }

    // --- AGC: silence -> sudden full signal must not spike, then settle ---
    {
        Agc agc(audioFs);
        const int an = (int)audioFs;
        float maxOut = 0.0f;
        double sumSq = 0.0;
        const int from = an / 2;
        for (int i = 0; i < an; ++i) {
            const float x = 0.4f * std::sin(2.0 * M_PI * 800.0 * i / audioFs);
            const float y = agc.process(x);
            maxOut = std::max(maxOut, std::fabs(y));
            if (i >= from) sumSq += (double)y * y;
        }
        const double steadyRms = std::sqrt(sumSq / (an - from));
        std::printf("agc:\n");
        check(maxOut < 1.5, "peak output (no startup spike)", maxOut, "");
        check(steadyRms > 0.15 && steadyRms < 0.30, "steady-state rms", steadyRms, "");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
