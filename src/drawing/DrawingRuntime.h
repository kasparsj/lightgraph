#ifndef LIGHTGRAPH_CORE_DRAWING_RUNTIME_H
#define LIGHTGRAPH_CORE_DRAWING_RUNTIME_H

#pragma once

#include <array>
#include <memory>
#include <cstddef>
#include <cstdint>

namespace lightgraph::geometry {
class GeometryProvider;
}

class State;
class TopologyObject;
class BgLight;
struct ColorRGB;

namespace lightgraph::drawing {

constexpr std::size_t DRAWING_MAX_SHAPES = 16;
constexpr std::size_t DRAWING_MAX_IMAGE_WIDTH = 32;
constexpr std::size_t DRAWING_MAX_IMAGE_HEIGHT = 32;
constexpr std::size_t DRAWING_MAX_IMAGE_BYTES =
    DRAWING_MAX_IMAGE_WIDTH * DRAWING_MAX_IMAGE_HEIGHT * 3;

enum class DrawingMode : std::uint8_t { Palette = 0, Drawing = 1 };
enum class DrawingPlacement : std::uint8_t { Background = 0, Overlay = 1 };
enum class DrawingSource : std::uint8_t { Scene = 0, Image = 1 };
enum class DrawingBaseKind : std::uint8_t { Solid = 0, Gradient = 1 };
enum class DrawingShapeKind : std::uint8_t {
    Band = 1,
    Circle = 2,
    Ring = 3,
    Line = 4,
    Rectangle = 5,
};
enum class DrawingResult : std::uint8_t {
    Applied = 0,
    NoChange,
    Invalid,
    Unsupported,
    Busy,
    Stale,
};

enum class DrawingHandleKind : std::uint8_t { None = 0, Scene = 1, Image = 2 };

struct DrawingHandle {
    DrawingHandleKind kind = DrawingHandleKind::None;
    std::uint8_t slot = 0;
    std::uint32_t slotGeneration = 0;
    std::uint64_t runtimeGeneration = 0;
};

struct DrawingShape {
    DrawingShapeKind kind = DrawingShapeKind::Band;
    std::uint8_t id = 0;
    std::uint32_t rgb = 0;
    std::uint8_t opacity = 255;
    std::array<float, 6> params{};
};

struct DrawingScene {
    DrawingBaseKind baseKind = DrawingBaseKind::Solid;
    std::uint32_t rgb1 = 0;
    std::uint32_t rgb2 = 0;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float rotationDegrees = 0.0f;
    std::uint8_t shapeCount = 0;
    std::array<DrawingShape, DRAWING_MAX_SHAPES> shapes{};
};

struct DrawingPrecondition {
    bool required = false;
    std::array<char, 17> generation{};
    std::uint64_t revision = 0;
};

struct DrawingControl {
    bool hasEnabled = false;
    bool enabled = false;
    bool hasVisible = false;
    bool visible = false;
    bool hasRotation = false;
    float rotationDegrees = 0.0f;
    bool hasPlacement = false;
    DrawingPlacement placement = DrawingPlacement::Background;
    bool hasBlendMode = false;
    std::uint8_t blendMode = 0;
    bool hasClipToContent = false;
    bool clipToContent = false;
    bool clear = false;
};

struct DrawingStatus {
    bool supported = false;
    std::array<char, 17> generation{};
    std::uint64_t revision = 0;
    DrawingMode mode = DrawingMode::Palette;
    bool visible = false;
    DrawingPlacement placement = DrawingPlacement::Background;
    std::uint8_t blendMode = 0;
    bool clipToContent = false;
    std::uint8_t brightness = 255;
    bool compositingSupported = false;
    DrawingSource activeSource = DrawingSource::Scene;
    float rotationDegrees = 0.0f;
    std::uint8_t shapeCount = 0;
    std::int32_t displayedFrameId = -1;
    std::int32_t frameHighWater = -1;
    std::uint8_t imageWidth = 0;
    std::uint8_t imageHeight = 0;
    std::uint32_t acceptedFrames = 0;
    std::array<char, 64> lastRejection{{'n', 'o', 'n', 'e', '\0'}};
};

// Callers must serialize drawing mutation, topology mutation, and rendering on one
// owner thread or under the same recursive lock. The RGB cache updates in place.
class DrawingRuntime {
  public:
    DrawingRuntime(const DrawingRuntime&) = delete;
    DrawingRuntime& operator=(const DrawingRuntime&) = delete;
    ~DrawingRuntime();

