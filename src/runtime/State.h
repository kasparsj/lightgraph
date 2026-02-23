#pragma once

#include <vector>

#include "../Config.h"
#include "EmitParams.h"
#include "../topology/LPObject.h"
#include "BgLight.h"

class State {

  public:

    static EmitParams autoParams;

    LPObject &object;
    LightList *lightLists[MAX_LIGHT_LISTS] = {0};
    uint16_t totalLights = 0;
    uint8_t totalLightLists = 0;
    unsigned long nextEmit = 0;
    std::vector<uint16_t> pixelValuesR;
    std::vector<uint16_t> pixelValuesG;
    std::vector<uint16_t> pixelValuesB;
    std::vector<uint8_t> pixelDiv;
    bool autoEnabled = false;
    uint8_t currentPalette = 0;
    bool showIntersections = false;
    bool showConnections = false;

    State(LPObject &obj)
        : object(obj),
          pixelValuesR(obj.pixelCount, 0),
          pixelValuesG(obj.pixelCount, 0),
          pixelValuesB(obj.pixelCount, 0),
          pixelDiv(obj.pixelCount, 0) {
        // there is always lightList[0]
        setupBg(0);
    }
    ~State() {
      for (uint8_t i = 0; i < MAX_LIGHT_LISTS; i++) {
        if (lightLists[i] != NULL) {
          delete lightLists[i];
          lightLists[i] = NULL;
        }
      }
    }

    uint8_t randomModel();
    ColorRGB paletteColor(uint8_t index, uint8_t maxBrightness = FULL_BRIGHTNESS);
    void autoEmit(unsigned long millis);
    int8_t emit(EmitParams &params);
    void emit(LightList& lightList);
    int8_t getOrCreateList(EmitParams &params);
    int8_t setupListFrom(uint8_t i, EmitParams &params);
    LPOwner* getEmitter(Model* model, Behaviour* behaviour, EmitParams& params);
    void update();
    void updateLight(LPLight* light);
    void colorAll();
    void splitAll();
    void stopAll();
    int8_t findList(uint8_t noteId) const;
    LightList* findListById(uint16_t id);
    void stopNote(uint8_t i);
    ColorRGB getPixel(uint16_t i, uint8_t maxBrightness = FULL_BRIGHTNESS);
    void debug();
    bool isOn() {
        for (uint8_t i=0; i<MAX_LIGHT_LISTS; i++) {
            if (lightLists[i] != NULL && lightLists[i]->visible) {
                return true;
            }
        }
        return false;
    }
    void setOn(bool newState) {
        if (lightLists[0]) {
            lightLists[0]->visible = newState;
        }
        if (newState == false) {
            autoEnabled = false;
        }
    }
    void setupBg(uint8_t i);
    void doEmit(LPOwner* from, LightList *lightList, uint8_t emitOffset = 0);

  private:
    void doEmit(LPOwner* from, LightList *lightList, EmitParams& params);
    void setPixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setPixel(uint16_t pixel, ColorRGB &color, const LightList* const lightList);

};
