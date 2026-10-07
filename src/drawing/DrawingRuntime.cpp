#include "DrawingRuntime.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#include "../Globals.h"
#include "../geometry/GeometryProvider.h"
#include "../runtime/BgLight.h"
#include "../topology/TopologyObject.h"

namespace lightgraph::drawing {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
std::atomic<std::uint32_t> gGenerationCounter{1};

bool finite(float value) { return std::isfinite(static_cast<double>(value)); }
bool validRgb(std::uint32_t rgb) { return rgb <= 0xFFFFFFu; }

void reportAllocationFailure(const TopologyObject& object, std::uint16_t stage, std::size_t bytes) {
    lightgraphReportAllocationFailure(
        object.runtimeContext(), LightgraphAllocationFailureSite::DrawingRuntimeAllocation, stage,
        static_cast<std::uint16_t>(
            std::min<std::size_t>(bytes, std::numeric_limits<std::uint16_t>::max())));
}

float normalizedDegrees(float degrees) {
    const float value = std::fmod(degrees, 360.0f);
    return value == -0.0f ? 0.0f : value;
}

bool sameFloat(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

template <std::size_t Size>
void copyText(std::array<char, Size>& destination, const char* source) {
    static_assert(Size > 0, "text storage must include a terminator");
    const char* value = source != nullptr ? source : "none";
    const std::size_t length = std::min<std::size_t>(std::strlen(value), Size - 1);
    std::memmove(destination.data(), value, length);
    destination[length] = '\0';
    std::fill(destination.begin() + static_cast<std::ptrdiff_t>(length + 1), destination.end(),
              '\0');
}

bool sameShape(const DrawingShape& a, const DrawingShape& b) {
    if (a.kind != b.kind || a.id != b.id || a.rgb != b.rgb || a.opacity != b.opacity) {
        return false;
    }
    for (std::size_t i = 0; i < a.params.size(); ++i) {
        if (!sameFloat(a.params[i], b.params[i]))
            return false;
    }
    return true;
}

bool sameScene(const DrawingScene& a, const DrawingScene& b) {
    if (a.baseKind != b.baseKind || a.rgb1 != b.rgb1 || a.rgb2 != b.rgb2 ||
        !sameFloat(a.x1, b.x1) || !sameFloat(a.y1, b.y1) || !sameFloat(a.x2, b.x2) ||
        !sameFloat(a.y2, b.y2) || !sameFloat(a.rotationDegrees, b.rotationDegrees) ||
        a.shapeCount != b.shapeCount) {
        return false;
    }
    for (std::size_t i = 0; i < a.shapeCount; ++i) {
        if (!sameShape(a.shapes[i], b.shapes[i]))
            return false;
    }
    return true;
}

struct Vec2 {
    double x;
    double y;
};

Vec2 rotate(Vec2 point, double degrees) {
    const double radians = degrees * kPi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return {c * point.x + s * point.y, -s * point.x + c * point.y};
}

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

Rgb unpack(std::uint32_t rgb) {
    return {static_cast<std::uint8_t>(rgb >> 16), static_cast<std::uint8_t>(rgb >> 8),
            static_cast<std::uint8_t>(rgb)};
}

Rgb composite(Rgb destination, Rgb source, std::uint8_t opacity) {
    const unsigned inverse = 255u - opacity;
    return {
        static_cast<std::uint8_t>((source.r * opacity + destination.r * inverse + 127u) / 255u),
        static_cast<std::uint8_t>((source.g * opacity + destination.g * inverse + 127u) / 255u),
        static_cast<std::uint8_t>((source.b * opacity + destination.b * inverse + 127u) / 255u),
    };
}

double distanceToSegment(Vec2 p, Vec2 a, Vec2 b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared == 0.0)
        return std::hypot(p.x - a.x, p.y - a.y);
    const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

} // namespace

class DrawingRuntime::Impl {
  public:
    Impl(TopologyObject& topology, BgLight& bg)
        : object(topology), background(bg), ledCount(topology.pixelCount) {
        const std::uint64_t counter = gGenerationCounter.fetch_add(1, std::memory_order_relaxed);
        const std::uint64_t mixed =
            counter * 0x9e3779b97f4a7c15ULL ^
            static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(this));
        runtimeGeneration = mixed != 0 ? mixed : counter;
        std::snprintf(generation.data(), generation.size(), "%016llx",
                      static_cast<unsigned long long>(runtimeGeneration));
        lastMode = modeFromBackground();
        lastVisible = background.visible;
        rememberComposition();

        supported = object.supportsGeometry();
        if (!supported)
            reject("unsupported_geometry");
    }

