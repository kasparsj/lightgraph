#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

#include "Behaviour.h"
#include "EmitParams.h"
#include "Light.h"
#include "LightList.h"
#include "../Globals.h"

class Model;

namespace lightlist_build {

enum class PopulationKind : uint8_t {
  DerivedFromLength = 0,
  DenseSnapshot = 1,
  SparseSnapshot = 2,
  SingleLight = 3,
};

enum class AllocationMode : uint8_t {
  DefaultHeap = 0,
  ContiguousLights = 1,
};

struct SparseEntry {
  uint16_t lightIdx = 0;
  uint8_t brightness = 0;
  ColorRGB color;
};

struct StyleSpec {
  Model* model = nullptr;
  uint16_t noteId = 0;
  float speed = DEFAULT_SPEED;
  uint8_t easeIndex = EASE_NONE;
  uint8_t fadeSpeed = 0;
  uint8_t fadeThresh = 0;
  uint8_t fadeEaseIndex = EASE_NONE;
  uint8_t minBri = 0;
  uint8_t maxBri = 255;
  ListOrder order = LIST_ORDER_SEQUENTIAL;
  ListHead head = LIST_HEAD_FRONT;
  bool linked = true;
  bool visible = true;
  bool editable = false;
  BlendMode blendMode = BLEND_NORMAL;
  uint16_t behaviourFlags = 0;
  uint8_t colorChangeGroups = 0;
  Palette palette;
};

struct Spec {
  StyleSpec style;
  PopulationKind population = PopulationKind::DerivedFromLength;
  uint16_t numLights = 0;
  uint16_t length = 0;
  uint16_t trail = 0;
  int16_t positionOffset = 0;
  uint32_t lifeMillis = 0;
  uint32_t durationMillis = 0;
  uint8_t singleBrightness = 0;
  ColorRGB singleColor;
  std::vector<SparseEntry> sparseEntries;
};

struct Policy {
  AllocationMode allocation = AllocationMode::DefaultHeap;
  bool allocateBehaviour = false;
  LightgraphAllocationFailureSite behaviourFailureSite = LightgraphAllocationFailureSite::Unknown;
  LightgraphAllocationFailureSite listFailureSite = LightgraphAllocationFailureSite::Unknown;
  LightgraphAllocationFailureSite lightFailureSite = LightgraphAllocationFailureSite::Unknown;
  LightgraphAllocationFailureSite exceptionFailureSite = LightgraphAllocationFailureSite::Unknown;
};

inline void reportAllocationFailure(LightgraphAllocationFailureSite site,
                                    uint16_t detail0 = 0,
                                    uint16_t detail1 = 0) {
  if (site == LightgraphAllocationFailureSite::Unknown) {
    return;
  }
  lightgraphReportAllocationFailure(site, detail0, detail1);
}

inline uint8_t sanitizePixelDensity(uint8_t density) {
  return density > 0 ? density : 1;
}

inline uint16_t scaleLengthForDensity(uint16_t sourceLength,
                                      uint8_t senderPixelDensity,
                                      uint8_t receiverPixelDensity) {
  if (sourceLength == 0) {
    return 0;
  }

  const float sender = static_cast<float>(sanitizePixelDensity(senderPixelDensity));
  const float receiver = static_cast<float>(sanitizePixelDensity(receiverPixelDensity));
  const float ratio = receiver / sender;
  const float scaled = static_cast<float>(sourceLength) * ratio;

  uint32_t rounded = static_cast<uint32_t>(std::lround(scaled));
  if (rounded == 0) {
    rounded = 1;
  }
  if (rounded > std::numeric_limits<uint16_t>::max()) {
    rounded = std::numeric_limits<uint16_t>::max();
  }
  return static_cast<uint16_t>(rounded);
}

inline float scaleSpeedForDensity(float sourceSpeed,
                                  uint8_t senderPixelDensity,
                                  uint8_t receiverPixelDensity) {
  const float sender = static_cast<float>(sanitizePixelDensity(senderPixelDensity));
  const float receiver = static_cast<float>(sanitizePixelDensity(receiverPixelDensity));
  const float ratio = receiver / sender;
  return sourceSpeed * ratio;
}

inline uint16_t mapLightIndexByDensity(uint16_t sourceIdx,
                                       uint16_t sourceCount,
                                       uint16_t targetCount) {
  if (targetCount <= 1 || sourceCount <= 1) {
    return 0;
  }
  const float sourceNorm = static_cast<float>(sourceIdx) / static_cast<float>(sourceCount - 1);
  const float mapped = sourceNorm * static_cast<float>(targetCount - 1);
  uint32_t rounded = static_cast<uint32_t>(std::lround(mapped));
  if (rounded >= targetCount) {
    rounded = static_cast<uint32_t>(targetCount - 1);
  }
  return static_cast<uint16_t>(rounded);
}

inline uint32_t addLifeDelayClamped(uint32_t baseLifeMillis, uint32_t delayMillis) {
  if (baseLifeMillis >= INFINITE_DURATION || delayMillis >= INFINITE_DURATION) {
    return INFINITE_DURATION;
  }
  if (baseLifeMillis >= static_cast<uint32_t>(INFINITE_DURATION - delayMillis)) {
    return INFINITE_DURATION;
  }
  return static_cast<uint32_t>(baseLifeMillis + delayMillis);
}

inline Spec makeSpecFromEmitParams(const EmitParams& params, uint16_t resolvedLength) {
  Spec spec;
  spec.population = PopulationKind::DerivedFromLength;
  spec.length = resolvedLength;
  spec.durationMillis = params.getDuration();
  spec.style.noteId = params.noteId;
  spec.style.speed = params.getSpeed();
  spec.style.easeIndex = params.ease;
  spec.style.fadeSpeed = params.fadeSpeed;
  spec.style.fadeThresh = params.fadeThresh;
  spec.style.fadeEaseIndex = params.fadeEase;
  spec.style.minBri = params.minBri;
  spec.style.maxBri = params.getMaxBri();
  spec.style.order = params.order;
  spec.style.head = params.head;
  spec.style.linked = params.linked;
  spec.style.behaviourFlags = params.behaviourFlags;
  spec.style.colorChangeGroups = params.colorChangeGroups;
  spec.style.palette = params.palette;
  spec.trail = (params.speed == 0) ? params.trail : params.getSpeedTrail(spec.style.speed, resolvedLength);
  return spec;
}

inline bool applyBehaviour(LightList* list, const Spec& spec, const Policy& policy) {
  if (list == nullptr) {
    return false;
  }
  if (!policy.allocateBehaviour) {
    return true;
  }
  list->behaviour = new (std::nothrow) Behaviour(spec.style.behaviourFlags, spec.style.colorChangeGroups);
  if (list->behaviour == nullptr) {
    reportAllocationFailure(
        policy.behaviourFailureSite,
        spec.style.behaviourFlags,
        spec.style.colorChangeGroups);
    return false;
  }
  return true;
}

inline void applyStyle(LightList* list, const Spec& spec) {
  list->order = spec.style.order;
  list->head = spec.style.head;
  list->linked = spec.style.linked;
  list->noteId = spec.style.noteId;
  list->minBri = spec.style.minBri;
  list->maxBri = spec.style.maxBri;
  list->visible = spec.style.visible;
  list->editable = spec.style.editable;
  list->blendMode = spec.style.blendMode;
  list->emitter = nullptr;
  list->model = spec.style.model;
  list->clearExternalBatchForwardState();
  list->setSpeed(spec.style.speed, spec.style.easeIndex);
  list->setFade(spec.style.fadeSpeed, spec.style.fadeThresh, spec.style.fadeEaseIndex);
}

inline Light* allocateExplicitLight(LightList* list,
                                    uint16_t slot,
                                    const Spec& spec,
                                    const Policy& policy,
                                    uint32_t lifeMillis,
                                    uint16_t lightIdx,
                                    uint8_t brightness) {
  Light* light = nullptr;
  if (policy.allocation == AllocationMode::ContiguousLights) {
    light = list->createContiguousLight(slot, spec.style.speed, lifeMillis, lightIdx, brightness);
  } else {
    light = new (std::nothrow) Light(list, spec.style.speed, lifeMillis, lightIdx, brightness);
    if (light != nullptr) {
      (*list)[slot] = light;
    }
  }
  if (light == nullptr) {
    reportAllocationFailure(policy.lightFailureSite, slot, list != nullptr ? list->numLights : 0);
  }
  return light;
}

inline bool buildDerivedFromLength(LightList* list, const Spec& spec) {
  const uint16_t bodyLights =
      (spec.length > spec.trail) ? static_cast<uint16_t>(spec.length - spec.trail) : static_cast<uint16_t>(1);
  list->length = spec.length;
  list->numLights = bodyLights;
  list->duration = spec.durationMillis;
  list->palette = spec.style.palette;
  list->setLeadTrail(spec.trail);
  list->numEmitted = 0;
  list->numSplits = 0;
  list->setup(bodyLights, spec.style.maxBri);
  list->setDuration(spec.durationMillis);
  list->setPalette(spec.style.palette);
  return true;
}

inline bool buildDenseSnapshot(LightList* list, const Spec& spec, const Policy& policy) {
  const uint16_t targetLights = (spec.numLights > 0) ? spec.numLights : static_cast<uint16_t>(1);
  list->length = (spec.length >= targetLights) ? spec.length : targetLights;
  list->duration = spec.durationMillis;
  list->lifeMillis = spec.lifeMillis;

  if (policy.allocation == AllocationMode::ContiguousLights) {
    if (!list->initContiguousLights(targetLights)) {
      return false;
    }
  } else {
    list->init(targetLights);
  }
  if (list->numLights != targetLights || list->lights == nullptr) {
    return false;
  }

  const uint16_t trail = (list->length > list->numLights) ? static_cast<uint16_t>(list->length - list->numLights) : 0;
  list->setLeadTrail(trail);

  for (uint16_t i = 0; i < list->numLights; i++) {
    const uint16_t lightIdx = list->linked ? i : 0;
    const float bri = static_cast<float>(list->maxBri) * list->getBriMult(i);
    const uint8_t clampedBri = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, bri)));
    Light* created = allocateExplicitLight(list, i, spec, policy, list->lifeMillis, lightIdx, clampedBri);
    if (created == nullptr) {
      return false;
    }

    float position = (list->speed != 0.0f) ? static_cast<float>(i) * -1.0f
                                           : static_cast<float>(list->numLights - 1 - i);
    position += static_cast<float>(spec.positionOffset);
    created->position = position;

    if (list->order == LIST_ORDER_SEQUENTIAL && list->speed > 0.0f) {
      const uint32_t delayFrames = static_cast<uint32_t>(std::ceil((1.0f / list->speed) * static_cast<float>(i)));
      created->lifeMillis = addLifeDelayClamped(
          list->lifeMillis,
          static_cast<uint32_t>(delayFrames * EmitParams::frameMs()));
    } else {
      created->lifeMillis = list->lifeMillis;
    }
  }

  list->setPalette(spec.style.palette);
  list->numEmitted = list->numLights;
  return true;
}

