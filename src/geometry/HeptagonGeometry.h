#ifndef PACKAGES_LIGHTGRAPH_SRC_GEOMETRY_HEPTAGONGEOMETRY_H_
#define PACKAGES_LIGHTGRAPH_SRC_GEOMETRY_HEPTAGONGEOMETRY_H_

#pragma once

#include <memory>

#include "GeometryProvider.h"

// Approximate front-view coordinates. Positive y points down, as in the simulator.
class HeptagonGeometry : public lightgraph::geometry::GeometryProvider {
public:
    struct OrphanAssignment {
        uint16_t pixel;
        uint16_t neighbor;
    };

    HeptagonGeometry(TopologyObject& object, uint16_t firstStripPixelCount);

    static Point intersectionPosition(unsigned group, unsigned index);
    lightgraph::geometry::GeometryResult refresh() noexcept override;
    bool compatible() const noexcept override { return isCompatibleHeptagon(); }
    bool admissionRejected() const noexcept {
        return lastResult_ == lightgraph::geometry::GeometryResult::AdmissionDenied;
    }
    bool isCompatibleHeptagon() const noexcept;
    void reportAllocationFailure(uint16_t stage) const;
    ArrayView<Point> points() const noexcept override;
    ArrayView<Point> outline() const noexcept override;
    ArrayView<OrphanAssignment> orphanAssignments() const noexcept;

private:
    bool findNearestAnchor(uint16_t realPixel,
                           uint16_t logicalPixel,
                           const uint8_t* mapped,
                           size_t coordinateCount,
                           uint16_t& nearest) const noexcept;

    uint16_t firstStripPixelCount_;
    bool initialized_ = false;
    lightgraph::geometry::GeometryResult lastResult_ = lightgraph::geometry::GeometryResult::Unsupported;
    std::unique_ptr<Point[]> coordinates_;
    size_t coordinateCount_ = 0;
    std::unique_ptr<OrphanAssignment[]> orphans_;
    size_t orphanCount_ = 0;
    Point outline_[7]{};
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_GEOMETRY_HEPTAGONGEOMETRY_H_