    bool ensureGeometry() {
        rasterFailure = DrawingResult::Unsupported;
        if (!object.supportsGeometry()) {
            enterUnsupportedFallback();
            return false;
        }
        if (!geometry) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
            try {
#endif
            geometry = object.createGeometry();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
            } catch (const std::bad_alloc&) {
                rasterFailure = DrawingResult::Busy;
                reportAllocationFailure(object, 1, 0);
                reject("allocation_failed");
                return false;
            }
#endif
            if (!geometry) {
                rasterFailure = DrawingResult::Busy;
                const auto failure = object.geometryCreationResult();
                if (failure == lightgraph::geometry::GeometryResult::Unsupported) {
                    rasterFailure = DrawingResult::Unsupported;
                    enterUnsupportedFallback();
                } else {
                    reject(failure == lightgraph::geometry::GeometryResult::AdmissionDenied
                               ? "insufficient_heap"
                               : "allocation_failed");
                }
                return false;
            }
        }
        if (!geometry->compatible()) {
            enterUnsupportedFallback();
            return false;
        }
        return true;
    }

    DrawingMode modeFromBackground() const {
        switch (drawingMode) {
        case DrawingMode::Drawing:
            return DrawingMode::Drawing;
        default:
            return DrawingMode::Palette;
        }
    }

    void reconcile() {
        if (drawingMode == DrawingMode::Drawing && (geometry && !geometry->compatible())) {
            enterUnsupportedFallback();
        }
        const DrawingMode mode = modeFromBackground();
        if (mode != lastMode || background.visible != lastVisible ||
            drawingPlacement != lastPlacement || background.blendMode != lastBlendMode ||
            drawingClipToContent != lastClipToContent || background.maxBri != lastBrightness) {
            lastMode = mode;
            lastVisible = background.visible;
            rememberComposition();
            ++revision;
        }
    }

    bool preconditionMatches(const DrawingPrecondition* precondition) {
        reconcile();
        if (precondition == nullptr || !precondition->required)
            return true;
        return precondition->revision == revision &&
               std::memcmp(precondition->generation.data(), generation.data(), 16) == 0;
    }

    void changed() {
        lastMode = modeFromBackground();
        lastVisible = background.visible;
        rememberComposition();
        ++revision;
        reject("none");
    }

    void rememberComposition() {
        lastPlacement = drawingPlacement;
        lastBlendMode = background.blendMode;
        lastClipToContent = drawingClipToContent;
        lastBrightness = background.maxBri;
    }

    void reject(const char* reason) { copyText(lastRejection, reason); }

    void retireQueued() {
        for (int slot = 0; slot < 2; ++slot) {
            if (sceneSlotOccupied[slot]) {
                sceneSlotOccupied[slot] = false;
                ++sceneSlotGeneration[slot];
            }
        }
        if (imageQueued && imageQueuedSlot >= 0) {
            ++imageSlotGeneration[imageQueuedSlot];
        }
        imageQueued = false;
        imageQueuedSlot = -1;
    }

    void invalidatePendingStorage() {
        retireQueued();
        if (imageLeased && leasedSlot >= 0) {
            ++imageSlotGeneration[leasedSlot];
        }
        imageLeased = false;
        leasedSlot = -1;
    }

    void enterUnsupportedFallback() {
        reject("unsupported_geometry");
        retireQueued();
        deferredGeometryRevision = 0;
        if (drawingMode == DrawingMode::Drawing) {
            drawingMode = DrawingMode::Palette;
            lastMode = DrawingMode::Palette;
            ++revision;
        }
    }

    bool geometryCoordinatesValid() {
        const auto points = geometry->points();
        if (points.size() != ledCount)
            return false;
        for (const auto point : points) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y))
                return false;
        }
        return true;
    }

    bool ensureResources() {
        if (!supported)
            return false;
        if (resourcesReady)
            return true;
        lightgraph::memory::Estimate estimate;
        estimate.addAllocation(ledCount * 3u);
        if (!lightgraph::memory::admitted(object.runtimeContext().memoryAdmission,
                                          lightgraph::memory::Operation::DrawingCache, estimate)) {
            reject("insufficient_heap");
            return false;
        }
        std::unique_ptr<std::uint8_t[]> nextCache0(new (std::nothrow)
                                                       std::uint8_t[ledCount * 3u]());
        if (!nextCache0) {
            reportAllocationFailure(object, 2, ledCount * 3u);
            reject("allocation_failed");
            return false;
        }
        cache = std::move(nextCache0);
        resourcesReady = true;
        return true;
    }

    bool ensureImageResources() {
        if (images[0] && images[1])
            return true;
        lightgraph::memory::Estimate estimate;
        estimate.addAllocation(DRAWING_MAX_IMAGE_BYTES, 2);
        if (!lightgraph::memory::admitted(object.runtimeContext().memoryAdmission,
                                          lightgraph::memory::Operation::DrawingImages, estimate)) {
            reject("insufficient_heap");
            return false;
        }
        std::unique_ptr<std::uint8_t[]> first(new (std::nothrow)
                                                  std::uint8_t[DRAWING_MAX_IMAGE_BYTES]);
        if (!first) {
            reportAllocationFailure(object, 3, DRAWING_MAX_IMAGE_BYTES);
            reject("allocation_failed");
            return false;
        }
        lightgraph::memory::Estimate remaining;
        remaining.addAllocation(DRAWING_MAX_IMAGE_BYTES);
        if (!lightgraph::memory::admitted(object.runtimeContext().memoryAdmission,
                                          lightgraph::memory::Operation::DrawingImages,
                                          remaining)) {
            reject("insufficient_heap");
            return false;
        }
        std::unique_ptr<std::uint8_t[]> second(new (std::nothrow)
                                                   std::uint8_t[DRAWING_MAX_IMAGE_BYTES]);
        if (!second) {
            reportAllocationFailure(object, 4, DRAWING_MAX_IMAGE_BYTES);
            reject("allocation_failed");
            return false;
        }
        images[0] = std::move(first);
        images[1] = std::move(second);
        return true;
    }

    bool prepareRaster() {
        if (!ensureGeometry()) {
            return false;
        }
        if (!ensureResources()) {
            rasterFailure = DrawingResult::Busy;
            return false;
        }
        const auto refreshed = geometry->refresh();
        if (refreshed != lightgraph::geometry::GeometryResult::Ready) {
            rasterFailure = refreshed == lightgraph::geometry::GeometryResult::Unsupported
                                ? DrawingResult::Unsupported
                                : DrawingResult::Busy;
            reject(refreshed == lightgraph::geometry::GeometryResult::AdmissionDenied
                       ? "insufficient_heap"
                   : refreshed == lightgraph::geometry::GeometryResult::Unsupported
                       ? "unsupported_geometry"
                       : "allocation_failed");
            if (rasterFailure == DrawingResult::Unsupported)
                enterUnsupportedFallback();
            return false;
        }
        if (!geometryCoordinatesValid() || geometry->outline().size() == 0) {
            rasterFailure = DrawingResult::Unsupported;
            enterUnsupportedFallback();
            return false;
        }
        return true;
    }

    Rgb baseColor(const DrawingScene& value, Vec2 point) const {
        if (value.baseKind == DrawingBaseKind::Solid)
            return unpack(value.rgb1);
        const double dx = static_cast<double>(value.x2) - static_cast<double>(value.x1);
        const double dy = static_cast<double>(value.y2) - static_cast<double>(value.y1);
        const double denom = dx * dx + dy * dy;
        const double t =
            std::clamp(((point.x - value.x1) * dx + (point.y - value.y1) * dy) / denom, 0.0, 1.0);
        const Rgb a = unpack(value.rgb1);
        const Rgb b = unpack(value.rgb2);
        return {
            static_cast<std::uint8_t>(std::lround(a.r + (b.r - a.r) * t)),
            static_cast<std::uint8_t>(std::lround(a.g + (b.g - a.g) * t)),
            static_cast<std::uint8_t>(std::lround(a.b + (b.b - a.b) * t)),
        };
    }

    bool shapeContains(const DrawingShape& shape, Vec2 point, Vec2 original,
                       float sceneRotation) const {
        const auto& p = shape.params;
        switch (shape.kind) {
        case DrawingShapeKind::Band: {
            const double angle = -(static_cast<double>(sceneRotation) + p[2]);
            const Vec2 projected = rotate({original.x * 2.0 - 1.0, original.y * 2.0 - 1.0}, angle);
            double minY = std::numeric_limits<double>::max();
            double maxY = -minY;
            for (const auto outer : geometry->outline()) {
                const Vec2 vertex = rotate({outer.x, outer.y}, angle);
                minY = std::min(minY, vertex.y);
                maxY = std::max(maxY, vertex.y);
            }
            const double h = maxY == minY ? 0.5 : (projected.y - minY) / (maxY - minY);
            const double lower = p[0] - p[1] / 2.0;
            const double upper = p[0] + p[1] / 2.0;
            return h >= lower && h < upper;
        }
        case DrawingShapeKind::Circle:
            return std::hypot(point.x - p[0], point.y - p[1]) <= p[2];
        case DrawingShapeKind::Ring: {
            const double distance = std::hypot(point.x - p[0], point.y - p[1]);
            const double inner = std::max(0.0, p[2] - p[3] / 2.0);
            return distance >= inner && distance <= p[2] + p[3] / 2.0;
        }
        case DrawingShapeKind::Line:
            return distanceToSegment(point, {p[0], p[1]}, {p[2], p[3]}) <= p[4] / 2.0;
        case DrawingShapeKind::Rectangle: {
            Vec2 local = rotate({point.x - p[0], point.y - p[1]}, -p[4]);
            return std::abs(local.x) <= p[2] / 2.0 && std::abs(local.y) <= p[3] / 2.0;
        }
        }
        return false;
    }

    // Callers serialize mutation and rendering. Complete all failable preparation
    // before writing the single cache; the loops below allocate nothing and cannot fail.
    bool rasterizeScene(const DrawingScene& value) {
        if (!prepareRaster())
            return false;
        const auto points = geometry->points();
        std::uint8_t* target = cache.get();
        for (std::size_t i = 0; i < ledCount; ++i) {
            const Vec2 original{(points[i].x + 1.0) / 2.0, (points[i].y + 1.0) / 2.0};
            const Vec2 centered{original.x - 0.5, original.y - 0.5};
            const Vec2 inverse = rotate(centered, -value.rotationDegrees);
            const Vec2 sample{inverse.x + 0.5, inverse.y + 0.5};
            Rgb color = baseColor(value, sample);
            for (std::size_t shapeIndex = 0; shapeIndex < value.shapeCount; ++shapeIndex) {
                const DrawingShape& shape = value.shapes[shapeIndex];
                if (shapeContains(shape, sample, original, value.rotationDegrees)) {
                    color = composite(color, unpack(shape.rgb), shape.opacity);
                }
            }
            target[i * 3] = color.r;
            target[i * 3 + 1] = color.g;
            target[i * 3 + 2] = color.b;
        }
        cacheGeometryRevision = geometry->topologyRevision();
        deferredGeometryRevision = 0;
        return true;
    }

    Rgb sampleImage(const std::uint8_t* image, std::uint8_t width, std::uint8_t height,
                    Vec2 point) const {
        const double x = std::clamp(point.x, 0.0, 1.0) * (width - 1u);
        const double y = std::clamp(point.y, 0.0, 1.0) * (height - 1u);
        const unsigned x0 = static_cast<unsigned>(std::floor(x));
        const unsigned y0 = static_cast<unsigned>(std::floor(y));
        const unsigned x1 = std::min<unsigned>(x0 + 1u, width - 1u);
        const unsigned y1 = std::min<unsigned>(y0 + 1u, height - 1u);
        const double tx = x - x0;
        const double ty = y - y0;
        Rgb out{};
        std::uint8_t* channels = &out.r;
        for (unsigned channel = 0; channel < 3; ++channel) {
            const auto at = [image, width, channel](unsigned px, unsigned py) {
                return image[(py * width + px) * 3u + channel];
            };
            const double top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
            const double bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
            channels[channel] = static_cast<std::uint8_t>(std::lround(top + (bottom - top) * ty));
        }
        return out;
    }

    bool rasterizeImage(const std::uint8_t* image, std::uint8_t width, std::uint8_t height,
                        float rotationDegrees) {
        if (!prepareRaster())
            return false;
        const auto points = geometry->points();
        std::uint8_t* target = cache.get();
        for (std::size_t i = 0; i < ledCount; ++i) {
            Vec2 sample{points[i].x / 2.0, points[i].y / 2.0};
            sample = rotate(sample, -rotationDegrees);
            sample.x += 0.5;
            sample.y += 0.5;
            const Rgb color = sampleImage(image, width, height, sample);
            target[i * 3] = color.r;
            target[i * 3 + 1] = color.g;
            target[i * 3 + 2] = color.b;
        }
        cacheGeometryRevision = geometry->topologyRevision();
        deferredGeometryRevision = 0;
        return true;
    }

    DrawingResult presentImage(std::int32_t frameId, std::uint8_t width, std::uint8_t height,
                               int slot, const DrawingPrecondition* precondition) {
        if (!preconditionMatches(precondition))
            return DrawingResult::Stale;
        if (!supported)
            return DrawingResult::Unsupported;
        if (frameId < 0 || width == 0 || height == 0 || width > DRAWING_MAX_IMAGE_WIDTH ||
            height > DRAWING_MAX_IMAGE_HEIGHT || frameId != frameHighWater) {
            reject("stale_frame");
            return DrawingResult::Invalid;
        }
        if (!rasterizeImage(images[slot].get(), width, height, sceneValue.rotationDegrees)) {
            return rasterFailure;
        }
        activeImageSlot = slot;
        displayedFrameId = frameId;
        frameHighWater = frameId;
        imageWidth = width;
        imageHeight = height;
        source = DrawingSource::Image;
        ++acceptedFrames;
        changed();
        return DrawingResult::Applied;
    }

    ColorRGB color(std::uint16_t pixel) {
        if (pixel >= ledCount || !cache)
            return ColorRGB(0);
        if (geometry && geometry->sourceTopologyRevision() != cacheGeometryRevision &&
            geometry->sourceTopologyRevision() != deferredGeometryRevision) {
            const bool ok = source == DrawingSource::Image && activeImageSlot >= 0
                                ? rasterizeImage(images[activeImageSlot].get(), imageWidth,
                                                 imageHeight, sceneValue.rotationDegrees)
                                : rasterizeScene(sceneValue);
            if (!ok && rasterFailure != DrawingResult::Busy)
                return ColorRGB(0);
            if (!ok) {
                deferredGeometryRevision = geometry->sourceTopologyRevision();
                const std::uint8_t* retained = cache.get() + pixel * 3u;
                return ColorRGB(retained[0], retained[1], retained[2]);
            }
            deferredGeometryRevision = 0;
            ++revision;
            reject("none");
        }
        const std::uint8_t* rgb = cache.get() + pixel * 3u;
        return ColorRGB(rgb[0], rgb[1], rgb[2]);
    }

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

