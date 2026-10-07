#include <lightgraph/integration/geometry.hpp>
#include <lightgraph/integration/objects.hpp>
#include <lightgraph/integration/runtime.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using lightgraph::integration::Heptagon3024;
using lightgraph::integration::Heptagon919;
using lightgraph::integration::HeptagonGeometry;
using lightgraph::integration::GeometryProvider;
using lightgraph::integration::GeometryResult;
using lightgraph::integration::Line;
using lightgraph::integration::Cross;
using lightgraph::integration::Triangle;
using lightgraph::integration::RuntimeState;

int failures = 0;

void fail(const char* file, int line, const std::string& expression, const std::string& detail = {}) {
    std::cerr << file << ':' << line << ": check failed: " << expression;
    if (!detail.empty()) std::cerr << " (" << detail << ')';
    std::cerr << '\n';
    failures++;
}

#define CHECK(expression) do { if (!(expression)) fail(__FILE__, __LINE__, #expression); } while (0)
#define CHECK_EQ(actual, expected) do { \
    const auto actualValue = (actual); \
    const auto expectedValue = (expected); \
    if (!(actualValue == expectedValue)) { \
        std::ostringstream detail; \
        detail << "actual=" << +actualValue << ", expected=" << +expectedValue; \
        fail(__FILE__, __LINE__, #actual " == " #expected, detail.str()); \
    } \
} while (0)

bool close(float left, float right, float tolerance = 0.00001f) {
    return std::fabs(left - right) <= tolerance;
}

void checkPoint(const HeptagonGeometry::Point& actual, const HeptagonGeometry::Point& expected) {
    CHECK(close(actual.x, expected.x));
    CHECK(close(actual.y, expected.y));
}

HeptagonGeometry::Point intersectionPoint(const TopologyObject& object,
                                          const Intersection* candidate) {
    for (unsigned group = 0; group < 3; ++group) {
        for (unsigned index = 0; index < object.inter[group].size(); ++index) {
            if (object.inter[group][index] == candidate) {
                return HeptagonGeometry::intersectionPosition(group, index);
            }
        }
    }
    return {NAN, NAN};
}

template <typename Heptagon>
void checkCoverage(Heptagon& object, std::size_t expectedOrphans,
                   std::uint16_t firstStripPixelCount) {
    CHECK(object.supportsGeometry());
    std::unique_ptr<GeometryProvider> provider = object.createGeometry();
    CHECK(provider != nullptr);
    if (provider) {
        CHECK(provider->compatible());
        CHECK(provider->refresh() == GeometryResult::Ready);
        CHECK_EQ(provider->points().size(), object.pixelCount);
        CHECK_EQ(provider->outline().size(), 7u);
    }
    HeptagonGeometry geometry(object, firstStripPixelCount);
    CHECK(geometry.isCompatibleHeptagon());
    CHECK(geometry.refresh() == GeometryResult::Ready);

    const auto points = geometry.points();
    CHECK_EQ(points.size(), object.pixelCount);
    for (std::uint16_t real = 0; real < object.realPixelCount; ++real) {
        const std::uint16_t logical = object.translateToLogicalPixel(real);
        CHECK(logical < points.size());
        if (logical < points.size()) {
            CHECK(std::isfinite(points[logical].x));
            CHECK(std::isfinite(points[logical].y));
        }
        CHECK_EQ(object.translateToRealPixel(logical), real);
    }

    CHECK_EQ(object.inter[0].size(), 14u);
    CHECK_EQ(object.inter[1].size(), 7u);
    CHECK_EQ(object.inter[2].size(), 7u);
    for (unsigned tip = 0; tip < 7; ++tip) {
        checkPoint(HeptagonGeometry::intersectionPosition(0, tip * 2),
                   HeptagonGeometry::intersectionPosition(0, tip * 2 + 1));
    }
    for (unsigned group = 0; group < 3; ++group) {
        for (unsigned index = 0; index < object.inter[group].size(); ++index) {
            const Intersection* intersection = object.inter[group][index];
            const auto expected = HeptagonGeometry::intersectionPosition(group, index);
            checkPoint(points[intersection->topPixel], expected);
            if (intersection->bottomPixel >= 0) {
                checkPoint(points[static_cast<std::uint16_t>(intersection->bottomPixel)], expected);
            }
        }
    }

    for (unsigned group = 0; group < MAX_GROUPS; ++group) {
        for (const Connection* connection : object.conn[group]) {
            const auto from = intersectionPoint(object, connection->from);
            const auto to = intersectionPoint(object, connection->to);
            for (std::uint16_t index = 0; index < connection->numLeds; ++index) {
                const float progress = static_cast<float>(index + 1) / (connection->numLeds + 1);
                checkPoint(points[connection->getPixel(index)], {
                    from.x + (to.x - from.x) * progress,
                    from.y + (to.y - from.y) * progress,
                });
            }
        }
    }

    const auto assignments = geometry.orphanAssignments();
    CHECK_EQ(assignments.size(), expectedOrphans);
    std::set<std::uint16_t> orphanPixels;
    for (const auto& assignment : assignments) orphanPixels.insert(assignment.pixel);
    for (const auto& assignment : assignments) {
        CHECK(assignment.pixel < points.size());
        CHECK(assignment.neighbor < points.size());
        if (assignment.pixel < points.size() && assignment.neighbor < points.size()) {
            checkPoint(points[assignment.pixel], points[assignment.neighbor]);
        }
        const int16_t orphanReal = object.translateToRealPixel(assignment.pixel);
        const int16_t neighborReal = object.translateToRealPixel(assignment.neighbor);
        CHECK(orphanReal >= 0);
        CHECK(neighborReal >= 0);
        if (orphanReal < 0 || neighborReal < 0) continue;
        CHECK_EQ(orphanReal < firstStripPixelCount, neighborReal < firstStripPixelCount);

        std::uint16_t expected = 0;
        unsigned bestDistance = std::numeric_limits<unsigned>::max();
        for (std::uint16_t anchorReal = 0; anchorReal < object.realPixelCount; ++anchorReal) {
            if ((anchorReal < firstStripPixelCount) != (orphanReal < firstStripPixelCount)) continue;
            const std::uint16_t anchor = object.translateToLogicalPixel(anchorReal);
            if (orphanPixels.count(anchor) != 0) continue;
            const unsigned distance = anchor > assignment.pixel
                ? anchor - assignment.pixel : assignment.pixel - anchor;
            if (distance < bestDistance || (distance == bestDistance && anchor < expected)) {
                bestDistance = distance;
                expected = anchor;
            }
        }
        CHECK_EQ(assignment.neighbor, expected);
    }

    for (const PixelGap& gap : object.gaps) {
        for (std::uint16_t logical = gap.fromPixel; logical <= gap.toPixel; ++logical) {
            CHECK_EQ(object.translateToRealPixel(logical), -1);
        }
    }
}

