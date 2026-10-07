#include <cstdint>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>

#include "lightgraph/integration.hpp"

using namespace lightgraph::integration;

namespace {

int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << __FILE__ << ':' << __LINE__ << " check failed: " #condition "\n";         \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)
#define CHECK_EQ(actual, expected) CHECK((actual) == (expected))

bool denyGeometry(lightgraph::memory::Operation operation, const lightgraph::memory::Estimate&,
                  void* user) noexcept {
    return operation != lightgraph::memory::Operation::Geometry || !*static_cast<bool*>(user);
}

struct DrawingAdmissionProbe {
    bool denied = true;
    unsigned attempts = 0;
};

bool admitDrawing(lightgraph::memory::Operation operation, const lightgraph::memory::Estimate&,
                  void* user) noexcept {
    auto& probe = *static_cast<DrawingAdmissionProbe*>(user);
    if (operation != lightgraph::memory::Operation::DrawingCache)
        return true;
    ++probe.attempts;
    return !probe.denied;
}

void checkPaletteSurvivesDrawingDenial() {
    Line object(16);
    DrawingAdmissionProbe probe;
    object.runtimeContext().memoryAdmission = {admitDrawing, &probe};
    RuntimeState state(object);
    CHECK(state.ready());
    auto* background = state.lightLists[0]->asBgLight();
    background->setPalette(Palette({0xFF0000}, {0.0f}));
    background->maxBri = 255;
    background->visible = true;

    for (unsigned long now : {0ul, 16ul, 96ul}) {
        object.runtimeContext().nowMillis = now;
        object.runtimeContext().hasExplicitNowMillis = true;
        probe.attempts = 0;
        state.update();
        CHECK(state.ready());
        CHECK_EQ(probe.attempts, 1u);
        for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel)
            CHECK_EQ(state.getPixel(pixel).get(), 0xFF0000u);
    }
    CHECK(object.runtimeContext().currentStepMillis < object.runtimeContext().frameElapsedMillis);

    background->visible = false;
    EmitParams params(0, 1.0f, 0x00FF00);
    params.setLength(3);
    const int8_t listIndex = state.emit(params);
    CHECK(listIndex >= 0);
    probe.attempts = 0;
    state.update();
    CHECK_EQ(probe.attempts, 1u);
    bool lit = false;
    for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel)
        lit = lit || state.getPixel(pixel).get() != 0;
    CHECK(lit);
    if (listIndex >= 0)
        CHECK(state.clearListSlot(static_cast<uint8_t>(listIndex)));
    background->visible = true;

    probe.denied = false;
    probe.attempts = 0;
    state.update();
    CHECK_EQ(probe.attempts, 1u);
    CHECK(state.drawing().status().supported);
    CHECK_EQ(state.drawing().setFill(0x0000FF), DrawingResult::Applied);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    probe.attempts = 0;
    state.update();
    CHECK_EQ(probe.attempts, 0u);
    for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel)
        CHECK_EQ(state.getPixel(pixel).get(), 0x0000FFu);
}

void mutateGeometry(Heptagon919& object) {
    Intersection* changed = object.inter[1][0];
    TopologyIntersectionUpdate update;
    update.numPorts = changed->numPorts;
    update.topPixel = static_cast<std::uint16_t>(changed->topPixel + 1);
    update.bottomPixel = changed->bottomPixel;
    update.group = changed->group;
    update.allowEndOfLife = changed->allowEndOfLife;
    update.allowEmit = changed->allowEmit;
    CHECK(object.updateIntersection(changed, update));
}

void denyRefresh(Heptagon919& object, bool& deny) {
    mutateGeometry(object);
    deny = true;
    object.runtimeContext().memoryAdmission = {denyGeometry, &deny};
}

DrawingScene scene(std::uint32_t rgb) {
    DrawingScene value;
    value.rgb1 = rgb;
    return value;
}

class ThrowingGeometryLine final : public Line {
public:
    ThrowingGeometryLine() : Line(32) {}

    std::unique_ptr<lightgraph::geometry::GeometryProvider> createGeometry() override {
        if (throwBadAlloc_) {
            throwBadAlloc_ = false;
            throw std::bad_alloc();
        }
        return Line::createGeometry();
    }

private:
    bool throwBadAlloc_ = true;
};