bool DrawingRuntime::initialize(TopologyObject& object, BgLight& background) {
    shutdown();
    lightgraph::memory::Estimate estimate;
    estimate.addAllocation(sizeof(Impl));
    estimate.addAllocation(sizeof(Lifetime::Token));
    if (!lightgraph::memory::admitted(object.runtimeContext().memoryAdmission,
                                      lightgraph::memory::Operation::DrawingCache, estimate))
        return false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
        impl_ = new (std::nothrow) Impl(object, background);
    } catch (const std::bad_alloc&) {
        reportAllocationFailure(object, 0, sizeof(Impl));
        return false;
    }
#else
    impl_ = new (std::nothrow) Impl(object, background);
#endif
    if (impl_ == nullptr) {
        reportAllocationFailure(object, 0, sizeof(Impl));
        return false;
    }
    lifetime_ = Lifetime(new (std::nothrow) Lifetime::Token);
    if (lifetime_.expired()) {
        reportAllocationFailure(object, 1, sizeof(Lifetime::Token));
        shutdown();
        return false;
    }
    return true;
}

void DrawingRuntime::synchronize(TopologyObject& object, BgLight* background) {
    if (background == nullptr) {
        shutdown();
        return;
    }
    if (impl_ == nullptr || &impl_->background != background) {
        initialize(object, *background);
    }
}

