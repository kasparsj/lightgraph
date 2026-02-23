#include "State.h"
#include "Model.h"
#include "Behaviour.h"
#include "LightList.h"
#include "Palettes.h"

#ifdef HD_OSC_REPLY
#include <ArduinoOSC.h>
#endif

EmitParams State::autoParams(EmitParams::DEFAULT_MODEL, RANDOM_SPEED);

uint8_t State::randomModel() {
  return floor(LP_RANDOM(object.models.size()));
}

ColorRGB State::paletteColor(uint8_t index, uint8_t /*maxBrightness*/) {
    Palette palette = getPalette(currentPalette);
    return Palette::wrapColors(index, 60, palette.getRGBColors(), palette.getWrapMode());
}

void State::autoEmit(unsigned long ms) {
    if (autoEnabled && nextEmit <= ms) {
        emit(autoParams);
        nextEmit = ms + LPRandom::randomNextEmit();
    }
}

int8_t State::emit(EmitParams &params) {
    uint8_t which = params.model >= 0 ? params.model : randomModel();
    Model *model = object.getModel(which);
    if (model == NULL) {
        LP_LOGF("emit failed, model %d not found\n", which);
        return -1;
    }
    int8_t index = getOrCreateList(params);
    if (index > -1) {
        lightLists[index]->model = model;
        LPOwner *emitter = getEmitter(model, lightLists[index]->behaviour, params);
        if (emitter == NULL) {
            LP_LOGF("emit failed, no free emitter %d %d.\n", params.getEmit(), params.getEmitGroups(model->emitGroups));
            return -1;
        }
        doEmit(emitter, lightLists[index], params);
        #ifdef LP_OSC_REPLY
        LP_OSC_REPLY(index);
        #endif
    }
    return index;
}

int8_t State::getOrCreateList(EmitParams &params) {
    if (params.noteId > 0) {
        int8_t listIndex = findList(params.noteId);
        if (listIndex > -1) {
            return setupListFrom(listIndex, params);
        }
    }
    for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
        if (lightLists[i] == NULL) {
            return setupListFrom(i, params);
        }
    }
    LP_LOGF("emit failed: no free light lists (%d)\n", MAX_LIGHT_LISTS);
    return -1;
}

int8_t State::setupListFrom(uint8_t i, EmitParams &params) {
    LightList* lightList = lightLists[i];
    uint16_t oldLen = (lightList != NULL ? lightList->length : 0);
    uint16_t oldLights = (lightList != NULL ? lightList->numLights : 0);
    uint16_t newLen = params.getLength();
    Behaviour* newBehaviour = new Behaviour(params);
    if (oldLen > 0 && newBehaviour->smoothChanges()) {
        newLen = oldLen + (int) round((float)(newLen - oldLen) * 0.1f);
    }
    if (totalLights - oldLights + newLen > MAX_TOTAL_LIGHTS) {
        // todo: if it's a change, maybe emit max possible?
        LP_LOGF("emit failed, %d is over max %d lights\n", totalLights + newLen, MAX_TOTAL_LIGHTS);
        delete newBehaviour;
        return -1;
    }
    if (lightList == NULL) {
        lightList = new LightList();
        lightLists[i] = lightList;
    }
    lightList->length = newLen;
    if (lightList->behaviour != NULL) {
        delete lightList->behaviour;
    }
    lightList->behaviour = newBehaviour;
    lightList->setupFrom(params);
    if (oldLights > 0) {
        totalLights -= oldLights;
        totalLightLists--;
    }
    return i;
}

LPOwner* State::getEmitter(Model* model, Behaviour* behaviour, EmitParams& params) {
    if (model == NULL || behaviour == NULL) {
        return NULL;
    }
    int8_t from = params.getEmit();
    if (behaviour->emitFromConnection()) {
        uint8_t emitGroups = params.emitGroups;
        uint8_t connCount = object.countConnections(emitGroups);
        if (connCount == 0) {
            LP_LOGF("emit failed, no connections for groups %d\n", emitGroups);
            return NULL;
        }
        from = from >= 0 ? from : LP_RANDOM(connCount);
        return object.getConnection(from % connCount, emitGroups);
    }
    else {
        uint8_t emitGroups = params.getEmitGroups(model->emitGroups);
        uint8_t interCount = object.countIntersections(emitGroups);
        if (interCount == 0) {
            LP_LOGF("emit failed, no intersections for groups %d\n", emitGroups);
            return NULL;
        }
        from = from >= 0 ? from : LP_RANDOM(interCount);
        return object.getIntersection(from % interCount, emitGroups);
    }
}

