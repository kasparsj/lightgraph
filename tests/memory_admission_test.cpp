#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <stdexcept>

#include "lightgraph/integration.hpp"
#include "lightgraph/integration/observability.hpp"
#include "lightgraph/internal/runtime.hpp"

namespace {

struct AdmissionProbe {
    bool rejectParameters = false;
    bool rejectEmit = false;
    bool rejectRuntimeState = false;
    std::size_t parameterChecks = 0;
    std::size_t emitChecks = 0;
    std::size_t runtimeStateChecks = 0;
    lightgraph::memory::Estimate lastParameters{};
    lightgraph::memory::Estimate lastEmit{};
    lightgraph::memory::Estimate lastRuntimeState{};
    bool armThrowingFailure = false;
    bool armThrowingFailureOnParameters = false;
    bool armThrowingFailureOnRuntimeState = false;
    std::size_t throwingFailureAttempt = static_cast<std::size_t>(-1);
};

std::size_t contiguousAttempts = 0;
std::size_t contiguousDeallocations = 0;
bool failContiguous = false;
bool throwingFailureEnabled = false;
std::size_t throwingFailureAttempt = static_cast<std::size_t>(-1);
std::size_t throwingAttempts = 0;

struct StateAllocationProbe {
    std::size_t listPixelValuesR = 0;
    std::size_t listPixelValuesG = 0;
    std::size_t listPixelValuesB = 0;
    std::size_t listTouchedPixels = 0;
    std::size_t listBufferCheckpoints = 0;
};

StateAllocationProbe stateAllocationProbe;

void captureStateAllocation(const char* stage, std::size_t requestedBytes) {
    if (std::strcmp(stage, "state.listPixelValuesR") == 0) {
        stateAllocationProbe.listPixelValuesR = requestedBytes;
        ++stateAllocationProbe.listBufferCheckpoints;
    } else if (std::strcmp(stage, "state.listPixelValuesG") == 0) {
        stateAllocationProbe.listPixelValuesG = requestedBytes;
        ++stateAllocationProbe.listBufferCheckpoints;
    } else if (std::strcmp(stage, "state.listPixelValuesB") == 0) {
        stateAllocationProbe.listPixelValuesB = requestedBytes;
        ++stateAllocationProbe.listBufferCheckpoints;
    } else if (std::strcmp(stage, "state.listTouchedPixels") == 0) {
        stateAllocationProbe.listTouchedPixels = requestedBytes;
        ++stateAllocationProbe.listBufferCheckpoints;
    }
}

void throwNonAllocationFailure(const char* stage, std::size_t) {
    if (std::string(stage) == "state.pixelValuesG") {
        throw std::runtime_error("injected non-allocation failure");
    }
}

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

bool checkAdmission(lightgraph::memory::Operation operation,
                    const lightgraph::memory::Estimate& estimate,
                    void* user) noexcept {
    auto* probe = static_cast<AdmissionProbe*>(user);
    if (operation == lightgraph::memory::Operation::EmitParameters) {
        ++probe->parameterChecks;
        probe->lastParameters = estimate;
        if (probe->armThrowingFailureOnParameters) {
            throwingAttempts = 0;
            throwingFailureAttempt = probe->throwingFailureAttempt;
            throwingFailureEnabled = true;
        }
        return !probe->rejectParameters;
    }
    if (operation == lightgraph::memory::Operation::Emit) {
        ++probe->emitChecks;
        probe->lastEmit = estimate;
        if (probe->armThrowingFailure) {
            throwingAttempts = 0;
            throwingFailureAttempt = probe->throwingFailureAttempt;
            throwingFailureEnabled = true;
        }
        return !probe->rejectEmit;
    }
    if (operation == lightgraph::memory::Operation::RuntimeState) {
        ++probe->runtimeStateChecks;
        probe->lastRuntimeState = estimate;
        if (probe->armThrowingFailureOnRuntimeState) {
            throwingAttempts = 0;
            throwingFailureAttempt = probe->throwingFailureAttempt;
            throwingFailureEnabled = true;
        }
        return !probe->rejectRuntimeState;
    }
    return true;
}

struct AllocatorUser { std::size_t allocations = 0; std::size_t frees = 0; };

void* allocateContiguous(std::size_t bytes, void* user) noexcept {
    if (user) ++static_cast<AllocatorUser*>(user)->allocations;
    ++contiguousAttempts;
    return failContiguous ? nullptr : std::malloc(bytes);
}

void deallocateContiguous(void* pointer, void* user) noexcept {
    if (user) ++static_cast<AllocatorUser*>(user)->frees;
    ++contiguousDeallocations;
    std::free(pointer);
}

EmitParams capturedLegacy(std::uint16_t noteId) {
    EmitParams params(0, 0.13213f, 0xFFFFFF);
    params.setLength(56);
    params.noteId = noteId;
    params.duration = 2100;
    params.maxBri = 237;
    params.behaviourFlags = B_FILL_EASE;
    return params;
}

EmitParams centered(std::uint16_t noteId) {
    EmitParams params(0, 0.5f, 0xFFFFFF);
    params.setLength(48);
    params.noteId = noteId;
    params.duration = 4000;
    params.lengthMode = lightgraph::LengthMode::Centered;
    params.visibleLength = 0.0f;
    return params;
}

}  // namespace

