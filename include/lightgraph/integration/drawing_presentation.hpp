#ifndef LIGHTGRAPH_INTEGRATION_DRAWING_PRESENTATION_HPP
#define LIGHTGRAPH_INTEGRATION_DRAWING_PRESENTATION_HPP

#pragma once
#include "drawing.hpp"

namespace lightgraph::integration {

// Serialized by the runtime's owner. Owns handles, never scene/image storage.
class PendingDrawingPresentation {
  public:
    static constexpr std::size_t capacity = 3;
    PendingDrawingPresentation() = default;
    ~PendingDrawingPresentation() { reset(); }
    PendingDrawingPresentation(const PendingDrawingPresentation&) = delete;
    PendingDrawingPresentation& operator=(const PendingDrawingPresentation&) = delete;

    void bind(DrawingRuntime& runtime) {
        const auto generation = runtime.status().generation;
        if (runtime_ != &runtime || lifetime_.expired() || generation_ != generation) {
            reset();
            runtime_ = &runtime;
            lifetime_ = runtime.lifetime();
            generation_ = generation;
        }
    }

    bool enqueue(const DrawingHandle& handle) {
        if (!synchronize() || !runtime_->validQueued(handle))
            return false;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& entry = handles_[i];
            if (entry.kind == handle.kind && entry.slot == handle.slot &&
                entry.slotGeneration == handle.slotGeneration &&
                entry.runtimeGeneration == handle.runtimeGeneration)
                return true;
        }
        if (count_ == capacity) {
            runtime_->releaseQueued(handle);
            runtime_->reject("performance_queue_full");
            return false;
        }
        handles_[count_++] = handle;
        if (!blocked_)
            drain();
        return true;
    }

    // Call once per owner-loop iteration. Busy keeps FIFO ownership for retry.
    DrawingResult poll() {
        if (!synchronize())
            return DrawingResult::Stale;
        blocked_ = false;
        return drain();
    }

    void reset() {
        if (runtime_ && !lifetime_.expired()) {
            for (std::size_t i = 0; i < count_; ++i)
                runtime_->releaseQueued(handles_[i]);
        }
        count_ = 0;
        blocked_ = false;
        runtime_ = nullptr;
        lifetime_.reset();
        generation_ = {};
    }
    std::size_t size() const { return count_; }

  private:
    DrawingResult drain() {
        DrawingResult result = DrawingResult::NoChange;
        while (count_ != 0) {
            result = runtime_->applyQueued(handles_[0]);
            if (result == DrawingResult::Busy) {
                blocked_ = true;
                return result;
            }
            if (result != DrawingResult::Applied && result != DrawingResult::NoChange)
                runtime_->releaseQueued(handles_[0]);
            for (std::size_t i = 1; i < count_; ++i)
                handles_[i - 1] = handles_[i];
            --count_;
        }
        return result;
    }

    bool synchronize() {
        if (!runtime_ || lifetime_.expired()) {
            reset();
            return false;
        }
        if (runtime_->status().generation != generation_) {
            reset();
            return false;
        }
        // Core fallback or explicit release can retire slots without changing generation.
        std::size_t live = 0;
        for (std::size_t i = 0; i < count_; ++i) {
            if (runtime_->validQueued(handles_[i]))
                handles_[live++] = handles_[i];
        }
        count_ = live;
        if (count_ == 0)
            blocked_ = false;
        return true;
    }
    DrawingRuntime* runtime_ = nullptr;
    DrawingRuntime::Lifetime lifetime_;
    std::array<char, 17> generation_{};
    std::array<DrawingHandle, capacity> handles_{};
    std::size_t count_ = 0;
    bool blocked_ = false;
};

} // namespace lightgraph::integration
#endif
