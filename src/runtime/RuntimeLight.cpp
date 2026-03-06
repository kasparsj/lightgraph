#include "RuntimeLight.h"
#include "LightList.h"
#include "../topology/Connection.h"
#include "../topology/Port.h"
#include "../../vendor/ofxEasing/ofxEasing.h"
#include "../Globals.h"

LightgraphRuntimeContext& RuntimeLight::runtimeContext() {
  return (list != nullptr) ? list->runtimeContext() : lightgraphDefaultRuntimeContext();
}

const LightgraphRuntimeContext& RuntimeLight::runtimeContext() const {
  return (list != nullptr) ? list->runtimeContext() : lightgraphDefaultRuntimeContext();
}

void RuntimeLight::resetPixels() {
  pixel1 = -1;
#if LIGHTGRAPH_FRACTIONAL_RENDERING
  pixel1Weight = FULL_BRIGHTNESS;
  pixel2 = -1;
  pixel2Weight = 0;
#endif
}

void RuntimeLight::setRenderedPixel(uint16_t pixel) {
  pixel1 = static_cast<int16_t>(pixel);
#if LIGHTGRAPH_FRACTIONAL_RENDERING
  pixel1Weight = FULL_BRIGHTNESS;
  pixel2 = -1;
  pixel2Weight = 0;
#endif
}

#if LIGHTGRAPH_FRACTIONAL_RENDERING
void RuntimeLight::setRenderedPixelWeighted(uint16_t pixel, uint8_t weight) {
  pixel1 = static_cast<int16_t>(pixel);
  pixel1Weight = weight;
  pixel2 = -1;
  pixel2Weight = 0;
}

void RuntimeLight::setRenderedPixels(uint16_t primaryPixel,
                                     uint16_t secondaryPixel,
                                     uint8_t secondaryWeight) {
  pixel1 = static_cast<int16_t>(primaryPixel);
  pixel1Weight = static_cast<uint8_t>(FULL_BRIGHTNESS - secondaryWeight);
  if (secondaryWeight == 0 || primaryPixel == secondaryPixel) {
    pixel1Weight = FULL_BRIGHTNESS;
    pixel2 = -1;
    pixel2Weight = 0;
    return;
  }

  pixel2 = static_cast<int16_t>(secondaryPixel);
  pixel2Weight = secondaryWeight;
}

void RuntimeLight::setRenderedPixelsWeighted(uint16_t primaryPixel,
                                             uint8_t primaryWeight,
                                             uint16_t secondaryPixel,
                                             uint8_t secondaryWeight) {
  pixel1 = static_cast<int16_t>(primaryPixel);
  pixel1Weight = primaryWeight;
  if (secondaryWeight == 0 || primaryPixel == secondaryPixel) {
    pixel2 = -1;
    pixel2Weight = 0;
    return;
  }

  pixel2 = static_cast<int16_t>(secondaryPixel);
  pixel2Weight = secondaryWeight;
}

bool RuntimeLight::hasSecondaryPixel() const {
  return pixel2 >= 0 && pixel2Weight > 0;
}

uint8_t RuntimeLight::getPrimaryPixelWeight() const {
  return pixel1Weight;
}
#endif

Port* RuntimeLight::getOutPort(uint8_t intersectionId) const {
  for (uint8_t i=0; i<OUT_PORTS_MEMORY; i++) {
    if (outPortsInt[i] == intersectionId) {
      return outPorts[i];
    }
  }
  return NULL;
}

void RuntimeLight::setOutPort(Port* const port, int8_t intersectionId) {
  outPort = port;
  if (intersectionId > -1) {
    for (uint8_t i=(OUT_PORTS_MEMORY-1); i>0; i--) {
      outPorts[i] = outPorts[i-1];
      outPortsInt[i] = outPortsInt[i-1];
    }
    outPorts[0] = port;
    outPortsInt[0] = intersectionId;
  }
}

void RuntimeLight::update() {
    if (owner) {
        owner->update(this);
    }
    brightness = getBrightness();
}

uint8_t RuntimeLight::getBrightness() const {
    uint16_t value = bri % 511;
    value = (value > 255 ? 511 - value : value);

    const uint8_t fadeThresh = (list != NULL ? list->fadeThresh : 0);
    const int16_t fadeRange = 255 - static_cast<int16_t>(fadeThresh);
    if (fadeRange <= 0) {
        return 0;
    }

    const float adjusted = (static_cast<float>(static_cast<int16_t>(value) - static_cast<int16_t>(fadeThresh)) /
                            static_cast<float>(fadeRange)) * 511.f;
    if (adjusted > 0.f) {
        const float clamped = (adjusted > 511.f ? 511.f : adjusted);
        if (list != NULL) {
            return ofxeasing::map(clamped, 0, 511, list->minBri, maxBri, list->fadeEase);
        }
        return ofxeasing::map(clamped, 0, 511, 0, maxBri, ofxeasing::linear::easeNone);
    }
    return 0;
}

