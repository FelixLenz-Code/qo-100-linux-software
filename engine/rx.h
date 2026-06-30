#pragma once
#include "ssb.h"
#include <complex>
#include <vector>

// Receive chain: wideband IQ -> tune -> decimate to audio rate -> USB demod -> AGC.
namespace qo100 {

using cf32 = std::complex<float>;

// Numerically controlled oscillator / complex mixer (shifts the spectrum down).
class Nco {
public:
    void setFreq(double hz, double fs);
    cf32 mixDown(cf32 x); // multiply by exp(-j*2*pi*f*t)
    void reset() { phase_ = 0.0; }

private:
    double phase_ = 0.0;
    double dphi_ = 0.0;
};

// Low-pass FIR + integer decimation for complex IQ. Anti-aliases into the
// audio band before downsampling.
class FirDecimator {
public:
    FirDecimator(int decim, double fsIn, double audioFs);
    // Push one input sample; returns true and writes `out` when an output is ready.
    bool process(cf32 x, cf32& out);
    void reset();
    int decimation() const { return decim_; }

private:
    std::vector<float> taps_;
    std::vector<cf32> buf_;
    int pos_ = 0;
    int decim_;
    int count_ = 0;
};

// Audio automatic gain control: peak-follower envelope with a fast attack
// (turn gain down quickly when the signal gets loud) and a slow release
// (raise gain gently when it goes quiet). The asymmetry, plus a bounded
// maximum gain, avoids the start-up spike a symmetric loop would produce.
class Agc {
public:
    explicit Agc(double audioFs, float target = 0.3f);
    float process(float x);
    void reset();

private:
    float target_, peak_ = 1e-6f, gain_ = 1.0f;
    float envDecay_, gainUp_, gainDown_;
    float maxGain_ = 100.0f;
};

// Full RX chain. `setTune` selects, in Hz relative to the centre of the input
// band, where the wanted SSB carrier sits.
class RxChain {
public:
    RxChain(double fsIn, int decim);
    void setTune(double hz);
    double audioRate() const { return audioFs_; }
    // Demodulate a block of input IQ; appends audio samples to `audioOut`.
    void process(const std::vector<cf32>& in, std::vector<float>& audioOut);
    void reset();

private:
    double fsIn_;
    double audioFs_;
    Nco nco_;
    FirDecimator dec_;
    SsbDemodulator dem_;
    Agc agc_;
};

} // namespace qo100