void checkGeometryMath() {
    const float radii[] = {1.0f, 4.0f / 9.0f, 5.0f / 18.0f};
    for (unsigned group = 0; group < 3; ++group) {
        const auto point = HeptagonGeometry::intersectionPosition(group, 0);
        CHECK(close(std::hypot(point.x, point.y), radii[group]));
    }
}

void checkTopologyReplacementInvalidatesGeometry() {
    Heptagon919 object;
    const std::size_t originalPixelCount = object.pixelCount;
    HeptagonGeometry geometry(object, HEPTAGON919_PIXEL_COUNT1);
    CHECK(geometry.refresh() == GeometryResult::Ready);
    CHECK_EQ(geometry.points().size(), originalPixelCount);

    TopologySnapshot original;
    CHECK(object.exportSnapshot(original));
    Line line(300);
    TopologySnapshot incompatible;
    CHECK(line.exportSnapshot(incompatible));
    CHECK(object.importSnapshot(incompatible));
    CHECK(!geometry.isCompatibleHeptagon());
    CHECK(geometry.refresh() == GeometryResult::Unsupported);
    CHECK_EQ(geometry.points().size(), originalPixelCount);

    CHECK(object.importSnapshot(original));
    CHECK(geometry.isCompatibleHeptagon());
    CHECK(geometry.refresh() == GeometryResult::Ready);
    CHECK_EQ(geometry.points().size(), object.pixelCount);
}

void checkUnsupportedObject() {
    class UnsupportedObject final : public TopologyObject {
    public:
        UnsupportedObject() : TopologyObject(1) {}
        uint16_t* getMirroredPixels(uint16_t, Owner*, bool) override { return mirror_; }
        EmitParams getModelParams(int) const override { return {}; }
    private:
        uint16_t mirror_[1] = {0};
    } object;
    CHECK(!object.supportsGeometry());
    CHECK(object.createGeometry() == nullptr);
}

bool denyGeometry(lightgraph::memory::Operation operation,
                  const lightgraph::memory::Estimate&, void* user) noexcept {
    return operation != lightgraph::memory::Operation::Geometry || !*static_cast<bool*>(user);
}

struct StagedAdmission {
    std::size_t denyAt = 0;
    std::size_t checks = 0;
};

