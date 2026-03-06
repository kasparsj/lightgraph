#include <math.h>
#include "Light.h"
#include "LightList.h"
#include "../Globals.h"

Light::Light(LightList *list, float speed, uint32_t lifeMillis, uint16_t idx, uint8_t maxBri) : RuntimeLight(list, idx, maxBri) {
    this->speed = speed;
    this->lifeMillis = lifeMillis;
    this->color = ColorRGB(255, 255, 255);
}

uint8_t Light::getBrightness() const {
  uint16_t value = bri % 511;
  value = (value > 255 ? 511 - value : value);
  const uint8_t fadeThresh = (list != NULL ? list->fadeThresh : 0);
  const int16_t fadeRange = 255 - static_cast<int16_t>(fadeThresh);
  if (fadeRange <= 0) {
    return 0;
  }
  const int16_t aboveThreshold = static_cast<int16_t>(value) - static_cast<int16_t>(fadeThresh);
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
    position += lightgraphMotionDistance(speed);
  }
  else {
    position = list->getPosition(this);
  }
}

bool Light::shouldExpire() const {
  if (lifeMillis >= INFINITE_DURATION) {
    return false;
  }
  return gMillis >= lifeMillis && (list->fadeSpeed == 0 || brightness == 0);
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