void DrawingRuntime::shutdown() {
    if (lifetime_.token_)
        lifetime_.token_->alive = false;
    lifetime_.reset();
    if (impl_ != nullptr) {
        delete impl_;
        impl_ = nullptr;
    }
}

DrawingRuntime::Lifetime DrawingRuntime::lifetime() const { return lifetime_; }

DrawingRuntime::~DrawingRuntime() { shutdown(); }

DrawingStatus DrawingRuntime::status() {
    DrawingStatus value;
    if (!impl_) {
        copyText(value.lastRejection, "allocation_failed");
        return value;
    }
    impl_->reconcile();
    value.supported = impl_->supported && (!impl_->geometry || impl_->geometry->compatible());
    value.generation = impl_->generation;
    value.revision = impl_->revision;
    value.mode = impl_->modeFromBackground();
    value.visible = impl_->background.visible;
    value.placement = impl_->drawingPlacement;
    value.blendMode = static_cast<std::uint8_t>(impl_->background.blendMode);
    value.clipToContent = impl_->drawingClipToContent;
    value.brightness = impl_->background.maxBri;
    value.compositingSupported = value.supported;
    value.activeSource = impl_->source;
    value.rotationDegrees = impl_->sceneValue.rotationDegrees;
    value.shapeCount = impl_->sceneValue.shapeCount;
    value.displayedFrameId = impl_->displayedFrameId;
    value.frameHighWater = impl_->frameHighWater;
    value.imageWidth = impl_->imageWidth;
    value.imageHeight = impl_->imageHeight;
    value.acceptedFrames = impl_->acceptedFrames;
    value.lastRejection = impl_->lastRejection;
    return value;
}

DrawingScene DrawingRuntime::scene() const { return impl_ ? impl_->sceneValue : DrawingScene{}; }

ColorRGB DrawingRuntime::sampleColor(std::uint16_t pixel) {
    return impl_ ? impl_->color(pixel) : ColorRGB(0);
}