bool denyStagedGeometry(lightgraph::memory::Operation operation,
                        const lightgraph::memory::Estimate&, void* user) noexcept {
    if (operation != lightgraph::memory::Operation::Geometry)
        return true;
    auto& staged = *static_cast<StagedAdmission*>(user);
    return staged.checks++ != staged.denyAt;
}

template <typename Object>
void checkFiniteCoverage(Object& object) {
    CHECK(object.supportsGeometry());
    std::unique_ptr<GeometryProvider> geometry = object.createGeometry();
    CHECK(geometry != nullptr);
    if (!geometry) return;
    CHECK(geometry->refresh() == GeometryResult::Ready);
    CHECK_EQ(geometry->points().size(), object.pixelCount);
    CHECK(!geometry->outline().empty());
    for (const auto& point : geometry->points()) {
        CHECK(std::isfinite(point.x));
        CHECK(std::isfinite(point.y));
    }
}

void checkBuiltinGeometry() {
    Line singlePixelLine(1);
    auto singlePixelGeometry = singlePixelLine.createGeometry();
    CHECK(singlePixelGeometry != nullptr);
    CHECK(singlePixelGeometry->refresh() == GeometryResult::Ready);
    CHECK_EQ(singlePixelGeometry->points().size(), 1u);
    if (singlePixelGeometry->points().size() == 1) {
        checkPoint(singlePixelGeometry->points()[0], {0.0f, 0.0f});
    }

    Line line(300);
    checkFiniteCoverage(line);
    auto lineGeometry = line.createGeometry();
    CHECK(lineGeometry->refresh() == GeometryResult::Ready);
    checkPoint(lineGeometry->points()[0], {-1.0f, 0.0f});
    checkPoint(lineGeometry->points()[299], {1.0f, 0.0f});
    const auto previousRevision = lineGeometry->topologyRevision();
    TopologyIntersectionUpdate compatibleUpdate;
    compatibleUpdate.numPorts = line.inter[0][0]->numPorts;
    compatibleUpdate.topPixel = static_cast<std::uint16_t>(line.inter[0][0]->topPixel - 1);
    compatibleUpdate.bottomPixel = line.inter[0][0]->bottomPixel;
    compatibleUpdate.group = line.inter[0][0]->group;
    compatibleUpdate.allowEndOfLife = !line.inter[0][0]->allowEndOfLife;
    compatibleUpdate.allowEmit = line.inter[0][0]->allowEmit;
    CHECK(line.updateIntersection(line.inter[0][0], compatibleUpdate));
    CHECK(lineGeometry->compatible());
    CHECK(lineGeometry->refresh() == GeometryResult::Ready);
    CHECK(lineGeometry->topologyRevision() != previousRevision);

    TopologyIntersectionUpdate movedLineStart;
    movedLineStart.numPorts = line.inter[0][1]->numPorts;
    movedLineStart.topPixel = 10;
    movedLineStart.bottomPixel = line.inter[0][1]->bottomPixel;
    movedLineStart.group = line.inter[0][1]->group;
    movedLineStart.allowEndOfLife = line.inter[0][1]->allowEndOfLife;
    movedLineStart.allowEmit = line.inter[0][1]->allowEmit;
    CHECK(line.updateIntersection(line.inter[0][1], movedLineStart));
    CHECK(lineGeometry->refresh() == GeometryResult::Ready);
    checkPoint(lineGeometry->points()[0], {-1.0f, 0.0f});
    checkPoint(lineGeometry->points()[10], {-1.0f, 0.0f});
    checkPoint(lineGeometry->points()[298], {1.0f, 0.0f});
    checkPoint(lineGeometry->points()[299], {1.0f, 0.0f});

    Cross cross(288);
    checkFiniteCoverage(cross);
    auto crossGeometry = cross.createGeometry();
    CHECK(crossGeometry->refresh() == GeometryResult::Ready);
    checkPoint(crossGeometry->points()[0], {-1.0f, 0.0f});
    checkPoint(crossGeometry->points()[72], {0.0f, 0.0f});
    checkPoint(crossGeometry->points()[144], {0.0f, -1.0f});
    checkPoint(crossGeometry->points()[216], {0.0f, 0.0f});
    checkPoint(crossGeometry->points()[287], {0.0f, 1.0f});

    Intersection* crossCenter = cross.inter[0][4];
    const uint8_t crossCenterId = crossCenter->id;
    TopologyIntersectionUpdate movedCross;
    movedCross.numPorts = crossCenter->numPorts;
    movedCross.topPixel = 70;
    movedCross.bottomPixel = 220;
    movedCross.group = crossCenter->group;
    movedCross.allowEndOfLife = crossCenter->allowEndOfLife;
    movedCross.allowEmit = crossCenter->allowEmit;
    CHECK(cross.updateIntersection(crossCenter, movedCross));
    CHECK(crossGeometry->refresh() == GeometryResult::Ready);
    checkPoint(crossGeometry->points()[70], {0.0f, 0.0f});
    checkPoint(crossGeometry->points()[220], {0.0f, 0.0f});
    CHECK(crossGeometry->points()[72].x > 0.0f);
    CHECK(crossGeometry->points()[216].y < 0.0f);

    auto moveTopPixel = [&cross](Intersection* intersection, uint16_t topPixel) {
        TopologyIntersectionUpdate update;
        update.numPorts = intersection->numPorts;
        update.topPixel = topPixel;
        update.bottomPixel = intersection->bottomPixel;
        update.group = intersection->group;
        update.allowEndOfLife = intersection->allowEndOfLife;
        update.allowEmit = intersection->allowEmit;
        return cross.updateIntersection(intersection, update);
    };
    CHECK(moveTopPixel(cross.inter[0][1], 5));
    CHECK(moveTopPixel(cross.inter[0][0], 138));
    CHECK(moveTopPixel(cross.inter[0][3], 150));
    CHECK(moveTopPixel(cross.inter[0][2], 280));
    CHECK(crossGeometry->refresh() == GeometryResult::Ready);
    checkPoint(crossGeometry->points()[0], {-1.0f, 0.0f});
    checkPoint(crossGeometry->points()[5], {-1.0f, 0.0f});
    checkPoint(crossGeometry->points()[138], {1.0f, 0.0f});
    checkPoint(crossGeometry->points()[143], {1.0f, 0.0f});
    checkPoint(crossGeometry->points()[144], {0.0f, -1.0f});
    checkPoint(crossGeometry->points()[150], {0.0f, -1.0f});
    checkPoint(crossGeometry->points()[280], {0.0f, 1.0f});
    checkPoint(crossGeometry->points()[287], {0.0f, 1.0f});

    Triangle triangle(900);
    checkFiniteCoverage(triangle);
    auto triangleGeometry = triangle.createGeometry();
    CHECK(triangleGeometry->refresh() == GeometryResult::Ready);
    checkPoint(triangleGeometry->points()[0], {0.0f, -1.0f});
    checkPoint(triangleGeometry->points()[299], {0.8660254f, 0.5f});
    checkPoint(triangleGeometry->points()[300], {0.8660254f, 0.5f});
    checkPoint(triangleGeometry->points()[599], {-0.8660254f, 0.5f});
    checkPoint(triangleGeometry->points()[899], {0.0f, -1.0f});

    Intersection* vertex2 = triangle.inter[0][1];
    TopologyIntersectionUpdate movedVertex2;
    movedVertex2.numPorts = vertex2->numPorts;
    movedVertex2.topPixel = 280;
    movedVertex2.bottomPixel = 310;
    movedVertex2.group = vertex2->group;
    movedVertex2.allowEndOfLife = vertex2->allowEndOfLife;
    movedVertex2.allowEmit = vertex2->allowEmit;
    CHECK(triangle.updateIntersection(vertex2, movedVertex2));
    Intersection* vertex3 = triangle.inter[0][2];
    TopologyIntersectionUpdate movedVertex3;
    movedVertex3.numPorts = vertex3->numPorts;
    movedVertex3.topPixel = 610;
    movedVertex3.bottomPixel = 620;
    movedVertex3.group = vertex3->group;
    movedVertex3.allowEndOfLife = vertex3->allowEndOfLife;
    movedVertex3.allowEmit = vertex3->allowEmit;
    CHECK(triangle.updateIntersection(vertex3, movedVertex3));
    CHECK(triangleGeometry->refresh() == GeometryResult::Ready);
    checkPoint(triangleGeometry->points()[280], {0.8660254f, 0.5f});
    checkPoint(triangleGeometry->points()[310], {0.8660254f, 0.5f});
    checkPoint(triangleGeometry->points()[610], {-0.8660254f, 0.5f});
    checkPoint(triangleGeometry->points()[620], {-0.8660254f, 0.5f});

    TopologySnapshot editedSnapshot;
    CHECK(cross.exportSnapshot(editedSnapshot));
    for (TopologyIntersectionSnapshot& intersection : editedSnapshot.intersections) {
        if (intersection.id == crossCenterId) {
            intersection.topPixel = 68;
            intersection.bottomPixel = 222;
        }
    }
    CHECK(cross.importSnapshot(editedSnapshot));
    CHECK(crossGeometry->refresh() == GeometryResult::Ready);
    checkPoint(crossGeometry->points()[68], {0.0f, 0.0f});
    checkPoint(crossGeometry->points()[222], {0.0f, 0.0f});

    const uint8_t originalCenterId = crossCenterId;
    uint8_t replacementCenterId = static_cast<uint8_t>(originalCenterId + 1);
    auto idIsUsed = [&editedSnapshot](uint8_t id) {
        return std::any_of(editedSnapshot.intersections.begin(), editedSnapshot.intersections.end(),
                           [id](const TopologyIntersectionSnapshot& intersection) {
                               return intersection.id == id;
                           });
    };
    while (idIsUsed(replacementCenterId)) {
        replacementCenterId = static_cast<uint8_t>(replacementCenterId + 1);
    }
    for (TopologyIntersectionSnapshot& intersection : editedSnapshot.intersections) {
        if (intersection.id == originalCenterId) intersection.id = replacementCenterId;
    }
    for (TopologyConnectionSnapshot& connection : editedSnapshot.connections) {
        if (connection.fromIntersectionId == originalCenterId) {
            connection.fromIntersectionId = replacementCenterId;
        }
        if (connection.toIntersectionId == originalCenterId) {
            connection.toIntersectionId = replacementCenterId;
        }
    }
    for (TopologyPortSnapshot& port : editedSnapshot.ports) {
        if (port.intersectionId == originalCenterId) port.intersectionId = replacementCenterId;
        if (port.targetIntersectionId == originalCenterId) {
            port.targetIntersectionId = replacementCenterId;
        }
    }
    CHECK(cross.importSnapshot(editedSnapshot));
    CHECK(crossGeometry->refresh() == GeometryResult::Unsupported);
    CHECK_EQ(crossGeometry->points().size(), 288u);

    TopologyIntersectionUpdate reversedLineEnd;
    reversedLineEnd.numPorts = line.inter[0][0]->numPorts;
    reversedLineEnd.topPixel = 5;
    reversedLineEnd.bottomPixel = line.inter[0][0]->bottomPixel;
    reversedLineEnd.group = line.inter[0][0]->group;
    reversedLineEnd.allowEndOfLife = line.inter[0][0]->allowEndOfLife;
    reversedLineEnd.allowEmit = line.inter[0][0]->allowEmit;
    CHECK(line.updateIntersection(line.inter[0][0], reversedLineEnd));
    CHECK(lineGeometry->refresh() == GeometryResult::Unsupported);
    CHECK_EQ(lineGeometry->points().size(), 300u);
}

