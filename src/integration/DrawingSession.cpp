#include "lightgraph/integration/drawing_session.hpp"
#include <algorithm>
#include <cstring>

namespace lightgraph::integration {
DrawingSession::~DrawingSession() { detach(); }
void DrawingSession::detach() {
    if (runtime_ && !lifetime_.expired() && assembling_)
        runtime_->releaseImage(lease_);
    runtime_ = nullptr;
    lifetime_.reset();
    lease_ = {};
    assembling_ = queued_ = poisoned_ = committed_ = false;
}
void DrawingSession::bind(DrawingRuntime& runtime) {
    const auto generation = runtime.status().generation;
    if (runtime_ != &runtime || generation_ != generation || lifetime_.expired()) {
        detach();
        runtime_ = &runtime;
        lifetime_ = runtime.lifetime();
        generation_ = generation;
        lease_ = {};
        assembling_ = queued_ = poisoned_ = committed_ = false;
        producer_ = started_ = mask_ = dropped_ = 0;
    }
    synchronize();
}
void DrawingSession::synchronize() {
    if (!runtime_)
        return;
    if (lifetime_.expired()) {
        detach();
        return;
    }
    const auto generation = runtime_->status().generation;
    if (generation != generation_) {
        bind(*runtime_);
        return;
    }
    if ((assembling_ || queued_) && !runtime_->validImageLease(lease_)) {
        assembling_ = queued_ = false;
        mask_ = 0;
    }
}
void DrawingSession::reject(const char* reason) {
    if (runtime_)
        runtime_->reject(reason);
}
DrawingStatus DrawingSession::status() {
    synchronize();
    DrawingStatus result = runtime_ ? DrawingStatus(runtime_->status()) : DrawingStatus{};
    if (assembling_ || queued_) {
        unsigned mask = mask_;
        while (mask != 0) {
            result.transferChunksReceived += static_cast<std::uint8_t>(mask & 1u);
            mask >>= 1u;
        }
        result.transferChunksExpected = static_cast<std::uint8_t>(
            (lease_.size + DRAWING_IMAGE_CHUNK_BYTES - 1u) / DRAWING_IMAGE_CHUNK_BYTES);
    }
    result.droppedTransfers = dropped_;
    return result;
}
void DrawingSession::expireTransfer(std::uint32_t nowMillis) {
    synchronize();
    if (assembling_ && nowMillis - started_ >= 500u) {
        runtime_->releaseImage(lease_);
        assembling_ = false;
        ++dropped_;
        reject("transfer_timeout");
    }
}
DrawingResult DrawingSession::present(DrawingHandle* ready) {
    const auto result = runtime_->submitImage(lease_, ready);
    if (result == DrawingResult::Applied || result == DrawingResult::NoChange) {
        assembling_ = false;
        queued_ = ready != nullptr && result == DrawingResult::Applied;
    }
    return result;
}
DrawingResult DrawingSession::imageChunk(std::uint64_t producer, std::uint32_t nowMillis,
                                         std::int32_t frameId, std::uint8_t width,
                                         std::uint8_t height, std::uint8_t chunkIndex,
                                         const std::uint8_t* bytes, std::size_t size,
                                         DrawingHandle* ready) {
    if (ready)
        *ready = {};
    expireTransfer(nowMillis);
    if (!runtime_)
        return DrawingResult::Busy;
    const std::size_t total = static_cast<std::size_t>(width) * height * 3u;
    const std::size_t offset = static_cast<std::size_t>(chunkIndex) * DRAWING_IMAGE_CHUNK_BYTES;
    const std::size_t required =
        offset < total ? std::min(DRAWING_IMAGE_CHUNK_BYTES, total - offset) : 0;
    if (queued_ || runtime_->queuedImage()) {
        if (queued_ && frameId == lease_.frameId && producer == producer_ &&
            width == lease_.width && height == lease_.height && chunkIndex < 4 &&
            required == size && required != 0 && bytes &&
            (mask_ & static_cast<std::uint8_t>(1u << chunkIndex)) != 0 &&
            std::memcmp(lease_.bytes + offset, bytes, size) == 0)
            return DrawingResult::NoChange;
        ++dropped_;
        reject("image_mailbox_busy");
        return DrawingResult::Busy;
    }
    const auto current = runtime_->status();
    const bool continuing = assembling_ && frameId == lease_.frameId;
    if (!current.supported || !bytes || frameId < 0 || width == 0 || height == 0 ||
        width > DRAWING_MAX_IMAGE_WIDTH || height > DRAWING_MAX_IMAGE_HEIGHT || chunkIndex >= 4 ||
        required == 0 || size != required || frameId < current.frameHighWater ||
        (frameId == current.frameHighWater && !continuing)) {
        if (frameId >= 0 && frameId <= current.displayedFrameId)
            return DrawingResult::NoChange;
        reject("invalid_chunk");
        return current.supported ? DrawingResult::Invalid : DrawingResult::Unsupported;
    }
    if (!assembling_ || frameId > lease_.frameId) {
        DrawingRuntime::ImageLease next;
        const auto result =
            runtime_->leaseImage(frameId, width, height, next, assembling_ ? &lease_ : nullptr);
        if (result != DrawingResult::Applied)
            return result;
        if (assembling_)
            ++dropped_;
        lease_ = next;
        assembling_ = true;
        poisoned_ = committed_ = false;
        producer_ = producer;
        started_ = nowMillis;
        mask_ = 0;
    }
    if (frameId != lease_.frameId || producer != producer_ || width != lease_.width ||
        height != lease_.height) {
        reject(producer != producer_ ? "producer_conflict" : "metadata_conflict");
        if (frameId == lease_.frameId && producer == producer_) {
            poisoned_ = true;
            ++dropped_;
        }
        return DrawingResult::Invalid;
    }
    if (poisoned_)
        return DrawingResult::Invalid;
    const auto bit = static_cast<std::uint8_t>(1u << chunkIndex);
    auto* destination = lease_.bytes + offset;
    if ((mask_ & bit) != 0) {
        if (std::memcmp(destination, bytes, size) == 0)
            return DrawingResult::NoChange;
        poisoned_ = true;
        ++dropped_;
        reject("conflicting_duplicate");
        return DrawingResult::Invalid;
    }
    std::memcpy(destination, bytes, size);
    mask_ |= bit;
    const auto expectedMask = static_cast<std::uint8_t>(
        (1u << ((total + DRAWING_IMAGE_CHUNK_BYTES - 1u) / DRAWING_IMAGE_CHUNK_BYTES)) - 1u);
    return committed_ && mask_ == expectedMask ? present(ready) : DrawingResult::Applied;
}
DrawingResult DrawingSession::imageCommit(std::uint64_t producer, std::uint32_t nowMillis,
                                          std::int32_t frameId, DrawingHandle* ready) {
    if (ready)
        *ready = {};
    expireTransfer(nowMillis);
    if (!runtime_)
        return DrawingResult::Busy;
    if (queued_ && frameId == lease_.frameId && producer == producer_)
        return DrawingResult::NoChange;
    if (frameId >= 0 && frameId <= runtime_->status().displayedFrameId)
        return DrawingResult::NoChange;
    if (!assembling_ || frameId != lease_.frameId || producer != producer_ || poisoned_) {
        reject("invalid_commit");
        return DrawingResult::Invalid;
    }
    committed_ = true;
    const auto expectedMask = static_cast<std::uint8_t>(
        (1u << ((lease_.size + DRAWING_IMAGE_CHUNK_BYTES - 1u) / DRAWING_IMAGE_CHUNK_BYTES)) - 1u);
    return mask_ == expectedMask ? present(ready) : DrawingResult::Applied;
}
} // namespace lightgraph::integration
