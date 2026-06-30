#pragma once
#include <complex>
#include <cstddef>

// Abstraction over an IQ source/sink. The real Pluto (libiio) and the
// hardware-free FileDevice both implement this, so the streaming engine and
// GUI never need to know which one they are driving.
namespace qo100 {

using cf32 = std::complex<float>;

class IqDevice {
public:
    virtual ~IqDevice() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // Non-blocking: copy up to maxCount received samples into out, return count.
    virtual size_t readRx(cf32* out, size_t maxCount) = 0;

    // Submit TX samples (no-op for receive-only devices).
    virtual void writeTx(const cf32*, size_t) {}

    virtual double sampleRate() const = 0;
    virtual const char* name() const = 0;
};

} // namespace qo100
