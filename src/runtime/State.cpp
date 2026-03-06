#include "State.h"

#include <cmath>
#include <limits>
#include <new>

#include "../core/Platform.h"
#include "../topology/TopologyObject.h"
#include "../topology/Model.h"
#include "Behaviour.h"
#include "BgLight.h"
#include "EmitParams.h"
#include "LightListBuild.h"
#include "LightList.h"
#include "../rendering/Palettes.h"
#include "../Globals.h"

#ifdef HD_OSC_REPLY
#include <ArduinoOSC.h>
#endif

EmitParams State::autoParams(EmitParams::DEFAULT_MODEL, RANDOM_SPEED);

namespace {

uint8_t clampReservedTailSlots(uint8_t slots) {
    if (MAX_LIGHT_LISTS <= 1) {
        return 0;
    }
    if (slots >= MAX_LIGHT_LISTS) {
        return static_cast<uint8_t>(MAX_LIGHT_LISTS - 1);
    }
    return slots;
}

} // namespace

State::State(TopologyObject& obj)
    : object(obj),
      pixelValuesR(obj.pixelCount, 0),
      pixelValuesG(obj.pixelCount, 0),
      pixelValuesB(obj.pixelCount, 0),
      pixelDiv(obj.pixelCount, 0)
#if LIGHTGRAPH_FRACTIONAL_RENDERING
      ,
      listPixelValuesR(obj.pixelCount, 0),
      listPixelValuesG(obj.pixelCount, 0),
      listPixelValuesB(obj.pixelCount, 0)
#endif
{
#if LIGHTGRAPH_FRACTIONAL_RENDERING
    listTouchedPixels.reserve(obj.pixelCount);
#endif
    setupBg(0);
}

State::~State() {
    for (uint8_t i = 0; i < MAX_LIGHT_LISTS; i++) {
        if (lightLists[i] != NULL) {
            delete lightLists[i];
            lightLists[i] = NULL;
        }
    }
}

uint8_t State::randomModel() {
  return floor(LG_RANDOM(object.models.size()));
}

ColorRGB State::paletteColor(uint8_t index, uint8_t /*maxBrightness*/) {
    Palette palette = getPalette(currentPalette);
    return Palette::wrapColors(index, 60, palette.getRGBColors(), palette.getWrapMode());
}

void State::autoEmit(unsigned long ms) {
    if (autoEnabled && nextEmit <= ms) {
        emit(autoParams);
        nextEmit = ms + Random::randomNextEmit();
    }
}