class UnexpectedExceptionLine final : public Line {
public:
    UnexpectedExceptionLine() : Line(32) {}

    std::unique_ptr<lightgraph::geometry::GeometryProvider> createGeometry() override {
        throw std::runtime_error("factory failure");
    }
};

void checkGeometryFactoryBadAllocRecovery() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    ThrowingGeometryLine object;
    RuntimeState state(object);
    bool escaped = false;
    DrawingResult first = DrawingResult::Invalid;
    try {
        first = state.drawing().setFill(0x123456);
    } catch (...) {
        escaped = true;
    }
    CHECK(!escaped);
    CHECK_EQ(first, DrawingResult::Busy);
    CHECK(std::strcmp(state.drawing().status().lastRejection.data(), "allocation_failed") == 0);
    CHECK_EQ(state.drawing().setFill(0x123456), DrawingResult::Applied);

    UnexpectedExceptionLine unexpected;
    RuntimeState unexpectedState(unexpected);
    bool propagated = false;
    try {
        (void)unexpectedState.drawing().setFill(0x123456);
    } catch (const std::runtime_error&) {
        propagated = true;
    }
    CHECK(propagated);
#endif
}

void checkRejectionStatusOwnsText() {
    DrawingStatus snapshot;
    char reason[] = "custom_reason";
    {
        Heptagon919 object;
        RuntimeState state(object);
        state.drawing().reject(reason);
        snapshot = state.drawing().status();
        reason[0] = 'X';
        CHECK(std::strcmp(snapshot.lastRejection.data(), "custom_reason") == 0);
    }
    CHECK(std::strcmp(snapshot.lastRejection.data(), "custom_reason") == 0);

    Heptagon919 object;
    RuntimeState state(object);
    state.drawing().reject(nullptr);
    CHECK(std::strcmp(state.drawing().status().lastRejection.data(), "none") == 0);

    char longReason[80];
    std::memset(longReason, 'q', sizeof(longReason));
    longReason[sizeof(longReason) - 1] = '\0';
    state.drawing().reject(longReason);
    const DrawingStatus truncated = state.drawing().status();
    CHECK_EQ(std::strlen(truncated.lastRejection.data()), 63u);
    CHECK(truncated.lastRejection[63] == '\0');
}

void checkGenerationChangeInvalidatesPendingStorage() {
    {
        Heptagon919 object;
        RuntimeState state(object);
        DrawingHandle oldScene;
        CHECK_EQ(state.drawing().queueScene(scene(0x123456), oldScene), DrawingResult::Applied);
        CHECK(state.drawing().validQueued(oldScene));
        CHECK(state.drawing().setGeneration(0x101));
        CHECK(!state.drawing().validQueued(oldScene));
        CHECK_EQ(state.drawing().applyQueued(oldScene), DrawingResult::Stale);
        DrawingHandle freshScene;
        CHECK_EQ(state.drawing().queueScene(scene(0x654321), freshScene), DrawingResult::Applied);
        CHECK(state.drawing().validQueued(freshScene));
        state.drawing().releaseQueued(freshScene);
        CHECK(!state.drawing().validQueued(freshScene));
    }

    {
        Heptagon919 object;
        RuntimeState state(object);
        DrawingRuntime::ImageLease oldLease;
        CHECK_EQ(state.drawing().leaseImage(1, 1, 1, oldLease), DrawingResult::Applied);
        CHECK(state.drawing().setGeneration(0x202));
        CHECK(!state.drawing().validImageLease(oldLease));
        CHECK_EQ(state.drawing().submitImage(oldLease), DrawingResult::Stale);
        DrawingRuntime::ImageLease freshLease;
        CHECK_EQ(state.drawing().leaseImage(2, 1, 1, freshLease), DrawingResult::Applied);
        state.drawing().releaseImage(freshLease);
    }

    {
        Heptagon919 object;
        RuntimeState state(object);
        DrawingRuntime::ImageLease lease;
        CHECK_EQ(state.drawing().leaseImage(1, 1, 1, lease), DrawingResult::Applied);
        DrawingHandle oldImage;
        CHECK_EQ(state.drawing().submitImage(lease, &oldImage), DrawingResult::Applied);
        CHECK(state.drawing().queuedImage());
        CHECK(state.drawing().validQueued(oldImage));
        CHECK(state.drawing().setGeneration(0x303));
        CHECK(!state.drawing().queuedImage());
        CHECK(!state.drawing().validQueued(oldImage));
        CHECK_EQ(state.drawing().applyQueued(oldImage), DrawingResult::Stale);
        DrawingRuntime::ImageLease freshLease;
        CHECK_EQ(state.drawing().leaseImage(2, 1, 1, freshLease), DrawingResult::Applied);
        state.drawing().releaseImage(freshLease);
    }
}

