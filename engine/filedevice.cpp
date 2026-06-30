#include "filedevice.h"
#include "iqfile.h"

#include <algorithm>
#include <chrono>

namespace qo100 {

FileDevice::FileDevice(const std::string& path, double sampleRate, bool realtime)
    : path_(path), fs_(sampleRate), realtime_(realtime),
      ring_((size_t)std::max(1.0, sampleRate * 0.5)) {} // ~0.5 s buffer

FileDevice::~FileDevice() { stop(); }

bool FileDevice::start() {
    if (!iqfile::read(path_, data_) || data_.empty()) return false;
    pos_ = 0;
    run_ = true;
    if (realtime_) thread_ = std::thread(&FileDevice::producer, this);
    return true;
}

void FileDevice::stop() {
    run_ = false;
    if (thread_.joinable()) thread_.join();
}

size_t FileDevice::readRx(cf32* out, size_t maxCount) {
    if (data_.empty()) return 0;
    if (realtime_) return ring_.pop(out, maxCount);
    // Deterministic direct serve (tests): loop the capture.
    for (size_t i = 0; i < maxCount; ++i) {
        out[i] = data_[pos_];
        pos_ = (pos_ + 1) % data_.size();
    }
    return maxCount;
}

void FileDevice::producer() {
    const size_t chunk = std::max<size_t>(1, (size_t)(fs_ / 100.0)); // ~10 ms
    std::vector<cf32> tmp(chunk);
    while (run_) {
        for (size_t i = 0; i < chunk; ++i) {
            tmp[i] = data_[pos_];
            pos_ = (pos_ + 1) % data_.size();
        }
        size_t pushed = 0;
        while (run_ && pushed < chunk) {
            pushed += ring_.push(tmp.data() + pushed, chunk - pushed);
            if (pushed < chunk) // ring full: let the consumer catch up
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // real-time pacing
    }
}

} // namespace qo100