inline bool buildSparseSnapshot(LightList* list, const Spec& spec, const Policy& policy) {
  const uint16_t targetLights = (spec.numLights > 0) ? spec.numLights : static_cast<uint16_t>(1);
  list->length = (spec.length >= targetLights) ? spec.length : targetLights;
  list->lifeMillis = spec.lifeMillis;
  list->init(targetLights);
  if (list->numLights != targetLights || list->lights == nullptr) {
    return false;
  }
  list->numEmitted = targetLights;

  for (size_t i = 0; i < spec.sparseEntries.size(); i++) {
    const SparseEntry& entry = spec.sparseEntries[i];
    if (entry.lightIdx >= list->numLights) {
      continue;
    }

    RuntimeLight* existing = (*list)[entry.lightIdx];
    if (existing != nullptr) {
      if (existing->getBrightness() >= entry.brightness) {
        continue;
      }
      list->releaseOwnedLight(existing);
    }

    uint32_t lightLifeMillis = list->lifeMillis;
    if (list->speed > 0.0f) {
      const uint32_t delayFrames =
          static_cast<uint32_t>(std::ceil((1.0f / list->speed) * static_cast<float>(entry.lightIdx)));
      lightLifeMillis = addLifeDelayClamped(
          list->lifeMillis,
          static_cast<uint32_t>(delayFrames * EmitParams::frameMs()));
    }

    Light* light = allocateExplicitLight(
        list,
        entry.lightIdx,
        spec,
        policy,
        lightLifeMillis,
        entry.lightIdx,
        entry.brightness);
    if (light == nullptr) {
      return false;
    }
    light->setColor(entry.color);
    light->position = static_cast<float>(spec.positionOffset) - static_cast<float>(entry.lightIdx);
  }

  return true;
}