int8_t State::emit(EmitParams &params) {
    uint8_t which = params.model >= 0 ? params.model : randomModel();
    Model *model = object.getModel(which);
    if (model == NULL) {
        LG_LOGF("emit failed, model %d not found\n", which);
        return -1;
    }
    int8_t index = getOrCreateList(params);
    if (index > -1) {
        lightLists[index]->model = model;
        Owner *emitter = getEmitter(model, lightLists[index]->behaviour, params);
        if (emitter == NULL) {
            LG_LOGF("emit failed, no free emitter %d %d.\n", params.getEmit(), params.getEmitGroups(model->emitGroups));
            return -1;
        }
        doEmit(emitter, lightLists[index], params);
        #ifdef LG_OSC_REPLY
        LG_OSC_REPLY(index);
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
    const uint8_t localSlotsEndExclusive = getLocalSlotEndExclusive();
    for (uint8_t i = 0; i < localSlotsEndExclusive; i++) {
        if (lightLists[i] == NULL) {
            return setupListFrom(i, params);
        }
    }
    LG_LOGF("emit failed: no free local light lists (%d, reserved tail=%d)\n",
            localSlotsEndExclusive,
            clampReservedTailSlots(reservedTailSlots));
    return -1;
}

int8_t State::setupListFrom(uint8_t i, EmitParams &params) {
    LightList* lightList = lightLists[i];
    uint16_t oldLen = (lightList != NULL ? lightList->length : 0);
    uint16_t oldLights = (lightList != NULL ? lightList->numLights : 0);
    const bool hadCountedList = (lightList != NULL && lightList->emitter != NULL);
    const uint16_t countedOldLights = hadCountedList ? oldLights : 0;
    uint16_t newLen = params.getLength();
    Behaviour smoothProbe(params);
    if (oldLen > 0 && smoothProbe.smoothChanges()) {
        newLen = oldLen + (int) round((float)(newLen - oldLen) * 0.1f);
    }
    const uint32_t retainedLights = (totalLights >= countedOldLights)
                                        ? static_cast<uint32_t>(totalLights - countedOldLights)
                                        : 0U;
    if (retainedLights + newLen > MAX_TOTAL_LIGHTS) {
        // todo: if it's a change, maybe emit max possible?
        LG_LOGF("emit failed, %d is over max %d lights\n", totalLights + newLen, MAX_TOTAL_LIGHTS);
        return -1;
    }

    lightlist_build::Spec spec = lightlist_build::makeSpecFromEmitParams(params, newLen);
    lightlist_build::Policy policy;
    policy.allocateBehaviour = true;
    policy.behaviourFailureSite = LightgraphAllocationFailureSite::StateBehaviourAllocation;
    policy.listFailureSite = LightgraphAllocationFailureSite::StateListAllocation;
    policy.exceptionFailureSite = LightgraphAllocationFailureSite::StateSetupException;
    LightList* replacement = lightlist_build::buildLightList(spec, policy);
    if (replacement == NULL) {
        if (lightList != NULL) {
            delete lightList;
            lightLists[i] = NULL;
        }
        if (hadCountedList) {
            if (totalLights >= oldLights) {
                totalLights = static_cast<uint16_t>(totalLights - oldLights);
            } else {
                totalLights = 0;
            }
            if (totalLightLists > 0) {
                totalLightLists--;
            }
        }
        return -1;
    }

    if (lightList != NULL) {
        delete lightList;
    }
    lightLists[i] = replacement;
    if (hadCountedList) {
        if (totalLights >= oldLights) {
            totalLights = static_cast<uint16_t>(totalLights - oldLights);
        } else {
            totalLights = 0;
        }
        if (totalLightLists > 0) {
            totalLightLists--;
        }
    }
    return i;
}

Owner* State::getEmitter(Model* model, Behaviour* behaviour, EmitParams& params) {
    if (model == NULL || behaviour == NULL) {
        return NULL;
    }
    int8_t from = params.getEmit();
    if (behaviour->emitFromConnection()) {
        uint8_t emitGroups = params.emitGroups;
        uint8_t connCount = object.countConnections(emitGroups);
        if (connCount == 0) {
            LG_LOGF("emit failed, no connections for groups %d\n", emitGroups);
            return NULL;
        }
        from = from >= 0 ? from : LG_RANDOM(connCount);
        return object.getConnection(from % connCount, emitGroups);
    }
    else {
        uint8_t emitGroups = params.getEmitGroups(model->emitGroups);
        uint8_t interCount = object.countEmittableIntersections(emitGroups);
        if (interCount == 0) {
            LG_LOGF("emit failed, no intersections for groups %d\n", emitGroups);
            return NULL;
        }
        from = from >= 0 ? from : LG_RANDOM(interCount);
        return object.getEmittableIntersection(from % interCount, emitGroups);
    }
}

void State::activateList(Owner* from, LightList *lightList, uint8_t emitOffset, bool countTotals) {
    if (lightList == NULL) {
        return;
    }
    lightList->emitOffset = emitOffset;
    lightList->numEmitted = 0;
    lightList->numSplits = 0;
    lightList->initEmit(emitOffset);
    lightList->emitter = from;
    if (countTotals) {
        totalLights += lightList->numLights;
        totalLightLists++;
    }
}

void State::doEmit(Owner* from, LightList *lightList, uint8_t emitOffset) {
    activateList(from, lightList, emitOffset, true);
}

void State::doEmit(Owner* from, LightList *lightList, EmitParams& params) {
    doEmit(from, lightList, params.emitOffset);
}

void State::update() {
  lightgraphAdvanceFrameTiming(gMillis);
  const uint8_t substeps = lightgraphSimulationSubsteps();
  for (uint8_t step = 0; step < substeps; step++) {
    lightgraphSetSimulationSubstep(substeps);
    updatePass(step + 1 == substeps);
  }
}

void State::updatePass(bool renderStep) {
  if (renderStep) {
    std::fill(pixelValuesR.begin(), pixelValuesR.end(), 0);
    std::fill(pixelValuesG.begin(), pixelValuesG.end(), 0);
    std::fill(pixelValuesB.begin(), pixelValuesB.end(), 0);
    std::fill(pixelDiv.begin(), pixelDiv.end(), 0);
  }

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
          clearListSlot(i);
        }
        else if (lightList->visible) {
#if LIGHTGRAPH_FRACTIONAL_RENDERING
      if (renderStep) {
        beginListRender(lightList);
      }
#endif
      // Check if the lightList is a BgLight
      if (lightList->editable && lightList->numLights == 0) {
        if (renderStep) {
          for (uint16_t p = 0; p < object.pixelCount; p++) {
              ColorRGB color = lightList->getColor(p);
              setPixel(p, color, lightList);
          }
        }
      }
      else {
        // Normal light list processing
        for (uint16_t j=0; j<lightList->numLights; j++) {
            RuntimeLight* light = lightList->lights[j];
            if (light == NULL) continue;
            if (renderStep) {
              updateLight(light);
            } else {
              light->nextFrame();
            }
        }
      }
#if LIGHTGRAPH_FRACTIONAL_RENDERING
      if (renderStep) {
        endListRender(lightList);
      }
#endif
    }
  }
}

