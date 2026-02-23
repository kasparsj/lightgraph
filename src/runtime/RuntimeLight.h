#pragma once

#include "../core/Types.h"
#include "../core/Limits.h"
#include "../../vendor/ofxEasing/ofxEasing.h"

class LightList;
class Model;
class Behaviour;
class Owner;
class Port;

class RuntimeLight
{

  public:

    uint16_t idx;
    uint8_t maxBri;
    LightList *list;
    Port *inPort = 0;
    Port *outPort = 0;
    Port *outPorts[OUT_PORTS_MEMORY] = {0}; // 4 bytes * 7
    int8_t outPortsInt[OUT_PORTS_MEMORY] = {-1};
    int16_t pixel1 = -1;
    bool isExpired = false;
    float position;
    uint16_t bri = 255;
    uint8_t brightness = 0;
    const Owner *owner = 0;
    uint32_t lifeMillis = 0; // for RuntimeLight this is offsetMillis

    RuntimeLight(LightList* const list, uint16_t idx = 0, uint8_t maxBri = 255) : idx(idx), maxBri(maxBri), list(list) {
        position = -1;
    }
    virtual ~RuntimeLight() = default;

    void setInPort(Port* const port) {
      inPort = port;
    }
    Port* getOutPort(uint8_t intersectionId) const;
    void setOutPort(Port* const port, int8_t intersectionId = -1);
    void resetPixels();
    void update();
    virtual void nextFrame();
    virtual bool shouldExpire() const;

    RuntimeLight* getPrev() const;
    RuntimeLight* getNext() const;
    virtual const Model* getModel() const;
    virtual const Behaviour* getBehaviour() const;
    virtual float getSpeed() const;
    virtual ofxeasing::function getEasing() const;
    virtual float getFadeSpeed() const;
    virtual uint32_t getLife() const;
    virtual void setDuration(uint32_t /*durMillis*/) {}
    virtual ColorRGB getColor() const;
    virtual void setColor(ColorRGB /*color*/) {}
    virtual uint8_t getBrightness() const;
    virtual ColorRGB getPixelColor() const;
    uint16_t* getPixels();
    uint16_t getListId() const;

  private:
    void setPixel1();
    void setSegmentPixels();
    void setLinkPixels();

    static uint16_t pixels[CONNECTION_MAX_LEDS];
};
