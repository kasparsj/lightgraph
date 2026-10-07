#include <cstdlib>
#include <iostream>
#include <new>

#include "lightgraph/internal/debug.hpp"
#include "lightgraph/internal/objects.hpp"

namespace {
bool inject = false;
size_t failAt = 0;
size_t attempts = 0;
void* tracked[64]{};
size_t outstanding = 0;
size_t observerCalls = 0;
LightgraphAllocationFailureSite observedSite = LightgraphAllocationFailureSite::Unknown;

void observeAllocationFailure(LightgraphAllocationFailureSite site, uint16_t, uint16_t) {
    ++observerCalls;
    observedSite = site;
}

void* allocateArray(size_t size) noexcept {
    if (inject && attempts++ == failAt) return nullptr;
    void* result = std::malloc(size == 0 ? 1 : size);
    if (inject && result != nullptr) {
        for (auto& entry : tracked) {
            if (entry == nullptr) {
                entry = result;
                ++outstanding;
                break;
            }
        }
    }
    return result;
}

void releaseArray(void* pointer) noexcept {
    if (pointer == nullptr) return;
    for (auto& entry : tracked) {
        if (entry == pointer) {
            entry = nullptr;
            --outstanding;
            break;
        }
    }
    std::free(pointer);
}

bool emptyCache(const TopologyPixels& pixels) {
    for (uint16_t i = 0; i < HEPTAGON919_PIXEL_COUNT; ++i) {
        if (pixels.isIntersection(i) || pixels.isConnection(i)) return false;
        for (uint8_t model = 0; model < 7; ++model) {
            if (pixels.isModelWeight(model, i)) return false;
        }
    }
    return true;
}

void startFailure(size_t index) {
    failAt = index;
    attempts = 0;
    inject = true;
    observerCalls = 0;
    observedSite = LightgraphAllocationFailureSite::Unknown;
}
} // namespace

void* operator new[](size_t size) {
    if (void* result = allocateArray(size)) return result;
    throw std::bad_alloc();
}
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return allocateArray(size);
}
void operator delete[](void* pointer) noexcept { releaseArray(pointer); }
void operator delete[](void* pointer, size_t) noexcept { releaseArray(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { releaseArray(pointer); }

int main() {
    Heptagon919 object;
    lightgraphSetAllocationFailureObserver(object.runtimeContext(), observeAllocationFailure);
    try {
        // Fail each allocation, including every partially initialized model row.
        for (size_t index = 0; index < 10; ++index) {
            startFailure(index);
            {
                TopologyPixels pixels(object);
                inject = false;
                if (pixels.isValid() || !emptyCache(pixels) || outstanding != 0 ||
                    observerCalls != 1 ||
                    observedSite != LightgraphAllocationFailureSite::TopologyPixelsAllocation) return 1;
                pixels.refresh();
                if (!pixels.isValid() || !pixels.isIntersection(object.inter[0][0]->topPixel)) return 2;
            }
            if (outstanding != 0) return 3;

            // A failed refresh must also release the old cache and recover later.
            TopologyPixels pixels(object);
            startFailure(index);
            pixels.refresh();
            inject = false;
            if (pixels.isValid() || !emptyCache(pixels) || outstanding != 0 ||
                observerCalls != 1 ||
                observedSite != LightgraphAllocationFailureSite::TopologyPixelsAllocation) return 4;
            pixels.refresh();
            if (!pixels.isValid() || !pixels.isIntersection(object.inter[0][0]->topPixel)) return 5;
        }
    } catch (const std::bad_alloc&) {
        inject = false;
        std::cerr << "Debug cache allocation escaped as bad_alloc\n";
        return 6;
    }
    std::cout << "TopologyPixels allocation failure and recovery checks passed\n";
    return 0;
}