ColorRGB RuntimeLight::getPixelColorAt(int16_t pixel) const {
    if (brightness == 255) {
        return list->getColor(pixel);
    }
    return list->getColor(pixel).dim(brightness);
}

ColorRGB RuntimeLight::getPixelColor() const {
    return getPixelColorAt(pixel1);
}

uint16_t RuntimeLight::writePixels(uint16_t* buffer, size_t capacity) const {
  if (pixel1 < 0 || buffer == NULL || capacity == 0) {
    return 0;
  }

  const Behaviour *behaviour = getBehaviour();
  if (behaviour != NULL && behaviour->renderSegment()) {
    return setSegmentPixels(buffer, capacity);
  }
  if (behaviour != NULL && behaviour->fillEase()) {
    return setLinkPixels(buffer, capacity);
  }
  return setPixel1(buffer, capacity);
}

uint16_t RuntimeLight::setPixel1(uint16_t* buffer, size_t capacity) const {
    if (buffer == NULL || capacity < 2) {
        return 0;
    }
    buffer[0] = 1;
    buffer[1] = static_cast<uint16_t>(pixel1);
    return buffer[0];
}

uint16_t RuntimeLight::setSegmentPixels(uint16_t* buffer, size_t capacity) const {
    if (outPort != NULL) {
        const uint16_t numPixels = outPort->connection->numLeds;
        const uint32_t required = static_cast<uint32_t>(numPixels) + 3U;
        if (required > capacity) {
            return setPixel1(buffer, capacity);
        }
        buffer[0] = static_cast<uint16_t>(numPixels + 2);
        buffer[1] = outPort->connection->getFromPixel();
        buffer[2] = outPort->connection->getToPixel();
        for (uint16_t i=0; i<numPixels; i++) {
            buffer[i + 3] = outPort->connection->getPixel(i);
        }
        return buffer[0];
    }
    return setPixel1(buffer, capacity);
}

uint16_t RuntimeLight::setLinkPixels(uint16_t* buffer, size_t capacity) const {
    RuntimeLight* prev = getPrev();
    if (prev != NULL && owner == prev->owner) {
        uint16_t numPixels = abs(pixel1 - prev->pixel1);
        const uint32_t required = static_cast<uint32_t>(numPixels) + 1U;
        if (required > capacity) {
            return setPixel1(buffer, capacity);
        }
        buffer[0] = numPixels;
        for (uint16_t i=1; i<numPixels+1; i++) {
            buffer[i] = static_cast<uint16_t>(
                pixel1 + (i-1) * (pixel1 < prev->pixel1 ? 1 : -1));
        }
        return buffer[0];
    }
    return setPixel1(buffer, capacity);
}

void RuntimeLight::nextFrame() {
  bri = list->getBri(this);
  position = list->getPosition(this);
}

bool RuntimeLight::shouldExpire() const {
  if (list->lifeMillis >= INFINITE_DURATION) {
    return false;
  }
  return runtimeContext().nowMillis >= (list->lifeMillis + lifeMillis) &&
         (list->fadeSpeed == 0 || brightness == 0);
}

RuntimeLight* RuntimeLight::getPrev() const {
    if (list == NULL || list->lights == NULL || list->numLights == 0 || idx == 0 || idx >= list->numLights) {
      return NULL;
    }
    return (*list)[idx - 1];
}

RuntimeLight* RuntimeLight::getNext() const {
    if (list == NULL || list->lights == NULL || list->numLights == 0) {
      return NULL;
    }
    const uint16_t nextIdx = idx + 1;
    return (nextIdx < list->numLights) ? (*list)[nextIdx] : NULL;
}

float RuntimeLight::getSpeed() const {
    return list->speed;
}

ofxeasing::function RuntimeLight::getEasing() const {
    return list->ease;
}

uint32_t RuntimeLight::getLife() const {
    return list->lifeMillis;
}

ColorRGB RuntimeLight::getColor() const {
    return list->getColor();
}

const Model* RuntimeLight::getModel() const {
    return list->model;
}

const Behaviour* RuntimeLight::getBehaviour() const {
    return list->behaviour;
}

uint16_t RuntimeLight::getListId() const {
    return list->id;
}

float RuntimeLight::getFadeSpeed() const {
    return list->fadeSpeed;
}
