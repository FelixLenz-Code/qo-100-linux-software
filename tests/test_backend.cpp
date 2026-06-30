#include "../engine/dsp.h"
#include "../engine/fft.h"
#include "../engine/spectrum.h"
#include "../engine/wavfile.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace qo100;

namespace {
int g_failures = 0;

void check(bool cond, const char* name, double measured, const char* unit) {
    std::printf("  [%s] %-34s = %10.4f %s\n", cond ? "PASS" : "FAIL", name, measured, unit);
    if (!cond) ++g_failures;
}

int argmax(const std::vector<float>& v) {
    int best = 0;
    for (int i = 1; i < (int)v.size(); ++i)
        if (v[i] > v[best]) best = i;
    return best;
}
} // namespace

int main() {
    std::printf("QO-100 DSP backend self-test\n");

    // --- FFT: a complex tone lands in exactly one bin, and ifft(fft(x))==x ---
    {
        const int size = 1024, kBin = 64;
        auto x = complexTone((double)kBin / size, 1.0, size);
        auto orig = x;
        fft(x, false);
        std::vector<float> mag(size);
        for (int i = 0; i < size; ++i) mag[i] = std::abs(x[i]);
        const int peak = argmax(mag);

        fft(x, true); // inverse
        double maxErr = 0.0;
        for (int i = 0; i < size; ++i) maxErr = std::max(maxErr, (double)std::abs(x[i] - orig[i]));

        std::printf("fft:\n");
        check(peak == kBin, "peak bin index", peak, "");
        check(maxErr < 1e-4, "ifft(fft(x)) max error", maxErr, "");
    }

    // --- Spectrum: peak appears at the correct fftshifted bin, near 0 dBFS ---
    {
        const int size = 4096, kNat = 410;
        const double fs = 48000.0;
        const double f = (double)kNat / size * fs;
        auto x = complexTone(f, fs, size, 1.0f);
        Spectrum spec(size);
        std::vector<float> db;
        spec.compute(x.data(), db);
        const int peak = argmax(db);
        const int expected = (kNat + size / 2) % size; // fftshifted location
        std::printf("spectrum:\n");
        check(peak == expected, "peak bin (fftshifted)", peak, "");
        check(db[peak] > -3.0f, "peak level", db[peak], "dBFS");
    }

    // --- WAV: write -> read roundtrip keeps rate and samples (16-bit quantised) ---
    {
        const int fs = 48000;
        auto sig = tone(1000.0, fs, 4800, 0.8f);
        const char* path = "test_audio.wav";
        wavfile::writeMono(path, sig, fs);
        std::vector<float> back;
        int rate = 0;
        const bool ok = wavfile::readMono(path, back, rate);
        double maxErr = 0.0;
        if (back.size() == sig.size())
            for (size_t i = 0; i < sig.size(); ++i)
                maxErr = std::max(maxErr, (double)std::fabs(back[i] - sig[i]));
        else
            maxErr = 1.0;
        std::printf("wav:\n");
        check(ok && rate == fs, "sample rate preserved", rate, "Hz");
        check(maxErr < 1e-3, "sample roundtrip max error", maxErr, "");
        std::remove(path);
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