void* operator new(std::size_t bytes) {
    if (throwingFailureEnabled && throwingAttempts++ == throwingFailureAttempt) {
        throw std::bad_alloc();
    }
    if (void* result = std::malloc(bytes == 0 ? 1 : bytes)) {
        return result;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(bytes);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept {
    return ::operator new(bytes, std::nothrow);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    std::free(pointer);
}

int main() {
    const auto empty = lightgraph::memory::estimateEmitParameters(0, false);
    if (empty.peakBytes != 0 || empty.largestBlock != 0) {
        return fail("empty parameter estimates should not reserve heap");
    }

    {
        const auto oversizedTrail = lightgraph::memory::resolveEmitSpan(
            1, 0, 0.0f, 100, false, true, true, 0);
        if (oversizedTrail.length != 1 || oversizedTrail.trail != 100 ||
            oversizedTrail.lightCount != 101) {
            return fail("emit span must count explicit trails beyond requested length");
        }
        const auto smoothed = lightgraph::memory::resolveEmitSpan(
            1000, 10, 0.0f, 0, false, true, true, B_SMOOTH_CHANGES);
        if (smoothed.length != 109 || smoothed.trail != 0 || smoothed.lightCount != 109) {
            return fail("emit span must share legacy smooth-change capacity semantics");
        }
        const auto unboundedSpeed = lightgraph::memory::resolveEmitSpan(
            255, 0, std::numeric_limits<float>::infinity(), 0, false, true, true, 0);
        if (unboundedSpeed.trail != Random::MAX_LENGTH - 1) {
            return fail("emit span must clamp unbounded speed before integer conversion");
        }
        EmitParams actualBuilderParams(0, std::numeric_limits<float>::max(), 0xFFFFFF);
        if (actualBuilderParams.getSpeedTrail(actualBuilderParams.speed, 255) !=
            Random::MAX_LENGTH - 1) {
            return fail("EmitParams trail construction must use the shared bounded resolver");
        }
    }

    for (const std::uint16_t pixelCount : {std::uint16_t{919}, std::uint16_t{3024}}) {
        const auto withoutFractional =
            lightgraph::memory::estimateStateInitialization(pixelCount, false);
        const auto withFractional =
            lightgraph::memory::estimateStateInitialization(pixelCount, true);
        const std::size_t expectedFractionalCharge =
            static_cast<std::size_t>(pixelCount) *
                (3u * sizeof(std::uint8_t) + sizeof(std::uint16_t)) +
            4u * lightgraph::memory::kAllocationOverheadBytes;
        if (withFractional.peakBytes < withoutFractional.peakBytes ||
            withFractional.peakBytes - withoutFractional.peakBytes !=
                expectedFractionalCharge) {
            return fail("fractional State estimate must add 3*N color bytes plus 2*N touched-index bytes");
        }
    }

    {
        constexpr std::uint16_t pixelCount = 96;
        Line line(pixelCount);
        AdmissionProbe probe;
        lightgraph::integration::setMemoryAdmissionPolicy(
            line, lightgraph::memory::Policy{checkAdmission, &probe});
        stateAllocationProbe = {};
        State::allocationCheckpoint = captureStateAllocation;
        State state(line);
        State::allocationCheckpoint = nullptr;
        if (!state.ready() || probe.runtimeStateChecks != 1) {
            return fail("State allocation checkpoint fixture did not initialize");
        }
        const auto expected = lightgraph::memory::estimateStateInitialization(
            pixelCount, LIGHTGRAPH_FRACTIONAL_RENDERING != 0);
        if (probe.lastRuntimeState.peakBytes != expected.peakBytes ||
            probe.lastRuntimeState.largestBlock != expected.largestBlock) {
            return fail("State construction must submit the exact initialization estimate");
        }
#if LIGHTGRAPH_FRACTIONAL_RENDERING
        if (stateAllocationProbe.listBufferCheckpoints != 4 ||
            stateAllocationProbe.listPixelValuesR != pixelCount * sizeof(std::uint8_t) ||
            stateAllocationProbe.listPixelValuesG != pixelCount * sizeof(std::uint8_t) ||
            stateAllocationProbe.listPixelValuesB != pixelCount * sizeof(std::uint8_t) ||
            stateAllocationProbe.listTouchedPixels != pixelCount * sizeof(std::uint16_t)) {
            return fail("fractional State scratch checkpoints must charge 3*N bytes plus 2*N touched bytes");
        }
        const std::size_t actualColorScratchBytes =
            state.listPixelValuesR.capacity() *
                sizeof(decltype(state.listPixelValuesR)::value_type) +
            state.listPixelValuesG.capacity() *
                sizeof(decltype(state.listPixelValuesG)::value_type) +
            state.listPixelValuesB.capacity() *
                sizeof(decltype(state.listPixelValuesB)::value_type);
        const std::size_t actualTouchedBytes =
            state.listTouchedPixels.capacity() *
            sizeof(decltype(state.listTouchedPixels)::value_type);
        if (actualColorScratchBytes != 3u * pixelCount ||
            actualTouchedBytes != 2u * pixelCount) {
            return fail("fractional State vectors must allocate 3*N color bytes plus 2*N touched-index bytes");
        }
#else
        if (stateAllocationProbe.listBufferCheckpoints != 0) {
            return fail("non-fractional State must not allocate fractional scratch buffers");
        }
#endif
    }

    {
        Line line(96);
        State state(line);
        EmitParams params = capturedLegacy(61008);
        params.speed = std::numeric_limits<float>::quiet_NaN();
        if (state.emit(params) >= 0 || state.findList(61008) >= 0) {
            return fail("State must reject non-finite speeds before resolution");
        }
    }
    {
        Line line(96);
        State state(line);
        EmitParams params = capturedLegacy(61013);
        params.visibleLength = std::numeric_limits<float>::quiet_NaN();
        if (state.emit(params) >= 0 || state.findList(61013) >= 0 ||
            state.lastEmitFailure != State::EmitFailure::InvalidArgument) {
            return fail("State must reject non-finite legacy visible lengths");
        }
    }
    {
        Line line(8);
        State::allocationCheckpoint = throwNonAllocationFailure;
        bool propagated = false;
        try {
            State state(line);
        } catch (const std::runtime_error&) {
            propagated = true;
        }
        State::allocationCheckpoint = nullptr;
        if (!propagated) {
            return fail("State construction must propagate unrelated exceptions");
        }
    }
    {
        Line line(96);
        line.runtimeContext().contiguousAllocation =
            {allocateContiguous, deallocateContiguous, nullptr};
        contiguousDeallocations = 0;
        {
            State state(line);
            state.lightLists[0]->visible = false;
            failContiguous = false;
            EmitParams params = centered(61012);
            if (state.emit(params) < 1) {
                return fail("deallocator retention fixture could not emit centered list");
            }
            line.runtimeContext().contiguousAllocation = {};
        }
        if (contiguousDeallocations != 1) {
            return fail("contiguous storage must retain its matching context deallocator");
        }
    }

    {
        Line line(96);
        AdmissionProbe probe;
        probe.rejectRuntimeState = true;
        lightgraph::integration::setMemoryAdmissionPolicy(
            line, lightgraph::memory::Policy{checkAdmission, &probe});
        State state(line);
        lightgraph::integration::setMemoryAdmissionPolicy(line, {});
        EmitParams params = capturedLegacy(61007);
        state.update();
        if (state.emit(params) >= 0 || state.getPixel(0).R != 0 ||
            state.initializationResult != State::InitializationResult::AdmissionDenied ||
            state.ready() || state.initializationAdmitted || state.hasRequiredFrameBuffers() ||
            state.lightLists[0] != nullptr || state.totalLightLists != 0 ||
            probe.runtimeStateChecks != 1) {
            return fail("admission-denied State must remain permanently inert");
        }
        State recovered(line);
        if (!recovered.ready() ||
            recovered.initializationResult != State::InitializationResult::Ready) {
            return fail("a fresh State must recover after admission becomes available");
        }
    }
    {
        Line line(96);
        AdmissionProbe probe;
        probe.armThrowingFailureOnRuntimeState = true;
        probe.throwingFailureAttempt = 0;
        lightgraph::integration::setMemoryAdmissionPolicy(
            line, lightgraph::memory::Policy{checkAdmission, &probe});
        State state(line);
        throwingFailureEnabled = false;
        if (state.ready() ||
            state.initializationResult != State::InitializationResult::AllocationFailed ||
            state.hasRequiredFrameBuffers() || state.lightLists[0] != nullptr) {
            return fail("State buffer allocation failure must produce an inert failed State");
        }
    }
    lightgraph::memory::Estimate saturation;
    saturation.addAllocation(static_cast<std::size_t>(-1), 2);
    if (saturation.peakBytes != static_cast<std::size_t>(-1) ||
        saturation.largestBlock != static_cast<std::size_t>(-1)) {
        return fail("memory estimates must saturate instead of wrapping");
    }

    {
        Line line(96);
        State state(line);
        state.lightLists[0]->visible = false;
        AdmissionProbe probe;
        probe.rejectParameters = true;
        lightgraph::integration::setMemoryAdmissionPolicy(
            line, lightgraph::memory::Policy{checkAdmission, &probe});

        EmitParams params = capturedLegacy(61000);
        if (state.emit(params) >= 0 || state.findList(61000) >= 0 ||
            state.totalLights != 0 || state.totalLightLists != 1 ||
            state.emitMemoryRejections != 1 || probe.parameterChecks != 1 ||
            probe.emitChecks != 0 ||
            state.lastEmitRejection.operation != lightgraph::memory::Operation::EmitParameters ||
            state.lastEmitRejection.reason != State::MemoryRejectionReason::Admission) {
            return fail("parameter admission rejection changed live state or diagnostics");
        }
    }

    {
        Line line(96);
        State state(line);
        state.lightLists[0]->visible = false;
        AdmissionProbe probe;
        probe.armThrowingFailureOnParameters = true;
        probe.throwingFailureAttempt = 0;
        lightgraph::integration::setMemoryAdmissionPolicy(
            line, lightgraph::memory::Policy{checkAdmission, &probe});
        EmitParams params = capturedLegacy(61009);
        const int8_t result = state.emit(params);
        throwingFailureEnabled = false;
        if (result >= 0 || state.findList(61009) >= 0 ||
            state.lastEmitRejection.operation != lightgraph::memory::Operation::EmitParameters ||
            state.lastEmitRejection.reason != State::MemoryRejectionReason::AllocationFailure) {
            return fail("EmitParams copy allocation failure was not contained and recorded");
        }
    }

    std::size_t candidateThrowingAllocations = 0;
    {
        Line line(96);
        State state(line);
        state.lightLists[0]->visible = false;
        AdmissionProbe probe;
        probe.armThrowingFailure = true;
        lightgraph::integration::setMemoryAdmissionPolicy(
            line, lightgraph::memory::Policy{checkAdmission, &probe});
        EmitParams params = capturedLegacy(61010);
        params.palette = Palette({0x112233, 0x445566, 0x778899}, {0.0f, 0.5f, 1.0f});
        const int8_t result = state.emit(params);
        candidateThrowingAllocations = throwingAttempts;
        throwingFailureEnabled = false;
        if (result < 1 || candidateThrowingAllocations < 2) {
            return fail("candidate allocation calibration did not exercise palette and interpolation storage");
        }
    }

    for (std::size_t failureAttempt = 0;
         failureAttempt < candidateThrowingAllocations;
         ++failureAttempt) {
        Line line(96);
        State state(line);
        state.lightLists[0]->visible = false;
        EmitParams initial = capturedLegacy(61011);
        initial.palette = Palette({0x112233, 0x445566, 0x778899}, {0.0f, 0.5f, 1.0f});
        const int8_t index = state.emit(initial);
        if (index < 1) {
            return fail("throwing-allocation fixture could not create its original list");
        }
        LightList* const original = state.lightLists[index];
        const std::uint16_t originalLights = state.totalLights;

        AdmissionProbe probe;
        probe.armThrowingFailure = true;
        probe.throwingFailureAttempt = failureAttempt;
        lightgraph::integration::setMemoryAdmissionPolicy(
            line, lightgraph::memory::Policy{checkAdmission, &probe});
        EmitParams replacement = initial;
        const int8_t result = state.emit(replacement);
        throwingFailureEnabled = false;
        if (result >= 0 || state.lightLists[index] != original ||
            state.totalLights != originalLights || state.totalLightLists != 2 ||
            state.lastEmitRejection.reason != State::MemoryRejectionReason::AllocationFailure) {
            return fail("throwing palette/interpolation failure did not preserve the original list");
        }
    }

    {
        Line line(8);
        State state(line);
        state.emitMemoryRejections = std::numeric_limits<std::uint32_t>::max();
        state.recordEmitAllocationFailure(lightgraph::memory::Operation::Emit);
        if (state.emitMemoryRejections != std::numeric_limits<std::uint32_t>::max() ||
            state.lastEmitRejection.reason != State::MemoryRejectionReason::AllocationFailure) {
            return fail("memory rejection diagnostics counter should saturate");
        }
    }

    {
        Line line(96);
        State state(line);
        state.lightLists[0]->visible = false;
        AdmissionProbe probe;
        lightgraph::integration::setMemoryAdmissionPolicy(
            line, lightgraph::memory::Policy{checkAdmission, &probe});

        EmitParams initial = capturedLegacy(61001);
        const int8_t initialIndex = state.emit(initial);
        if (initialIndex < 1) {
            return fail("captured legacy request should pass an unrestricted admission policy");
        }
        LightList* const original = state.lightLists[initialIndex];
        const std::uint16_t originalLights = state.totalLights;
        const std::uint8_t originalLists = state.totalLightLists;

        probe.rejectEmit = true;
        EmitParams replacement = capturedLegacy(61001);
        replacement.setLength(20);
        if (state.emit(replacement) >= 0 || state.lightLists[initialIndex] != original ||
            state.totalLights != originalLights || state.totalLightLists != originalLists ||
            state.emitMemoryRejections != 1 || probe.emitChecks < 2 ||
            state.lastEmitRejection.operation != lightgraph::memory::Operation::Emit ||
            state.lastEmitRejection.reason != State::MemoryRejectionReason::Admission ||
            state.lastEmitRejection.peakBytes == 0 ||
            state.lastEmitRejection.largestBlock == 0) {
            return fail("candidate admission rejection did not preserve the existing list");
        }
    }

    {
        Line line(96);
        line.runtimeContext().contiguousAllocation =
            {allocateContiguous, deallocateContiguous, nullptr};
        State state(line);
        state.lightLists[0]->visible = false;
        contiguousAttempts = 0;
        failContiguous = false;
        EmitParams params = centered(61002);
        const int8_t index = state.emit(params);
        if (index < 1 || contiguousAttempts != 1 || state.lightLists[index] == nullptr ||
            state.lightLists[index]->numLights != 48) {
            return fail("centered lists should use exactly one contiguous Light allocation");
        }

        const std::size_t allocationsBeforeResize = contiguousAttempts;
        const lightgraph::ListLengthUpdate updates[] = {{61002, 0.0f}, {61002, 31.5f}};
        if (!state.setListLengths(updates, 1) ||
            !state.setListLengths(updates + 1, 1) ||
            contiguousAttempts != allocationsBeforeResize) {
            return fail("centered visible-length changes must not allocate storage");
        }
    }
    {
        Line line(96);
        line.runtimeContext().contiguousAllocation =
            {allocateContiguous, deallocateContiguous, nullptr};
        State state(line);
        state.lightLists[0]->visible = false;
        contiguousAttempts = 0;
        failContiguous = true;
        EmitParams params = centered(61003);
        if (state.emit(params) >= 0 || contiguousAttempts != 1 ||
            state.findList(61003) >= 0 || state.totalLights != 0 ||
            state.totalLightLists != 1) {
            return fail("failed centered contiguous allocation must roll back atomically");
        }
    }
    failContiguous = false;
    for (bool allocateOnly : {false, true}) {
        Line line(96);
        State state(line);
        line.runtimeContext().contiguousAllocation = allocateOnly
            ? LightgraphContiguousAllocationPolicy{allocateContiguous, nullptr, nullptr}
            : LightgraphContiguousAllocationPolicy{nullptr, deallocateContiguous, nullptr};
        contiguousAttempts = contiguousDeallocations = 0;
        auto params = centered(61004);
        if (state.emit(params) >= 0 || state.findList(61004) >= 0 ||
            contiguousAttempts != 0 || contiguousDeallocations != 0) {
            return fail("incomplete allocator pairs must fail before allocating or freeing storage");
        }
    }
    {
        Line line(96);
        contiguousAttempts = contiguousDeallocations = 0;
        AllocatorUser original, replacement;
        {
            State state(line);
            line.runtimeContext().contiguousAllocation =
                {allocateContiguous, deallocateContiguous, &original};
            auto params = centered(61005);
            if (state.emit(params) < 1) return fail("paired allocator should emit successfully");
            line.runtimeContext().contiguousAllocation = {nullptr, nullptr, &replacement};
        }
        if (contiguousAttempts != 1 || contiguousDeallocations != 1 ||
            original.allocations != 1 || original.frees != 1 || replacement.frees != 0)
            return fail("storage must retain its matching deallocator after policy changes");
    }
    const auto individual = lightgraph::memory::estimateEmitCandidate(48, 1, false, false);
    const auto contiguous = lightgraph::memory::estimateEmitCandidate(48, 1, false, true);
    if (individual.peakBytes <= contiguous.peakBytes ||
        contiguous.largestBlock < 48 * sizeof(Light)) {
        return fail("candidate estimates should account for per-allocation overhead and contiguous block size");
    }
    const auto paletteHeavy = lightgraph::memory::estimateEmitCandidate(1, 32, true, false);
    const std::size_t expandedPaletteBlock =
        32u * 8u * sizeof(ColorRGB) + lightgraph::memory::kAllocationOverheadBytes;
    if (paletteHeavy.largestBlock < expandedPaletteBlock) {
        return fail("candidate estimates must size interpolation scratch from an expanded palette");
    }

    std::cout << "Memory admission and contiguous-list regressions passed\n";
    return 0;
}