    // A non-owning runtime lifetime guard. Access follows the runtime's owner lock.
    class Lifetime {
      public:
        Lifetime() = default;
        Lifetime(const Lifetime& other) : token_(other.token_) { retain(); }
        Lifetime(Lifetime&& other) noexcept : token_(other.token_) { other.token_ = nullptr; }
        Lifetime& operator=(Lifetime&& other) noexcept {
            if (this != &other) {
                reset();
                token_ = other.token_;
                other.token_ = nullptr;
            }
            return *this;
        }
        Lifetime& operator=(const Lifetime& other) {
            if (this != &other) {
                reset();
                token_ = other.token_;
                retain();
            }
            return *this;
        }
        ~Lifetime() { reset(); }
        bool expired() const { return !token_ || !token_->alive; }
        void reset() {
            if (token_ && --token_->references == 0)
                delete token_;
            token_ = nullptr;
        }

      private:
        friend class DrawingRuntime;
        struct Token {
            std::size_t references = 1;
            bool alive = true;
        };
        explicit Lifetime(Token* token) : token_(token) {}
        void retain() {
            if (token_)
                ++token_->references;
        }
        Token* token_ = nullptr;
    };
    Lifetime lifetime() const;
    DrawingStatus status();
    DrawingScene scene() const;
    ::ColorRGB sampleColor(std::uint16_t pixel);
    DrawingResult refreshGeometry();
    DrawingResult replaceScene(const DrawingScene& scene,
                               const DrawingPrecondition* precondition = nullptr);
    DrawingResult queueScene(const DrawingScene& scene, DrawingHandle& handle);
    DrawingResult applyQueued(const DrawingHandle& handle);
    bool validQueued(const DrawingHandle& handle) const;
    void releaseQueued(const DrawingHandle& handle);
    bool setGeneration(std::uint64_t generation);
    void reject(const char* reason);
    DrawingResult control(const DrawingControl& control,
                          const DrawingPrecondition* precondition = nullptr);
    DrawingResult setFill(std::uint32_t rgb);
    DrawingResult setGradient(float x1, float y1, float x2, float y2, std::uint32_t rgb1,
                              std::uint32_t rgb2);
    DrawingResult setShape(const DrawingShape& shape);
    DrawingResult removeShape(std::uint8_t id);
    DrawingResult setRotation(float degrees);
    DrawingResult clear();
    DrawingResult setEnabled(bool enabled);
    DrawingResult setVisible(bool visible);
    DrawingResult setComposite(DrawingPlacement placement, std::uint8_t blendMode,
                               bool clipToContent);

    DrawingResult applyImage(std::int32_t frameId, std::uint8_t width, std::uint8_t height,
                             const std::uint8_t* rgb, std::size_t size,
                             const DrawingPrecondition* precondition = nullptr);
    // Lease the inactive image slot; ownership stays with the runtime.
    struct ImageLease {
        DrawingHandle handle{};
        std::uint8_t* bytes = nullptr;
        std::size_t size = 0;
        std::int32_t frameId = -1;
        std::uint8_t width = 0;
        std::uint8_t height = 0;
    };
    DrawingResult leaseImage(std::int32_t frameId, std::uint8_t width, std::uint8_t height,
                             ImageLease& lease, const ImageLease* previous = nullptr);
    DrawingResult submitImage(const ImageLease& lease, DrawingHandle* ready = nullptr);
    void releaseImage(const ImageLease& lease);
    bool validImageLease(const ImageLease& lease) const;
    bool queuedImage() const;
    DrawingMode mode() const;
    DrawingPlacement placement() const;
    bool clipToContent() const;
    ::ColorRGB layerColor(std::uint16_t pixel);
    const lightgraph::geometry::GeometryProvider* geometry() const;

    static bool validateScene(const DrawingScene& scene);

  private:
    friend class ::State;
    class Impl;
    DrawingRuntime() = default;
    bool initialize(::TopologyObject& object, ::BgLight& background);
    void synchronize(::TopologyObject& object, ::BgLight* background);
    void shutdown();
    Impl* impl_ = nullptr;
    Lifetime lifetime_;
};

} // namespace lightgraph::drawing

#endif // LIGHTGRAPH_CORE_DRAWING_RUNTIME_H