void State::doEmit(LPOwner* from, LightList *lightList, uint8_t emitOffset) {
    lightList->initEmit(emitOffset);
    lightList->emitter = from;
    totalLights += lightList->numLights;
    totalLightLists++;
}

void State::doEmit(LPOwner* from, LightList *lightList, EmitParams& params) {
    doEmit(from, lightList, params.emitOffset);
}

void State::update() {
  memset(pixelValuesR, 0, sizeof(uint16_t) * object.pixelCount);
  memset(pixelValuesG, 0, sizeof(uint16_t) * object.pixelCount);
  memset(pixelValuesB, 0, sizeof(uint16_t) * object.pixelCount);
  memset(pixelDiv, 0, object.pixelCount);
  for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
    LightList* lightList = lightLists[i];
    if (lightList == NULL) continue;

    bool allExpired = lightList->update();
    if (allExpired) {
      // Keep slot 0 allocated for background, but make it non-visible once expired.
      if (i == 0) {
          lightList->visible = false;
          continue;
      }

      totalLights -= lightList->numLights;
      if (totalLightLists > 0) {
          totalLightLists--;
      }
      delete lightLists[i];
      lightLists[i] = NULL;
    }
    else if (lightList->visible) {
      // Check if the lightList is a BgLight
      if (lightList->editable && lightList->numLights == 0) {
        for (uint16_t p = 0; p < object.pixelCount; p++) {
            ColorRGB color = lightList->getColor(p);
            setPixel(p, color, lightList);
        }
      }
      else {
        // Normal light list processing
        for (uint16_t j=0; j<lightList->numLights; j++) {
            LPLight* light = lightList->lights[j];
            if (light == NULL) continue;
            updateLight(light);
        }
      }
    }
  }
}

void State::updateLight(LPLight* light) {
    ColorRGB color = light->getPixelColor();
    // todo: perhaps it's OK to always retrieve pixels
    if (light->list->behaviour != NULL && (light->list->behaviour->renderSegment() || light->list->behaviour->fillEase())) {
      uint16_t* pixels = light->getPixels();
      if (pixels != NULL) {
        // first value is length
        uint16_t numPixels = pixels[0];
        for (uint16_t k=1; k<numPixels+1; k++) {
          setPixels(pixels[k], color, light->list);
        }
      }
    }
    else if (light->pixel1 >= 0) {
      setPixels(light->pixel1, color, light->list);
    }
    light->nextFrame();
}

ColorRGB State::getPixel(uint16_t i, uint8_t maxBrightness) {
  ColorRGB color = ColorRGB(0, 0, 0);
  if (pixelDiv[i]) {
    color.R = min(pixelValuesR[i] / pixelDiv[i] / 255.f, 1.f) * maxBrightness;
    color.G = min(pixelValuesG[i] / pixelDiv[i] / 255.f, 1.f) * maxBrightness;
    color.B = min(pixelValuesB[i] / pixelDiv[i] / 255.f, 1.f) * maxBrightness;
  }
  return color;
}

void State::setPixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList) {
    setPixel(pixel, color, lightList);
    if (lightList->behaviour != NULL && (lightList->behaviour->mirrorFlip() || lightList->behaviour->mirrorRotate())) {
        uint16_t* mirrorPixels = object.getMirroredPixels(pixel, lightList->behaviour->mirrorFlip() ? lightList->emitter : 0, lightList->behaviour->mirrorRotate());
        if (mirrorPixels != NULL) {
            // first value is length
            uint16_t numPixels = mirrorPixels[0];
            for (uint16_t k=1; k<numPixels+1; k++) {
                setPixel(mirrorPixels[k], color, lightList);
            }
        }
    }
}

