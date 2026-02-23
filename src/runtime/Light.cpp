#include <math.h>
#include "Light.h"
#include "LightList.h"
#include "Globals.h"

Light::Light(LightList *list, float speed, uint32_t lifeMillis, uint16_t idx, uint8_t maxBri) : LPLight(list, idx, maxBri) {
    this->speed = speed;
    this->lifeMillis = lifeMillis;
    this->color = ColorRGB(255, 255, 255);
}

uint8_t Light::getBrightness() const {
  uint16_t value = bri % 511;
  value = (value > 255 ? 511 - value : value);
  return ((float) (value - list->fadeThresh) / (255.f - list->fadeThresh)) * maxBri;
}

ColorRGB Light::getPixelColor() const {
    if (brightness == 255) {
        return color;
    }
    return color.Dim(brightness);
}

void Light::nextFrame() {
  bri = list->getBri(this);
  brightness = getBrightness();
  if (list == NULL) {
    position += speed;
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
