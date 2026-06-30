#include "../engine/dsp.h"
#include "../engine/filedevice.h"
#include "../engine/iqfile.h"
#include "../engine/ringbuffer.h"
#include "../engine/stream.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace qo100;

namespace {
int g_failures = 0;

void check(bool cond, const char* name, double measured, const char* unit) {
    std::printf("  [%s] %-34s = %12.1f %s\n", cond ? "PASS" : "FAIL", name, measured, unit);
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
    std::printf("QO-100 streaming engine self-test\n");

    // --- RingBuffer integrity across producer/consumer threads ---
    {
        RingBuffer<int> rb(1000);
        const int M = 200000;
        std::thread prod([&] {
            int i = 0;
            while (i < M) {
                if (rb.push(&i, 1) == 1) ++i;
                else std::this_thread::yield();
            }
        });
        long long sum = 0;
        int got = 0;
        while (got < M) {
            int v;
            if (rb.pop(&v, 1) == 1) { sum += v; ++got; }
            else std::this_thread::yield();
        }
        prod.join();
        const long long expected = (long long)M * (M - 1) / 2;
        std::printf("ring buffer:\n");
        check(got == M, "items received", got, "");
        check(sum == expected, "checksum matches", (double)(sum - expected), "(0=ok)");
    }

    // --- StreamEngine produces correct spectrum + audio from a FileDevice ---
    {
        const double fs = 384000.0;
        const int fftSize = 1024;
        const double sigOffset = 49875.0;          // exact FFT bin 133
        auto scene = complexTone(sigOffset, fs, fftSize * 40, 1.0f);
        const char* path = "stream_scene.cf32";
        iqfile::write(path, scene);

        FileDevice dev(path, fs, /*realtime=*/false);
        if (!dev.start()) { std::printf("  FAIL device start\n"); ++g_failures; }
        StreamEngine eng(dev, 8, fftSize);
        eng.setTune(sigOffset - 1000.0); // tone lands at 1 kHz audio
        for (int i = 0; i < 60; ++i) eng.pump();

        std::vector<float> spec;
        const bool haveSpec = eng.latestSpectrum(spec);
        const int peak = haveSpec ? argmax(spec) : -1;
        const int expected = (133 + fftSize / 2) % fftSize; // fftshifted = 645
        std::printf("stream engine (deterministic):\n");
        check(eng.rowsProduced() > 0, "rows produced", (double)eng.rowsProduced(), "");
        check(peak == expected, "spectrum peak bin", peak, "");
        check(eng.audioLevel() > 1e-3, "demod audio level", eng.audioLevel() * 1000.0, "(x1e-3)");
        dev.stop();
        std::remove(path);
    }

    // --- Threaded path: device + engine on their own threads, no deadlock ---
    {
        const double fs = 384000.0;
        auto scene = complexTone(40000.0, fs, 1024 * 40, 1.0f);
        const char* path = "stream_rt.cf32";
        iqfile::write(path, scene);

        FileDevice dev(path, fs, /*realtime=*/true);
        dev.start();
        StreamEngine eng(dev, 8, 1024);
        eng.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        eng.stop();
        dev.stop();
        std::printf("stream engine (threaded):\n");
        check(eng.rowsProduced() > 0, "rows produced in 200 ms", (double)eng.rowsProduced(), "");
        std::remove(path);
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
