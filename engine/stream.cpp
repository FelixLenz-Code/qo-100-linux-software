#include "stream.h"

#include <chrono>
#include <cmath>

namespace qo100 {

StreamEngine::StreamEngine(IqDevice& dev, int decim, int fftSize)
    : dev_(dev), fftSize_(fftSize), rx_(dev.sampleRate(), decim), spec_(fftSize) {
    blk_.resize(fftSize_);
}

bool StreamEngine::pump() {
    const size_t want = (size_t)fftSize_ * 4;
    std::vector<cf32> tmp(want);
    const size_t got = dev_.readRx(tmp.data(), want);
    if (got == 0) return false;
    stage_.insert(stage_.end(), tmp.begin(), tmp.begin() + got);

    const double t = tune_.load();
    if (t != appliedTune_) {
        rx_.setTune(t);
        appliedTune_ = t;
    }

    bool any = false;
    size_t idx = 0;
    while (stage_.size() - idx >= (size_t)fftSize_) {
        const cf32* b = stage_.data() + idx;
        spec_.compute(b, specTmp_);

        blk_.assign(b, b + fftSize_);
        rxAudio_.clear();
        rx_.process(blk_, rxAudio_);
        double sq = 0.0;
        for (float a : rxAudio_) sq += (double)a * a;
        const double rms = rxAudio_.empty() ? 0.0 : std::sqrt(sq / rxAudio_.size());

        {
            std::lock_guard<std::mutex> lk(specMutex_);
            latest_ = specTmp_;
            haveNew_ = true;
        }
        level_.store(rms);
        rows_.fetch_add(1);
        idx += fftSize_;
        any = true;
    }
    stage_.erase(stage_.begin(), stage_.begin() + idx);
    return any;
}

bool StreamEngine::latestSpectrum(std::vector<float>& out) {
    std::lock_guard<std::mutex> lk(specMutex_);
    if (!haveNew_) return false;
    out = latest_;
    haveNew_ = false;
    return true;
}

void StreamEngine::run() {
    while (running_) {
        if (!pump()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void StreamEngine::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&StreamEngine::run, this);
}

void StreamEngine::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

} // namespace qo100
