#ifndef LIGHTGRAPH_INTEGRATION_DRAWING_SESSION_HPP
#define LIGHTGRAPH_INTEGRATION_DRAWING_SESSION_HPP

#pragma once
#include "drawing.hpp"
namespace lightgraph::integration {
constexpr std::size_t DRAWING_IMAGE_CHUNK_BYTES = 768;

// Serialized by the same owner/lock as its runtime. Owns only transfer metadata;
// image bytes remain in the runtime's two image slots.
class DrawingSession {
  public:
    DrawingSession() = default;
    explicit DrawingSession(DrawingRuntime& runtime) { bind(runtime); }
    ~DrawingSession();
    DrawingSession(const DrawingSession&) = delete;
    DrawingSession& operator=(const DrawingSession&) = delete;
    void bind(DrawingRuntime& runtime);
    DrawingStatus status();
    DrawingResult imageChunk(std::uint64_t producer, std::uint32_t nowMillis, std::int32_t frameId,
                             std::uint8_t width, std::uint8_t height, std::uint8_t chunkIndex,
                             const std::uint8_t* bytes, std::size_t size,
                             DrawingHandle* ready = nullptr);
    DrawingResult imageCommit(std::uint64_t producer, std::uint32_t nowMillis, std::int32_t frameId,
                              DrawingHandle* ready = nullptr);
    void expireTransfer(std::uint32_t nowMillis);

  private:
    void detach();
    void synchronize();
    void reject(const char* reason);
    DrawingResult present(DrawingHandle* ready);
    DrawingRuntime* runtime_ = nullptr;
    DrawingRuntime::Lifetime lifetime_;
    std::array<char, 17> generation_{};
    DrawingRuntime::ImageLease lease_{};
    bool assembling_ = false;
    bool queued_ = false;
    bool poisoned_ = false;
    bool committed_ = false;
    std::uint64_t producer_ = 0;
    std::uint32_t started_ = 0;
    std::uint8_t mask_ = 0;
    std::uint32_t dropped_ = 0;
};
} // namespace lightgraph::integration

#endif // LIGHTGRAPH_INTEGRATION_DRAWING_SESSION_HPP
