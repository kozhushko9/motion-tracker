#pragma once
#include <atomic>
#include <vector>

template <typename T>
class LockFreeRingBuffer {
   public:
    explicit LockFreeRingBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity), head_(0), tail_(0) {
    }

    // Producer: NEVER blocks, drops oldest if full
    bool push(const T& item) {
        bool dropped = false;

        // only the producer writes this, so no other thread can race on it
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = increment(tail);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            // buffer full → DROP oldest
            head_.store(increment(head_.load(std::memory_order_relaxed)),
                        std::memory_order_release);
            dropped = true;
        }

        buffer_[tail] = item;
        tail_.store(next_tail, std::memory_order_release);

        return dropped;
    }

    // Consumer
    bool pop(T& item) {
        // only the consumer writes head_, so relaxed is fine — no race
        size_t head = head_.load(std::memory_order_relaxed);

        // acquire pairs with the producer's release store to tail_
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;  // empty
        }

        item = buffer_[head];
        head_.store(increment(head), std::memory_order_release);
        return true;
    }

    size_t size() const {
        // both are "acquire" because "size()" can be called
        // from any thread and needs a consistent snapshot.
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);

        if (tail >= head) return tail - head;
        return capacity_ - (head - tail);
    }

   private:
    size_t increment(size_t i) const {
        return (i + 1) % capacity_;
    }

   private:
    static constexpr size_t kCacheLineSize = 64;

    std::vector<T> buffer_;
    const size_t capacity_;

    alignas(kCacheLineSize) std::atomic<size_t> head_;  // consumer
    alignas(kCacheLineSize) std::atomic<size_t> tail_;  // producer
};