DrawingResult DrawingRuntime::refreshGeometry() {
    if (!impl_)
        return DrawingResult::Busy;
    if (impl_->drawingMode != DrawingMode::Drawing) {
        return DrawingResult::NoChange;
    }
    if (!impl_->ensureGeometry())
        return impl_->rasterFailure;
    const std::uint32_t requestedRevision = impl_->geometry->sourceTopologyRevision();
    if (requestedRevision == impl_->cacheGeometryRevision) {
        impl_->deferredGeometryRevision = 0;
        return DrawingResult::NoChange;
    }
    const bool ok =
        impl_->source == DrawingSource::Image && impl_->activeImageSlot >= 0
            ? impl_->rasterizeImage(impl_->images[impl_->activeImageSlot].get(), impl_->imageWidth,
                                    impl_->imageHeight, impl_->sceneValue.rotationDegrees)
            : impl_->rasterizeScene(impl_->sceneValue);
    if (!ok) {
        if (impl_->rasterFailure == DrawingResult::Busy) {
            impl_->deferredGeometryRevision = requestedRevision;
        }
        return impl_->rasterFailure;
    }
    impl_->deferredGeometryRevision = 0;
    impl_->changed();
    return DrawingResult::Applied;
}

bool DrawingRuntime::setGeneration(std::uint64_t value) {
    if (!impl_ || impl_->revision != 0 || value == 0)
        return false;
    impl_->invalidatePendingStorage();
    impl_->runtimeGeneration = value;
    std::snprintf(impl_->generation.data(), impl_->generation.size(), "%016llx",
                  static_cast<unsigned long long>(value));
    return true;
}

void DrawingRuntime::reject(const char* reason) {
    if (impl_)
        impl_->reject(reason);
}

DrawingResult DrawingRuntime::queueScene(const DrawingScene& value, DrawingHandle& handle) {
    handle = DrawingHandle{};
    if (!impl_)
        return DrawingResult::Busy;
    if (!impl_->ensureGeometry())
        return impl_->rasterFailure;
    if (!validateScene(value)) {
        impl_->reject("invalid_scene");
        return DrawingResult::Invalid;
    }
    int slot = -1;
    for (int index = 0; index < 2; ++index) {
        if (!impl_->sceneSlotOccupied[index]) {
            slot = index;
            break;
        }
    }
    if (slot < 0) {
        impl_->reject("scene_mailbox_busy");
        return DrawingResult::Busy;
    }
    DrawingScene canonical = value;
    canonical.rotationDegrees = normalizedDegrees(canonical.rotationDegrees);
    std::sort(canonical.shapes.begin(), canonical.shapes.begin() + canonical.shapeCount,
              [](const DrawingShape& a, const DrawingShape& b) { return a.id < b.id; });
    impl_->sceneSlots[slot] = canonical;
    impl_->sceneSlotOccupied[slot] = true;
    ++impl_->sceneSlotGeneration[slot];
    handle.kind = DrawingHandleKind::Scene;
    handle.slot = static_cast<std::uint8_t>(slot);
    handle.slotGeneration = impl_->sceneSlotGeneration[slot];
    handle.runtimeGeneration = impl_->runtimeGeneration;
    return DrawingResult::Applied;
}

DrawingResult DrawingRuntime::applyQueued(const DrawingHandle& handle) {
    if (!impl_)
        return DrawingResult::Busy;
    if (handle.runtimeGeneration != impl_->runtimeGeneration)
        return DrawingResult::Stale;
    if (handle.kind == DrawingHandleKind::Scene) {
        if (handle.slot >= 2 || !impl_->sceneSlotOccupied[handle.slot] ||
            impl_->sceneSlotGeneration[handle.slot] != handle.slotGeneration)
            return DrawingResult::Stale;
        const DrawingScene value = impl_->sceneSlots[handle.slot];
        const bool noChange =
            sameScene(value, impl_->sceneValue) && impl_->source == DrawingSource::Scene &&
            impl_->geometry &&
            impl_->cacheGeometryRevision == impl_->geometry->sourceTopologyRevision();
        if (!impl_->rasterizeScene(value)) {
            if (impl_->rasterFailure != DrawingResult::Busy) {
                impl_->sceneSlotOccupied[handle.slot] = false;
            }
            return impl_->rasterFailure;
        }
        impl_->sceneValue = value;
        impl_->source = DrawingSource::Scene;
        impl_->sceneSlotOccupied[handle.slot] = false;
        if (noChange)
            return DrawingResult::NoChange;
        impl_->changed();
        return DrawingResult::Applied;
    }
    if (handle.kind == DrawingHandleKind::Image) {
        if (handle.slot >= 2 || !impl_->imageQueued || impl_->imageQueuedSlot != handle.slot ||
            impl_->imageSlotGeneration[handle.slot] != handle.slotGeneration)
            return DrawingResult::Stale;
        const DrawingResult result = impl_->presentImage(impl_->queuedFrameId, impl_->queuedWidth,
                                                         impl_->queuedHeight, handle.slot, nullptr);
        if (result != DrawingResult::Busy) {
            impl_->imageQueued = false;
            impl_->imageQueuedSlot = -1;
        }
        return result;
    }
    return DrawingResult::Invalid;
}

bool DrawingRuntime::validQueued(const DrawingHandle& handle) const {
    if (!impl_ || handle.runtimeGeneration != impl_->runtimeGeneration)
        return false;
    if (handle.kind == DrawingHandleKind::Scene) {
        return handle.slot < 2 && impl_->sceneSlotOccupied[handle.slot] &&
               impl_->sceneSlotGeneration[handle.slot] == handle.slotGeneration;
    }
    if (handle.kind == DrawingHandleKind::Image) {
        return handle.slot < 2 && impl_->imageQueued &&
               impl_->imageQueuedSlot == handle.slot &&
               impl_->imageSlotGeneration[handle.slot] == handle.slotGeneration;
    }
    return false;
}

