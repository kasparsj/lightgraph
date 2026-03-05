#include <algorithm>
#include <math.h>
#include "Connection.h"
#include "Intersection.h"
#include "TopologyObject.h"
#include "../runtime/Behaviour.h"
#include "../runtime/RuntimeLight.h"

namespace {

bool hasAvailablePortSlot(const Intersection* intersection) {
  if (intersection == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < intersection->numPorts; i++) {
    if (intersection->ports[i] == nullptr) {
      return true;
    }
  }
  return false;
}

bool isPortAttachedToIntersection(const Intersection* intersection, const Port* port) {
  if (intersection == nullptr || port == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < intersection->numPorts; i++) {
    if (intersection->ports[i] == port) {
      return true;
    }
  }
  return false;
}

} // namespace

Connection::Connection(Intersection *from, Intersection *to, uint8_t group, int16_t forceNumLeds) : Owner(group) {
  this->from = from;
  this->to = to;

  fromPort = nullptr;
  toPort = nullptr;
  pixelDir = false;
  fromPixel = 0;
  toPixel = 0;

  if (from == nullptr || to == nullptr) {
    return;
  }
  if (!hasAvailablePortSlot(from) || !hasAvailablePortSlot(to)) {
    return;
  }

  fromPort = new InternalPort(this, from, false, group);
  toPort = new InternalPort(this, to, true, group);

  if (!isPortAttachedToIntersection(from, fromPort) || !isPortAttachedToIntersection(to, toPort)) {
    if (fromPort != nullptr) {
      delete fromPort;
      fromPort = nullptr;
    }
    if (toPort != nullptr) {
      delete toPort;
      toPort = nullptr;
    }
    return;
  }

  pixelDir = to->topPixel > from->topPixel;
  fromPixel = from->topPixel + (pixelDir ? 1 : -1);
  toPixel = to->topPixel - (pixelDir ? 1 : -1);
  
  // If forceNumLeds is provided, use it, otherwise calculate
  if (forceNumLeds > -1) {
    numLeds = forceNumLeds;
  } else {
    uint16_t diff = abs(fromPixel - toPixel);
    uint16_t leds = diff > 4 && diff < (TopologyObject::instance->pixelCount - 4) ? diff + 1 : 0;
    if (from->bottomPixel > -1) {
      if (abs(from->bottomPixel - to->topPixel) < leds) {
        pixelDir = to->topPixel > from->bottomPixel;
        fromPixel = from->bottomPixel + (pixelDir ? 1 : -1);
        toPixel = to->topPixel - (pixelDir ? 1 : -1);
        leds = abs(fromPixel - toPixel) + 1;
      }
    }
    if (to->bottomPixel > -1) {
      if (abs(from->topPixel - to->bottomPixel) < leds) {
        pixelDir = to->bottomPixel > from->topPixel;
        fromPixel = from->topPixel + (pixelDir ? 1 : -1);
        toPixel = to->bottomPixel - (pixelDir ? 1 : -1);
        leds = abs(fromPixel - toPixel) + 1;
      }
    }
    if (from->bottomPixel > -1 && to->bottomPixel > -1) {
      if (abs(from->bottomPixel - to->bottomPixel) < leds) {
        pixelDir = to->bottomPixel > from->bottomPixel;
        fromPixel = from->bottomPixel + (pixelDir ? 1 : -1);
        toPixel = to->bottomPixel - (pixelDir ? 1 : -1);
        leds = abs(fromPixel - toPixel) + 1;
      }
    }
    if (leds > 0) {
      numLeds = leds;
    }
  }
}

Connection::~Connection() {
  if (fromPort != nullptr) {
    delete fromPort;
    fromPort = nullptr;
  }
  if (toPort != nullptr) {
    delete toPort;
    toPort = nullptr;
  }
}

void Connection::add(RuntimeLight* const light) const {
    if (numLeds > 0) {
        Owner::add(light);
    } else {
        outgoing(light);
    }
}

void Connection::emit(RuntimeLight* const light) const {
    light->setOutPort(fromPort, from->id);
    add(light);
}

void Connection::update(RuntimeLight* const light) const {
    if (light == nullptr || light->outPort == nullptr) {
        if (light != nullptr) {
            light->isExpired = true;
            light->owner = NULL;
        }
        return;
    }
    light->resetPixels();
    if (shouldExpire(light)) {
        light->isExpired = true;
        light->owner = NULL;
        return;
    }
    if (render(light)) {
        return;
    }
    outgoing(light);
}

bool Connection::shouldExpire(const RuntimeLight* const light) const {
    const Behaviour *behaviour = light->getBehaviour();
    return (light->shouldExpire() &&
        (light->getSpeed() == 0 || (behaviour != NULL && behaviour->expireImmediately())));
}

bool Connection::render(RuntimeLight* const light) const {
    // handle float inprecision
    float pos = round(light->position * 1000) / 1000.0;
    if (numLeds > 0 && pos < numLeds) {
        pos = ofxeasing::map(light->position, 0, numLeds, 0, numLeds, light->getEasing());
        const float led_idx =
            light->outPort->direction ? ceil((float) numLeds - pos - 1.0) : floor(pos);
        const int32_t clamped_led_idx = std::clamp<int32_t>(
            static_cast<int32_t>(led_idx),
            0,
            static_cast<int32_t>(numLeds) - 1);
        light->pixel1 = getPixel(static_cast<uint16_t>(clamped_led_idx));
        return true;
    }
    return false;
}

inline void Connection::outgoing(RuntimeLight* const light) const {
    light->position -= numLeds;
    const bool dir = light->outPort->direction;
    if (dir) {
        light->setInPort(fromPort);
    }
    else {
        light->setInPort(toPort);
    }
    light->setOutPort(NULL);
    if (dir) {
        from->add(light);
    }
    else {
        to->add(light);
    }
}

uint16_t Connection::getFromPixel() const {
  return fromPort->intersection->topPixel;
}

uint16_t Connection::getToPixel() const {
  return toPort->intersection->topPixel;
}
