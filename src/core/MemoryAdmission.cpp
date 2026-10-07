#include "MemoryAdmission.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../Globals.h"
#include "../runtime/Behaviour.h"
#include "../runtime/BgLight.h"
#include "../runtime/Light.h"
#include "../runtime/LightList.h"

namespace lightgraph::memory {
namespace {

size_t expandedPaletteCount(size_t paletteStopCount, bool hasColorRule) noexcept {
  if (!hasColorRule || paletteStopCount == 0) return paletteStopCount;
  constexpr size_t maximum = std::numeric_limits<size_t>::max();
  return paletteStopCount > maximum / 8u ? maximum : paletteStopCount * 8u;
}

size_t saturatedBytes(size_t count, size_t elementSize) noexcept {
  constexpr size_t maximum = std::numeric_limits<size_t>::max();
  return elementSize != 0 && count > maximum / elementSize ? maximum : count * elementSize;
}

void addPaletteStorage(Estimate& estimate, size_t stops, bool hasRule, size_t copies) noexcept {
  if (stops == 0 || copies == 0) return;
  estimate.addAllocation(saturatedBytes(stops, sizeof(int64_t)), copies);
  estimate.addAllocation(saturatedBytes(stops, sizeof(float)), copies);
  estimate.addAllocation(saturatedBytes(expandedPaletteCount(stops, hasRule), sizeof(ColorRGB)), copies);
}

}  // namespace

Estimate estimateEmitParameters(size_t paletteStopCount, bool hasColorRule) noexcept {
  Estimate estimate;
  addPaletteStorage(estimate, paletteStopCount, hasColorRule, 1);
  return estimate;
}

Estimate estimateEmitCandidate(uint16_t lightCount, size_t paletteStopCount,
                               bool hasColorRule, bool contiguousLights) noexcept {
  Estimate estimate;
  estimate.addAllocation(sizeof(LightList));
  estimate.addAllocation(sizeof(Behaviour));
  estimate.addAllocation(static_cast<size_t>(lightCount) * sizeof(RuntimeLight*));
  estimate.addAllocation(contiguousLights ? static_cast<size_t>(lightCount) * sizeof(Light)
                                          : sizeof(Light),
                         contiguousLights ? 1 : lightCount);
  addPaletteStorage(estimate, paletteStopCount, hasColorRule, 2);
  const size_t colors = std::max<size_t>(lightCount, expandedPaletteCount(paletteStopCount, hasColorRule));
  estimate.addAllocation(saturatedBytes(colors, sizeof(ColorRGB)), 3);
  return estimate;
}

Estimate estimateStateInitialization(uint16_t pixelCount, bool fractionalRendering) noexcept {
  Estimate estimate;
  const size_t pixels = pixelCount;
  estimate.addAllocation(pixels * sizeof(uint16_t), 3);
  estimate.addAllocation(pixels * sizeof(uint8_t));
  estimate.addAllocation(((pixels + 7u) / 8u) * sizeof(uint8_t));
  estimate.addAllocation((pixels + 3u) * sizeof(uint16_t));
  if (fractionalRendering) {
    estimate.addAllocation(pixels * sizeof(uint8_t), 3);
    estimate.addAllocation(pixels * sizeof(uint16_t)); // Touched pixel indices.
  }
  estimate.addAllocation(sizeof(BgLight));
  estimate.addAllocation(sizeof(int64_t), 2);
  estimate.addAllocation(sizeof(float), 2);
  estimate.addAllocation(sizeof(ColorRGB), 2);
  return estimate;
}

EmitSpan resolveEmitSpan(uint16_t requestedLength, uint16_t oldLength, float speed,
                         uint16_t explicitTrail, bool centered, bool sequential,
                         bool linked, uint16_t behaviourFlags) noexcept {
  EmitSpan span;
  span.length = requestedLength;
  if (!centered && oldLength > 0 && (behaviourFlags & B_SMOOTH_CHANGES) != 0) {
    const float delta = static_cast<float>(static_cast<int32_t>(requestedLength) -
                                           static_cast<int32_t>(oldLength));
    const int32_t smoothed = static_cast<int32_t>(oldLength) +
                             static_cast<int32_t>(std::lround(delta * 0.1f));
    span.length = static_cast<uint16_t>(std::max<int32_t>(
        0, std::min<int32_t>(std::numeric_limits<uint16_t>::max(), smoothed)));
  }
  if (!centered) {
    if (speed == 0.0f) {
      span.trail = explicitTrail;
    } else if (sequential && linked && (behaviourFlags & B_RENDER_SEGMENT) == 0) {
      const uint16_t halfLength = std::max<uint16_t>(1, span.length / 2u);
      const double speedTrail = static_cast<double>(speed) * halfLength;
      if (std::isfinite(speedTrail) && speedTrail > 0.0) {
        span.trail = static_cast<uint16_t>(std::min<double>(speedTrail, Random::MAX_LENGTH - 1));
      } else if (speedTrail > 0.0) {
        span.trail = static_cast<uint16_t>(Random::MAX_LENGTH - 1);
      }
    }
  }
  const uint32_t body = span.length > span.trail ? span.length - span.trail : 1u;
  span.lightCount = body + span.trail;
  return span;
}

}  // namespace lightgraph::memory
