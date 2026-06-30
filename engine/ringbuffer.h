#pragma once
#include <cstddef>
#include <mutex>
#include <vector>

// Simple thread-safe single-producer/single-consumer ring buffer.
// Mutex-based — ample for SDR block rates and keeps the logic obviously correct.
namespace qo100 {

template <class T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : buf_(capacity + 1) {}

    // Push up to n items; returns how many fit before the buffer was full.
    size_t push(const T* p, size_t n) {
        std::lock_guard<std::mutex> lk(m_);
        const size_t cap = buf_.size();
        size_t pushed = 0;
        while (pushed < n) {
            const size_t next = (head_ + 1) % cap;
            if (next == tail_) break; // full
            buf_[head_] = p[pushed++];
            head_ = next;
        }
        return pushed;
    }

    // Pop up to n items; returns how many were available.
    size_t pop(T* p, size_t n) {
        std::lock_guard<std::mutex> lk(m_);
        size_t popped = 0;
        while (popped < n && tail_ != head_) {
            p[popped++] = buf_[tail_];
            tail_ = (tail_ + 1) % buf_.size();
        }
        return popped;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        const size_t cap = buf_.size();
        return (head_ + cap - tail_) % cap;
    }

private:
    std::vector<T> buf_;
    size_t head_ = 0, tail_ = 0;
    mutable std::mutex m_;
};

} // namespace qo100
