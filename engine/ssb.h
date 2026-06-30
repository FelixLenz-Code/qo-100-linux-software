#pragma once
#include <complex>
#include <vector>

// Self-contained SSB (USB) engine for QO-100 — no external dependencies.
// Everything operates on complex baseband IQ at a given sample rate.
namespace qo100 {

using cf32 = std::complex<float>;

// Linear-phase Hilbert-transform FIR (90 degree phase shifter).
// process() returns the delay-matched in-phase sample and the quadrature
// (90 degree shifted) sample for the same input.
class HilbertFIR {
public:
    explicit HilbertFIR(int numTaps = 101); // forced odd
    void process(float x, float& inPhase, float& quad);
    int delay() const { return static_cast<int>(taps_.size()) / 2; }
    void reset();

private:
    std::vector<float> taps_;
    std::vector<float> buf_;
    int pos_ = 0;
};

// Pure delay line of N samples (used to delay-match the in-phase path).
class DelayLine {
public:
    explicit DelayLine(int n) : buf_(n + 1, 0.0f) {}
    float process(float x) {
        buf_[pos_] = x;
        pos_ = (pos_ + 1) % static_cast<int>(buf_.size());
        return buf_[pos_];
    }
    void reset() { std::fill(buf_.begin(), buf_.end(), 0.0f); pos_ = 0; }

private:
    std::vector<float> buf_;
    int pos_ = 0;
};

// USB modulator: real audio -> complex baseband IQ.
// The suppressed carrier sits at carrierHz inside the IQ band; audio
// frequencies appear above it (upper sideband).
class SsbModulator {
public:
    SsbModulator(double sampleRate, double carrierHz, int hilbertTaps = 101);
    cf32 process(float audio);
    void setCarrier(double hz);
    void reset();

private:
    HilbertFIR hil_;
    double fs_;
    double phase_ = 0.0;
    double dphi_;
};

// USB demodulator: complex baseband IQ -> real audio.
// Tunes the suppressed carrier to 0 Hz, then keeps the upper sideband and
// rejects the lower-sideband image via the phasing method.
class SsbDemodulator {
public:
    SsbDemodulator(double sampleRate, double carrierHz, int hilbertTaps = 101);
    float process(cf32 iq);
    void setCarrier(double hz);
    void reset();

private:
    HilbertFIR hil_;
    DelayLine idelay_;
    double fs_;
    double phase_ = 0.0;
    double dphi_;
};

} // namespace qo100
