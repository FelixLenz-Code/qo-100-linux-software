#include "tx.h"
#include <algorithm>
#include <cmath>

namespace qo100 {

namespace {
constexpr double kTwoPi = 6.283185307179586476925287;
} // namespace

// ---- FirInterpolator ----

FirInterpolator::FirInterpolator(int interp, double fsOut, double audioFs)
    : interp_(interp) {
    const double cutoff = 0.45 * audioFs;
    const double fcn = cutoff / fsOut; // relative to the (high) output rate
    int numTaps = 16 * interp + 1;
    numTaps = std::clamp(numTaps, 31, 1023);
    if (numTaps % 2 == 0) ++numTaps;
    taps_.resize(numTaps);
    const int M = numTaps / 2;
    double sum = 0.0;
    for (int k = 0; k < numTaps; ++k) {
        const int m = k - M;
        double s;
        if (m == 0) s = 2.0 * fcn;
        else s = std::sin(kTwoPi * fcn * m) / (M_PI * m);
        const double w = 0.54 - 0.46 * std::cos(kTwoPi * k / (numTaps - 1)); // Hamming
        s *= w;
        taps_[k] = static_cast<float>(s);
        sum += s;
    }
    // Unity passband gain after zero-stuffing: normalise to unity DC, then the
    // per-sample scale-by-interp in process() compensates the stuffing loss.
    for (auto& t : taps_) t = static_cast<float>(t / sum);
    buf_.assign(numTaps, cf32(0.0f, 0.0f));
}

void FirInterpolator::reset() {
    std::fill(buf_.begin(), buf_.end(), cf32(0.0f, 0.0f));
    pos_ = 0;
}

void FirInterpolator::process(cf32 x, std::vector<cf32>& out) {
    const int N = static_cast<int>(taps_.size());
    for (int s = 0; s < interp_; ++s) {
        // Zero-stuff: only the first phase carries the sample (scaled to keep
        // the passband level after the L-fold rate increase).
        buf_[pos_] = (s == 0) ? x * static_cast<float>(interp_) : cf32(0.0f, 0.0f);
        pos_ = (pos_ + 1) % N;
        cf32 acc(0.0f, 0.0f);
        for (int k = 0; k < N; ++k) {
            int idx = pos_ - 1 - k;
            if (idx < 0) idx += N;
            acc += taps_[k] * buf_[idx];
        }
        out.push_back(acc);
    }
}

// ---- TxChain ----

TxChain::TxChain(double fsOut, int interp)
    : fsOut_(fsOut),
      audioFs_(fsOut / interp),
      mod_(fsOut / interp, 0.0), // baseband USB at audio rate
      filt_(interp, fsOut, fsOut / interp) {}

void TxChain::setTune(double hz) { nco_.setFreq(hz, fsOut_); }

void TxChain::reset() {
    mod_.reset();
    filt_.reset();
    nco_.reset();
}

void TxChain::process(const std::vector<float>& audioIn, std::vector<cf32>& iqOut) {
    for (const float a : audioIn) {
        const cf32 bb = mod_.process(a); // complex baseband USB
        const size_t before = iqOut.size();
        filt_.process(bb, iqOut);        // interpolate to fsOut
        for (size_t i = before; i < iqOut.size(); ++i)
            iqOut[i] = nco_.mixUp(iqOut[i]); // shift to the tune offset
    }
}

} // namespace qo100