void State::updateLight(RuntimeLight* light) {
    // todo: perhaps it's OK to always retrieve pixels
    if (light->list->behaviour != NULL && (light->list->behaviour->renderSegment() || light->list->behaviour->fillEase())) {
      ColorRGB color = light->getPixelColor();
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
#if LIGHTGRAPH_FRACTIONAL_RENDERING
      setPixelsWeighted(
          static_cast<uint16_t>(light->pixel1),
          light->getPixelColorAt(light->pixel1),
          light->list,
          light->getPrimaryPixelWeight());
      if (light->hasSecondaryPixel()) {
        setPixelsWeighted(
            static_cast<uint16_t>(light->pixel2),
            light->getPixelColorAt(light->pixel2),
            light->list,
            light->pixel2Weight);
      }
#else
      ColorRGB color = light->getPixelColor();
      setPixels(static_cast<uint16_t>(light->pixel1), color, light->list);
#endif
    }
    light->nextFrame();
}

ColorRGB State::getPixel(uint16_t i, uint8_t maxBrightness) {
  ColorRGB color = ColorRGB(0, 0, 0);
  if (i >= pixelDiv.size()) {
    return color;
  }
  const uint8_t div = pixelDiv[i];
  if (div != 0) {
    const uint16_t avgR = static_cast<uint16_t>(pixelValuesR[i] / div);
    const uint16_t avgG = static_cast<uint16_t>(pixelValuesG[i] / div);
    const uint16_t avgB = static_cast<uint16_t>(pixelValuesB[i] / div);

    const uint16_t clampedR = avgR > 255 ? 255 : avgR;
    const uint16_t clampedG = avgG > 255 ? 255 : avgG;
    const uint16_t clampedB = avgB > 255 ? 255 : avgB;

    color.R = static_cast<uint8_t>((clampedR * maxBrightness + 127u) / 255u);
    color.G = static_cast<uint8_t>((clampedG * maxBrightness + 127u) / 255u);
    color.B = static_cast<uint8_t>((clampedB * maxBrightness + 127u) / 255u);
  }
  return color;
}

void State::setPixelsWeighted(uint16_t pixel,
                              const ColorRGB& color,
                              const LightList* const lightList,
                              uint8_t weight) {
    if (weight == 0) {
        return;
    }
    if (weight == FULL_BRIGHTNESS) {
        ColorRGB fullColor = color;
        setPixels(pixel, fullColor, lightList);
        return;
    }

    ColorRGB weightedColor = color.dim(weight);
    if (weightedColor.R == 0 && weightedColor.G == 0 && weightedColor.B == 0) {
        return;
    }
    setPixels(pixel, weightedColor, lightList);
}

void State::setPixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList) {
#if LIGHTGRAPH_FRACTIONAL_RENDERING
    if (renderingList != nullptr) {
        setListPixels(pixel, color, lightList);
        return;
    }
#endif
    setFramePixels(pixel, color, lightList);
}