void checkBuiltinRoleValidation() {
    Line changedGroup(300);
    auto changedGroupGeometry = changedGroup.createGeometry();
    CHECK(changedGroupGeometry->refresh() == GeometryResult::Ready);
    Intersection* changedGroupStart = changedGroup.inter[0][1];
    TopologyIntersectionUpdate groupUpdate;
    groupUpdate.numPorts = changedGroupStart->numPorts;
    groupUpdate.topPixel = changedGroupStart->topPixel;
    groupUpdate.bottomPixel = changedGroupStart->bottomPixel;
    groupUpdate.group = GROUP2;
    groupUpdate.allowEndOfLife = changedGroupStart->allowEndOfLife;
    groupUpdate.allowEmit = changedGroupStart->allowEmit;
    CHECK(changedGroup.updateIntersection(changedGroupStart, groupUpdate));
    CHECK(changedGroupGeometry->refresh() == GeometryResult::Unsupported);
    CHECK_EQ(changedGroupGeometry->points().size(), 300u);

    Line changedEndpointKind(300);
    auto changedEndpointGeometry = changedEndpointKind.createGeometry();
    CHECK(changedEndpointGeometry->refresh() == GeometryResult::Ready);
    Intersection* changedEndpoint = changedEndpointKind.inter[0][1];
    TopologyIntersectionUpdate endpointUpdate;
    endpointUpdate.numPorts = changedEndpoint->numPorts;
    endpointUpdate.topPixel = changedEndpoint->topPixel;
    endpointUpdate.bottomPixel = 0;
    endpointUpdate.group = changedEndpoint->group;
    endpointUpdate.allowEndOfLife = changedEndpoint->allowEndOfLife;
    endpointUpdate.allowEmit = changedEndpoint->allowEmit;
    CHECK(changedEndpointKind.updateIntersection(changedEndpoint, endpointUpdate));
    CHECK(changedEndpointGeometry->refresh() == GeometryResult::Unsupported);

    Cross reducedCenter(288);
    auto reducedCenterGeometry = reducedCenter.createGeometry();
    CHECK(reducedCenterGeometry->refresh() == GeometryResult::Ready);
    Intersection* center = reducedCenter.inter[0][4];
    TopologyIntersectionUpdate moveCenter;
    moveCenter.numPorts = center->numPorts;
    moveCenter.topPixel = 70;
    moveCenter.bottomPixel = 220;
    moveCenter.group = center->group;
    moveCenter.allowEndOfLife = center->allowEndOfLife;
    moveCenter.allowEmit = center->allowEmit;
    CHECK(reducedCenter.updateIntersection(center, moveCenter));
    CHECK(reducedCenterGeometry->refresh() == GeometryResult::Ready);
    const uint32_t revisionBeforePortChange = reducedCenter.topologyRevision();
    TopologyIntersectionUpdate reduceCenterPorts = moveCenter;
    reduceCenterPorts.numPorts = 2;
    CHECK(reducedCenter.updateIntersection(center, reduceCenterPorts));
    CHECK_EQ(reducedCenter.topologyRevision(), revisionBeforePortChange);
    CHECK(reducedCenterGeometry->refresh() == GeometryResult::Unsupported);
    CHECK_EQ(reducedCenterGeometry->points().size(), 288u);

    Cross expandedCenter(288);
    auto expandedCenterGeometry = expandedCenter.createGeometry();
    CHECK(expandedCenterGeometry->refresh() == GeometryResult::Ready);
    Intersection* expandableCenter = expandedCenter.inter[0][4];
    CHECK(expandedCenter.ensureIntersectionHasFreePortSlot(expandableCenter));
    CHECK_EQ(expandableCenter->numPorts, 5);
    CHECK(expandedCenterGeometry->compatible());
    CHECK(expandedCenterGeometry->refresh() == GeometryResult::Ready);
}

