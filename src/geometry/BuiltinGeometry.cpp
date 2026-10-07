#include "BuiltinGeometry.h"

#include <array>
#include <cstdint>
#include <new>

#include "../topology/TopologyObject.h"

namespace lightgraph::geometry {
namespace {

class BuiltinGeometry final : public GeometryProvider {
  public:
    BuiltinGeometry(TopologyObject& object, BuiltinGeometryKind kind, uint16_t compatiblePixelCount,
                    const BuiltinGeometryRoles& roles)
        : GeometryProvider(object), kind_(kind), compatiblePixelCount_(compatiblePixelCount),
          roles_(roles) {}

    GeometryResult refresh() noexcept override {
        const uint32_t revision = sourceTopologyRevision();
        Layout layout;
        if (!resolveLayout(layout))
            return GeometryResult::Unsupported;
        if (points_ && geometryRevision_ == revision)
            return GeometryResult::Ready;

        const size_t count = object_.pixelCount;
        lightgraph::memory::Estimate estimate;
        estimate.addAllocation(count * sizeof(Point));
        if (!lightgraph::memory::admitted(object_.runtimeContext().memoryAdmission,
                                          lightgraph::memory::Operation::Geometry, estimate)) {
            return GeometryResult::AdmissionDenied;
        }
        std::unique_ptr<Point[]> candidate(new (std::nothrow) Point[count]);
        if (!candidate) {
            lightgraphReportAllocationFailure(
                object_.runtimeContext(),
                LightgraphAllocationFailureSite::HeptagonGeometryAllocation, object_.pixelCount, 5);
            return GeometryResult::AllocationFailed;
        }

        switch (kind_) {
        case BuiltinGeometryKind::Line:
            fillLine(candidate.get(), count, layout);
            break;
        case BuiltinGeometryKind::Cross:
            fillCross(candidate.get(), count, layout);
            break;
        case BuiltinGeometryKind::Triangle:
            fillTriangle(candidate.get(), count, layout);
            break;
        }
        points_ = std::move(candidate);
        pointCount_ = count;
        geometryRevision_ = revision;
        return GeometryResult::Ready;
    }

    bool compatible() const noexcept override {
        Layout layout;
        return resolveLayout(layout);
    }

    ArrayView<Point> points() const noexcept override { return {points_.get(), pointCount_}; }

    ArrayView<Point> outline() const noexcept override {
        switch (kind_) {
        case BuiltinGeometryKind::Line:
            return {kLineOutline, 2};
        case BuiltinGeometryKind::Cross:
            return {kCrossOutline, 4};
        case BuiltinGeometryKind::Triangle:
            return {kTriangleOutline, 3};
        }
        return {};
    }

  private:
    struct Layout {
        std::array<size_t, 6> anchors{};
    };

    bool resolveRoles(std::array<const Intersection*, 5>& resolved,
                      uint8_t expectedCount) const noexcept {
        if (object_.pixelCount != compatiblePixelCount_ || roles_.count != expectedCount) {
            return false;
        }
        for (uint8_t i = 0; i < expectedCount; ++i) {
            for (uint8_t previous = 0; previous < i; ++previous) {
                if (roles_.intersectionIds[i] == roles_.intersectionIds[previous])
                    return false;
            }
            resolved[i] = object_.findIntersectionById(roles_.intersectionIds[i]);
            if (resolved[i] == nullptr)
                return false;
        }
        return true;
    }

    bool resolveLayout(Layout& layout) const noexcept {
        std::array<const Intersection*, 5> role{};
        switch (kind_) {
        case BuiltinGeometryKind::Line: {
            if (!resolveRoles(role, 2))
                return false;
            if (!validRole(role[0], 2, false) || !validRole(role[1], 2, false))
                return false;
            const size_t first = role[0]->topPixel;
            const size_t last = role[1]->topPixel;
            if (object_.pixelCount == 1) {
                if (first != 0 || last != 0)
                    return false;
            } else if (last >= object_.pixelCount || first >= last) {
                return false;
            }
            layout.anchors[0] = first;
            layout.anchors[1] = last;
            return true;
        }
        case BuiltinGeometryKind::Cross: {
            if (!resolveRoles(role, 5))
                return false;
            for (uint8_t endpoint = 0; endpoint < 4; ++endpoint) {
                if (!validRole(role[endpoint], 3, false))
                    return false;
            }
            if (!validRole(role[4], 4, true))
                return false;
            const size_t verticalStart = object_.pixelCount / 2;
            if (verticalStart == 0)
                return false;
            const int16_t verticalCenter = role[4]->bottomPixel;
            if (verticalCenter < 0)
                return false;
            layout.anchors[0] = role[0]->topPixel;
            layout.anchors[1] = role[4]->topPixel;
            layout.anchors[2] = role[1]->topPixel;
            layout.anchors[3] = role[2]->topPixel;
            layout.anchors[4] = static_cast<size_t>(verticalCenter);
            layout.anchors[5] = role[3]->topPixel;
            if (!(layout.anchors[0] < layout.anchors[1] && layout.anchors[1] < layout.anchors[2] &&
                  layout.anchors[2] < verticalStart && verticalStart <= layout.anchors[3] &&
                  layout.anchors[3] < layout.anchors[4] && layout.anchors[4] < layout.anchors[5] &&
                  layout.anchors[5] < object_.pixelCount)) {
                return false;
            }
            return true;
        }
        case BuiltinGeometryKind::Triangle: {
            if (!resolveRoles(role, 3))
                return false;
            for (uint8_t vertex = 0; vertex < 3; ++vertex) {
                if (!validRole(role[vertex], 2, true))
                    return false;
            }
            layout.anchors[0] = role[0]->topPixel;
            layout.anchors[1] = role[1]->topPixel;
            layout.anchors[2] = static_cast<size_t>(role[1]->bottomPixel);
            layout.anchors[3] = role[2]->topPixel;
            layout.anchors[4] = static_cast<size_t>(role[2]->bottomPixel);
            layout.anchors[5] = static_cast<size_t>(role[0]->bottomPixel);
            if (!(layout.anchors[0] < layout.anchors[1] && layout.anchors[1] <= layout.anchors[2] &&
                  layout.anchors[2] < layout.anchors[3] && layout.anchors[3] <= layout.anchors[4] &&
                  layout.anchors[4] < layout.anchors[5] &&
                  layout.anchors[5] < object_.pixelCount)) {
                return false;
            }
            return true;
        }
        }
        return false;
    }