void DrawingRuntime::releaseQueued(const DrawingHandle& handle) {
    if (!impl_ || handle.runtimeGeneration != impl_->runtimeGeneration)
        return;
    if (handle.kind == DrawingHandleKind::Scene && handle.slot < 2 &&
        impl_->sceneSlotOccupied[handle.slot] &&
        impl_->sceneSlotGeneration[handle.slot] == handle.slotGeneration) {
        impl_->sceneSlotOccupied[handle.slot] = false;
    } else if (handle.kind == DrawingHandleKind::Image && handle.slot < 2 && impl_->imageQueued &&
               impl_->imageQueuedSlot == handle.slot &&
               impl_->imageSlotGeneration[handle.slot] == handle.slotGeneration) {
        impl_->imageQueued = false;
        impl_->imageQueuedSlot = -1;
    }
}

DrawingResult DrawingRuntime::replaceScene(const DrawingScene& value,
                                           const DrawingPrecondition* precondition) {
    if (!impl_)
        return DrawingResult::Busy;
    if (!impl_->preconditionMatches(precondition))
        return DrawingResult::Stale;
    if (!impl_->ensureGeometry())
        return impl_->rasterFailure;
    if (!validateScene(value)) {
        impl_->reject("invalid_scene");
        return DrawingResult::Invalid;
    }
    DrawingScene canonical = value;
    canonical.rotationDegrees = normalizedDegrees(canonical.rotationDegrees);
    std::sort(canonical.shapes.begin(), canonical.shapes.begin() + canonical.shapeCount,
              [](const DrawingShape& a, const DrawingShape& b) { return a.id < b.id; });
    if (sameScene(canonical, impl_->sceneValue) && impl_->source == DrawingSource::Scene) {
        return DrawingResult::NoChange;
    }
    if (!impl_->rasterizeScene(canonical))
        return impl_->rasterFailure;
    impl_->sceneValue = canonical;
    impl_->source = DrawingSource::Scene;
    impl_->changed();
    return DrawingResult::Applied;
}

DrawingResult DrawingRuntime::control(const DrawingControl& value,
                                      const DrawingPrecondition* precondition) {
    if (!impl_)
        return DrawingResult::Busy;
    if (!impl_->preconditionMatches(precondition))
        return DrawingResult::Stale;
    if ((!value.hasEnabled && !value.hasVisible && !value.hasRotation && !value.hasPlacement &&
         !value.hasBlendMode && !value.hasClipToContent && !value.clear) ||
        (value.clear && value.hasRotation) ||
        (value.hasRotation && !finite(value.rotationDegrees)) ||
        (value.hasPlacement && value.placement != DrawingPlacement::Background &&
         value.placement != DrawingPlacement::Overlay) ||
        (value.hasBlendMode && value.blendMode > BLEND_PIN_LIGHT)) {
        impl_->reject("invalid_control");
        return DrawingResult::Invalid;
    }
    if ((value.hasEnabled && value.enabled) || value.hasRotation || value.hasPlacement ||
        value.hasBlendMode || value.hasClipToContent) {
        if (!impl_->ensureGeometry())
            return impl_->rasterFailure;
    }
    bool changed = false;
    const bool needsInitialRaster =
        value.hasEnabled && value.enabled && impl_->cacheGeometryRevision == 0;
    DrawingScene next = impl_->sceneValue;
    DrawingSource nextSource = impl_->source;
    if (value.clear) {
        next = DrawingScene{};
        nextSource = DrawingSource::Scene;
        changed = !sameScene(next, impl_->sceneValue) || impl_->source != DrawingSource::Scene ||
                  impl_->activeImageSlot >= 0;
    }
    if (value.hasRotation) {
        const float normalized = normalizedDegrees(value.rotationDegrees);
        changed = changed || !sameFloat(next.rotationDegrees, normalized);
        next.rotationDegrees = normalized;
    }
    if (!value.clear && (changed || needsInitialRaster) &&
        (impl_->resourcesReady || needsInitialRaster) &&
        (nextSource == DrawingSource::Scene
             ? !impl_->rasterizeScene(next)
             : !impl_->rasterizeImage(impl_->images[impl_->activeImageSlot].get(),
                                      impl_->imageWidth, impl_->imageHeight,
                                      next.rotationDegrees))) {
        return impl_->rasterFailure;
    }
    if (value.clear) {
        if (impl_->resourcesReady) {
            std::memset(impl_->cache.get(), 0, impl_->ledCount * 3u);
        }
        impl_->cacheGeometryRevision = 0;
        impl_->deferredGeometryRevision = 0;
        impl_->activeImageSlot = -1;
        impl_->displayedFrameId = -1;
        impl_->imageWidth = impl_->imageHeight = 0;
        impl_->imageLeased = false;
        impl_->leasedSlot = -1;
        impl_->imageQueued = false;
        impl_->imageQueuedSlot = -1;
        ++impl_->imageSlotGeneration[0];
        ++impl_->imageSlotGeneration[1];
        for (int slot = 0; slot < 2; ++slot) {
            impl_->sceneSlotOccupied[slot] = false;
            ++impl_->sceneSlotGeneration[slot];
        }
    }
    if (value.clear) {
        impl_->images[0].reset();
        impl_->images[1].reset();
    }
    impl_->sceneValue = next;
    impl_->source = nextSource;
    if (value.hasEnabled) {
        const auto desired = value.enabled ? DrawingMode::Drawing : DrawingMode::Palette;
        changed = changed || impl_->drawingMode != desired;
        impl_->drawingMode = desired;
    }
    if (value.hasVisible) {
        changed = changed || impl_->background.visible != value.visible;
        impl_->background.visible = value.visible;
    }
    if (value.hasPlacement) {
        changed = changed || impl_->drawingPlacement != value.placement;
        impl_->drawingPlacement = value.placement;
    }
    if (value.hasBlendMode) {
        const BlendMode desired = static_cast<BlendMode>(value.blendMode);
        changed = changed || impl_->background.blendMode != desired;
        impl_->background.blendMode = desired;
    }
    if (value.hasClipToContent) {
        changed = changed || impl_->drawingClipToContent != value.clipToContent;
        impl_->drawingClipToContent = value.clipToContent;
    }
    if (!changed)
        return DrawingResult::NoChange;
    impl_->changed();
    return DrawingResult::Applied;
}

