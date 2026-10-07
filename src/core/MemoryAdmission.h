#ifndef PACKAGES_LIGHTGRAPH_SRC_CORE_MEMORYADMISSION_H_
#define PACKAGES_LIGHTGRAPH_SRC_CORE_MEMORYADMISSION_H_

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace lightgraph::memory {

constexpr size_t kAllocationOverheadBytes = 32u;

enum class Operation : uint8_t {
  Emit = 0,
  EmitParameters = 1,
  DrawingCache = 2,
  DrawingImages = 3,
  Geometry = 4,
  // Values 5 and 6 were previously transport-specific HTTP operations.
  // Keep RuntimeState's numeric value stable for diagnostics consumers.
  RuntimeState = 7,
};

struct EmitSpan {
  uint16_t length = 0;
  uint16_t trail = 0;
  uint32_t lightCount = 0;
};

struct Estimate {
  size_t peakBytes = 0;
  size_t largestBlock = 0;

  void addAllocation(size_t bytes, size_t count = 1) noexcept {
    if (count == 0) {
      return;
    }

    constexpr size_t maximum = std::numeric_limits<size_t>::max();
    const size_t chargedBlock = bytes > maximum - kAllocationOverheadBytes
        ? maximum
        : bytes + kAllocationOverheadBytes;
    if (chargedBlock > largestBlock) {
      largestBlock = chargedBlock;
    }

    const size_t charge = chargedBlock != 0 && count > maximum / chargedBlock
        ? maximum
        : chargedBlock * count;
    peakBytes = peakBytes > maximum - charge ? maximum : peakBytes + charge;
  }
};

struct Policy {
  bool (*check)(Operation operation, const Estimate& estimate, void* user) noexcept = nullptr;
  void* user = nullptr;
};

inline bool admitted(const Policy& policy,
                     Operation operation,
                     const Estimate& estimate) noexcept {
  return policy.check == nullptr || policy.check(operation, estimate, policy.user);
}

// Heap estimates used by transports before constructing EmitParams and by
// State immediately before candidate construction. These accept only scalar
// inputs so firmware queues do not need to expose transport-specific types to
// the core.
Estimate estimateEmitParameters(size_t paletteStopCount,
                                bool hasColorRule = false) noexcept;
Estimate estimateEmitCandidate(uint16_t lightCount,
                               size_t paletteStopCount,
                               bool hasColorRule,
                               bool contiguousLights) noexcept;
Estimate estimateStateInitialization(uint16_t pixelCount,
                                    bool fractionalRendering) noexcept;
EmitSpan resolveEmitSpan(uint16_t requestedLength,
                         uint16_t oldLength,
                         float speed,
                         uint16_t explicitTrail,
                         bool centered,
                         bool sequential,
                         bool linked,
                         uint16_t behaviourFlags) noexcept;

}  // namespace lightgraph::memory

#endif  // PACKAGES_LIGHTGRAPH_SRC_CORE_MEMORYADMISSION_H_
