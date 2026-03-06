#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <esp_heap_caps.h>
#endif

#include "LightListBuild.h"
#include "LightList.h"
#include "../Globals.h"

class Model;

namespace remote_snapshot {

struct SequentialEntry {
  uint16_t lightIdx = 0;
  uint8_t brightness = 0;
  uint8_t colorR = 0;
  uint8_t colorG = 0;
  uint8_t colorB = 0;
};

struct SingleSnapshotDescriptor {
  float speed = 0.0f;
  uint32_t lifeMillis = 0;
  uint8_t brightness = 0;
  uint8_t colorR = 0;
  uint8_t colorG = 0;
  uint8_t colorB = 0;
  bool hasBehaviour = false;
  uint16_t behaviourFlags = 0;
  uint8_t colorChangeGroups = 0;
  Model* model = nullptr;
};

struct TemplateSnapshotDescriptor {
  uint16_t numLights = 0;
  uint16_t length = 0;
  float speed = 0.0f;
  uint32_t lifeMillis = 0;
  uint32_t duration = 0;
  uint8_t easeIndex = 0;
  uint8_t fadeSpeed = 0;
  uint8_t fadeThresh = 0;
  uint8_t fadeEaseIndex = 0;
  uint8_t minBri = 0;
  uint8_t maxBri = 255;
  uint8_t head = LIST_HEAD_FRONT;
  bool linked = true;
  uint8_t blendMode = BLEND_NORMAL;
  bool hasBehaviour = false;
  uint16_t behaviourFlags = 0;
  uint8_t colorChangeGroups = 0;
  Model* model = nullptr;
  int8_t colorRule = -1;
  int8_t interpolationMode = 1;
  int8_t wrapMode = 0;
  float segmentation = 0.0f;
  uint8_t senderPixelDensity = 1;
  uint8_t receiverPixelDensity = 1;
};

struct SequentialSnapshotDescriptor {
  uint16_t numLights = 0;
  int16_t positionOffset = 0;
  float speed = 0.0f;
  uint32_t lifeMillis = 0;
  bool hasBehaviour = false;
  uint16_t behaviourFlags = 0;
  uint8_t colorChangeGroups = 0;
  Model* model = nullptr;
  uint8_t senderPixelDensity = 1;
  uint8_t receiverPixelDensity = 1;
};

using lightlist_build::addLifeDelayClamped;
using lightlist_build::mapLightIndexByDensity;
using lightlist_build::sanitizePixelDensity;
using lightlist_build::scaleLengthForDensity;
using lightlist_build::scaleSpeedForDensity;

struct RemoteSnapshotHeapBudget {
  bool hasTotalHeap = true;
  bool hasContiguousLightBlock = true;
  uint32_t estimatedBytes = 0;
  uint32_t requiredContiguousLightBytes = 0;
};

inline RemoteSnapshotHeapBudget assessRemoteSnapshotHeapBudget(uint16_t scaledNumLights,
                                                               size_t paletteStopCount) {
  RemoteSnapshotHeapBudget budget;
#if defined(ARDUINO_ARCH_ESP32)
  constexpr uint32_t kHeapGuardBytes = 4096U;
  constexpr uint32_t kAllocatorOverheadBytes = 1024U;
  constexpr uint32_t kLargestBlockGuardBytes = 256U;
  const uint32_t lightBytes = static_cast<uint32_t>(scaledNumLights) *
                              static_cast<uint32_t>(sizeof(Light) + sizeof(RuntimeLight*) + sizeof(ColorRGB));
  const uint32_t paletteBytes = static_cast<uint32_t>(paletteStopCount) *
                                static_cast<uint32_t>(sizeof(int64_t) + sizeof(float));
  budget.estimatedBytes = static_cast<uint32_t>(sizeof(LightList) + sizeof(Palette)) +
                          lightBytes + paletteBytes + kAllocatorOverheadBytes;
  budget.requiredContiguousLightBytes =
      static_cast<uint32_t>(scaledNumLights) * static_cast<uint32_t>(sizeof(Light));
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  budget.hasTotalHeap = freeHeap >= (budget.estimatedBytes + kHeapGuardBytes);
  budget.hasContiguousLightBlock =
      largestBlock >= (budget.requiredContiguousLightBytes + kLargestBlockGuardBytes);
#else
  (void) scaledNumLights;
  (void) paletteStopCount;
#endif
  return budget;
}

inline bool hasRemoteSnapshotTotalHeapBudget(const RemoteSnapshotHeapBudget& budget,
                                             uint16_t scaledNumLights,
                                             LightgraphAllocationFailureSite failureSite) {
  if (budget.hasTotalHeap) {
    return true;
  }
  lightgraphReportAllocationFailure(
      failureSite,
      scaledNumLights,
      static_cast<uint16_t>(std::min<uint32_t>(budget.estimatedBytes, std::numeric_limits<uint16_t>::max())));
  return false;
}

inline LightList* buildSingleLightSnapshot(const SingleSnapshotDescriptor& descriptor) {
  const RemoteSnapshotHeapBudget heapBudget = assessRemoteSnapshotHeapBudget(1, 0);
  if (!hasRemoteSnapshotTotalHeapBudget(
          heapBudget, 1, LightgraphAllocationFailureSite::RemoteLightAllocation)) {
    return nullptr;
  }

  lightlist_build::Spec spec;
  spec.population = lightlist_build::PopulationKind::SingleLight;
  spec.length = 1;
  spec.numLights = 1;
  spec.lifeMillis = descriptor.lifeMillis;
  spec.style.order = LIST_ORDER_RANDOM;
  spec.style.linked = false;
  spec.style.speed = descriptor.speed;
  spec.style.model = descriptor.model;
  spec.style.behaviourFlags = descriptor.behaviourFlags;
  spec.style.colorChangeGroups = descriptor.colorChangeGroups;
  spec.singleBrightness = descriptor.brightness;
  spec.singleColor = ColorRGB(descriptor.colorR, descriptor.colorG, descriptor.colorB);

  lightlist_build::Policy policy;
  policy.allocateBehaviour = descriptor.hasBehaviour;
  policy.behaviourFailureSite = LightgraphAllocationFailureSite::RemoteBehaviourAllocation;
  policy.listFailureSite = LightgraphAllocationFailureSite::RemoteListAllocation;
  policy.lightFailureSite = LightgraphAllocationFailureSite::RemoteLightAllocation;
  policy.exceptionFailureSite = LightgraphAllocationFailureSite::RemoteLightAllocation;
  return lightlist_build::buildLightList(spec, policy);
}

inline LightList* buildTemplateSnapshot(const TemplateSnapshotDescriptor& descriptor,
                                        const std::vector<int64_t>& colors,
                                        const std::vector<float>& positions) {
  if (descriptor.numLights == 0 || colors.empty() || colors.size() != positions.size()) {
    return nullptr;
  }

  const uint16_t senderBodyLights = descriptor.numLights;
  const uint16_t senderLength = (descriptor.length > 0) ? descriptor.length : senderBodyLights;
  uint16_t scaledBodyLights = scaleLengthForDensity(
      senderBodyLights, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
  uint16_t scaledLength = scaleLengthForDensity(
      senderLength, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
  if (scaledBodyLights == 0) {
    scaledBodyLights = 1;
  }
  if (scaledLength < scaledBodyLights) {
    scaledLength = scaledBodyLights;
  }
  const uint16_t scaledEdgeLights =
      (scaledLength > scaledBodyLights) ? static_cast<uint16_t>(scaledLength - scaledBodyLights) : 0;
  const RemoteSnapshotHeapBudget heapBudget =
      assessRemoteSnapshotHeapBudget(scaledLength, colors.size());
  if (!hasRemoteSnapshotTotalHeapBudget(
          heapBudget, scaledLength, LightgraphAllocationFailureSite::RemoteLightAllocation)) {
    return nullptr;
  }

  uint16_t offsetLength = scaledLength;
  if (offsetLength > 32767) {
    offsetLength = 32767;
  }
  const int16_t scaledPositionOffset = -static_cast<int16_t>(offsetLength);
  const float scaledSpeed = scaleSpeedForDensity(
      descriptor.speed, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);

  Palette palette(colors, positions);
  palette.setColorRule(descriptor.colorRule);
  palette.setInterpolationMode(descriptor.interpolationMode);
  palette.setWrapMode(descriptor.wrapMode);
  palette.setSegmentation((descriptor.segmentation >= 0.0f) ? descriptor.segmentation : 0.0f);

  lightlist_build::Spec spec;
  spec.population = lightlist_build::PopulationKind::DenseSnapshot;
  spec.numLights = scaledLength;
  spec.length = scaledLength;
  spec.trail = scaledEdgeLights;
  spec.positionOffset = scaledPositionOffset;
  spec.lifeMillis = descriptor.lifeMillis;
  spec.durationMillis = descriptor.duration;
  spec.style.order = LIST_ORDER_SEQUENTIAL;
  spec.style.head = (descriptor.head <= LIST_HEAD_BACK) ? static_cast<ListHead>(descriptor.head) : LIST_HEAD_FRONT;
  spec.style.linked = descriptor.linked;
  spec.style.speed = scaledSpeed;
  spec.style.easeIndex = descriptor.easeIndex;
  spec.style.fadeSpeed = descriptor.fadeSpeed;
  spec.style.fadeThresh = descriptor.fadeThresh;
  spec.style.fadeEaseIndex = descriptor.fadeEaseIndex;
  spec.style.minBri = descriptor.minBri;
  spec.style.maxBri = descriptor.maxBri;
  spec.style.blendMode =
      (descriptor.blendMode <= BLEND_PIN_LIGHT) ? static_cast<BlendMode>(descriptor.blendMode) : BLEND_NORMAL;
  spec.style.behaviourFlags = descriptor.behaviourFlags;
  spec.style.colorChangeGroups = descriptor.colorChangeGroups;
  spec.style.model = descriptor.model;
  spec.style.palette = palette;

  lightlist_build::Policy policy;
  policy.allocation = heapBudget.hasContiguousLightBlock
      ? lightlist_build::AllocationMode::ContiguousLights
      : lightlist_build::AllocationMode::DefaultHeap;
  policy.allocateBehaviour = descriptor.hasBehaviour;
  policy.behaviourFailureSite = LightgraphAllocationFailureSite::RemoteBehaviourAllocation;
  policy.listFailureSite = LightgraphAllocationFailureSite::RemoteListAllocation;
  policy.lightFailureSite = LightgraphAllocationFailureSite::RemoteLightAllocation;
  policy.exceptionFailureSite = LightgraphAllocationFailureSite::RemoteLightAllocation;
  LightList* list = lightlist_build::buildLightList(spec, policy);
  if (list != nullptr || policy.allocation != lightlist_build::AllocationMode::ContiguousLights) {
    return list;
  }

  // Heap shape can change between the preflight check and allocation.
  policy.allocation = lightlist_build::AllocationMode::DefaultHeap;
  return lightlist_build::buildLightList(spec, policy);
}

inline LightList* buildSequentialSnapshot(const SequentialSnapshotDescriptor& descriptor,
                                          const std::vector<SequentialEntry>& entries) {
  if (descriptor.numLights == 0) {
    return nullptr;
  }

  const uint16_t senderBodyLights = descriptor.numLights;
  uint16_t senderLength = senderBodyLights;
  const int32_t senderOffsetAbs = std::abs(static_cast<int32_t>(descriptor.positionOffset));
  if (senderOffsetAbs > 0 && senderOffsetAbs <= std::numeric_limits<uint16_t>::max()) {
    senderLength = static_cast<uint16_t>(std::max<int32_t>(senderBodyLights, senderOffsetAbs));
  }

  uint16_t scaledBodyLights = scaleLengthForDensity(
      senderBodyLights, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
  uint16_t scaledLength = scaleLengthForDensity(
      senderLength, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
  if (scaledBodyLights == 0) {
    scaledBodyLights = 1;
  }
  if (scaledLength < scaledBodyLights) {
    scaledLength = scaledBodyLights;
  }
  const uint16_t scaledEdgeLights =
      (scaledLength > scaledBodyLights) ? static_cast<uint16_t>(scaledLength - scaledBodyLights) : 0;
  const RemoteSnapshotHeapBudget heapBudget = assessRemoteSnapshotHeapBudget(scaledLength, 0);
  if (!hasRemoteSnapshotTotalHeapBudget(
          heapBudget, scaledLength, LightgraphAllocationFailureSite::RemoteLightAllocation)) {
    return nullptr;
  }

  uint16_t offsetLength = scaledLength;
  if (offsetLength > 32767) {
    offsetLength = 32767;
  }
  const int16_t scaledPositionOffset = -static_cast<int16_t>(offsetLength);

  lightlist_build::Spec spec;
  spec.population = lightlist_build::PopulationKind::SparseSnapshot;
  spec.numLights = scaledLength;
  spec.length = scaledLength;
  spec.trail = scaledEdgeLights;
  spec.positionOffset = scaledPositionOffset;
  spec.lifeMillis = descriptor.lifeMillis;
  spec.style.order = LIST_ORDER_SEQUENTIAL;
  spec.style.linked = true;
  spec.style.speed = descriptor.speed;
  spec.style.model = descriptor.model;
  spec.style.behaviourFlags = descriptor.behaviourFlags;
  spec.style.colorChangeGroups = descriptor.colorChangeGroups;

  for (size_t i = 0; i < entries.size(); i++) {
    const SequentialEntry& entry = entries[i];
    if (entry.lightIdx >= senderLength) {
      continue;
    }

    lightlist_build::SparseEntry sparseEntry;
    sparseEntry.lightIdx = mapLightIndexByDensity(entry.lightIdx, senderLength, scaledLength);
    sparseEntry.brightness = entry.brightness;
    sparseEntry.color = ColorRGB(entry.colorR, entry.colorG, entry.colorB);
    spec.sparseEntries.push_back(sparseEntry);
  }

  lightlist_build::Policy policy;
  policy.allocateBehaviour = descriptor.hasBehaviour;
  policy.behaviourFailureSite = LightgraphAllocationFailureSite::RemoteBehaviourAllocation;
  policy.listFailureSite = LightgraphAllocationFailureSite::RemoteListAllocation;
  policy.lightFailureSite = LightgraphAllocationFailureSite::RemoteLightAllocation;
  policy.exceptionFailureSite = LightgraphAllocationFailureSite::RemoteLightAllocation;
  return lightlist_build::buildLightList(spec, policy);
}

}  // namespace remote_snapshot
