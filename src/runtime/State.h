#pragma once

#include <vector>

#include "../core/Types.h"
#include "../core/Limits.h"

class EmitParams;
class TopologyObject;
class LightList;
class Model;
class Behaviour;
class Owner;
class RuntimeLight;

class State {

  public:

    static EmitParams autoParams;

    TopologyObject &object;
    LightList *lightLists[MAX_LIGHT_LISTS] = {0};
    uint16_t totalLights = 0;
    uint8_t totalLightLists = 0;
    unsigned long nextEmit = 0;
    std::vector<uint16_t> pixelValuesR;
    std::vector<uint16_t> pixelValuesG;
    std::vector<uint16_t> pixelValuesB;
    std::vector<uint8_t> pixelDiv;
#if LIGHTGRAPH_FRACTIONAL_RENDERING
    std::vector<uint16_t> listPixelValuesR;
    std::vector<uint16_t> listPixelValuesG;
    std::vector<uint16_t> listPixelValuesB;
    std::vector<uint16_t> listTouchedPixels;
#endif
    bool autoEnabled = false;
    uint8_t currentPalette = 0;
    bool showIntersections = false;
    bool showConnections = false;
    uint8_t reservedTailSlots = 0;

    explicit State(TopologyObject &obj);
    ~State();

    uint8_t randomModel();
    ColorRGB paletteColor(uint8_t index, uint8_t maxBrightness = FULL_BRIGHTNESS);
    void autoEmit(unsigned long millis);
    int8_t emit(EmitParams &params);
    void emit(LightList& lightList);
    int8_t getOrCreateList(EmitParams &params);
    int8_t setupListFrom(uint8_t i, EmitParams &params);
    Owner* getEmitter(Model* model, Behaviour* behaviour, EmitParams& params);
    void update();
    void updateLight(RuntimeLight* light);
    void colorAll();
    void splitAll();
    void stopAll();
    int8_t findList(uint16_t noteId) const;
    LightList* findListById(uint16_t id);
    void stopNote(uint16_t noteId);
    ColorRGB getPixel(uint16_t i, uint8_t maxBrightness = FULL_BRIGHTNESS);
    void debug();
    bool isOn();
    void setOn(bool newState);
    void setupBg(uint8_t i);
    void activateList(Owner* from, LightList *lightList, uint8_t emitOffset = 0, bool countTotals = true);
    void doEmit(Owner* from, LightList *lightList, uint8_t emitOffset = 0);
    void setReservedTailSlots(uint8_t slots);
    uint8_t getReservedTailSlots() const;
    uint8_t getLocalSlotEndExclusive() const;
    bool clearListSlot(uint8_t slot);
    bool replaceListSlot(uint8_t slot, LightList* replacement);

  private:
    void doEmit(Owner* from, LightList *lightList, EmitParams& params);
    void updatePass(bool renderStep);
    void setPixelsWeighted(uint16_t pixel, const ColorRGB& color, const LightList* const lightList, uint8_t weight);
    void setPixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setPixel(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setFramePixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setFramePixel(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
#if LIGHTGRAPH_FRACTIONAL_RENDERING
    void beginListRender(const LightList* lightList);
    void endListRender(const LightList* lightList);
    void setListPixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setListPixel(uint16_t pixel, ColorRGB &color);
    const LightList* renderingList = nullptr;
#endif

};
