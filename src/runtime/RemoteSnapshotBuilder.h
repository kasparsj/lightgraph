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
  int8_t interMode = 1;
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

inline uint8_t sanitizePixelDensity(uint8_t density) {
  return density > 0 ? density : 1;
}

inline uint16_t scaleLengthForDensity(uint16_t sourceLength, uint8_t senderPixelDensity, uint8_t receiverPixelDensity) {
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

inline float scaleSpeedForDensity(float sourceSpeed, uint8_t senderPixelDensity, uint8_t receiverPixelDensity) {
  const float sender = static_cast<float>(sanitizePixelDensity(senderPixelDensity));
  const float receiver = static_cast<float>(sanitizePixelDensity(receiverPixelDensity));
  const float ratio = receiver / sender;
  return sourceSpeed * ratio;
}

inline uint16_t mapLightIndexByDensity(uint16_t sourceIdx, uint16_t sourceCount, uint16_t targetCount) {
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

inline void applyLightListBehaviour(LightList* lightList, bool hasBehaviour, uint16_t flags,
                                    uint8_t colorChangeGroups) {
  if (lightList == nullptr) {
    return;
  }
  if (!hasBehaviour) {
    if (lightList->behaviour != nullptr) {
      delete lightList->behaviour;
      lightList->behaviour = nullptr;
    }
    return;
  }
  if (lightList->behaviour == nullptr) {
    lightList->behaviour = new (std::nothrow) Behaviour(flags, colorChangeGroups);
    if (lightList->behaviour == nullptr) {
      lightgraphReportAllocationFailure(
          LightgraphAllocationFailureSite::RemoteBehaviourAllocation,
          flags,
          colorChangeGroups);
      return;
    }
  } else {
    lightList->behaviour->flags = flags;
    lightList->behaviour->colorChangeGroups = colorChangeGroups;
  }
}

inline LightList* buildSingleLightSnapshot(const SingleSnapshotDescriptor& descriptor) {
  LightList* list = new (std::nothrow) LightList();
  if (list == nullptr) {
    lightgraphReportAllocationFailure(
        LightgraphAllocationFailureSite::RemoteListAllocation,
        1,
        0);
    return nullptr;
  }
  list->order = LIST_ORDER_RANDOM;
  list->linked = false;
  list->speed = descriptor.speed;
  list->lifeMillis = descriptor.lifeMillis;
  list->length = 1;
  list->visible = true;
  list->editable = false;
  list->emitter = nullptr;
  list->model = descriptor.model;
  applyLightListBehaviour(list, descriptor.hasBehaviour, descriptor.behaviourFlags, descriptor.colorChangeGroups);
  list->init(1);
  if (list->numLights == 0 || list->lights == nullptr) {
    delete list;
    return nullptr;
  }
  list->numEmitted = 1;

  Light* light = new (std::nothrow) Light(list, descriptor.speed, descriptor.lifeMillis, 0, descriptor.brightness);
  if (light == nullptr) {
    lightgraphReportAllocationFailure(
        LightgraphAllocationFailureSite::RemoteLightAllocation,
        0,
        1);
    delete list;
    return nullptr;
  }
  light->setColor(ColorRGB(descriptor.colorR, descriptor.colorG, descriptor.colorB));
  (*list)[0] = light;
  return list;
}

inline LightList* buildTemplateSnapshot(const TemplateSnapshotDescriptor& descriptor,
                                        const std::vector<int64_t>& colors,
                                        const std::vector<float>& positions) {
  if (descriptor.numLights == 0 || colors.empty() || colors.size() != positions.size()) {
    return nullptr;
  }

  const uint16_t senderNumLights = descriptor.numLights;
  const uint16_t senderLength = (descriptor.length > 0) ? descriptor.length : senderNumLights;
  uint16_t scaledNumLights = scaleLengthForDensity(
      senderNumLights, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
  uint16_t scaledLength = scaleLengthForDensity(
      senderLength, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
  if (scaledNumLights == 0) {
    scaledNumLights = 1;
  }
  if (scaledLength < scaledNumLights) {
    scaledLength = scaledNumLights;
  }

  uint16_t offsetLength = scaledLength;
  if (offsetLength > 32767) {
    offsetLength = 32767;
  }
  const int16_t scaledPositionOffset = -static_cast<int16_t>(offsetLength);
  const float scaledSpeed = scaleSpeedForDensity(
      descriptor.speed, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);

  LightList* list = new (std::nothrow) LightList();
  if (list == nullptr) {
    lightgraphReportAllocationFailure(
        LightgraphAllocationFailureSite::RemoteListAllocation,
        scaledNumLights,
        scaledLength);
    return nullptr;
  }
  list->order = LIST_ORDER_SEQUENTIAL;
  list->head = (descriptor.head <= LIST_HEAD_BACK) ? static_cast<ListHead>(descriptor.head) : LIST_HEAD_FRONT;
  list->linked = descriptor.linked;
  list->length = scaledLength;
  list->visible = true;
  list->editable = false;
  list->emitter = nullptr;
  list->duration = descriptor.duration;
  list->minBri = descriptor.minBri;
  list->maxBri = descriptor.maxBri;
  list->blendMode =
      (descriptor.blendMode <= BLEND_PIN_LIGHT) ? static_cast<BlendMode>(descriptor.blendMode) : BLEND_NORMAL;
  list->setSpeed(scaledSpeed, descriptor.easeIndex);
  list->setFade(descriptor.fadeSpeed, descriptor.fadeThresh, descriptor.fadeEaseIndex);
  list->lifeMillis = descriptor.lifeMillis;
  applyLightListBehaviour(list, descriptor.hasBehaviour, descriptor.behaviourFlags, descriptor.colorChangeGroups);
  list->model = descriptor.model;

  list->init(scaledNumLights);
  if (list->numLights != scaledNumLights || list->lights == nullptr) {
    delete list;
    return nullptr;
  }
  const uint16_t numTrail =
      (scaledLength > list->numLights) ? static_cast<uint16_t>(scaledLength - list->numLights) : 0;
  list->setLeadTrail(numTrail);

  for (uint16_t i = 0; i < list->numLights; i++) {
    const uint16_t lightIdx = list->linked ? i : 0;
    const float bri = static_cast<float>(list->maxBri) * list->getBriMult(i);
    const uint8_t clampedBri = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, bri)));
    Light* createdLight = new (std::nothrow) Light(list, list->speed, list->lifeMillis, lightIdx, clampedBri);
    if (createdLight == nullptr) {
      lightgraphReportAllocationFailure(
          LightgraphAllocationFailureSite::RemoteLightAllocation,
          i,
          list->numLights);
      delete list;
      return nullptr;
    }
    RuntimeLight* created = createdLight;
    (*list)[i] = created;

    float position = (list->speed != 0.0f) ? static_cast<float>(i) * -1.0f
                                           : static_cast<float>(list->numLights - 1 - i);
    position += static_cast<float>(scaledPositionOffset);
    created->position = position;

    if (list->order == LIST_ORDER_SEQUENTIAL && list->speed > 0.0f) {
      const uint32_t delayFrames = static_cast<uint32_t>(std::ceil((1.0f / list->speed) * static_cast<float>(i)));
      created->lifeMillis = addLifeDelayClamped(
          list->lifeMillis, static_cast<uint32_t>(delayFrames * EmitParams::frameMs()));
    } else {
      created->lifeMillis = list->lifeMillis;
    }
  }

  Palette palette(colors, positions);
  palette.setColorRule(descriptor.colorRule);
  palette.setInterMode(descriptor.interMode);
  palette.setWrapMode(descriptor.wrapMode);
  palette.setSegmentation((descriptor.segmentation >= 0.0f) ? descriptor.segmentation : 0.0f);
  list->setPalette(palette);
  list->numEmitted = list->numLights;
  return list;
}

inline LightList* buildSequentialSnapshot(const SequentialSnapshotDescriptor& descriptor,
                                          const std::vector<SequentialEntry>& entries) {
  if (descriptor.numLights == 0) {
    return nullptr;
  }

  const uint16_t senderNumLights = descriptor.numLights;
  uint16_t senderLength = senderNumLights;
  const int32_t senderOffsetAbs = std::abs(static_cast<int32_t>(descriptor.positionOffset));
  if (senderOffsetAbs > 0 && senderOffsetAbs <= std::numeric_limits<uint16_t>::max()) {
    senderLength = static_cast<uint16_t>(std::max<int32_t>(senderNumLights, senderOffsetAbs));
  }

  uint16_t scaledNumLights = scaleLengthForDensity(
      senderNumLights, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
  uint16_t scaledLength = scaleLengthForDensity(
      senderLength, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
  if (scaledNumLights == 0) {
    scaledNumLights = 1;
  }
  if (scaledLength < scaledNumLights) {
    scaledLength = scaledNumLights;
  }

  uint16_t offsetLength = scaledLength;
  if (offsetLength > 32767) {
    offsetLength = 32767;
  }
  const int16_t scaledPositionOffset = -static_cast<int16_t>(offsetLength);

  LightList* list = new (std::nothrow) LightList();
  if (list == nullptr) {
    lightgraphReportAllocationFailure(
        LightgraphAllocationFailureSite::RemoteListAllocation,
        scaledNumLights,
        scaledLength);
    return nullptr;
  }
  list->order = LIST_ORDER_SEQUENTIAL;
  list->linked = true;
  list->speed = descriptor.speed;
  list->lifeMillis = descriptor.lifeMillis;
  list->length = scaledLength;
  list->visible = true;
  list->editable = false;
  list->emitter = nullptr;
  list->model = descriptor.model;
  applyLightListBehaviour(list, descriptor.hasBehaviour, descriptor.behaviourFlags, descriptor.colorChangeGroups);
  list->init(scaledNumLights);
  if (list->numLights != scaledNumLights || list->lights == nullptr) {
    delete list;
    return nullptr;
  }
  list->numEmitted = scaledNumLights;

  for (size_t i = 0; i < entries.size(); i++) {
    const SequentialEntry& entry = entries[i];
    if (entry.lightIdx >= senderNumLights) {
      continue;
    }

    const uint16_t scaledLightIdx = mapLightIndexByDensity(entry.lightIdx, senderNumLights, list->numLights);
    RuntimeLight* existing = (*list)[scaledLightIdx];
    if (existing != nullptr) {
      if (existing->getBrightness() >= entry.brightness) {
        continue;
      }
      delete existing;
      (*list)[scaledLightIdx] = nullptr;
    }

    uint32_t lightLifeMillis = list->lifeMillis;
    if (list->speed > 0.0f) {
      const uint32_t delayFrames =
          static_cast<uint32_t>(std::ceil((1.0f / list->speed) * static_cast<float>(scaledLightIdx)));
      lightLifeMillis = addLifeDelayClamped(
          list->lifeMillis, static_cast<uint32_t>(delayFrames * EmitParams::frameMs()));
    }
    Light* light = new (std::nothrow) Light(list, descriptor.speed, lightLifeMillis, scaledLightIdx, entry.brightness);
    if (light == nullptr) {
      lightgraphReportAllocationFailure(
          LightgraphAllocationFailureSite::RemoteLightAllocation,
          scaledLightIdx,
          list->numLights);
      delete list;
      return nullptr;
    }
    light->setColor(ColorRGB(entry.colorR, entry.colorG, entry.colorB));
    light->position = static_cast<float>(scaledPositionOffset) - static_cast<float>(scaledLightIdx);

    (*list)[scaledLightIdx] = light;
  }

  return list;
}

}  // namespace remote_snapshot
