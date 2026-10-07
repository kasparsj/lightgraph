#include <cstdlib>
#include <cmath>
#include <iostream>
#include <new>
#include <cstring>
#include <vector>

#include "lightgraph/integration.hpp"
#include "lightgraph/integration/observability.hpp"

using namespace lightgraph::integration;

namespace {
struct AdmissionProbe {
    lightgraph::memory::Operation denied = lightgraph::memory::Operation::DrawingCache;
    bool enabled = true;
    std::size_t checks = 0;
};
bool admit(lightgraph::memory::Operation operation, const lightgraph::memory::Estimate&,
           void* user) noexcept {
    auto& probe = *static_cast<AdmissionProbe*>(user);
    if (operation == probe.denied)
        ++probe.checks;
    return !probe.enabled || operation != probe.denied;
}
bool tracking = false;
std::size_t failAt = static_cast<std::size_t>(-1);
std::size_t attempts = 0;
std::size_t requestedBytes = 0;
std::size_t liveBytes = 0;
std::size_t peakBytes = 0;
void* tracked[64]{};
std::size_t trackedBytes[64]{};
std::size_t outstanding = 0;
bool scalarTracking = false;
std::size_t scalarFailAt = static_cast<std::size_t>(-1);
std::size_t scalarAttempts = 0;
std::size_t scalarFailurePeriod = 0;
void* scalarTracked[16]{};
std::size_t scalarOutstanding = 0;
LightgraphAllocationFailureSite observedSite = LightgraphAllocationFailureSite::Unknown;
std::uint16_t observedStage = 0;
std::uint16_t observedDetail1 = 0;
std::size_t observerCalls = 0;

void observeAllocationFailure(LightgraphAllocationFailureSite site, std::uint16_t detail0,
                              std::uint16_t detail1) {
    observedSite = site;
    observedStage = detail0;
    observedDetail1 = detail1;
    ++observerCalls;
}

void resetObservedFailure() {
    observedSite = LightgraphAllocationFailureSite::Unknown;
    observedStage = 0;
    observedDetail1 = 0;
    observerCalls = 0;
}

void* allocateArray(std::size_t size) noexcept {
    if (tracking && attempts++ == failAt)
        return nullptr;
    void* result = std::malloc(size == 0 ? 1 : size);
    if (tracking && result != nullptr) {
        requestedBytes += size;
        for (std::size_t i = 0; i < 64; ++i) {
            if (tracked[i] == nullptr) {
                tracked[i] = result;
                trackedBytes[i] = size;
                ++outstanding;
                liveBytes += size;
                if (liveBytes > peakBytes)
                    peakBytes = liveBytes;
                break;
            }
        }
    }
    return result;
}

void releaseArray(void* pointer) noexcept {
    if (!pointer)
        return;
    for (std::size_t i = 0; i < 64; ++i) {
        if (tracked[i] == pointer) {
            tracked[i] = nullptr;
            liveBytes -= trackedBytes[i];
            trackedBytes[i] = 0;
            --outstanding;
            break;
        }
    }
    std::free(pointer);
}

void begin(std::size_t failure = static_cast<std::size_t>(-1)) {
    attempts = 0;
    requestedBytes = 0;
    peakBytes = liveBytes;
    failAt = failure;
    tracking = true;
}

void* allocateScalar(std::size_t size) noexcept {
    if (scalarTracking) {
        const std::size_t attempt = scalarAttempts++;
        if (attempt == scalarFailAt ||
            (scalarFailurePeriod != 0 && attempt > 0 &&
             (attempt - 1) % scalarFailurePeriod == scalarFailurePeriod - 1))
            return nullptr;
    }
    void* result = std::malloc(size == 0 ? 1 : size);
    if (scalarTracking && result != nullptr) {
        for (void*& entry : scalarTracked) {
            if (entry == nullptr) {
                entry = result;
                ++scalarOutstanding;
                break;
            }
        }
    }
    return result;
}

void releaseScalar(void* pointer) noexcept {
    if (!pointer)
        return;
    for (void*& entry : scalarTracked) {
        if (entry == pointer) {
            entry = nullptr;
            --scalarOutstanding;
            break;
        }
    }
    std::free(pointer);
}
} // namespace

