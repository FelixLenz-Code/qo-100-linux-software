#include "rx.h"
#include <algorithm>
#include <cmath>

namespace qo100 {

namespace {
constexpr double kTwoPi = 6.283185307179586476925287;

void wrap(double& p) {
    if (p >= kTwoPi) p -= kTwoPi;
    else if (p < 0.0) p += kTwoPi;
}
} // namespace

// ---- Nco ----

void Nco::setFreq(double hz, double fs) { dphi_ = kTwoPi * hz / fs; }

cf32 Nco::mixDown(cf32 x) {
    const cf32 osc(static_cast<float>(std::cos(phase_)),
                   static_cast<float>(-std::sin(phase_)));
    phase_ += dphi_;
    wrap(phase_);
    return x * osc;
}

cf32 Nco::mixUp(cf32 x) {
    const cf32 osc(static_cast<float>(std::cos(phase_)),
                   static_cast<float>(std::sin(phase_)));
    phase_ += dphi_;
    wrap(phase_);
    return x * osc;
}

// ---- FirDecimator ----

FirDecimator::FirDecimator(int decim, double fsIn, double audioFs) : decim_(decim) {
    // Cutoff a little below the output Nyquist to leave a transition band.
    const double cutoff = 0.45 * audioFs;
    const double fcn = cutoff / fsIn; // normalised to input rate (0..0.5)
    int numTaps = 16 * decim + 1;     // odd; longer decimation -> sharper filter
    numTaps = std::clamp(numTaps, 31, 511);
    if (numTaps % 2 == 0) ++numTaps;
    taps_.resize(numTaps);
    const int M = numTaps / 2;
    double sum = 0.0;
    for (int k = 0; k < numTaps; ++k) {
        const int m = k - M;
        double s; // windowed-sinc low-pass
        if (m == 0) s = 2.0 * fcn;
        else s = std::sin(kTwoPi * fcn * m) / (M_PI * m);
        const double w = 0.54 - 0.46 * std::cos(kTwoPi * k / (numTaps - 1)); // Hamming
        s *= w;
        taps_[k] = static_cast<float>(s);
        sum += s;
    }
    for (auto& t : taps_) t = static_cast<float>(t / sum); // unity DC gain
    buf_.assign(numTaps, cf32(0.0f, 0.0f));
}

void FirDecimator::reset() {
    std::fill(buf_.begin(), buf_.end(), cf32(0.0f, 0.0f));
    pos_ = 0;
    count_ = 0;
}

bool FirDecimator::process(cf32 x, cf32& out) {
    const int N = static_cast<int>(taps_.size());
    buf_[pos_] = x;
    pos_ = (pos_ + 1) % N; // pos_ now points one past the newest sample
    if (++count_ < decim_) return false;
    count_ = 0;
    cf32 acc(0.0f, 0.0f);
    for (int k = 0; k < N; ++k) {
        int idx = pos_ - 1 - k; // taps_[0] aligns with the newest sample
        if (idx < 0) idx += N;
        acc += taps_[k] * buf_[idx];
    }
    out = acc;
    return true;
}

// ---- Agc ----

Agc::Agc(double audioFs, float target) : target_(target) {
    envDecay_ = static_cast<float>(std::exp(-1.0 / (0.300 * audioFs)));        // ~300 ms hold
    gainDown_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.002 * audioFs)));  // ~2 ms attack
    gainUp_ = static_cast<float>(1.0 - std::exp(-1.0 / (1.000 * audioFs)));    // ~1 s release
}

void Agc::reset() {
    peak_ = 1e-6f;
    gain_ = 1.0f;
}

float Agc::process(float x) {
    const float a = std::fabs(x);
    peak_ = std::max(a, peak_ * envDecay_);
    float desired = target_ / (peak_ + 1e-6f);
    if (desired > maxGain_) desired = maxGain_;
    const float c = (desired < gain_) ? gainDown_ : gainUp_; // fast down, slow up
    gain_ += c * (desired - gain_);
    return x * gain_;
}

// ---- RxChain ----

RxChain::RxChain(double fsIn, int decim)
    : fsIn_(fsIn),
      audioFs_(fsIn / decim),
      dec_(decim, fsIn, fsIn / decim),
      dem_(fsIn / decim, 0.0), // carrier tuned to 0 Hz by the Nco
      channel_(fsIn / decim, 300.0, 2700.0),
      agc_(fsIn / decim) {}

void RxChain::setTune(double hz) { nco_.setFreq(hz, fsIn_); }

void RxChain::reset() {
    nco_.reset();
    dec_.reset();
    dem_.reset();
    channel_.reset();
    agc_.reset();
}

void RxChain::process(const std::vector<cf32>& in, std::vector<float>& audioOut) {
    cf32 dec;
    for (const cf32 s : in) {
        const cf32 tuned = nco_.mixDown(s);
        if (dec_.process(tuned, dec)) {
            const float audio = channel_.process(dem_.process(dec)); // SSB channel filter
            audioOut.push_back(agc_.process(audio));
        }
    }
}

} // namespace qo100