    static bool validRole(const Intersection* role, uint8_t minimumPorts,
                          bool requiresBottomPixel) noexcept {
        if (role->group != GROUP1 || role->numPorts < minimumPorts)
            return false;
        return requiresBottomPixel ? role->bottomPixel >= 0 : role->bottomPixel == -1;
    }

    static float interpolateAxis(size_t value, size_t first, size_t center, size_t last) {
        if (value <= first)
            return -1.0f;
        if (value <= center) {
            return -1.0f + static_cast<float>(value - first) / static_cast<float>(center - first);
        }
        if (value >= last)
            return 1.0f;
        return static_cast<float>(value - center) / static_cast<float>(last - center);
    }

    static Point interpolatePoint(size_t value, size_t first, size_t last, const Point& from,
                                  const Point& to) {
        if (value <= first)
            return from;
        if (value >= last)
            return to;
        const float progress = static_cast<float>(value - first) / static_cast<float>(last - first);
        return {from.x + (to.x - from.x) * progress, from.y + (to.y - from.y) * progress};
    }

    static void fillLine(Point* points, size_t count, const Layout& layout) {
        if (count == 1) {
            points[0] = {0.0f, 0.0f};
            return;
        }
        for (size_t i = 0; i < count; ++i) {
            points[i] = interpolatePoint(i, layout.anchors[0], layout.anchors[1], kLineOutline[0],
                                         kLineOutline[1]);
        }
    }

    static void fillCross(Point* points, size_t count, const Layout& layout) {
        const size_t verticalStart = count / 2;
        for (size_t i = 0; i < count; ++i) {
            if (i < verticalStart) {
                points[i] = {
                    interpolateAxis(i, layout.anchors[0], layout.anchors[1], layout.anchors[2]),
                    0.0f};
            } else {
                points[i] = {0.0f, interpolateAxis(i, layout.anchors[3], layout.anchors[4],
                                                   layout.anchors[5])};
            }
        }
    }

    static void fillTriangle(Point* points, size_t count, const Layout& layout) {
        for (size_t i = 0; i < count; ++i) {
            if (i <= layout.anchors[1]) {
                points[i] = interpolatePoint(i, layout.anchors[0], layout.anchors[1],
                                             kTriangleOutline[0], kTriangleOutline[1]);
            } else if (i < layout.anchors[2]) {
                points[i] = kTriangleOutline[1];
            } else if (i <= layout.anchors[3]) {
                points[i] = interpolatePoint(i, layout.anchors[2], layout.anchors[3],
                                             kTriangleOutline[1], kTriangleOutline[2]);
            } else if (i < layout.anchors[4]) {
                points[i] = kTriangleOutline[2];
            } else {
                points[i] = interpolatePoint(i, layout.anchors[4], layout.anchors[5],
                                             kTriangleOutline[2], kTriangleOutline[0]);
            }
        }
    }

    BuiltinGeometryKind kind_;
    uint16_t compatiblePixelCount_;
    BuiltinGeometryRoles roles_;
    std::unique_ptr<Point[]> points_;
    size_t pointCount_ = 0;

    static constexpr Point kLineOutline[2] = {{-1.0f, 0.0f}, {1.0f, 0.0f}};
    static constexpr Point kCrossOutline[4] = {
        {-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f}};
    static constexpr Point kTriangleOutline[3] = {
        {0.0f, -1.0f}, {0.8660254f, 0.5f}, {-0.8660254f, 0.5f}};
};

constexpr GeometryPoint BuiltinGeometry::kLineOutline[2];
constexpr GeometryPoint BuiltinGeometry::kCrossOutline[4];
constexpr GeometryPoint BuiltinGeometry::kTriangleOutline[3];

} // namespace

std::unique_ptr<GeometryProvider> createBuiltinGeometry(TopologyObject& object,
                                                        BuiltinGeometryKind kind,
                                                        uint16_t compatiblePixelCount,
                                                        const BuiltinGeometryRoles& roles,
                                                        GeometryResult& result) noexcept {
    result = GeometryResult::AdmissionDenied;
    lightgraph::memory::Estimate estimate;
    estimate.addAllocation(sizeof(BuiltinGeometry));
    if (!lightgraph::memory::admitted(object.runtimeContext().memoryAdmission,
                                      lightgraph::memory::Operation::Geometry, estimate))
        return nullptr;
    std::unique_ptr<GeometryProvider> geometry(
        new (std::nothrow) BuiltinGeometry(object, kind, compatiblePixelCount, roles));
    if (!geometry) {
        result = GeometryResult::AllocationFailed;
        lightgraphReportAllocationFailure(
            object.runtimeContext(), LightgraphAllocationFailureSite::HeptagonGeometryAllocation,
            object.pixelCount, 6);
    }
    if (geometry)
        result = GeometryResult::Ready;
    return geometry;
}

} // namespace lightgraph::geometry