DrawingResult DrawingRuntime::setFill(std::uint32_t rgb) {
    if (!validRgb(rgb))
        return DrawingResult::Invalid;
    DrawingScene next = scene();
    next.baseKind = DrawingBaseKind::Solid;
    next.rgb1 = rgb;
    next.rgb2 = 0;
    next.x1 = next.y1 = next.x2 = next.y2 = 0.0f;
    return replaceScene(next);
}

DrawingResult DrawingRuntime::setGradient(float x1, float y1, float x2, float y2,
                                          std::uint32_t rgb1, std::uint32_t rgb2) {
    DrawingScene next = scene();
    next.baseKind = DrawingBaseKind::Gradient;
    next.rgb1 = rgb1;
    next.rgb2 = rgb2;
    next.x1 = x1;
    next.y1 = y1;
    next.x2 = x2;
    next.y2 = y2;
    return replaceScene(next);
}

DrawingResult DrawingRuntime::setShape(const DrawingShape& shape) {
    DrawingScene next = scene();
    for (std::size_t i = 0; i < next.shapeCount; ++i) {
        if (next.shapes[i].id == shape.id) {
            next.shapes[i] = shape;
            return replaceScene(next);
        }
    }
    if (next.shapeCount >= DRAWING_MAX_SHAPES)
        return DrawingResult::Busy;
    next.shapes[next.shapeCount++] = shape;
    return replaceScene(next);
}

DrawingResult DrawingRuntime::removeShape(std::uint8_t id) {
    if (id >= DRAWING_MAX_SHAPES)
        return DrawingResult::Invalid;
    DrawingScene next = scene();
    for (std::size_t i = 0; i < next.shapeCount; ++i) {
        if (next.shapes[i].id == id) {
            for (std::size_t j = i + 1; j < next.shapeCount; ++j)
                next.shapes[j - 1] = next.shapes[j];
            --next.shapeCount;
            return replaceScene(next);
        }
    }
    return replaceScene(next);
}

DrawingResult DrawingRuntime::setRotation(float degrees) {
    DrawingControl value;
    value.hasRotation = true;
    value.rotationDegrees = degrees;
    return control(value);
}
DrawingResult DrawingRuntime::clear() {
    DrawingControl value;
    value.clear = true;
    return control(value);
}
DrawingResult DrawingRuntime::setEnabled(bool enabled) {
    DrawingControl value;
    value.hasEnabled = true;
    value.enabled = enabled;
    return control(value);
}
DrawingResult DrawingRuntime::setVisible(bool visible) {
    DrawingControl value;
    value.hasVisible = true;
    value.visible = visible;
    return control(value);
}

DrawingResult DrawingRuntime::setComposite(DrawingPlacement placement, std::uint8_t blendMode,
                                           bool clipToContent) {
    DrawingControl value;
    value.hasPlacement = true;
    value.placement = placement;
    value.hasBlendMode = true;
    value.blendMode = blendMode;
    value.hasClipToContent = true;
    value.clipToContent = clipToContent;
    return control(value);
}

DrawingResult DrawingRuntime::applyImage(std::int32_t frameId, std::uint8_t width,
                                         std::uint8_t height, const std::uint8_t* rgb,
                                         std::size_t size,
                                         const DrawingPrecondition* precondition) {
    if (!impl_)
        return DrawingResult::Busy;
    if (!impl_->preconditionMatches(precondition))
        return DrawingResult::Stale;
    if (impl_->imageQueued || impl_->imageLeased) {
        impl_->reject("image_mailbox_busy");
        return DrawingResult::Busy;
    }
    const std::size_t expected = static_cast<std::size_t>(width) * height * 3u;
    if (!impl_->ensureGeometry())
        return impl_->rasterFailure;
    if (!rgb || width == 0 || height == 0 || width > DRAWING_MAX_IMAGE_WIDTH ||
        height > DRAWING_MAX_IMAGE_HEIGHT || size != expected || frameId < 0 ||
        frameId <= impl_->frameHighWater) {
        impl_->reject("invalid_image");
        return DrawingResult::Invalid;
    }
    if (!impl_->ensureImageResources())
        return DrawingResult::Busy;
    if (!impl_->prepareRaster())
        return impl_->rasterFailure;
    impl_->frameHighWater = frameId;
    const int slot = impl_->activeImageSlot == 0 ? 1 : 0;
    std::memcpy(impl_->images[slot].get(), rgb, size);
    return impl_->presentImage(frameId, width, height, slot, nullptr);
}

bool DrawingRuntime::validateScene(const DrawingScene& value) {
    if (value.shapeCount > DRAWING_MAX_SHAPES || !validRgb(value.rgb1) || !validRgb(value.rgb2) ||
        !finite(value.rotationDegrees))
        return false;
    if (value.baseKind == DrawingBaseKind::Gradient) {
        if (!finite(value.x1) || !finite(value.y1) || !finite(value.x2) || !finite(value.y2) ||
            (value.x1 == value.x2 && value.y1 == value.y2))
            return false;
    } else if (value.baseKind != DrawingBaseKind::Solid || value.rgb2 != 0 || value.x1 != 0 ||
               value.y1 != 0 || value.x2 != 0 || value.y2 != 0) {
        return false;
    }
    std::uint16_t ids = 0;
    for (std::size_t index = 0; index < value.shapeCount; ++index) {
        const DrawingShape& shape = value.shapes[index];
        if (shape.id >= DRAWING_MAX_SHAPES || !validRgb(shape.rgb) || (ids & (1u << shape.id)) != 0)
            return false;
        ids |= static_cast<std::uint16_t>(1u << shape.id);
        for (float param : shape.params)
            if (!finite(param))
                return false;
        std::size_t used = 0;
        switch (shape.kind) {
        case DrawingShapeKind::Band:
            used = 3;
            if (!(shape.params[1] > 0))
                return false;
            break;
        case DrawingShapeKind::Circle:
            used = 3;
            if (!(shape.params[2] > 0))
                return false;
            break;
        case DrawingShapeKind::Ring:
            used = 4;
            if (!(shape.params[2] > 0 && shape.params[3] > 0))
                return false;
            break;
        case DrawingShapeKind::Line:
            used = 5;
            if (!(shape.params[4] > 0))
                return false;
            break;
        case DrawingShapeKind::Rectangle:
            used = 5;
            if (!(shape.params[2] > 0 && shape.params[3] > 0))
                return false;
            break;
        default:
            return false;
        }
        for (std::size_t i = used; i < shape.params.size(); ++i)
            if (shape.params[i] != 0)
                return false;
    }
    return true;
}

} // namespace lightgraph::drawing

