#ifndef LIGHTGRAPH_CORE_DRAWING_RUNTIME_INTERNAL_H
#define LIGHTGRAPH_CORE_DRAWING_RUNTIME_INTERNAL_H

#include "DrawingRuntime.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "../Globals.h"

namespace lightgraph::geometry {
class GeometryProvider;
}

class BgLight;
class TopologyObject;
struct ColorRGB;

namespace lightgraph::drawing::detail {

struct Vec2 {
    double x;
    double y;
};

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

bool finite(float value);
bool validRgb(std::uint32_t rgb);
float normalizedDegrees(float degrees);
bool sameFloat(float a, float b);
bool sameScene(const DrawingScene& a, const DrawingScene& b);
bool canonicalizeScene(const DrawingScene& value, DrawingScene& canonical);
inline Vec2 rotate(Vec2 point, double degrees) {
    constexpr double kPi = 3.1415926535897932384626433832795;
    const double radians = degrees * kPi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return {c * point.x + s * point.y, -s * point.x + c * point.y};
}
Rgb unpack(std::uint32_t rgb);
Rgb composite(Rgb destination, Rgb source, std::uint8_t opacity);
double distanceToSegment(Vec2 p, Vec2 a, Vec2 b);
void reportAllocationFailure(const TopologyObject& object, std::uint16_t stage, std::size_t bytes);

} // namespace lightgraph::drawing::detail

namespace lightgraph::drawing {

class DrawingRuntime::Impl {
  public:
    Impl(TopologyObject& topology, BgLight& bg);

    bool ensureGeometry();
    DrawingMode modeFromBackground() const;
    void reconcile();
    bool preconditionMatches(const DrawingPrecondition* precondition);
    void changed();
    void rememberComposition();
    void reject(const char* reason);
    bool validQueued(const DrawingHandle& handle) const;
    void retireQueued();
    void invalidatePendingStorage();
    void enterUnsupportedFallback();
    bool geometryCoordinatesValid();
    bool ensureResources();
    bool ensureImageResources();
    bool prepareRaster();
    detail::Rgb baseColor(const DrawingScene& value, detail::Vec2 point) const;
    bool shapeContains(const DrawingShape& shape, detail::Vec2 point, detail::Vec2 original,
                       float sceneRotation) const;
    bool rasterizeScene(const DrawingScene& value);
    detail::Rgb sampleImage(const std::uint8_t* image, std::uint8_t width, std::uint8_t height,
                            detail::Vec2 point) const;
    bool rasterizeImage(const std::uint8_t* image, std::uint8_t width, std::uint8_t height,
                        float rotationDegrees);
    DrawingResult presentImage(std::int32_t frameId, std::uint8_t width, std::uint8_t height,
                               int slot, const DrawingPrecondition* precondition);
    ColorRGB color(std::uint16_t pixel);

    TopologyObject& object;
    BgLight& background;
    std::size_t ledCount;
    bool supported = false;
    bool resourcesReady = false;
    DrawingResult rasterFailure = DrawingResult::Unsupported;
    std::unique_ptr<lightgraph::geometry::GeometryProvider> geometry;
    DrawingMode drawingMode = DrawingMode::Palette;
    DrawingPlacement drawingPlacement = DrawingPlacement::Background;
    bool drawingClipToContent = false;
    std::unique_ptr<std::uint8_t[]> cache;
    std::uint32_t cacheGeometryRevision = 0;
    std::uint32_t deferredGeometryRevision = 0;
    DrawingScene sceneValue{};
    DrawingSource source = DrawingSource::Scene;
    std::array<char, 17> generation{};
    std::uint64_t runtimeGeneration = 0;
    std::uint64_t revision = 0;
    DrawingMode lastMode = DrawingMode::Palette;
    bool lastVisible = false;
    DrawingPlacement lastPlacement = DrawingPlacement::Background;
    BlendMode lastBlendMode = BLEND_NORMAL;
    bool lastClipToContent = false;
    std::uint8_t lastBrightness = 255;
    std::array<char, 64> lastRejection{{'n', 'o', 'n', 'e', '\0'}};

    std::unique_ptr<std::uint8_t[]> images[2];
    int activeImageSlot = -1;
    std::int32_t displayedFrameId = -1;
    std::int32_t frameHighWater = -1;
    std::uint8_t imageWidth = 0;
    std::uint8_t imageHeight = 0;
    std::uint32_t acceptedFrames = 0;

    DrawingScene sceneSlots[2]{};
    bool sceneSlotOccupied[2] = {false, false};
    std::uint32_t sceneSlotGeneration[2] = {0, 0};
    bool imageQueued = false;
    int imageQueuedSlot = -1;
    std::uint32_t imageSlotGeneration[2] = {0, 0};

    std::int32_t imageSlotFrame[2] = {-1, -1};
    std::uint8_t imageSlotWidth[2] = {0, 0};
    std::uint8_t imageSlotHeight[2] = {0, 0};
    bool imageLeased = false;
    int leasedSlot = -1;
    std::int32_t queuedFrameId = -1;
    std::uint8_t queuedWidth = 0;
    std::uint8_t queuedHeight = 0;
};

} // namespace lightgraph::drawing

#endif // LIGHTGRAPH_CORE_DRAWING_RUNTIME_INTERNAL_H
