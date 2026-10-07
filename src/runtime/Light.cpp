#include <math.h>
#include "Light.h"
#include "LightList.h"
#include "../Globals.h"

Light::Light(LightList *list, float speedValue, uint32_t expirationMillis, uint16_t idx,
             uint8_t maxBrightness)
    : RuntimeLight(list, idx, maxBrightness) {
    speed = speedValue;
    lifeMillis = expirationMillis;
    color = ColorRGB(255, 255, 255);
}

uint8_t Light::getBrightness() const {
  uint16_t value = bri % 511;
  value = (value > 255 ? 511 - value : value);
  const uint8_t fadeThreshold = (list != NULL ? list->fadeThresh : 0);
  const int16_t fadeRange = 255 - static_cast<int16_t>(fadeThreshold);
  if (fadeRange <= 0) {
    return 0;
  }
  const int16_t aboveThreshold = static_cast<int16_t>(value) - static_cast<int16_t>(fadeThreshold);
  if (aboveThreshold <= 0) {
    return 0;
  }
  const float normalized = static_cast<float>(aboveThreshold) / static_cast<float>(fadeRange);
  const float scaled = normalized * maxBri;
  if (scaled >= maxBri) {
    return maxBri;
  }
  return static_cast<uint8_t>(scaled);
}

ColorRGB Light::getPixelColor() const {
    return getPixelColorAt(pixel1);
}

ColorRGB Light::getPixelColorAt(int16_t /*pixel*/) const {
    if (brightness == 255) {
        return color;
    }
    return color.dim(brightness);
}

void Light::nextFrame() {
  bri = list->getBri(this);
  brightness = getBrightness();
  if (list == NULL) {
    position += lightgraphMotionDistance(runtimeContext(), speed);
  }
  else {
    position = list->getPosition(this);
  }
}

bool Light::shouldExpire() const {
  if (lifeMillis >= INFINITE_DURATION) {
    return false;
  }
  const uint8_t fadeSpeedValue = (list != nullptr) ? list->fadeSpeed : 0;
  return runtimeContext().nowMillis >= lifeMillis && (fadeSpeedValue == 0 || brightness == 0);
}

const Model* Light::getModel() const {
  if (list != NULL) {
    return list->model;
  }
  return NULL;
}

const Behaviour* Light::getBehaviour() const {
  if (list != NULL) {
    return list->behaviour;
  }
  return NULL;
}