void State::setPixel(uint16_t pixel, ColorRGB &color, const LightList* const lightList) {
    // Apply blend mode based on the light list's setting
    BlendMode mode = lightList ? lightList->blendMode : BLEND_NORMAL;

    // Variables for current color components
    float r = 0.0f, g = 0.0f, b = 0.0f;
    // Normalize the new color to 0-1 range
    float newR = color.R / 255.0f;
    float newG = color.G / 255.0f;
    float newB = color.B / 255.0f;

    // Check most common basic blend modes first for efficiency
    if (mode == BLEND_NORMAL) {
        // Standard blend mode - add values and later divide by count
        pixelValuesR[pixel] += color.R;
        pixelValuesG[pixel] += color.G;
        pixelValuesB[pixel] += color.B;
        pixelDiv[pixel]++;
        return;
    } else if (mode == BLEND_REPLACE) {
        // Replace any existing color completely
        pixelValuesR[pixel] = color.R * pixelDiv[pixel];
        pixelValuesG[pixel] = color.G * pixelDiv[pixel];
        pixelValuesB[pixel] = color.B * pixelDiv[pixel];
        return;
    } else if (mode == BLEND_ADD) {
        // Direct addition without division later
        pixelValuesR[pixel] += color.R;
        pixelValuesG[pixel] += color.G;
        pixelValuesB[pixel] += color.B;
        return;
    }

    // For other blend modes, we need current color values
    if (pixelDiv[pixel] > 0) {
        r = (pixelValuesR[pixel] / (float)pixelDiv[pixel]) / 255.0f;
        g = (pixelValuesG[pixel] / (float)pixelDiv[pixel]) / 255.0f;
        b = (pixelValuesB[pixel] / (float)pixelDiv[pixel]) / 255.0f;
    } else {
        // No existing color, just use the new color for most blend modes
        pixelValuesR[pixel] = color.R;
        pixelValuesG[pixel] = color.G;
        pixelValuesB[pixel] = color.B;
        pixelDiv[pixel] = 1;
        return;
    }

    // Now handle the more complex blend modes
    switch (mode) {
        case BLEND_MULTIPLY:
            r *= newR;
            g *= newG;
            b *= newB;
            break;

        case BLEND_SCREEN:
            r = 1.0f - (1.0f - r) * (1.0f - newR);
            g = 1.0f - (1.0f - g) * (1.0f - newG);
            b = 1.0f - (1.0f - b) * (1.0f - newB);
            break;

        case BLEND_OVERLAY:
            r = (r < 0.5f) ? (2.0f * r * newR) : (1.0f - 2.0f * (1.0f - r) * (1.0f - newR));
            g = (g < 0.5f) ? (2.0f * g * newG) : (1.0f - 2.0f * (1.0f - g) * (1.0f - newG));
            b = (b < 0.5f) ? (2.0f * b * newB) : (1.0f - 2.0f * (1.0f - b) * (1.0f - newB));
            break;

        case BLEND_SUBTRACT:
            r = max(0.0f, r - newR);
            g = max(0.0f, g - newG);
            b = max(0.0f, b - newB);
            break;

        case BLEND_DIFFERENCE:
            r = abs(r - newR);
            g = abs(g - newG);
            b = abs(b - newB);
            break;

        case BLEND_EXCLUSION:
            r = r + newR - 2.0f * r * newR;
            g = g + newG - 2.0f * g * newG;
            b = b + newB - 2.0f * b * newB;
            break;

        case BLEND_DODGE:
            r = (newR == 1.0f) ? 1.0f : min(1.0f, r / (1.0f - newR));
            g = (newG == 1.0f) ? 1.0f : min(1.0f, g / (1.0f - newG));
            b = (newB == 1.0f) ? 1.0f : min(1.0f, b / (1.0f - newB));
            break;

        case BLEND_BURN:
            r = (newR == 0.0f) ? 0.0f : max(0.0f, 1.0f - (1.0f - r) / newR);
            g = (newG == 0.0f) ? 0.0f : max(0.0f, 1.0f - (1.0f - g) / newG);
            b = (newB == 0.0f) ? 0.0f : max(0.0f, 1.0f - (1.0f - b) / newB);
            break;

        case BLEND_HARD_LIGHT:
            r = (newR < 0.5f) ? (2.0f * newR * r) : (1.0f - 2.0f * (1.0f - newR) * (1.0f - r));
            g = (newG < 0.5f) ? (2.0f * newG * g) : (1.0f - 2.0f * (1.0f - newG) * (1.0f - g));
            b = (newB < 0.5f) ? (2.0f * newB * b) : (1.0f - 2.0f * (1.0f - newB) * (1.0f - b));
            break;

        case BLEND_SOFT_LIGHT:
            r = (newR < 0.5f) ? (r - (1.0f - 2.0f * newR) * r * (1.0f - r)) : (r + (2.0f * newR - 1.0f) * (sqrt(r) - r));
            g = (newG < 0.5f) ? (g - (1.0f - 2.0f * newG) * g * (1.0f - g)) : (g + (2.0f * newG - 1.0f) * (sqrt(g) - g));
            b = (newB < 0.5f) ? (b - (1.0f - 2.0f * newB) * b * (1.0f - b)) : (b + (2.0f * newB - 1.0f) * (sqrt(b) - b));
            break;

        case BLEND_LINEAR_LIGHT:
            r = (newR < 0.5f) ? max(0.0f, r + 2.0f * newR - 1.0f) : min(1.0f, r + 2.0f * (newR - 0.5f));
            g = (newG < 0.5f) ? max(0.0f, g + 2.0f * newG - 1.0f) : min(1.0f, g + 2.0f * (newG - 0.5f));
            b = (newB < 0.5f) ? max(0.0f, b + 2.0f * newB - 1.0f) : min(1.0f, b + 2.0f * (newB - 0.5f));
            break;

        case BLEND_VIVID_LIGHT:
            r = (newR < 0.5f) ? (newR == 0.0f ? 0.0f : max(0.0f, 1.0f - (1.0f - r) / (2.0f * newR))) :
                               (newR == 1.0f ? 1.0f : min(1.0f, r / (2.0f * (1.0f - newR))));
            g = (newG < 0.5f) ? (newG == 0.0f ? 0.0f : max(0.0f, 1.0f - (1.0f - g) / (2.0f * newG))) :
                               (newG == 1.0f ? 1.0f : min(1.0f, g / (2.0f * (1.0f - newG))));
            b = (newB < 0.5f) ? (newB == 0.0f ? 0.0f : max(0.0f, 1.0f - (1.0f - b) / (2.0f * newB))) :
                               (newB == 1.0f ? 1.0f : min(1.0f, b / (2.0f * (1.0f - newB))));
            break;

        case BLEND_PIN_LIGHT:
            r = (newR < 0.5f) ? min(r, 2.0f * newR) : max(r, 2.0f * (newR - 0.5f));
            g = (newG < 0.5f) ? min(g, 2.0f * newG) : max(g, 2.0f * (newG - 0.5f));
            b = (newB < 0.5f) ? min(b, 2.0f * newB) : max(b, 2.0f * (newB - 0.5f));
            break;

        default:
            // Fallback to normal blend for any unrecognized modes
            pixelValuesR[pixel] += color.R;
            pixelValuesG[pixel] += color.G;
            pixelValuesB[pixel] += color.B;
            pixelDiv[pixel]++;
            return;
    }

    // Set the calculated color values
    pixelValuesR[pixel] = r * 255.0f * pixelDiv[pixel];
    pixelValuesG[pixel] = g * 255.0f * pixelDiv[pixel];
    pixelValuesB[pixel] = b * 255.0f * pixelDiv[pixel];
}

