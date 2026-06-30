#pragma once
#include "device.h"
#include "ringbuffer.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

// Hardware-free IqDevice: streams a .cf32 capture, looping forever. In
// real-time mode a producer thread paces samples at the configured rate (for
// the GUI); in non-real-time mode readRx serves directly and deterministically
// (for tests).
namespace qo100 {

class FileDevice : public IqDevice {
public:
    FileDevice(const std::string& path, double sampleRate, bool realtime = true);
    ~FileDevice() override;

    bool start() override;
    void stop() override;
    size_t readRx(cf32* out, size_t maxCount) override;
    double sampleRate() const override { return fs_; }
    const char* name() const override { return "FileDevice"; }

    bool loaded() const { return !data_.empty(); }

private:
    void producer();

    std::string path_;
    double fs_;
    bool realtime_;
    std::vector<cf32> data_;
    size_t pos_ = 0;
    RingBuffer<cf32> ring_;
    std::thread thread_;
    std::atomic<bool> run_{false};
};

} // namespace qo100