void checkAdmissionRecoveryAndTransactionalRefresh() {
    Heptagon919 object;
    bool deny = true;
    object.runtimeContext().memoryAdmission = {denyGeometry, &deny};
    CHECK(object.createGeometry() == nullptr);
    CHECK(object.geometryCreationResult() == GeometryResult::AdmissionDenied);
    deny = false;
    auto created = object.createGeometry();
    CHECK(created != nullptr);
    CHECK(object.geometryCreationResult() == GeometryResult::Ready);
    CHECK(created->refresh() == GeometryResult::Ready);

    HeptagonGeometry geometry(object, HEPTAGON919_PIXEL_COUNT1);
    deny = true;
    CHECK(geometry.refresh() == GeometryResult::AdmissionDenied);
    CHECK(geometry.points().empty());
    deny = false;
    CHECK(geometry.refresh() == GeometryResult::Ready);
    object.runtimeContext().memoryAdmission = {};
    const auto before = geometry.points();
    CHECK_EQ(before.size(), object.pixelCount);

    TopologySnapshot incompatible;
    Line line(300);
    CHECK(line.exportSnapshot(incompatible));
    CHECK(object.importSnapshot(incompatible));
    CHECK(geometry.refresh() == GeometryResult::Unsupported);
    CHECK_EQ(geometry.points().size(), before.size());
}