void State::setupBg(uint8_t i) {
    BgLight* bgLight = new BgLight();
    lightLists[i] = bgLight;

    // Configure the BgLight
    bgLight->model = object.getModel(0); // Use first model
    bgLight->setDuration(INFINITE_DURATION);

    bgLight->setup(object.pixelCount);

    // Use the palette directly, colorRule is now managed by the Palette
    bgLight->setPalette(Palette({0xFF0000}, {0.0f}));

    totalLightLists++;
}

void State::colorAll() {
    ColorRGB color;
    color.setRandom();
    for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
        if (lightLists[i] == NULL) continue;
        lightLists[i]->palette.setColors({color});
    }
}

void State::splitAll() {
  for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
    if (lightLists[i] == NULL) continue;
    lightLists[i]->split();
  }
}

void State::stopAll() {
  for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
    if (lightLists[i] == NULL) continue;
    lightLists[i]->setDuration(0);
  }
}

int8_t State::findList(uint8_t noteId) const {
    for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
      if (lightLists[i] == NULL) continue;
      if (lightLists[i]->noteId == noteId) {
        return i;
      }
    }
    return -1;
}

LightList* State::findListById(uint16_t id) {
  for (uint8_t i = 0; i < MAX_LIGHT_LISTS; i++) {
    if (lightLists[i] != NULL && lightLists[i]->id == id) {
      return lightLists[i];
    }
  }
  return nullptr;
}

void State::stopNote(uint8_t noteId) {
    int8_t index = findList(noteId);
    if (index > -1) {
        lightLists[index]->setDuration(0);
    }
}

void State::debug() {
  for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
    if (lightLists[i] == NULL) continue;
    LP_STRING lights = "";
    for (uint16_t j=0; j<lightLists[i]->numLights; j++) {
      if (lightLists[i]->lights[j] == NULL || lightLists[i]->lights[j]->isExpired) {
        continue;
      }
      else {
        lights += j;
        lights += "(";
        lights += lightLists[i]->lights[j]->pixel1;
        lights += ")";
        lights += ", ";
      }
    }
    LP_LOGF("LightList %d (%d) active lights:", i, lightLists[i]->numLights);
    LP_LOG(lights);
    LP_LOGLN("");
  }
}
