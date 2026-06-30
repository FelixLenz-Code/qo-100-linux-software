#pragma once
#include "audiofilter.h"
#include "rx.h" // Nco
#include "ssb.h"
#include <complex>
#include <vector>

// Transmit chain: audio -> USB modulate -> interpolate to the wide TX rate ->
// mix up to the wanted offset. Mirror image of RxChain.
namespace qo100 {

using cf32 = std::complex<float>;

// Low-pass FIR interpolator (zero-stuff + anti-imaging filter), complex input.
class FirInterpolator {
public:
    FirInterpolator(int interp, double fsOut, double audioFs);
    // Push one audio-rate sample; appends `interp` output-rate samples.
    void process(cf32 x, std::vector<cf32>& out);
    void reset();
    int interpolation() const { return interp_; }

private:
    std::vector<float> taps_;
    std::vector<cf32> buf_;
    int pos_ = 0;
    int interp_;
};

// Full TX chain. `setTune` places the suppressed carrier at the given offset
// (Hz, relative to the centre of the output band).
class TxChain {
public:
    TxChain(double fsOut, int interp);
    void setTune(double hz);
    double audioRate() const { return audioFs_; }
    void process(const std::vector<float>& audioIn, std::vector<cf32>& iqOut);
    void reset();

private:
    double fsOut_;
    double audioFs_;
    BandpassFIR channel_;  // 300-2700 Hz: keep TX inside the band plan
    SsbModulator mod_;     // baseband USB (carrier at 0 Hz)
    FirInterpolator filt_; // up to the wide TX rate
    Nco nco_;              // mix up to the tune offset
};

} // namespace qo100
