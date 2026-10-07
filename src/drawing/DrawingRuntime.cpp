#include "DrawingRuntimeInternal.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

#include "../geometry/GeometryProvider.h"
#include "../runtime/BgLight.h"
#include "../topology/TopologyObject.h"

namespace lightgraph::drawing {
namespace {

std::atomic<std::uint32_t> gGenerationCounter{1};

template <std::size_t Size> void copyText(std::array<char, Size>& destination, const char* source) {
    static_assert(Size > 0, "text storage must include a terminator");
    const char* value = source != nullptr ? source : "none";
    const std::size_t length = std::min<std::size_t>(std::strlen(value), Size - 1);
    std::memmove(destination.data(), value, length);
    destination[length] = '\0';
    std::fill(destination.begin() + static_cast<std::ptrdiff_t>(length + 1), destination.end(),
              '\0');
}

} // namespace

using detail::canonicalizeScene;
using detail::finite;
using detail::normalizedDegrees;
using detail::reportAllocationFailure;
using detail::sameFloat;
using detail::sameScene;
using detail::validRgb;

DrawingRuntime::Impl::Impl(TopologyObject& topology, BgLight& bg)
    : object(topology), background(bg), ledCount(topology.pixelCount) {
    const std::uint64_t counter = gGenerationCounter.fetch_add(1, std::memory_order_relaxed);
    const std::uint64_t mixed = counter * 0x9e3779b97f4a7c15ULL ^
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

DrawingMode DrawingRuntime::Impl::modeFromBackground() const {
    switch (drawingMode) {
    case DrawingMode::Drawing:
        return DrawingMode::Drawing;
    default:
        return DrawingMode::Palette;
    }
}

void DrawingRuntime::Impl::reconcile() {
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

bool DrawingRuntime::Impl::preconditionMatches(const DrawingPrecondition* precondition) {
    reconcile();
    if (precondition == nullptr || !precondition->required)
        return true;
    return precondition->revision == revision &&
           std::memcmp(precondition->generation.data(), generation.data(), 16) == 0;
}

void DrawingRuntime::Impl::changed() {
    lastMode = modeFromBackground();
    lastVisible = background.visible;
    rememberComposition();
    ++revision;
    reject("none");
}

void DrawingRuntime::Impl::rememberComposition() {
    lastPlacement = drawingPlacement;
    lastBlendMode = background.blendMode;
    lastClipToContent = drawingClipToContent;
    lastBrightness = background.maxBri;
}

void DrawingRuntime::Impl::reject(const char* reason) { copyText(lastRejection, reason); }

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
    DrawingScene canonical;
    if (!canonicalizeScene(value, canonical)) {
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
        if (!impl_->validQueued(handle))
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
        if (!impl_->validQueued(handle))
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
    return impl_ && impl_->validQueued(handle);
}

void DrawingRuntime::releaseQueued(const DrawingHandle& handle) {
    if (!validQueued(handle))
        return;
    if (handle.kind == DrawingHandleKind::Scene) {
        impl_->sceneSlotOccupied[handle.slot] = false;
    } else if (handle.kind == DrawingHandleKind::Image) {
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
    DrawingScene canonical;
    if (!canonicalizeScene(value, canonical)) {
        impl_->reject("invalid_scene");
        return DrawingResult::Invalid;
    }
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
