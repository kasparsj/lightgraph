#include "DrawingRuntimeInternal.h"

#include <algorithm>
#include <cmath>
#include <new>

#include "../geometry/GeometryProvider.h"
#include "../runtime/BgLight.h"
#include "../topology/TopologyObject.h"

namespace lightgraph::drawing {

bool DrawingRuntime::Impl::validQueued(const DrawingHandle& handle) const {
    if (handle.runtimeGeneration != runtimeGeneration || handle.slot >= 2)
        return false;
    if (handle.kind == DrawingHandleKind::Scene)
        return sceneSlotOccupied[handle.slot] &&
               sceneSlotGeneration[handle.slot] == handle.slotGeneration;
    if (handle.kind == DrawingHandleKind::Image)
        return imageQueued && imageQueuedSlot == handle.slot &&
               imageSlotGeneration[handle.slot] == handle.slotGeneration;
    return false;
}

void DrawingRuntime::Impl::retireQueued() {
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

void DrawingRuntime::Impl::invalidatePendingStorage() {
    retireQueued();
    if (imageLeased && leasedSlot >= 0) {
        ++imageSlotGeneration[leasedSlot];
    }
    imageLeased = false;
    leasedSlot = -1;
}

bool DrawingRuntime::Impl::ensureImageResources() {
    if (images[0] && images[1])
        return true;
    lightgraph::memory::Estimate estimate;
    estimate.addAllocation(DRAWING_MAX_IMAGE_BYTES, 2);
    if (!lightgraph::memory::admitted(object.runtimeContext().memoryAdmission,
                                      lightgraph::memory::Operation::DrawingImages, estimate)) {
        reject("insufficient_heap");
        return false;
    }
    std::unique_ptr<std::uint8_t[]> first(new (std::nothrow) std::uint8_t[DRAWING_MAX_IMAGE_BYTES]);
    if (!first) {
        detail::reportAllocationFailure(object, 3, DRAWING_MAX_IMAGE_BYTES);
        reject("allocation_failed");
        return false;
    }
    lightgraph::memory::Estimate remaining;
    remaining.addAllocation(DRAWING_MAX_IMAGE_BYTES);
    if (!lightgraph::memory::admitted(object.runtimeContext().memoryAdmission,
                                      lightgraph::memory::Operation::DrawingImages, remaining)) {
        reject("insufficient_heap");
        return false;
    }
    std::unique_ptr<std::uint8_t[]> second(new (std::nothrow)
                                               std::uint8_t[DRAWING_MAX_IMAGE_BYTES]);
    if (!second) {
        detail::reportAllocationFailure(object, 4, DRAWING_MAX_IMAGE_BYTES);
        reject("allocation_failed");
        return false;
    }
    images[0] = std::move(first);
    images[1] = std::move(second);
    return true;
}

detail::Rgb DrawingRuntime::Impl::sampleImage(const std::uint8_t* image, std::uint8_t width,
                                              std::uint8_t height, detail::Vec2 point) const {
    const double x = std::clamp(point.x, 0.0, 1.0) * (width - 1u);
    const double y = std::clamp(point.y, 0.0, 1.0) * (height - 1u);
    const unsigned x0 = static_cast<unsigned>(std::floor(x));
    const unsigned y0 = static_cast<unsigned>(std::floor(y));
    const unsigned x1 = std::min<unsigned>(x0 + 1u, width - 1u);
    const unsigned y1 = std::min<unsigned>(y0 + 1u, height - 1u);
    const double tx = x - x0;
    const double ty = y - y0;
    detail::Rgb out{};
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

bool DrawingRuntime::Impl::rasterizeImage(const std::uint8_t* image, std::uint8_t width,
                                          std::uint8_t height, float rotationDegrees) {
    if (!prepareRaster())
        return false;
    const auto points = geometry->points();
    std::uint8_t* target = cache.get();
    for (std::size_t i = 0; i < ledCount; ++i) {
        detail::Vec2 sample{points[i].x / 2.0, points[i].y / 2.0};
        sample = detail::rotate(sample, -rotationDegrees);
        sample.x += 0.5;
        sample.y += 0.5;
        const detail::Rgb color = sampleImage(image, width, height, sample);
        target[i * 3] = color.r;
        target[i * 3 + 1] = color.g;
        target[i * 3 + 2] = color.b;
    }
    cacheGeometryRevision = geometry->topologyRevision();
    deferredGeometryRevision = 0;
    return true;
}

DrawingResult DrawingRuntime::Impl::presentImage(std::int32_t frameId, std::uint8_t width,
                                                 std::uint8_t height, int slot,
                                                 const DrawingPrecondition* precondition) {
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

ColorRGB DrawingRuntime::Impl::color(std::uint16_t pixel) {
    if (pixel >= ledCount || !cache)
        return ColorRGB(0);
    if (geometry && geometry->sourceTopologyRevision() != cacheGeometryRevision &&
        geometry->sourceTopologyRevision() != deferredGeometryRevision) {
        const bool ok = source == DrawingSource::Image && activeImageSlot >= 0
                            ? rasterizeImage(images[activeImageSlot].get(), imageWidth, imageHeight,
                                             sceneValue.rotationDegrees)
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

} // namespace lightgraph::drawing