inline bool buildSingleLight(LightList* list, const Spec& spec, const Policy& policy) {
  list->length = 1;
  list->lifeMillis = spec.lifeMillis;
  list->init(1);
  if (list->numLights == 0 || list->lights == nullptr) {
    return false;
  }
  list->numEmitted = 1;

  Light* light = allocateExplicitLight(list, 0, spec, policy, spec.lifeMillis, 0, spec.singleBrightness);
  if (light == nullptr) {
    return false;
  }
  light->setColor(spec.singleColor);
  return true;
}

inline LightList* buildLightList(const Spec& spec, const Policy& policy) {
  LightList* list = nullptr;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
  try {
#endif
    list = new (std::nothrow) LightList();
    if (list == nullptr) {
      reportAllocationFailure(policy.listFailureSite, spec.numLights, spec.length);
      return nullptr;
    }

    applyStyle(list, spec);
    if (!applyBehaviour(list, spec, policy)) {
      delete list;
      return nullptr;
    }

    bool built = false;
    switch (spec.population) {
      case PopulationKind::DerivedFromLength:
        built = buildDerivedFromLength(list, spec);
        break;
      case PopulationKind::DenseSnapshot:
        built = buildDenseSnapshot(list, spec, policy);
        break;
      case PopulationKind::SparseSnapshot:
        built = buildSparseSnapshot(list, spec, policy);
        break;
      case PopulationKind::SingleLight:
        built = buildSingleLight(list, spec, policy);
        break;
      default:
        break;
    }

    if (!built) {
      delete list;
      return nullptr;
    }
    return list;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
  } catch (const std::bad_alloc&) {
    delete list;
    reportAllocationFailure(policy.exceptionFailureSite, spec.numLights, spec.length);
    return nullptr;
  } catch (...) {
    delete list;
    reportAllocationFailure(policy.exceptionFailureSite, spec.numLights, spec.length);
    return nullptr;
  }
#endif
}

}  // namespace lightlist_build
