#pragma once
#include <array>
#include <atomic>
#include <mutex>
#include <utility>

// Latest complete state wins. Serialized producers; one audio consumer.
// The reader owns its front slot until the next exchange. The writer can
// only reuse the back/middle slots, so payload reads never race with writes.
template <typename State>
class LatestControl {
public:
    template <typename Update>
    void update(Update&& updateState) {
        std::lock_guard<std::mutex> lock(mProducerMutex);
        std::forward<Update>(updateState)(mDesired);
        mSlots[mBack] = mDesired;
        mBack = mMiddle.exchange(mBack | kDirty, std::memory_order_acq_rel) & kIndex;
    }

    bool consume(State& state) {
        if ((mMiddle.load(std::memory_order_acquire) & kDirty) == 0) return false;
        mFront = mMiddle.exchange(mFront, std::memory_order_acq_rel) & kIndex;
        state = mSlots[mFront];
        return true;
    }

private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static constexpr unsigned kDirty = 4;
    static constexpr unsigned kIndex = 3;
    std::mutex mProducerMutex;
    State mDesired{};
    std::array<State, 3> mSlots{};
    std::atomic<unsigned> mMiddle{1};
    unsigned mBack = 2;  // Producer only.
    unsigned mFront = 0; // Consumer only.
};