void checkQueuedSceneBusyRetry() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingHandle handle;
    CHECK_EQ(state.drawing().queueScene(scene(0x123456), handle), DrawingResult::Applied);
    bool deny = false;
    denyRefresh(object, deny);
    CHECK_EQ(state.drawing().applyQueued(handle), DrawingResult::Busy);
    deny = false;
    CHECK_EQ(state.drawing().applyQueued(handle), DrawingResult::Applied);
    CHECK_EQ(state.drawing().sampleColor(0).get(), 0x123456u);
    CHECK_EQ(state.drawing().applyQueued(handle), DrawingResult::Stale);
}

void checkQueuedSameSceneAdvancesRevisionAfterGeometryChange() {
    Heptagon919 object;
    RuntimeState state(object);
    CHECK_EQ(state.drawing().setFill(0x2468AC), DrawingResult::Applied);
    DrawingHandle handle;
    CHECK_EQ(state.drawing().queueScene(state.drawing().scene(), handle), DrawingResult::Applied);
    const std::uint64_t revision = state.drawing().status().revision;
    mutateGeometry(object);
    CHECK_EQ(state.drawing().applyQueued(handle), DrawingResult::Applied);
    CHECK_EQ(state.drawing().status().revision, revision + 1);
    CHECK_EQ(state.drawing().sampleColor(0).get(), 0x2468ACu);
}

void checkImmediateImageBusyRetry() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime::ImageLease lease;
    CHECK_EQ(state.drawing().leaseImage(1, 1, 1, lease), DrawingResult::Applied);
    lease.bytes[0] = 0x11;
    lease.bytes[1] = 0x22;
    lease.bytes[2] = 0x33;
    bool deny = false;
    denyRefresh(object, deny);
    CHECK_EQ(state.drawing().submitImage(lease), DrawingResult::Busy);
    CHECK(state.drawing().validImageLease(lease));
    deny = false;
    CHECK_EQ(state.drawing().submitImage(lease), DrawingResult::Applied);
    CHECK_EQ(state.drawing().sampleColor(0).get(), 0x112233u);
    CHECK(!state.drawing().validImageLease(lease));
}

void checkQueuedImageBusyRetry() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime::ImageLease lease;
    CHECK_EQ(state.drawing().leaseImage(1, 1, 1, lease), DrawingResult::Applied);
    lease.bytes[0] = 0x44;
    lease.bytes[1] = 0x55;
    lease.bytes[2] = 0x66;
    DrawingHandle handle;
    CHECK_EQ(state.drawing().submitImage(lease, &handle), DrawingResult::Applied);
    bool deny = false;
    denyRefresh(object, deny);
    CHECK_EQ(state.drawing().applyQueued(handle), DrawingResult::Busy);
    CHECK(state.drawing().queuedImage());
    deny = false;
    CHECK_EQ(state.drawing().applyQueued(handle), DrawingResult::Applied);
    CHECK_EQ(state.drawing().sampleColor(0).get(), 0x445566u);
    CHECK_EQ(state.drawing().applyQueued(handle), DrawingResult::Stale);
}

void checkClearInvalidatesLease() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime::ImageLease lease;
    CHECK_EQ(state.drawing().leaseImage(1, 1, 1, lease), DrawingResult::Applied);
    CHECK_EQ(state.drawing().clear(), DrawingResult::NoChange);
    CHECK(!state.drawing().validImageLease(lease));
    CHECK_EQ(state.drawing().submitImage(lease), DrawingResult::Stale);
}

