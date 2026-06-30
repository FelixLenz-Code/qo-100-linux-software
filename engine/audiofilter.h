#pragma once
#include <vector>

// Windowed-sinc FIR band-pass for the SSB audio channel. QO-100 allows a
// maximum signal bandwidth of 2.7 kHz, so the receive audio (and transmit
// audio) is limited to ~300-2700 Hz: this gives real SSB selectivity on RX
// and keeps the transmitted signal inside the band plan on TX.
namespace qo100 {

class BandpassFIR {
public:
    BandpassFIR(double sampleRate, double lowHz, double highHz, int numTaps = 511);
    float process(float x);
    void reset();

private:
    std::vector<float> taps_;
    std::vector<float> buf_;
    int pos_ = 0;
};

} // namespace qo100
