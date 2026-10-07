#include "HeptagonGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

#include "../topology/Connection.h"
#include "../topology/Intersection.h"
#include "../topology/TopologyObject.h"

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

HeptagonGeometry::Point HeptagonGeometry::intersectionPosition(unsigned group, unsigned index) {
    const float radii[] = {1.0f, 4.0f / 9.0f, 5.0f / 18.0f};
    if (group >= 3) {
        return {0.0f, 0.0f};
    }
    if (group == 0) {
        index /= 2;
    }
    const float angle = kPi / 2.0f + (group % 2) * kPi / 7.0f + 2.0f * kPi * index / 7.0f;
    return {std::cos(angle) * radii[group], std::sin(angle) * radii[group]};
}

HeptagonGeometry::HeptagonGeometry(TopologyObject& object, uint16_t firstStripPixelCount)
    : lightgraph::geometry::GeometryProvider(object), firstStripPixelCount_(firstStripPixelCount) {
    for (unsigned i = 0; i < 7; ++i) outline_[i] = intersectionPosition(0, i * 2);
}

void HeptagonGeometry::reportAllocationFailure(uint16_t stage) const {
    lightgraphReportAllocationFailure(
        object_.runtimeContext(),
        LightgraphAllocationFailureSite::HeptagonGeometryAllocation,
        object_.pixelCount,
        stage);
}

bool HeptagonGeometry::isCompatibleHeptagon() const noexcept {
    if (object_.inter[0].size() != 14 || object_.inter[1].size() != 7 ||
        object_.inter[2].size() != 7) {
        return false;
    }
    for (unsigned group = 3; group < MAX_GROUPS; ++group) {
        if (!object_.inter[group].empty()) return false;
    }
    const auto containsIntersection = [this](const Intersection* candidate) {
        for (unsigned group = 0; group < 3; ++group) {
            for (const Intersection* intersection : object_.inter[group]) {
                if (intersection == candidate) return true;
            }
        }
        return false;
    };
    for (unsigned group = 0; group < MAX_GROUPS; ++group) {
        for (const Connection* connection : object_.conn[group]) {
            if (connection == nullptr || connection->from == nullptr || connection->to == nullptr) {
                return false;
            }
            if (!containsIntersection(connection->from) || !containsIntersection(connection->to)) {
                return false;
            }
            for (uint16_t pixel = 0; pixel < connection->numLeds; ++pixel) {
                if (connection->getPixel(pixel) >= object_.pixelCount) return false;
            }
        }
    }
    for (unsigned group = 0; group < 3; ++group) {
        for (const Intersection* intersection : object_.inter[group]) {
            if (intersection == nullptr || intersection->topPixel >= object_.pixelCount ||
                intersection->bottomPixel >= static_cast<int32_t>(object_.pixelCount)) {
                return false;
            }
        }
    }
    return true;
}

bool HeptagonGeometry::findNearestAnchor(uint16_t realPixel,
                                         uint16_t logicalPixel,
                                         const uint8_t* mapped,
                                         size_t coordinateCount,
                                         uint16_t& nearest) const noexcept {
    unsigned bestDistance = std::numeric_limits<unsigned>::max();
    nearest = logicalPixel;
    const bool firstStrip = realPixel < firstStripPixelCount_;
    for (uint16_t anchorReal = 0; anchorReal < object_.realPixelCount; ++anchorReal) {
        if ((anchorReal < firstStripPixelCount_) != firstStrip) {
            continue;
        }
        const uint16_t anchor = object_.translateToLogicalPixel(anchorReal);
        if (anchor >= coordinateCount || mapped[anchor] == 0) {
            continue;
        }
        const unsigned distance = anchor > logicalPixel
            ? anchor - logicalPixel
            : logicalPixel - anchor;
        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = anchor;
        }
    }
    return bestDistance != std::numeric_limits<unsigned>::max();
}