void State::setFramePixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList) {
    setFramePixel(pixel, color, lightList);
    if (lightList != NULL && lightList->behaviour != NULL &&
        (lightList->behaviour->mirrorFlip() || lightList->behaviour->mirrorRotate())) {
        uint16_t* mirrorPixels = object.getMirroredPixels(pixel, lightList->behaviour->mirrorFlip() ? lightList->emitter : 0, lightList->behaviour->mirrorRotate());
        if (mirrorPixels != NULL) {
            // first value is length
            uint16_t numPixels = mirrorPixels[0];
            for (uint16_t k=1; k<numPixels+1; k++) {
                setFramePixel(mirrorPixels[k], color, lightList);
            }
        }
    }
}

#if LIGHTGRAPH_FRACTIONAL_RENDERING
void State::beginListRender(const LightList* lightList) {
    renderingList = lightList;
}

void State::endListRender(const LightList* lightList) {
    renderingList = nullptr;
    for (uint16_t pixel : listTouchedPixels) {
        const uint16_t red = listPixelValuesR[pixel];
        const uint16_t green = listPixelValuesG[pixel];
        const uint16_t blue = listPixelValuesB[pixel];
        if (red != 0 || green != 0 || blue != 0) {
            ColorRGB color(
                static_cast<uint8_t>(std::min<uint16_t>(red, FULL_BRIGHTNESS)),
                static_cast<uint8_t>(std::min<uint16_t>(green, FULL_BRIGHTNESS)),
                static_cast<uint8_t>(std::min<uint16_t>(blue, FULL_BRIGHTNESS)));
            setFramePixel(pixel, color, lightList);
        }
        listPixelValuesR[pixel] = 0;
        listPixelValuesG[pixel] = 0;
        listPixelValuesB[pixel] = 0;
    }
    listTouchedPixels.clear();
}

void State::setListPixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList) {
    setListPixel(pixel, color);
    if (lightList != NULL && lightList->behaviour != NULL &&
        (lightList->behaviour->mirrorFlip() || lightList->behaviour->mirrorRotate())) {
        uint16_t* mirrorPixels = object.getMirroredPixels(pixel, lightList->behaviour->mirrorFlip() ? lightList->emitter : 0, lightList->behaviour->mirrorRotate());
        if (mirrorPixels != NULL) {
            // first value is length
            uint16_t numPixels = mirrorPixels[0];
            for (uint16_t k=1; k<numPixels+1; k++) {
                setListPixel(mirrorPixels[k], color);
            }
        }
    }
}

void State::setListPixel(uint16_t pixel, ColorRGB &color) {
    if (pixel >= listPixelValuesR.size()) {
        return;
    }
    if (color.R == 0 && color.G == 0 && color.B == 0) {
        return;
    }

    if (listPixelValuesR[pixel] == 0 && listPixelValuesG[pixel] == 0 && listPixelValuesB[pixel] == 0) {
        listTouchedPixels.push_back(pixel);
    }

    listPixelValuesR[pixel] = std::min<uint16_t>(
        FULL_BRIGHTNESS,
        static_cast<uint16_t>(listPixelValuesR[pixel] + color.R));
    listPixelValuesG[pixel] = std::min<uint16_t>(
        FULL_BRIGHTNESS,
        static_cast<uint16_t>(listPixelValuesG[pixel] + color.G));
    listPixelValuesB[pixel] = std::min<uint16_t>(
        FULL_BRIGHTNESS,
        static_cast<uint16_t>(listPixelValuesB[pixel] + color.B));
}
#endif

void State::setPixel(uint16_t pixel, ColorRGB &color, const LightList* const lightList) {
#if LIGHTGRAPH_FRACTIONAL_RENDERING
    if (renderingList != nullptr) {
        setListPixel(pixel, color);
        return;
    }
#endif
    setFramePixel(pixel, color, lightList);
}