void checkSupersessionPreparationIsTransactional() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime::ImageLease first;
    CHECK_EQ(state.drawing().leaseImage(1, 1, 1, first), DrawingResult::Applied);
    first.bytes[0] = 0x77;
    first.bytes[1] = 0x88;
    first.bytes[2] = 0x99;
    bool deny = false;
    denyRefresh(object, deny);
    DrawingRuntime::ImageLease replacement;
    CHECK_EQ(state.drawing().leaseImage(2, 1, 1, replacement, &first), DrawingResult::Busy);
    CHECK(state.drawing().validImageLease(first));
    CHECK_EQ(first.bytes[0], 0x77u);
    deny = false;
    CHECK_EQ(state.drawing().leaseImage(2, 1, 1, replacement, &first), DrawingResult::Applied);
    CHECK(!state.drawing().validImageLease(first));
    CHECK(state.drawing().validImageLease(replacement));
}

void checkCommittedSessionBusyRetry() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingSession session(state.drawing());
    const std::uint8_t rgb[] = {0xAA, 0xBB, 0xCC};
    CHECK_EQ(session.imageChunk(7, 10, 1, 1, 1, 0, rgb, sizeof(rgb)), DrawingResult::Applied);
    bool deny = false;
    denyRefresh(object, deny);
    CHECK_EQ(session.imageCommit(7, 11, 1), DrawingResult::Busy);
    CHECK_EQ(session.status().transferChunksReceived, 1u);
    deny = false;
    CHECK_EQ(session.imageCommit(7, 12, 1), DrawingResult::Applied);
    CHECK_EQ(state.drawing().sampleColor(0).get(), 0xAABBCCu);
}

void checkSessionScopeReleasesLease() {
    Heptagon919 object;
    RuntimeState state(object);
    const std::uint8_t chunk[] = {1, 2, 3, 4, 5, 6};
    {
        DrawingSession session(state.drawing());
        CHECK_EQ(session.imageChunk(9, 1, 1, 2, 1, 0, chunk, sizeof(chunk)),
                 DrawingResult::Applied);
    }
    DrawingRuntime::ImageLease lease;
    CHECK_EQ(state.drawing().leaseImage(2, 1, 1, lease), DrawingResult::Applied);
}

void checkSessionRebindReleasesOldLease() {
    Heptagon919 firstObject;
    Heptagon919 secondObject;
    RuntimeState first(firstObject);
    RuntimeState second(secondObject);
    const std::uint8_t chunk[] = {1, 2, 3, 4, 5, 6};
    DrawingSession session(first.drawing());
    CHECK_EQ(session.imageChunk(9, 1, 1, 2, 1, 0, chunk, sizeof(chunk)), DrawingResult::Applied);
    session.bind(second.drawing());
    DrawingRuntime::ImageLease lease;
    CHECK_EQ(first.drawing().leaseImage(2, 1, 1, lease), DrawingResult::Applied);
}

void checkSessionSurvivesRuntimeDestruction() {
    Heptagon919 object;
    DrawingSession session;
    const std::uint8_t chunk[] = {1, 2, 3, 4, 5, 6};
    {
        RuntimeState state(object);
        session.bind(state.drawing());
        CHECK_EQ(session.imageChunk(9, 1, 1, 2, 1, 0, chunk, sizeof(chunk)),
                 DrawingResult::Applied);
    }
}

} // namespace

int main() {
    checkPaletteSurvivesDrawingDenial();
    checkGeometryFactoryBadAllocRecovery();
    checkRejectionStatusOwnsText();
    checkGenerationChangeInvalidatesPendingStorage();
    checkQueuedSceneBusyRetry();
    checkQueuedSameSceneAdvancesRevisionAfterGeometryChange();
    checkImmediateImageBusyRetry();
    checkQueuedImageBusyRetry();
    checkClearInvalidatesLease();
    checkSupersessionPreparationIsTransactional();
    checkCommittedSessionBusyRetry();
    checkSessionScopeReleasesLease();
    checkSessionRebindReleasesOldLease();
    checkSessionSurvivesRuntimeDestruction();
    if (failures != 0) {
        std::cerr << failures << " drawing recovery test(s) failed\n";
        return 1;
    }
    std::cout << "All drawing recovery tests passed\n";
    return 0;
}
