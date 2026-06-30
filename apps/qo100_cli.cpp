// Command-line front end for the QO-100 engine — usable with no audio hardware.
//
//   qo100_cli gen <out.cf32>
//       Write a synthetic wideband test scene (USB signal + interferer + noise).
//
//   qo100_cli decode <in.cf32> <fsIn> <decim> <tuneHz> <out.wav>
//       Tune, demodulate USB and render the audio to a WAV file you can listen
//       to later. Also prints the dominant recovered audio tone as a sanity check.
//
// Real QO-100 captures (interleaved float32 .cf32, e.g. from the BATC WebSDR or
// gqrx) work directly with `decode`.

#include "../engine/dsp.h"
#include "../engine/fft.h"
#include "../engine/iqfile.h"
#include "../engine/rx.h"
#include "../engine/wavfile.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace qo100;

namespace {

int usage() {
    std::printf(
        "usage:\n"
        "  qo100_cli gen <out.cf32>\n"
        "  qo100_cli decode <in.cf32> <fsIn> <decim> <tuneHz> <out.wav>\n");
    return 2;
}

// Synthetic scene: a USB signal carrying 300/800/1500 Hz tones at a +50 kHz
// carrier offset, plus a far interferer and a little noise. fsIn = 384 kHz.
int generate(const std::string& path) {
    const double fsIn = 384000.0, carrier = 50000.0;
    const int n = (int)fsIn * 3; // 3 seconds
    std::vector<cf32> scene(n, cf32(0.0f, 0.0f));

    // Clean USB = single complex tone per audio component at carrier+audio.
    for (double audio : {300.0, 800.0, 1500.0}) {
        auto t = complexTone(carrier + audio, fsIn, n, 0.4f);
        for (int i = 0; i < n; ++i) scene[i] += t[i];
    }
    auto interferer = complexTone(140000.0, fsIn, n, 0.3f); // outside the passband
    unsigned s = 12345;
    auto rnd = [&] { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f - 0.5f; };
    for (int i = 0; i < n; ++i) scene[i] += interferer[i] + cf32(0.002f * rnd(), 0.002f * rnd());

    if (!iqfile::write(path, scene)) {
        std::fprintf(stderr, "error: cannot write %s\n", path.c_str());
        return 1;
    }
    std::printf("wrote %s  (%d samples @ %.0f Hz)\n", path.c_str(), n, fsIn);
    std::printf("decode it with:\n  qo100_cli decode %s 384000 8 50000 out.wav\n", path.c_str());
    return 0;
}

// Report RMS and the dominant audio tone in 100..3500 Hz.
void summarise(const std::vector<float>& audio, double audioFs) {
    double sumSq = 0.0, peak = 0.0;
    for (float v : audio) { sumSq += (double)v * v; peak = std::max(peak, (double)std::fabs(v)); }
    const double rms = audio.empty() ? 0.0 : std::sqrt(sumSq / audio.size());

    const int size = 16384;
    std::vector<cf32> buf(size, cf32(0.0f, 0.0f));
    const int m = std::min((int)audio.size(), size);
    for (int i = 0; i < m; ++i) buf[i] = cf32(audio[i], 0.0f);
    fft(buf, false);
    int peakBin = 0;
    float peakMag = 0.0f;
    for (int k = 1; k < size / 2; ++k) {
        const double hz = (double)k * audioFs / size;
        if (hz < 100.0 || hz > 3500.0) continue;
        const float mag = std::abs(buf[k]);
        if (mag > peakMag) { peakMag = mag; peakBin = k; }
    }
    std::printf("audio: %zu samples @ %.0f Hz, rms=%.4f, peak=%.4f, dominant tone ~ %.0f Hz\n",
                audio.size(), audioFs, rms, peak, (double)peakBin * audioFs / size);
}

int decode(int argc, char** argv) {
    if (argc != 7) return usage();
    const std::string in = argv[2];
    const double fsIn = std::atof(argv[3]);
    const int decim = std::atoi(argv[4]);
    const double tune = std::atof(argv[5]);
    const std::string out = argv[6];
    if (fsIn <= 0 || decim <= 0) return usage();

    std::vector<cf32> iq;
    if (!iqfile::read(in, iq)) {
        std::fprintf(stderr, "error: cannot read %s\n", in.c_str());
        return 1;
    }
    RxChain rx(fsIn, decim);
    rx.setTune(tune);
    std::vector<float> audio;
    rx.process(iq, audio);

    if (!wavfile::writeMono(out, audio, (int)rx.audioRate())) {
        std::fprintf(stderr, "error: cannot write %s\n", out.c_str());
        return 1;
    }
    std::printf("read %zu IQ samples @ %.0f Hz, tuned %.0f Hz\n", iq.size(), fsIn, tune);
    summarise(audio, rx.audioRate());
    std::printf("wrote %s\n", out.c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    if (std::strcmp(argv[1], "gen") == 0 && argc == 3) return generate(argv[2]);
    if (std::strcmp(argv[1], "decode") == 0) return decode(argc, argv);
    return usage();
}
