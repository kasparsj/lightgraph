#include "LightList.h"
#include "Light.h"
#include "../core/Platform.h"
#include "../topology/Model.h"
#include "../Globals.h"
#include <stdio.h>
#include <vector>

uint16_t LightList::nextId = 0;

void LightList::init(uint16_t numLights) {
    if (lights != NULL) {
        for (uint16_t i = 0; i < allocatedLights; i++) {
            delete lights[i];
        }
        delete[] lights;
        lights = NULL;
        allocatedLights = 0;
    }
    this->numLights = numLights;
    allocatedLights = numLights;
    numEmitted = 0;
    lights = new LPLight*[numLights]();
}

void LightList::setup(uint16_t numLights, uint8_t maxBri) {
    init(lead + numLights + trail);
    this->maxBri = maxBri;
    for (uint16_t i=0; i<this->numLights; i++) {
        createLight(i, maxBri);
    }
}

float LightList::getBriMult(uint16_t i) {
    float mult = 1.f;
    if (i < lead) {
        mult = (255.f / (lead + 1)) * (i + 1) / 255.f;
    }
    else if (i >= lead + body()) {
        uint16_t j = i - (lead + body());
        mult = (255.f - (255.f / (trail + 1)) * (j + 1)) / 255.f;
    }
    return mult;
}

LPLight* LightList::createLight(uint16_t i, uint8_t brightness) {
    float mult = getBriMult(i);
    LPLight *light;
    // todo: fix if statement
    if (behaviour != NULL/* && behaviour->colorChangeGroups > 0*/) {
        light = new Light(this, speed, lifeMillis, linked ? i : 0, brightness * mult);
    }
    else {
        light = new LPLight(this, linked ? i : 0, brightness * mult);
    }
    (*this)[i] = light;
    return light;
}

LPLight* LightList::addLightFromMsg(const LightMessage* lightMsg) {
    LPLight* light;
    if (behaviour != NULL) {
        light = new Light(this, lightMsg->speed, lightMsg->life, lightMsg->lightIdx, lightMsg->brightness);
    }
    else {
        light = new LPLight(this, lightMsg->lightIdx, lightMsg->brightness);
    }
    return light;
}

void LightList::setDuration(uint32_t durMillis) {
    this->duration = durMillis;
    this->lifeMillis = MIN(gMillis + durMillis, INFINITE_DURATION);
    for (uint16_t i=0; i<numLights; i++) {
        if ((*this)[i] == 0) continue;
        ((*this)[i])->setDuration(durMillis);
    }
}

void LightList::setLightColors() {
    if (numLights > 0) {
        for (uint16_t i=0; i<numLights; i++) {
            if ((*this)[i] == 0) continue;
            ((*this)[i])->setColor(getLightColor(i));
        }
    }
}

void LightList::setLeadTrail(uint16_t trail) {
    if (head == LIST_HEAD_FRONT) {
        if (trail > 0) {
            this->lead = 1;
            trail -= 1;
        }
        this->trail = trail;
    }
    else if (head == LIST_HEAD_BACK) {
        this->lead = trail;
    }
    else {
        this->lead = (uint16_t) trail / 2;
        this->trail = (uint16_t) ceil(trail / 2.f);
    }
}

void LightList::setupFrom(const EmitParams &params) {
    order = params.order;
    head = params.head;
    linked = params.linked;
    minBri = params.minBri;
    
    setSpeed(params.getSpeed(), params.ease);
    setFade(params.fadeSpeed, params.fadeThresh, params.fadeEase);
    noteId = params.noteId;
    uint16_t numTrail = params.speed == 0 ? params.trail : params.getSpeedTrail(speed, length);
    maxBri = params.getMaxBri();
    numLights = max(1, length - numTrail);
    setLeadTrail(numTrail);
    
    duration = params.getDuration();
    palette = params.palette;
    reset();
}

void LightList::initEmit(uint8_t posOffset) {
    for (uint16_t i=0; i<numLights; i++) {
        LPLight *light = (*this)[i];
        initPosition(i, light);
        light->position += posOffset;
        initBri(i, light);
        initLife(i, light);
    }
}

float LightList::getPosition(LPLight* const light) const {
  if (behaviour != NULL) {
    return behaviour->getPosition(light);
  }
  return light->position + light->getSpeed();
}

void LightList::initPosition(uint16_t i, LPLight* const light) const {
  float position = (speed != 0 ? i * -1.f : numLights - 1 - i * 1.f);
  if (order == LIST_ORDER_RANDOM) {
    position = LP_RANDOM(model->getMaxLength());
  }
  light->position = position;
}

void LightList::initBri(uint16_t i, LPLight* const light) const {
  switch (order) {
    case LIST_ORDER_RANDOM:
      if (fadeThresh > 0) {
        light->bri = LP_RANDOM(fadeThresh * 3);
      }
      break;
    case LIST_ORDER_NOISE:
      light->bri = gPerlinNoise.GetValue(id * 10, i * 100) * FULL_BRIGHTNESS;
      break;
    default:
      break;
  }
}

uint16_t LightList::getBri(const LPLight* light) const {
  if (behaviour != NULL) {
    return behaviour->getBri(light);
  }
  return light->bri + fadeSpeed;
}

void LightList::initLife(uint16_t i, LPLight* const light) const {
  uint32_t lifeMillis = light->lifeMillis;
  if (order == LIST_ORDER_SEQUENTIAL && light->getSpeed() > 0) {
    lifeMillis += ceil(1.f / light->getSpeed() * i) * EmitParams::frameMs();
  }
  light->lifeMillis = lifeMillis;
}

bool LightList::update() {
    doEmit();
    bool allExpired = true;
    for (uint16_t j=0; j<numLights; j++) {
        LPLight* const light = lights[j];
        if (light == NULL) continue;
        if (light->isExpired) {
          LPLight* const next = light->getNext();
          if (next != NULL) {
            next->idx = 0;
          }
          delete lights[j];
          lights[j] = NULL;
          continue;
        }
        allExpired = false;
        light->update();
    }
    return allExpired;
}

void LightList::doEmit() {
    if (emitter == NULL) {
        LP_LOGF("LightList::doEmit failed: emitter NULL");
        return;
    }
    if (numEmitted < numLights) {
        const uint16_t batchSize = numLights - numEmitted;
        const uint16_t j = numEmitted;
        for (uint16_t i=0; i<batchSize; i++) {
            LPLight* const light = (*this)[i+j];
            if (light->position < 0) {
                break;
            }
            numEmitted++;
            emitter->emit(light);
        }
    }
}

void LightList::split() {
    numSplits++;
    if (numSplits < numLights) {
    for (uint8_t i=0; i<numSplits; i++) {
      uint16_t split = (i+1)*(numLights/(numSplits+1));
      if ((*this)[split] == 0) continue;
      (*this)[split]->idx = 0;
    }
    // todo: modify trail
  }
}

float LightList::getOffset() const {
    if (numLights > 0 && lights != NULL && lights[0] != NULL) {
        return lights[0]->position;
    }
    return 0.0f;
}

void LightList::setOffset(float newPosition) {
    if (numLights > 0 && lights != NULL && lights[0] != NULL) {
        float currentPosition = lights[0]->position;
        float offset = newPosition - currentPosition;
        
        // Apply the offset to all lights in the list
        for (uint16_t i = 0; i < numLights; i++) {
            if (lights[i] != NULL) {
                lights[i]->position += offset;
            }
        }
    }
}

void LightList::reset() {
    numEmitted = 0;
    numSplits = 0;
    setup(numLights, maxBri);
    setDuration(duration);
    setPalette(palette);
}
