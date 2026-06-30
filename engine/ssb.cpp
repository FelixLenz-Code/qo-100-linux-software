#include "ssb.h"
#include <algorithm>
#include <cmath>

namespace qo100 {

namespace {
constexpr double kTwoPi = 6.283185307179586476925287;

void wrap(double& phase) {
    if (phase >= kTwoPi) phase -= kTwoPi;
    else if (phase < 0.0) phase += kTwoPi;
}
} // namespace

HilbertFIR::HilbertFIR(int numTaps) {
    if (numTaps % 2 == 0) ++numTaps; // need odd length, center tap = 0
    taps_.resize(numTaps);
    buf_.assign(numTaps, 0.0f);
    const int M = numTaps / 2;
    for (int k = 0; k < numTaps; ++k) {
        const int m = k - M;
        double h = 0.0;
        if (m != 0 && (m % 2 != 0)) {
            h = 2.0 / (M_PI * m); // ideal Hilbert response, odd taps only
        }
        // Blackman window to tame the truncation ripple.
        const double w = 0.42
            - 0.5 * std::cos(kTwoPi * k / (numTaps - 1))
            + 0.08 * std::cos(2.0 * kTwoPi * k / (numTaps - 1));
        taps_[k] = static_cast<float>(h * w);
    }
}

void HilbertFIR::reset() {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    pos_ = 0;
}

void HilbertFIR::process(float x, float& inPhase, float& quad) {
    const int N = static_cast<int>(taps_.size());
    buf_[pos_] = x;
    float acc = 0.0f;
    for (int k = 0; k < N; ++k) {
        int idx = pos_ - k;
        if (idx < 0) idx += N;
        acc += taps_[k] * buf_[idx];
    }
    const int c = N / 2;
    int cidx = pos_ - c;
    if (cidx < 0) cidx += N;
    inPhase = buf_[cidx];
    quad = acc;
    pos_ = (pos_ + 1) % N;
}

// ---- Modulator ----

SsbModulator::SsbModulator(double sampleRate, double carrierHz, int hilbertTaps)
    : hil_(hilbertTaps), fs_(sampleRate) {
    setCarrier(carrierHz);
}

void SsbModulator::setCarrier(double hz) { dphi_ = kTwoPi * hz / fs_; }

void SsbModulator::reset() {
    hil_.reset();
    phase_ = 0.0;
}

cf32 SsbModulator::process(float audio) {
    float i, q;
    hil_.process(audio, i, q);           // analytic signal a + j*H{a}
    const cf32 analytic(i, q);           // positive frequencies only
    const cf32 osc(static_cast<float>(std::cos(phase_)),
                   static_cast<float>(std::sin(phase_)));
    phase_ += dphi_;
    wrap(phase_);
    return analytic * osc;               // shift up to the carrier -> USB
}

// ---- Demodulator ----

SsbDemodulator::SsbDemodulator(double sampleRate, double carrierHz, int hilbertTaps)
    : hil_(hilbertTaps), idelay_(HilbertFIR(hilbertTaps).delay()), fs_(sampleRate) {
    setCarrier(carrierHz);
}

void SsbDemodulator::setCarrier(double hz) { dphi_ = kTwoPi * hz / fs_; }

void SsbDemodulator::reset() {
    hil_.reset();
    idelay_.reset();
    phase_ = 0.0;
}

float SsbDemodulator::process(cf32 iq) {
    const cf32 osc(static_cast<float>(std::cos(phase_)),
                   static_cast<float>(-std::sin(phase_)));
    phase_ += dphi_;
    wrap(phase_);
    const cf32 y = iq * osc;             // tune carrier to 0 Hz
    float qd, hilQ;
    hil_.process(y.imag(), qd, hilQ);    // H{Q}
    const float iD = idelay_.process(y.real());
    return 0.5f * (iD - hilQ);           // USB; rejects LSB image
}

} // namespace qo100