template <typename Heptagon>
void checkWarmRefreshReusesCoordinates(Heptagon& object, std::uint16_t firstStripPixelCount) {
    HeptagonGeometry geometry(object, firstStripPixelCount);
    CHECK(geometry.refresh() == GeometryResult::Ready);
    const auto initial = geometry.points();
    const auto* const initialData = initial.data();
    std::vector<HeptagonGeometry::Point> initialCopy(initial.begin(), initial.end());
    const auto initialOrphans = geometry.orphanAssignments();
    std::vector<HeptagonGeometry::OrphanAssignment> orphanCopy(initialOrphans.begin(),
                                                               initialOrphans.end());
    const std::uint32_t initialRevision = geometry.topologyRevision();

    Intersection* changed = object.inter[1][0];
    TopologyIntersectionUpdate update;
    update.numPorts = changed->numPorts;
    update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
    update.bottomPixel = changed->bottomPixel;
    update.group = changed->group;
    update.allowEndOfLife = changed->allowEndOfLife;
    update.allowEmit = changed->allowEmit;
    CHECK(object.updateIntersection(changed, update));

    bool deny = true;
    object.runtimeContext().memoryAdmission = {denyGeometry, &deny};
    CHECK(geometry.refresh() == GeometryResult::AdmissionDenied);
    object.runtimeContext().memoryAdmission = {};
    CHECK_EQ(geometry.topologyRevision(), initialRevision);
    CHECK_EQ(geometry.points().data(), initialData);
    CHECK_EQ(geometry.points().size(), initialCopy.size());
    for (std::size_t index = 0; index < initialCopy.size(); ++index) {
        checkPoint(geometry.points()[index], initialCopy[index]);
    }
    CHECK_EQ(geometry.orphanAssignments().size(), orphanCopy.size());
    for (std::size_t index = 0; index < orphanCopy.size(); ++index) {
        CHECK_EQ(geometry.orphanAssignments()[index].pixel, orphanCopy[index].pixel);
        CHECK_EQ(geometry.orphanAssignments()[index].neighbor, orphanCopy[index].neighbor);
    }

    deny = false;
    CHECK(geometry.refresh() == GeometryResult::Ready);
    CHECK_EQ(geometry.points().data(), initialData);
    CHECK_EQ(geometry.topologyRevision(), object.topologyRevision());
    if (!orphanCopy.empty()) {
        const auto refreshedOrphans = geometry.orphanAssignments();
        bool changedOrphans = refreshedOrphans.size() != orphanCopy.size();
        for (std::size_t index = 0; !changedOrphans && index < orphanCopy.size(); ++index) {
            changedOrphans = refreshedOrphans[index].pixel != orphanCopy[index].pixel ||
                             refreshedOrphans[index].neighbor != orphanCopy[index].neighbor;
        }
        CHECK(changedOrphans);
    }
}