void* operator new[](std::size_t size) {
    if (void* result = allocateArray(size))
        return result;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return allocateArray(size);
}
void operator delete[](void* pointer) noexcept { releaseArray(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { releaseArray(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { releaseArray(pointer); }
void* operator new(std::size_t size) {
    if (void* result = std::malloc(size == 0 ? 1 : size))
        return result;
    throw std::bad_alloc();
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return allocateScalar(size);
}
void operator delete(void* pointer) noexcept { releaseScalar(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { releaseScalar(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { releaseScalar(pointer); }

namespace {
bool checkPersistentDrawingAllocationFailure(std::size_t failurePeriod) {
    Line object(16);
    scalarAttempts = 0;
    scalarFailAt = static_cast<std::size_t>(-1);
    scalarFailurePeriod = failurePeriod;
    scalarTracking = true;
    bool passed = true;
    {
        RuntimeState state(object);
        passed = state.ready();
        if (passed) {
            auto* background = state.lightLists[0]->asBgLight();
            background->setPalette(Palette({0xFF0000}, {0.0f}));
            background->maxBri = 255;
            background->visible = true;
            for (unsigned update = 0; update < 3; ++update) {
                const std::size_t before = scalarAttempts;
                state.update();
                passed = passed && state.ready() &&
                    scalarAttempts - before == failurePeriod && scalarOutstanding == 1;
                for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel)
                    passed = passed && state.getPixel(pixel).get() == 0xFF0000u;
            }
            scalarFailurePeriod = 0;
            const std::size_t before = scalarAttempts;
            state.update();
            passed = passed && scalarAttempts - before == 2 && state.drawing().status().supported;
            passed = passed && state.drawing().setFill(0x0000FF) == DrawingResult::Applied;
            passed = passed && state.drawing().setEnabled(true) == DrawingResult::Applied;
            state.update();
            for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel)
                passed = passed && state.getPixel(pixel).get() == 0x0000FFu;
        }
    }
    scalarFailurePeriod = 0;
    scalarTracking = false;
    if (!passed || scalarOutstanding != 0) {
        std::cerr << "Persistent drawing allocation failure regression: period "
                  << failurePeriod << '\n';
        return false;
    }
    return true;
}
} // namespace

int main() {
    // Count changes retain the old coordinate and orphan storage on every allocation failure.
    for (std::size_t failure = 0; failure < 3; ++failure) {
        {
            Heptagon919 object;
            HeptagonGeometry geometry(object, HEPTAGON919_PIXEL_COUNT1);
            if (geometry.refresh() != lightgraph::geometry::GeometryResult::Ready)
                return 80;
            const auto beforePoints = geometry.points();
            const auto* const beforeData = beforePoints.data();
            const std::vector<HeptagonGeometry::Point> points(beforePoints.begin(),
                                                              beforePoints.end());
            const auto beforeOrphans = geometry.orphanAssignments();
            const std::vector<HeptagonGeometry::OrphanAssignment> orphans(beforeOrphans.begin(),
                                                                          beforeOrphans.end());
            const auto revision = geometry.topologyRevision();
            object.pixelCount = static_cast<std::uint16_t>(object.pixelCount + 1);
            auto* changed = object.inter[1][0];
            TopologyIntersectionUpdate update;
            update.numPorts = changed->numPorts;
            update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
            update.bottomPixel = changed->bottomPixel;
            update.group = changed->group;
            update.allowEndOfLife = changed->allowEndOfLife;
            update.allowEmit = changed->allowEmit;
            if (!object.updateIntersection(changed, update))
                return 81;
            begin(failure);
            const auto result = geometry.refresh();
            tracking = false;
            if (result != lightgraph::geometry::GeometryResult::AllocationFailed ||
                attempts != failure + 1 || geometry.points().data() != beforeData ||
                geometry.points().size() != points.size() ||
                geometry.topologyRevision() != revision ||
                std::memcmp(geometry.points().data(), points.data(),
                            points.size() * sizeof(points[0])) != 0 ||
                geometry.orphanAssignments().size() != orphans.size())
                return 82;
            for (std::size_t i = 0; i < orphans.size(); ++i) {
                if (geometry.orphanAssignments()[i].pixel != orphans[i].pixel ||
                    geometry.orphanAssignments()[i].neighbor != orphans[i].neighbor)
                    return 83;
            }
            if (geometry.refresh() != lightgraph::geometry::GeometryResult::Ready ||
                geometry.points().data() == beforeData ||
                geometry.points().size() != object.pixelCount)
                return 84;
        }
        if (outstanding != 0 || liveBytes != 0)
            return 85;
    }
    if (!checkPersistentDrawingAllocationFailure(1) ||
        !checkPersistentDrawingAllocationFailure(2))
        return 60;
    for (std::size_t failure = 0; failure < 3; ++failure) {
        Heptagon919 object;
        setAllocationFailureObserver(object, observeAllocationFailure);
        resetObservedFailure();
        scalarAttempts = 0;
        scalarFailAt = failure;
        scalarTracking = true;
        {
            RuntimeState state(object);
            const DrawingStatus status = state.drawing().status();
            if (failure == 0 && state.hasRequiredFrameBuffers())
                return 36;
            if (failure == 0 && status.supported)
                return 37;
            if (failure > 0 &&
                (!state.ready() || !status.supported || observerCalls != 1 ||
                 observedSite != LightgraphAllocationFailureSite::DrawingRuntimeAllocation ||
                 observedStage != failure - 1))
                return 58;
        }
        scalarTracking = false;
        if (scalarOutstanding != 0)
            return 38;
    }

    std::size_t activationAllocations = 0;
    std::size_t activationBytes = 0;
    std::size_t activationPeak = 0;
    std::size_t retained919 = 0;
    std::size_t rebuild919Peak = 0;
    std::size_t rebuild919Allocations = 0;
    std::size_t rebuild919Bytes = 0;
    std::size_t rebuild919Orphans = 0;
    {
        Heptagon919 object;
        RuntimeState state(object);
        begin();
        if (state.drawing().setFill(0x102030) != DrawingResult::Applied)
            return 1;
        activationAllocations = attempts;
        activationBytes = requestedBytes;
        activationPeak = peakBytes;
        retained919 = liveBytes;
        TopologyIntersectionUpdate update;
        Intersection* changed = object.inter[1][0];
        update.numPorts = changed->numPorts;
        update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
        update.bottomPixel = changed->bottomPixel;
        update.group = changed->group;
        update.allowEndOfLife = changed->allowEndOfLife;
        update.allowEmit = changed->allowEmit;
        if (!object.updateIntersection(changed, update))
            return 11;
        begin();
        if (state.drawing().setFill(0x203040) != DrawingResult::Applied)
            return 12;
        rebuild919Allocations = attempts;
        rebuild919Bytes = requestedBytes;
        rebuild919Peak = peakBytes;
        const auto* geometry = dynamic_cast<const HeptagonGeometry*>(state.drawing().geometry());
        if (geometry == nullptr)
            return 70;
        rebuild919Orphans = geometry->orphanAssignments().size();
        tracking = false;
    }
    if (outstanding != 0 || activationAllocations != 4)
        return 2;

    {
        Heptagon919 object;
        setAllocationFailureObserver(object, observeAllocationFailure);
        RuntimeState state(object);
        resetObservedFailure();
        begin(0);
        if (state.drawing().setEnabled(true) != DrawingResult::Busy)
            return 13;
        if (state.drawing().status().mode != DrawingMode::Palette)
            return 14;
        if (observerCalls != 1 ||
            observedSite != LightgraphAllocationFailureSite::DrawingRuntimeAllocation ||
            observedStage != 2)
            return 67;
        tracking = false;
        if (state.drawing().setEnabled(true) != DrawingResult::Applied)
            return 15;
        if (state.drawing().layerColor(0).get() != 0)
            return 16;
    }
    if (outstanding != 0)
        return 17;

    for (std::size_t failure = 0; failure < 2; ++failure) {
        {
            Heptagon919 object;
            setAllocationFailureObserver(object, observeAllocationFailure);
            RuntimeState state(object);
            if (state.drawing().setFill(0x102030) != DrawingResult::Applied)
                return 29;
            if (state.drawing().setEnabled(true) != DrawingResult::Applied)
                return 30;
            DrawingRuntime::ImageLease lease;
            if (state.drawing().leaseImage(91, 1, 1, lease) != DrawingResult::Applied)
                return 72;
            const auto* geometry =
                dynamic_cast<const HeptagonGeometry*>(state.drawing().geometry());
            if (geometry == nullptr)
                return 73;
            const auto beforePoints = geometry->points();
            const auto* const beforeData = beforePoints.data();
            std::vector<HeptagonGeometry::Point> pointCopy(beforePoints.begin(),
                                                           beforePoints.end());
            const auto beforeOrphans = geometry->orphanAssignments();
            std::vector<HeptagonGeometry::OrphanAssignment> orphanCopy(beforeOrphans.begin(),
                                                                       beforeOrphans.end());
            const std::uint32_t geometryRevision = geometry->topologyRevision();

            Intersection* changed = object.inter[1][0];
            TopologyIntersectionUpdate update;
            update.numPorts = changed->numPorts;
            update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
            update.bottomPixel = changed->bottomPixel;
            update.group = changed->group;
            update.allowEndOfLife = changed->allowEndOfLife;
            update.allowEmit = changed->allowEmit;
            if (!object.updateIntersection(changed, update))
                return 31;
            const std::uint64_t before = state.drawing().status().revision;
            resetObservedFailure();
            begin(failure);
            state.update();
            if (attempts != failure + 1)
                return 32;
            tracking = false;
            const DrawingStatus afterFailure = state.drawing().status();
            if (afterFailure.mode != DrawingMode::Drawing || afterFailure.revision != before ||
                observerCalls != 1 ||
                observedSite != LightgraphAllocationFailureSite::HeptagonGeometryAllocation ||
                observedStage != object.pixelCount || observedDetail1 != failure + 3 ||
                !state.drawing().validImageLease(lease) ||
                state.drawing().layerColor(0).get() != 0x102030) {
                std::cerr << "Geometry failure=" << failure << ", attempts=" << attempts
                          << ", observerCalls=" << observerCalls << ", detail0=" << observedStage
                          << ", detail1=" << observedDetail1
                          << ", lease=" << state.drawing().validImageLease(lease)
                          << ", color=" << state.drawing().layerColor(0).get() << '\n';
                return 33;
            }
            if (geometry->topologyRevision() != geometryRevision ||
                geometry->points().data() != beforeData ||
                geometry->points().size() != pointCopy.size() ||
                geometry->orphanAssignments().size() != orphanCopy.size())
                return 74;
            for (std::size_t index = 0; index < pointCopy.size(); ++index) {
                const auto same = [](float left, float right) {
                    return left == right || (std::isnan(left) && std::isnan(right));
                };
                if (!same(geometry->points()[index].x, pointCopy[index].x) ||
                    !same(geometry->points()[index].y, pointCopy[index].y))
                    return 75;
            }
            for (std::size_t index = 0; index < orphanCopy.size(); ++index) {
                if (geometry->orphanAssignments()[index].pixel != orphanCopy[index].pixel ||
                    geometry->orphanAssignments()[index].neighbor != orphanCopy[index].neighbor)
                    return 76;
            }
            state.update();
            if (state.drawing().status().revision != before + 1 ||
                geometry->topologyRevision() != object.topologyRevision() ||
                !state.drawing().validImageLease(lease))
                return 34;
        }
        if (outstanding != 0)
            return 35;
    }

    for (std::size_t failure = 0; failure < activationAllocations; ++failure) {
        {
            Heptagon919 object;
            RuntimeState state(object);
            begin(failure);
            const DrawingResult failed = state.drawing().setFill(0x102030);
            tracking = false;
            if (failed == DrawingResult::Applied)
                return 3;
            if (state.drawing().scene().rgb1 != 0)
                return 4;
            if (state.drawing().setFill(0x102030) != DrawingResult::Applied)
                return 5;
        }
        if (outstanding != 0)
            return 6;
    }

    {
        Heptagon919 object;
        RuntimeState state(object);
        if (state.drawing().setFill(0x102030) != DrawingResult::Applied)
            return 7;
        DrawingShape circle;
        circle.kind = DrawingShapeKind::Circle;
        circle.id = 0;
        circle.rgb = 0xFFFFFF;
        circle.params[0] = 0.5f;
        circle.params[1] = 0.5f;
        circle.params[2] = 0.2f;
        begin();
        if (state.drawing().setShape(circle) != DrawingResult::Applied)
            return 8;
        state.update();
        tracking = false;
        if (attempts != 0)
            return 9;
    }
    if (outstanding != 0)
        return 10;

    std::size_t activation3024Peak = 0;
    std::size_t retained3024 = 0;
    std::size_t rebuild3024Peak = 0;
    std::size_t rebuild3024Allocations = 0;
    std::size_t rebuild3024Bytes = 0;
    std::size_t rebuild3024Orphans = 0;
    {
        Heptagon3024 object;
        RuntimeState state(object);
        begin();
        if (state.drawing().setEnabled(true) != DrawingResult::Applied)
            return 18;
        activation3024Peak = peakBytes;
        retained3024 = liveBytes;
        Intersection* changed = object.inter[1][0];
        TopologyIntersectionUpdate update;
        update.numPorts = changed->numPorts;
        update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
        update.bottomPixel = changed->bottomPixel;
        update.group = changed->group;
        update.allowEndOfLife = changed->allowEndOfLife;
        update.allowEmit = changed->allowEmit;
        if (!object.updateIntersection(changed, update))
            return 19;
        begin();
        if (state.drawing().setFill(0x102030) != DrawingResult::Applied)
            return 20;
        rebuild3024Allocations = attempts;
        rebuild3024Bytes = requestedBytes;
        rebuild3024Peak = peakBytes;
        const auto* geometry = dynamic_cast<const HeptagonGeometry*>(state.drawing().geometry());
        if (geometry == nullptr)
            return 71;
        rebuild3024Orphans = geometry->orphanAssignments().size();
        tracking = false;
    }
    if (outstanding != 0 || liveBytes != 0)
        return 21;
    if (rebuild919Allocations != 2 ||
        rebuild919Bytes != HEPTAGON919_PIXEL_COUNT +
                               rebuild919Orphans * sizeof(HeptagonGeometry::OrphanAssignment) ||
        rebuild919Peak != retained919 + rebuild919Bytes) {
        std::cerr << "919 warm geometry allocations=" << rebuild919Allocations
                  << ", bytes=" << rebuild919Bytes << ", retained=" << retained919
                  << ", peak=" << rebuild919Peak << '\n';
        return 68;
    }
    if (rebuild3024Allocations != 1 + (rebuild3024Orphans == 0 ? 0 : 1) ||
        rebuild3024Bytes != HEPTAGON3024_PIXEL_COUNT +
                                rebuild3024Orphans * sizeof(HeptagonGeometry::OrphanAssignment) ||
        rebuild3024Peak != retained3024 + rebuild3024Bytes) {
        std::cerr << "3024 warm geometry allocations=" << rebuild3024Allocations
                  << ", bytes=" << rebuild3024Bytes << ", retained=" << retained3024
                  << ", peak=" << rebuild3024Peak << '\n';
        return 69;
    }

    for (auto operation :
         {lightgraph::memory::Operation::DrawingCache, lightgraph::memory::Operation::DrawingImages,
          lightgraph::memory::Operation::Geometry}) {
        Heptagon919 object;
        RuntimeState state(object);
        AdmissionProbe probe;
        probe.denied = operation;
        if (operation != lightgraph::memory::Operation::DrawingCache) {
            if (state.drawing().setFill(0x102030) != DrawingResult::Applied ||
                state.drawing().setEnabled(true) != DrawingResult::Applied)
                return 53;
        }
        setMemoryAdmissionPolicy(object, {admit, &probe});
        const auto before = state.drawing().status();
        if (operation == lightgraph::memory::Operation::Geometry) {
            Intersection* changed = object.inter[1][0];
            TopologyIntersectionUpdate update;
            update.numPorts = changed->numPorts;
            update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
            update.bottomPixel = changed->bottomPixel;
            update.group = changed->group;
            update.allowEndOfLife = changed->allowEndOfLife;
            update.allowEmit = changed->allowEmit;
            if (!object.updateIntersection(changed, update))
                return 54;
        }
        const std::uint8_t rgb[] = {0x44, 0x55, 0x66};
        const auto result =
            operation == lightgraph::memory::Operation::DrawingImages
                ? DrawingSession(state.drawing()).imageChunk(1, 10, 7, 1, 1, 0, rgb, sizeof(rgb))
                : state.drawing().setFill(0x445566);
        const DrawingStatus failed = state.drawing().status();
        if (result != DrawingResult::Busy || probe.checks == 0 ||
            failed.revision != before.revision || failed.frameHighWater != before.frameHighWater ||
            failed.transferChunksExpected != 0 ||
            std::strcmp(failed.lastRejection.data(), "insufficient_heap") != 0)
            return 55;
        if (operation != lightgraph::memory::Operation::DrawingCache &&
            state.drawing().layerColor(0).get() != 0x102030)
            return 56;
        probe.enabled = false;
        const auto retry = operation == lightgraph::memory::Operation::DrawingImages
                               ? state.drawing().applyImage(7, 1, 1, rgb, sizeof(rgb))
                               : state.drawing().setFill(0x445566);
        if (retry != DrawingResult::Applied)
            return 57;
        setMemoryAdmissionPolicy(object, {});
    }

    // Composition-only control has no pixel/image resources. The first image must
    // allocate both original-image slots atomically without consuming a frame ID on failure.
    for (std::size_t failure = 0; failure < 2; ++failure) {
        {
            Heptagon919 object;
            setAllocationFailureObserver(object, observeAllocationFailure);
            RuntimeState state(object);
            begin();
            DrawingControl composition;
            composition.hasPlacement = true;
            composition.placement = DrawingPlacement::Overlay;
            composition.hasClipToContent = true;
            composition.clipToContent = true;
            if (state.drawing().control(composition) != DrawingResult::Applied || attempts != 0)
                return 45;
            tracking = false;
            if (state.drawing().setFill(0x102030) != DrawingResult::Applied ||
                state.drawing().setEnabled(true) != DrawingResult::Applied)
                return 46;
            const auto before = state.drawing().status();
            const std::uint8_t rgb[] = {0x44, 0x55, 0x66};
            resetObservedFailure();
            begin(failure);
            if (state.drawing().applyImage(7, 1, 1, rgb, sizeof(rgb)) != DrawingResult::Busy)
                return 47;
            tracking = false;
            if (observerCalls != 1 ||
                observedSite != LightgraphAllocationFailureSite::DrawingRuntimeAllocation ||
                observedStage != failure + 3)
                return 59;
            const DrawingStatus failed = state.drawing().status();
            if (failed.revision != before.revision || failed.displayedFrameId != -1 ||
                failed.frameHighWater != -1 || failed.transferChunksExpected != 0 ||
                state.drawing().layerColor(0).get() != 0x102030 || liveBytes != 0)
                return 48;
            begin();
            if (state.drawing().applyImage(7, 1, 1, rgb, sizeof(rgb)) != DrawingResult::Applied ||
                attempts != 2 || liveBytes != 6144)
                return 49;
            if (state.drawing().setEnabled(false) != DrawingResult::Applied || liveBytes != 6144)
                return 50;
            DrawingControl clear;
            clear.clear = true;
            if (state.drawing().control(clear) != DrawingResult::Applied || liveBytes != 0)
                return 51;
            tracking = false;
        }
        if (outstanding != 0)
            return 52;
    }

    // Once drawing and the two image slots are warm, transfer, presentation,
    // and frame composition retain the same bounded image storage.
    {
        Heptagon919 object;
        RuntimeState state(object);
        if (state.drawing().setFill(0x102030) != DrawingResult::Applied ||
            state.drawing().setEnabled(true) != DrawingResult::Applied)
            return 60;
        DrawingSession session(state.drawing());
        std::uint8_t image[DRAWING_MAX_IMAGE_BYTES]{};
        begin();
        for (std::uint8_t chunk = 0; chunk < 4; ++chunk) {
            if (session.imageChunk(1, 10, 1, 32, 32, chunk,
                                   image + chunk * DRAWING_IMAGE_CHUNK_BYTES,
                                   DRAWING_IMAGE_CHUNK_BYTES) != DrawingResult::Applied)
                return 61;
        }
        if (session.imageCommit(1, 11, 1) != DrawingResult::Applied || attempts != 2 ||
            liveBytes != 6144)
            return 62;

        attempts = 0;
        scalarAttempts = 0;
        scalarFailAt = static_cast<std::size_t>(-1);
        scalarTracking = true;
        for (std::uint8_t chunk = 0; chunk < 4; ++chunk) {
            if (session.imageChunk(1, 20, 2, 32, 32, chunk,
                                   image + chunk * DRAWING_IMAGE_CHUNK_BYTES,
                                   DRAWING_IMAGE_CHUNK_BYTES) != DrawingResult::Applied)
                return 63;
        }
        if (session.imageCommit(1, 21, 2) != DrawingResult::Applied)
            return 64;
        for (int frame = 0; frame < 8; ++frame)
            state.update();
        scalarTracking = false;
        tracking = false;
        if (attempts != 0 || scalarAttempts != 0 || liveBytes != 6144)
            return 65;
    }
    if (outstanding != 0 || scalarOutstanding != 0 || liveBytes != 0)
        return 66;

    // Centered visible-length updates mutate only the reserved window and allocate nothing.
    {
        Line object(64);
        RuntimeState state(object);
        state.lightLists[0]->visible = false;
        EmitParams params(0, 0.0f, 0xFFFFFF);
        params.setLength(8);
        params.noteId = 123;
        params.lengthMode = lightgraph::LengthMode::Centered;
        params.visibleLength = 0.0f;
        if (state.emit(params) < 1)
            return 39;

        attempts = 0;
        scalarAttempts = 0;
        failAt = 0;
        scalarFailAt = 0;
        tracking = true;
        scalarTracking = true;
        const lightgraph::ListLengthUpdate update = {123, 6.5f};
        const bool resized = state.setListLengths(&update, 1);
        tracking = false;
        scalarTracking = false;
        if (!resized || attempts != 0 || scalarAttempts != 0)
            return 40;
    }
    if (outstanding != 0 || scalarOutstanding != 0 || liveBytes != 0)
        return 41;

    // Centered capacity is all-or-nothing when its reserved block cannot allocate.
    {
        Line object(64);
        RuntimeState state(object);
        state.lightLists[0]->visible = false;
        EmitParams centered(0, 0.0f, 0xFFFFFF);
        centered.setLength(4);
        centered.noteId = 124;
        centered.lengthMode = lightgraph::LengthMode::Centered;
        centered.visibleLength = 4.0f;

        scalarAttempts = 0;
        begin(1); // pointer array, then reserved Light block.
        scalarTracking = true;
        const int8_t centeredResult = state.emit(centered);
        scalarTracking = false;
        tracking = false;
        if (centeredResult >= 0 || state.findList(124) >= 0 || state.totalLights != 0 ||
            state.totalLightLists != 1 || scalarOutstanding != 0) {
            return 42;
        }

        EmitParams legacy(0, 0.0f, 0xFFFFFF);
        legacy.setLength(4);
        legacy.noteId = 125;
        scalarAttempts = 0;
        scalarFailAt = 3;
        scalarTracking = true;
        const int8_t legacyResult = state.emit(legacy);
        scalarTracking = false;
        if (legacyResult >= 0 || state.findList(125) >= 0 || state.totalLights != 0 ||
            state.totalLightLists != 1 || scalarOutstanding != 0) {
            return 43;
        }
    }
    if (outstanding != 0 || scalarOutstanding != 0 || liveBytes != 0)
        return 44;

    std::cout << "Drawing activation arrays: " << activationAllocations
              << ", bytes: " << activationBytes << ", 919 activation peak: " << activationPeak
              << ", retained: " << retained919 << ", rebuild peak: " << rebuild919Peak
              << ", rebuild allocations: " << rebuild919Allocations
              << ", 3024 activation peak: " << activation3024Peak << ", retained: " << retained3024
              << ", rebuild peak: " << rebuild3024Peak
              << ", rebuild allocations: " << rebuild3024Allocations
              << ", sizeof scene: " << sizeof(DrawingScene) << ", shape: " << sizeof(DrawingShape)
              << '\n';
    return 0;
}
