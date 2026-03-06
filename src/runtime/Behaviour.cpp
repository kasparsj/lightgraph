#include "Behaviour.h"
#include "RuntimeLight.h"
#include "../core/Platform.h"
#include "../topology/Model.h"
#include "../Globals.h"

uint16_t Behaviour::getBri(const RuntimeLight *light) const {
  if (flags & B_BRI_CONST_NOISE) {
    return gPerlinNoise.GetValue(light->getListId() * 10, light->pixel1 * 100) * 255;
  }
  return light->bri + light->getFadeSpeed();
}

float Behaviour::getPosition(RuntimeLight* const light) const {
  if (flags & B_POS_CHANGE_FADE) {
    if (light->bri >= 511) {
      light->bri -= 511;
      return LG_RANDOM(light->getModel()->getMaxLength());
    }
  }
  return light->position + light->getSpeed();
}

ColorRGB Behaviour::getColor(const RuntimeLight *light, uint8_t /*group*/) const {
  if (light->getPrev() != NULL) {
    return light->getPrev()->getColor();
  }
  return ColorRGB(LG_RANDOM(255), LG_RANDOM(255), LG_RANDOM(255));
}
