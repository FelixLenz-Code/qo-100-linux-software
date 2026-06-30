#pragma once
#include "device.h"
#include "rx.h"
#include "spectrum.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

// Real-time receive engine: pulls IQ blocks from any IqDevice, produces
// waterfall spectrum rows and demodulated-audio level. The GUI polls the
// thread-safe snapshots; the device and engine run on their own threads.
namespace qo100 {

class StreamEngine {
public:
    StreamEngine(IqDevice& dev, int decim, int fftSize = 1024);

    void start(); // launch the background processing thread
    void stop();
    void setTune(double hz) { tune_.store(hz); }

    // Process one device read worth of data (used by run(); also callable
    // directly for deterministic tests). Returns true if a row was produced.
    bool pump();

    // Copy the most recent spectrum row; returns false if nothing new.
    bool latestSpectrum(std::vector<float>& out);
    double audioLevel() const { return level_.load(); }
    size_t rowsProduced() const { return rows_.load(); }
    int fftSize() const { return fftSize_; }

private:
    void run();

    IqDevice& dev_;
    int fftSize_;
    RxChain rx_;
    Spectrum spec_;

    std::vector<cf32> stage_, blk_;
    std::vector<float> specTmp_, latest_;
    std::vector<float> rxAudio_;
    double appliedTune_ = 1e18;

    std::atomic<double> tune_{0.0};
    std::atomic<double> level_{0.0};
    std::atomic<size_t> rows_{0};
    std::atomic<bool> running_{false};
    bool haveNew_ = false;
    std::mutex specMutex_;
    std::thread thread_;
};

} // namespace qo100