void checkLateAdmissionDenialIsTransactional() {
    Heptagon919 object;
    HeptagonGeometry geometry(object, HEPTAGON919_PIXEL_COUNT1);
    CHECK(geometry.refresh() == GeometryResult::Ready);

    Intersection* changed = object.inter[1][0];
    TopologyIntersectionUpdate update;
    update.numPorts = changed->numPorts;
    update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
    update.bottomPixel = changed->bottomPixel;
    update.group = changed->group;
    update.allowEndOfLife = changed->allowEndOfLife;
    update.allowEmit = changed->allowEmit;
    CHECK(object.updateIntersection(changed, update));

    const auto beforePoints = geometry.points();
    const auto* const beforeData = beforePoints.data();
    std::vector<HeptagonGeometry::Point> pointCopy(beforePoints.begin(), beforePoints.end());
    const auto beforeOrphans = geometry.orphanAssignments();
    std::vector<HeptagonGeometry::OrphanAssignment> orphanCopy(beforeOrphans.begin(),
                                                               beforeOrphans.end());
    const std::uint32_t beforeRevision = geometry.topologyRevision();

    StagedAdmission admission{2, 0};
    object.runtimeContext().memoryAdmission = {denyStagedGeometry, &admission};
    CHECK(geometry.refresh() == GeometryResult::AdmissionDenied);
    CHECK_EQ(admission.checks, 3u);
    CHECK_EQ(geometry.topologyRevision(), beforeRevision);
    CHECK_EQ(geometry.points().data(), beforeData);
    CHECK_EQ(geometry.points().size(), pointCopy.size());
    for (std::size_t index = 0; index < pointCopy.size(); ++index) {
        checkPoint(geometry.points()[index], pointCopy[index]);
    }
    CHECK_EQ(geometry.orphanAssignments().size(), orphanCopy.size());
    for (std::size_t index = 0; index < orphanCopy.size(); ++index) {
        CHECK_EQ(geometry.orphanAssignments()[index].pixel, orphanCopy[index].pixel);
        CHECK_EQ(geometry.orphanAssignments()[index].neighbor, orphanCopy[index].neighbor);
    }
}

void checkCountChangeAllocatesCoordinates() {
    Heptagon919 object;
    HeptagonGeometry geometry(object, HEPTAGON919_PIXEL_COUNT1);
    CHECK(geometry.refresh() == GeometryResult::Ready);
    const auto* const initialData = geometry.points().data();
    const auto before = geometry.points();
    const std::vector<HeptagonGeometry::Point> pointCopy(before.begin(), before.end());
    const auto orphansBefore = geometry.orphanAssignments();
    const std::vector<HeptagonGeometry::OrphanAssignment> orphanCopy(orphansBefore.begin(),
                                                                     orphansBefore.end());
    const auto revision = geometry.topologyRevision();

    object.pixelCount = static_cast<std::uint16_t>(object.pixelCount + 1);
    Intersection* changed = object.inter[1][0];
    TopologyIntersectionUpdate update;
    update.numPorts = changed->numPorts;
    update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
    update.bottomPixel = changed->bottomPixel;
    update.group = changed->group;
    update.allowEndOfLife = changed->allowEndOfLife;
    update.allowEmit = changed->allowEmit;
    CHECK(object.updateIntersection(changed, update));
    StagedAdmission admission{2, 0};
    object.runtimeContext().memoryAdmission = {denyStagedGeometry, &admission};
    CHECK(geometry.refresh() == GeometryResult::AdmissionDenied);
    object.runtimeContext().memoryAdmission = {};
    CHECK_EQ(admission.checks, 3u);
    CHECK_EQ(geometry.points().data(), initialData);
    CHECK_EQ(geometry.topologyRevision(), revision);
    CHECK_EQ(geometry.points().size(), pointCopy.size());
    for (std::size_t i = 0; i < pointCopy.size(); ++i)
        checkPoint(geometry.points()[i], pointCopy[i]);
    CHECK_EQ(geometry.orphanAssignments().size(), orphanCopy.size());
    for (std::size_t i = 0; i < orphanCopy.size(); ++i) {
        CHECK_EQ(geometry.orphanAssignments()[i].pixel, orphanCopy[i].pixel);
        CHECK_EQ(geometry.orphanAssignments()[i].neighbor, orphanCopy[i].neighbor);
    }
    CHECK(geometry.refresh() == GeometryResult::Ready);
    CHECK_EQ(geometry.points().size(), object.pixelCount);
    CHECK(geometry.points().data() != initialData);
}