lightgraph::geometry::GeometryResult HeptagonGeometry::refresh() noexcept {
    using lightgraph::geometry::GeometryResult;
    const uint32_t requestedRevision = object_.topologyRevision();
    if (initialized_ && geometryRevision_ == requestedRevision) {
        return lastResult_ = GeometryResult::Ready;
    }

    if (!isCompatibleHeptagon()) {
        return lastResult_ = GeometryResult::Unsupported;
    }

    const size_t coordinateCount = object_.pixelCount;
    const bool reuseCoordinates =
        initialized_ && coordinates_ && coordinateCount_ == coordinateCount;
    lightgraph::memory::Estimate estimate;
    if (!reuseCoordinates) {
        estimate.addAllocation(coordinateCount * sizeof(Point));
    }
    estimate.addAllocation(coordinateCount * sizeof(uint8_t));
    if (!lightgraph::memory::admitted(object_.runtimeContext().memoryAdmission,
            lightgraph::memory::Operation::Geometry, estimate)) {
        return lastResult_ = GeometryResult::AdmissionDenied;
    }
    std::unique_ptr<Point[]> candidateCoordinates;
    if (!reuseCoordinates) {
        candidateCoordinates.reset(new (std::nothrow) Point[coordinateCount]);
        if (!candidateCoordinates) {
            reportAllocationFailure(2);
            return lastResult_ = GeometryResult::AllocationFailed;
        }
    }
    lightgraph::memory::Estimate mappedEstimate;
    mappedEstimate.addAllocation(coordinateCount * sizeof(uint8_t));
    if (!lightgraph::memory::admitted(object_.runtimeContext().memoryAdmission,
            lightgraph::memory::Operation::Geometry, mappedEstimate)) {
        return lastResult_ = GeometryResult::AdmissionDenied;
    }
    std::unique_ptr<uint8_t[]> mapped(new (std::nothrow) uint8_t[coordinateCount]());
    if (!mapped) {
        reportAllocationFailure(3);
        return lastResult_ = GeometryResult::AllocationFailed;
    }

    const auto markMapped = [&mapped, coordinateCount](uint16_t pixel) {
        if (pixel < coordinateCount) {
            mapped[pixel] = 1;
        }
    };
    for (unsigned group = 0; group < MAX_GROUPS; ++group) {
        for (const Connection* connection : object_.conn[group]) {
            for (uint16_t pixel = 0; pixel < connection->numLeds; ++pixel) {
                markMapped(connection->getPixel(pixel));
            }
        }
    }
    for (unsigned group = 0; group < 3; ++group) {
        for (const Intersection* intersection : object_.inter[group]) {
            markMapped(intersection->topPixel);
            if (intersection->bottomPixel >= 0) {
                markMapped(static_cast<uint16_t>(intersection->bottomPixel));
            }
        }
    }

    size_t orphanCount = 0;
    for (uint16_t real = 0; real < object_.realPixelCount; ++real) {
        const uint16_t logical = object_.translateToLogicalPixel(real);
        if (logical >= coordinateCount || mapped[logical] != 0) {
            continue;
        }
        uint16_t best = logical;
        if (findNearestAnchor(real, logical, mapped.get(), coordinateCount, best)) {
            ++orphanCount;
        }
    }

    std::unique_ptr<OrphanAssignment[]> candidateOrphans;
    if (orphanCount > 0) {
        lightgraph::memory::Estimate orphanEstimate;
        orphanEstimate.addAllocation(orphanCount * sizeof(OrphanAssignment));
        if (!lightgraph::memory::admitted(object_.runtimeContext().memoryAdmission,
                                          lightgraph::memory::Operation::Geometry,
                                          orphanEstimate)) {
            return lastResult_ = GeometryResult::AdmissionDenied;
        }
        candidateOrphans.reset(new (std::nothrow) OrphanAssignment[orphanCount]);
        if (!candidateOrphans) {
            reportAllocationFailure(4);
            return lastResult_ = GeometryResult::AllocationFailed;
        }
    }
    size_t orphanIndex = 0;
    for (uint16_t real = 0; real < object_.realPixelCount; ++real) {
        const uint16_t logical = object_.translateToLogicalPixel(real);
        if (logical >= coordinateCount || mapped[logical] != 0) {
            continue;
        }
        uint16_t best = logical;
        if (findNearestAnchor(real, logical, mapped.get(), coordinateCount, best)) {
            candidateOrphans[orphanIndex++] = {logical, best};
        }
    }

    // All fallible preparation is complete. Keep this commit allocation- and callback-free.
    Point* const coordinates = reuseCoordinates ? coordinates_.get() : candidateCoordinates.get();
    for (size_t pixel = 0; pixel < coordinateCount; ++pixel) {
        coordinates[pixel] = {NAN, NAN};
    }
    const auto put = [coordinates, coordinateCount](uint16_t pixel, Point point) {
        if (pixel < coordinateCount) {
            coordinates[pixel] = point;
        }
    };
    const auto position = [this](const Intersection* intersection) {
        for (unsigned group = 0; group < 3; ++group) {
            for (unsigned index = 0; index < object_.inter[group].size(); ++index) {
                if (object_.inter[group][index] == intersection) {
                    return intersectionPosition(group, index);
                }
            }
        }
        return Point{NAN, NAN};
    };
    for (unsigned group = 0; group < MAX_GROUPS; ++group) {
        for (const Connection* connection : object_.conn[group]) {
            const Point from = position(connection->from);
            const Point to = position(connection->to);
            for (uint16_t pixel = 0; pixel < connection->numLeds; ++pixel) {
                const float progress = static_cast<float>(pixel + 1) / (connection->numLeds + 1);
                put(connection->getPixel(pixel), {
                                                     from.x + (to.x - from.x) * progress,
                                                     from.y + (to.y - from.y) * progress,
                                                 });
            }
        }
    }
    for (unsigned group = 0; group < 3; ++group) {
        for (unsigned index = 0; index < object_.inter[group].size(); ++index) {
            const Intersection* intersection = object_.inter[group][index];
            const Point point = intersectionPosition(group, index);
            put(intersection->topPixel, point);
            if (intersection->bottomPixel >= 0) {
                put(static_cast<uint16_t>(intersection->bottomPixel), point);
            }
        }
    }
    for (size_t index = 0; index < orphanCount; ++index) {
        const OrphanAssignment assignment = candidateOrphans[index];
        coordinates[assignment.pixel] = coordinates[assignment.neighbor];
    }

    if (!reuseCoordinates) {
        coordinates_ = std::move(candidateCoordinates);
    }
    coordinateCount_ = coordinateCount;
    orphans_ = std::move(candidateOrphans);
    orphanCount_ = orphanCount;
    geometryRevision_ = requestedRevision;
    initialized_ = true;
    return lastResult_ = GeometryResult::Ready;
}

HeptagonGeometry::ArrayView<HeptagonGeometry::Point> HeptagonGeometry::points() const noexcept {
    return {coordinates_.get(), coordinateCount_};
}

HeptagonGeometry::ArrayView<HeptagonGeometry::Point> HeptagonGeometry::outline() const noexcept {
    return {outline_, 7};
}

HeptagonGeometry::ArrayView<HeptagonGeometry::OrphanAssignment> HeptagonGeometry::orphanAssignments() const noexcept {
    return {orphans_.get(), orphanCount_};
}
