#include "RuntimeLight.h"
#include "LightList.h"
#include "../topology/Connection.h"
#include "../topology/Port.h"
#include "../../vendor/ofxEasing/ofxEasing.h"
#include "../Globals.h"

uint16_t RuntimeLight::pixels[CONNECTION_MAX_LEDS] = {0};

void RuntimeLight::resetPixels() {
  pixel1 = -1;
  // pixel2 = -1;
}

Port* RuntimeLight::getOutPort(uint8_t intersectionId) const {
  for (uint8_t i=0; i<OUT_PORTS_MEMORY; i++) {
    if (outPortsInt[i] == intersectionId) {
      return outPorts[i];
    }
  }
  return NULL;
}

void RuntimeLight::setOutPort(Port* const port, int8_t intersectionId) {
  outPort = port;
  if (intersectionId > -1) {
    for (uint8_t i=(OUT_PORTS_MEMORY-1); i>0; i--) {
      outPorts[i] = outPorts[i-1];
      outPortsInt[i] = outPortsInt[i-1];
    }
    outPorts[0] = port;
    outPortsInt[0] = intersectionId;
  }
}

void RuntimeLight::update() {
    if (owner) {
        owner->update(this);
    }
    brightness = getBrightness();
}

uint8_t RuntimeLight::getBrightness() const {
    uint16_t value = bri % 511;
    value = (value > 255 ? 511 - value : value);
    value = (float) (value - list->fadeThresh) / (255 - list->fadeThresh) * 511.f;
    if (value > 0) {
        return ofxeasing::map(value, 0, 511, list->minBri, maxBri, list->fadeEase);
    }
    return 0;
}

ColorRGB RuntimeLight::getPixelColor() const {
    if (brightness == 255) {
        return list->getColor(pixel1);
    }
    return list->getColor(pixel1).Dim(brightness);
}

uint16_t* RuntimeLight::getPixels() {
  if (pixel1 >= 0) {
    const Behaviour *behaviour = getBehaviour();
    if (behaviour != NULL && behaviour->renderSegment()) {
        setSegmentPixels();
    }
    else if (behaviour != NULL && behaviour->fillEase()) {
        setLinkPixels();
    }
    else {
        setPixel1();
    }
    return pixels;
  }
  return NULL;
}

void RuntimeLight::setPixel1() {
    pixels[0] = 1;
    pixels[1] = pixel1;
}

void RuntimeLight::setSegmentPixels() {
    if (outPort != NULL) {
        if (CONNECTION_MAX_LEDS < 3) {
            setPixel1();
            return;
        }
        const uint16_t maxPayload = CONNECTION_MAX_LEDS - 1;
        uint16_t numPixels = outPort->connection->numLeds;
        if (numPixels + 2 > maxPayload) {
            numPixels = maxPayload - 2;
        }
        pixels[0] = numPixels + 2;
        pixels[1] = outPort->connection->getFromPixel();
        pixels[2] = outPort->connection->getToPixel();
        for (uint16_t i=0; i<numPixels; i++) {
            pixels[i + 3] = outPort->connection->getPixel(i);
        }
    }
    else {
        setPixel1();
    }
}

void RuntimeLight::setLinkPixels() {
    RuntimeLight* prev = getPrev();
    if (prev != NULL && owner == prev->owner) {
        if (CONNECTION_MAX_LEDS < 2) {
            setPixel1();
            return;
        }
        uint16_t numPixels = abs(pixel1 - prev->pixel1);
        const uint16_t maxPayload = CONNECTION_MAX_LEDS - 1;
        if (numPixels > maxPayload) {
            numPixels = maxPayload;
        }
        pixels[0] = numPixels;
        for (uint16_t i=1; i<numPixels+1; i++) {
            pixels[i] = pixel1 + (i-1) * (pixel1 < prev->pixel1 ? 1 : -1);
        }
    }
    else {
        setPixel1();
    }
}

void RuntimeLight::nextFrame() {
  bri = list->getBri(this);
  position = list->getPosition(this);
}

bool RuntimeLight::shouldExpire() const {
  if (list->lifeMillis >= INFINITE_DURATION) {
    return false;
  }
  return gMillis >= (list->lifeMillis + lifeMillis) && (list->fadeSpeed == 0 || brightness == 0);
}

RuntimeLight* RuntimeLight::getPrev() const {
    return idx > 0 ? (*list)[idx - 1] : NULL;
}

RuntimeLight* RuntimeLight::getNext() const {
    return ((idx+1) < list->numLights) ? (*list)[idx+1] : NULL;
}

float RuntimeLight::getSpeed() const {
    return list->speed;
}

ofxeasing::function RuntimeLight::getEasing() const {
    return list->ease;
}

uint32_t RuntimeLight::getLife() const {
    return list->lifeMillis;
}

ColorRGB RuntimeLight::getColor() const {
    return list->getColor();
}

const Model* RuntimeLight::getModel() const {
    return list->model;
}

const Behaviour* RuntimeLight::getBehaviour() const {
    return list->behaviour;
}

uint16_t RuntimeLight::getListId() const {
    return list->id;
}

float RuntimeLight::getFadeSpeed() const {
    return list->fadeSpeed;
}