class CustomGeometry final : public GeometryProvider {
public:
    explicit CustomGeometry(TopologyObject& object) : GeometryProvider(object) {}
    GeometryResult refresh() noexcept override {
        geometryRevision_ = sourceTopologyRevision();
        return GeometryResult::Ready;
    }
    bool compatible() const noexcept override { return true; }
    ArrayView<Point> points() const noexcept override { return {points_, 2}; }
    ArrayView<Point> outline() const noexcept override { return {points_, 2}; }
private:
    Point points_[2] = {{-0.25f, 0.5f}, {0.25f, -0.5f}};
};

class CustomObject final : public TopologyObject {
public:
    CustomObject() : TopologyObject(2) {}
    bool supportsGeometry() const override { return true; }
    std::unique_ptr<GeometryProvider> createGeometry() override {
        setGeometryCreationResult(GeometryResult::Ready);
        return std::unique_ptr<GeometryProvider>(new CustomGeometry(*this));
    }
    uint16_t* getMirroredPixels(uint16_t, Owner*, bool) override { return mirror_; }
    EmitParams getModelParams(int) const override { return {}; }
private:
    uint16_t mirror_[1] = {0};
};

void checkCustomProvider() {
    CustomObject object;
    auto geometry = object.createGeometry();
    CHECK(geometry != nullptr);
    CHECK(geometry->refresh() == GeometryResult::Ready);
    CHECK_EQ(geometry->topologyRevision(), object.topologyRevision());
    CHECK_EQ(geometry->sourceTopologyRevision(), object.topologyRevision());
    checkPoint(geometry->points()[0], {-0.25f, 0.5f});
    checkPoint(geometry->outline()[1], {0.25f, -0.5f});
}

void checkKnown3024OutputMismatch() {
    Heptagon3024 object;
    constexpr std::uint16_t configuredPhysicalCount = 1728;
    CHECK_EQ(object.realPixelCount, 1726);
    RuntimeState state(object);
    for (std::uint16_t physical = object.realPixelCount;
         physical < configuredPhysicalCount; ++physical) {
        const std::uint16_t logical = object.translateToLogicalPixel(physical);
        CHECK(logical >= object.pixelCount);
        CHECK_EQ(state.getPixel(logical).get(), 0u);
    }
}

} // namespace

int main() {
    {
        Heptagon919 object;
        checkCoverage(object, 6, HEPTAGON919_PIXEL_COUNT1);
    }
    {
        Heptagon3024 object;
        checkCoverage(object, 0, HEPTAGON3024_PHYSICAL_PIXEL_COUNT1);
    }
    checkGeometryMath();
    checkTopologyReplacementInvalidatesGeometry();
    checkUnsupportedObject();
    checkBuiltinGeometry();
    checkBuiltinRoleValidation();
    checkAdmissionRecoveryAndTransactionalRefresh();
    {
        Heptagon919 object;
        checkWarmRefreshReusesCoordinates(object, HEPTAGON919_PIXEL_COUNT1);
    }
    {
        Heptagon3024 object;
        checkWarmRefreshReusesCoordinates(object, HEPTAGON3024_PHYSICAL_PIXEL_COUNT1);
    }
    checkCountChangeAllocatesCoordinates();
    checkLateAdmissionDenialIsTransactional();
    checkCustomProvider();
    checkKnown3024OutputMismatch();

    if (failures != 0) {
        std::cerr << failures << " heptagon geometry test(s) failed\n";
        return 1;
    }
    std::cout << "All heptagon geometry tests passed\n";
    return 0;
}