namespace lightgraph::drawing {
DrawingResult DrawingRuntime::leaseImage(std::int32_t frameId, std::uint8_t width,
                                         std::uint8_t height, ImageLease& lease,
                                         const ImageLease* previous) {
    lease = {};
    if (!impl_)
        return DrawingResult::Busy;
    if (impl_->imageQueued ||
        (impl_->imageLeased && (previous == nullptr || !validImageLease(*previous))))
        return DrawingResult::Busy;
    if (!impl_->ensureGeometry())
        return impl_->rasterFailure;
    if (frameId < 0 || frameId <= impl_->frameHighWater || width == 0 || height == 0 ||
        width > DRAWING_MAX_IMAGE_WIDTH || height > DRAWING_MAX_IMAGE_HEIGHT)
        return DrawingResult::Invalid;
    if (!impl_->ensureImageResources())
        return DrawingResult::Busy;
    if (!impl_->prepareRaster())
        return impl_->rasterFailure;
    const int slot = impl_->activeImageSlot == 0 ? 1 : 0;
    impl_->imageLeased = true;
    impl_->leasedSlot = slot;
    impl_->frameHighWater = frameId;
    ++impl_->imageSlotGeneration[slot];
    impl_->imageSlotFrame[slot] = frameId;
    impl_->imageSlotWidth[slot] = width;
    impl_->imageSlotHeight[slot] = height;
    lease.handle = {DrawingHandleKind::Image, static_cast<std::uint8_t>(slot),
                    impl_->imageSlotGeneration[slot], impl_->runtimeGeneration};
    lease.bytes = impl_->images[slot].get();
    lease.size = static_cast<std::size_t>(width) * height * 3u;
    lease.frameId = frameId;
    lease.width = width;
    lease.height = height;
    return DrawingResult::Applied;
}

bool DrawingRuntime::validImageLease(const ImageLease& lease) const {
    return impl_ && lease.handle.runtimeGeneration == impl_->runtimeGeneration &&
           lease.handle.slot < 2 &&
           lease.handle.slotGeneration == impl_->imageSlotGeneration[lease.handle.slot] &&
           lease.handle.kind == DrawingHandleKind::Image &&
           lease.bytes == impl_->images[lease.handle.slot].get() &&
           lease.frameId == impl_->imageSlotFrame[lease.handle.slot] &&
           lease.width == impl_->imageSlotWidth[lease.handle.slot] &&
           lease.height == impl_->imageSlotHeight[lease.handle.slot] &&
           lease.size == static_cast<std::size_t>(lease.width) * lease.height * 3u &&
           ((impl_->imageLeased && impl_->leasedSlot == lease.handle.slot) ||
            (impl_->imageQueued && impl_->imageQueuedSlot == lease.handle.slot));
}
DrawingResult DrawingRuntime::submitImage(const ImageLease& lease, DrawingHandle* ready) {
    if (!validImageLease(lease) || !impl_->imageLeased)
        return DrawingResult::Stale;
    if (!ready) {
        const DrawingResult result = impl_->presentImage(lease.frameId, lease.width, lease.height,
                                                         lease.handle.slot, nullptr);
        if (result != DrawingResult::Busy) {
            impl_->imageLeased = false;
            impl_->leasedSlot = -1;
        }
        return result;
    }
    impl_->imageLeased = false;
    impl_->leasedSlot = -1;
    impl_->imageQueued = true;
    impl_->imageQueuedSlot = lease.handle.slot;
    impl_->queuedFrameId = lease.frameId;
    impl_->queuedWidth = lease.width;
    impl_->queuedHeight = lease.height;
    *ready = lease.handle;
    return DrawingResult::Applied;
}
void DrawingRuntime::releaseImage(const ImageLease& lease) {
    if (validImageLease(lease) && impl_->imageLeased) {
        impl_->imageLeased = false;
        impl_->leasedSlot = -1;
        ++impl_->imageSlotGeneration[lease.handle.slot];
    }
}
bool DrawingRuntime::queuedImage() const { return impl_ && impl_->imageQueued; }
DrawingMode DrawingRuntime::mode() const {
    return impl_ ? impl_->drawingMode : DrawingMode::Palette;
}
DrawingPlacement DrawingRuntime::placement() const {
    return impl_ ? impl_->drawingPlacement : DrawingPlacement::Background;
}
bool DrawingRuntime::clipToContent() const { return impl_ && impl_->drawingClipToContent; }
ColorRGB DrawingRuntime::layerColor(std::uint16_t pixel) {
    if (!impl_)
        return ColorRGB(0);
    if (impl_->drawingMode == DrawingMode::Palette)
        return impl_->background.getColor(static_cast<int16_t>(pixel));
    ColorRGB color = sampleColor(pixel);
    if (impl_->drawingMode == DrawingMode::Palette)
        return impl_->background.getColor(static_cast<int16_t>(pixel));
    return impl_->background.maxBri < 255 ? color.dim(impl_->background.maxBri) : color;
}
const lightgraph::geometry::GeometryProvider* DrawingRuntime::geometry() const {
    return impl_ ? impl_->geometry.get() : nullptr;
}
} // namespace lightgraph::drawing