void State::setFramePixel(uint16_t pixel, ColorRGB &color, const LightList* const lightList) {
    if (pixel >= pixelValuesR.size()) {
        return;
    }

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
        pixelValuesR[pixel] = color.R;
        pixelValuesG[pixel] = color.G;
        pixelValuesB[pixel] = color.B;
        pixelDiv[pixel] = 1;
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
            r = std::abs(r - newR);
            g = std::abs(g - newG);
            b = std::abs(b - newB);
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
    BgLight* bgLight = new (std::nothrow) BgLight();
    if (bgLight == nullptr) {
        LG_LOGLN("setupBg failed: OOM creating background layer");
        lightgraphReportAllocationFailure(
            LightgraphAllocationFailureSite::SetupBgAllocation,
            static_cast<uint16_t>(i),
            0);
        return;
    }
    lightLists[i] = bgLight;

    // Configure the BgLight
    bgLight->model = object.getModel(0); // Use first model
    bgLight->setDuration(INFINITE_DURATION);

    bgLight->setup(object.pixelCount);

    // Use the palette directly, colorRule is now managed by the Palette
    bgLight->setPalette(Palette({0xFF0000}, {0.0f}));

    totalLightLists++;
}

void State::setReservedTailSlots(uint8_t slots) {
    reservedTailSlots = clampReservedTailSlots(slots);
}

uint8_t State::getReservedTailSlots() const {
    return reservedTailSlots;
}

uint8_t State::getLocalSlotEndExclusive() const {
    const uint8_t slotsToReserve = clampReservedTailSlots(reservedTailSlots);
    return static_cast<uint8_t>(MAX_LIGHT_LISTS - slotsToReserve);
}

bool State::clearListSlot(uint8_t slot) {
    if (slot >= MAX_LIGHT_LISTS) {
        return false;
    }

    LightList* existing = lightLists[slot];
    if (existing == nullptr) {
        return true;
    }

    if (slot == 0) {
        existing->visible = false;
        return true;
    }

    if (totalLights >= existing->numLights) {
        totalLights = static_cast<uint16_t>(totalLights - existing->numLights);
    } else {
        totalLights = 0;
    }

    if (totalLightLists > 0) {
        totalLightLists--;
    }

    delete existing;
    lightLists[slot] = nullptr;
    return true;
}

bool State::replaceListSlot(uint8_t slot, LightList* replacement) {
    if (slot >= MAX_LIGHT_LISTS) {
        delete replacement;
        return false;
    }

    if (slot == 0 && replacement == nullptr) {
        if (lightLists[0] != nullptr) {
            lightLists[0]->visible = false;
        }
        return true;
    }

    if (slot == 0 && lightLists[0] != nullptr) {
        LightList* existing = lightLists[0];
        if (totalLights >= existing->numLights) {
            totalLights = static_cast<uint16_t>(totalLights - existing->numLights);
        } else {
            totalLights = 0;
        }
        if (totalLightLists > 0) {
            totalLightLists--;
        }
        delete existing;
        lightLists[0] = nullptr;
    } else if (slot != 0) {
        clearListSlot(slot);
    }

    if (replacement == nullptr) {
        return true;
    }

    lightLists[slot] = replacement;
    totalLights = static_cast<uint16_t>(totalLights + replacement->numLights);
    if (totalLightLists < std::numeric_limits<uint8_t>::max()) {
        totalLightLists++;
    }
    return true;
}

void State::colorAll() {
    ColorRGB color;
    color.setRandom();
    for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
        if (lightLists[i] == NULL) continue;
        Palette updatedPalette = lightLists[i]->getPalette();
        updatedPalette.setColors({color});
        lightLists[i]->setPalette(updatedPalette);
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

bool State::isOn() {
    for (uint8_t i = 0; i < MAX_LIGHT_LISTS; i++) {
        if (lightLists[i] != NULL && lightLists[i]->visible) {
            return true;
        }
    }
    return false;
}

void State::setOn(bool newState) {
    if (lightLists[0]) {
        lightLists[0]->visible = newState;
    }
    if (!newState) {
        autoEnabled = false;
    }
}

int8_t State::findList(uint16_t noteId) const {
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

void State::stopNote(uint16_t noteId) {
    int8_t index = findList(noteId);
    if (index > -1) {
        lightLists[index]->setDuration(0);
    }
}

void State::debug() {
  for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
    if (lightLists[i] == NULL) continue;
    LG_STRING lights = "";
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
    LG_LOGF("LightList %d (%d) active lights:", i, lightLists[i]->numLights);
    LG_LOG(lights);
    LG_LOGLN("");
  }
}
