#include "DrawingRuntimeInternal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

#include "../Globals.h"
#include "../geometry/GeometryProvider.h"
#include "../runtime/BgLight.h"
#include "../topology/TopologyObject.h"

namespace lightgraph::drawing::detail {

void reportAllocationFailure(const TopologyObject& object, std::uint16_t stage, std::size_t bytes) {
    lightgraphReportAllocationFailure(
        object.runtimeContext(), LightgraphAllocationFailureSite::DrawingRuntimeAllocation, stage,
        static_cast<std::uint16_t>(
            std::min<std::size_t>(bytes, std::numeric_limits<std::uint16_t>::max())));
}

} // namespace lightgraph::drawing::detail

namespace lightgraph::drawing {

bool DrawingRuntime::Impl::ensureGeometry() {
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
            detail::reportAllocationFailure(object, 1, 0);
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

void DrawingRuntime::Impl::enterUnsupportedFallback() {
    reject("unsupported_geometry");
    retireQueued();
    deferredGeometryRevision = 0;
    if (drawingMode == DrawingMode::Drawing) {
        drawingMode = DrawingMode::Palette;
        lastMode = DrawingMode::Palette;
        ++revision;
    }
}

bool DrawingRuntime::Impl::geometryCoordinatesValid() {
    const auto points = geometry->points();
    if (points.size() != ledCount)
        return false;
    for (const auto point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
            return false;
    }
    return true;
}

bool DrawingRuntime::Impl::ensureResources() {
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
    std::unique_ptr<std::uint8_t[]> nextCache0(new (std::nothrow) std::uint8_t[ledCount * 3u]());
    if (!nextCache0) {
        detail::reportAllocationFailure(object, 2, ledCount * 3u);
        reject("allocation_failed");
        return false;
    }
    cache = std::move(nextCache0);
    resourcesReady = true;
    return true;
}

bool DrawingRuntime::Impl::prepareRaster() {
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

} // namespace lightgraph::drawing
