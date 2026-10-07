#include "Behaviour.h"
#include "RuntimeLight.h"
#include "../core/Platform.h"
#include "../topology/Model.h"
#include "../Globals.h"

uint16_t Behaviour::getBri(const RuntimeLight *light) const {
  if (flags & B_BRI_CONST_NOISE) {
    return static_cast<uint16_t>(light->runtimeContext().perlinNoise.GetValue(
        static_cast<FN_DECIMAL>(light->getListId() * 10),
        static_cast<FN_DECIMAL>(light->pixel1 * 100)) * 255);
  }
  return static_cast<uint16_t>(
      light->bri + lightgraphMotionDistance(light->runtimeContext(), light->getFadeSpeed()));
}

float Behaviour::getPosition(RuntimeLight* const light) const {
  if (flags & B_POS_CHANGE_FADE) {
    if (light->bri >= 511) {
      light->bri -= 511;
      return LG_RANDOM(light->getModel()->getMaxLength());
    }
  }
  return light->position + lightgraphMotionDistance(light->runtimeContext(), light->getSpeed());
}

ColorRGB Behaviour::getColor(const RuntimeLight *light, uint8_t /*group*/) const {
  if (light->getPrev() != NULL) {
    return light->getPrev()->getColor();
  }
  return ColorRGB(
      static_cast<uint8_t>(LG_RANDOM(255)),
      static_cast<uint8_t>(LG_RANDOM(255)),
      static_cast<uint8_t>(LG_RANDOM(255)));
}
