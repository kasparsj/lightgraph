#include "Intersection.h"
#include "Connection.h"
#include "Model.h"
#include "../core/Platform.h"
#include "../runtime/Behaviour.h"
#include "../runtime/Light.h"

uint8_t Intersection::nextId = 0;

Intersection::Intersection(uint8_t numPorts, uint16_t topPixel, int16_t bottomPixel, uint8_t group) : Owner(group) {
  this->id = nextId++;
  this->numPorts = numPorts;
  this->topPixel = topPixel;
  this->bottomPixel = bottomPixel;
  ports.assign(numPorts, nullptr);
}

void Intersection::addPort(Port *p) {
  for (uint8_t i=0; i<numPorts; i++) {
    if (ports[i] == nullptr) {
      ports[i] = p;
      p->intersection = this;
      break;
    }
  }
}

bool Intersection::addPortAt(Port* p, uint8_t slotIndex) {
    if (p == nullptr || slotIndex >= numPorts || ports[slotIndex] != nullptr) {
        return false;
    }
    ports[slotIndex] = p;
    p->intersection = this;
    return true;
}

void Intersection::removePort(const Port* p) {
    for (uint8_t i = 0; i < numPorts; i++) {
        if (ports[i] == p) {
            ports[i] = nullptr;
            break;
        }
    }
}

void Intersection::emit(RuntimeLight* const light) const {
    // go straight out of zeroConnection
    const Behaviour *behaviour = light->getBehaviour();
    if (numPorts == 2) {
      for (uint8_t i=0; i<2; i++) {
        Port* port = ports[i];
        if (port == nullptr) {
            continue;
        }
        const bool hasConnection = port->connection != nullptr;
        const bool allowForBounce = hasConnection ? (port->connection->numLeds > 0) : true;
        const bool allowForZero = hasConnection ? (port->connection->numLeds == 0) : true;
        if (behaviour->forceBounce() ? allowForBounce : allowForZero) {
          light->setInPort(ports[i]);
          break;
        }
      }
    }
    light->owner = this;
}

void Intersection::update(RuntimeLight* const light) const {
    if (!light->isExpired) {
        light->resetPixels();
        if (light->shouldExpire()) {
            if (light->getSpeed() == 0 || light->position >= 1.f) { // expire
                light->isExpired = true;
                light->owner = NULL;
            }
            return;
        }
        if (light->position >= 0.f && light->position < 1.f) { // render
            light->pixel1 = topPixel;
            return;
        }
        // sendOut
        Port* port = getPrevOutPort(light);
        if (port == NULL) {
            port = choosePort(light->getModel(), light);
        }
        light->setOutPort(port, id);
        light->setInPort(NULL);
        light->position -= 1.f;
        light->owner = NULL;
        if (port != NULL) {
            port->sendOut(light);
        }
  }
}

Port* Intersection::getPrevOutPort(const RuntimeLight* const light) const {
    Port* port = NULL;
    const RuntimeLight* linkedPrev = light->getPrev();
    if (linkedPrev != NULL) {
        port = linkedPrev->getOutPort(id);
    }
    return port;
}

uint16_t Intersection::sumW(const Model* const model, const Port* const incoming) const {
  uint16_t sum = 0;
  for (uint8_t i=0; i<numPorts; i++) {
    Port *port = ports[i];
    if (port == nullptr) {
        continue;
    }
    sum += model->get(port, incoming);
  }
  return sum;
}

Port* Intersection::randomPort(const Port* const incoming, const Behaviour* const behaviour) const {
  std::vector<Port*> candidates;
  candidates.reserve(numPorts);
  for (uint8_t i = 0; i < numPorts; i++) {
      Port* port = ports[i];
      if (port == nullptr) {
          continue;
      }
      const bool reject = (!behaviour->allowBounce() && behaviour->forceBounce()) ? (port != incoming)
                                                                                   : (port == incoming);
      if (!reject) {
          candidates.push_back(port);
      }
  }
  if (candidates.empty()) {
      return nullptr;
  }
  return candidates[(uint8_t) LP_RANDOM(candidates.size())];
}

Port* Intersection::choosePort(const Model* const model, const RuntimeLight* const light) const {
    Port *incoming = light->inPort;
    if (model == nullptr) {
        return nullptr;
    }

    if (model->getRoutingStrategy() == RoutingStrategy::Deterministic) {
      Port* bestPort = nullptr;
      uint8_t bestWeight = 0;
      for (uint8_t i = 0; i < numPorts; i++) {
        Port* port = ports[i];
        if (port == nullptr || port == incoming) {
          continue;
        }
        const uint8_t weight = model->get(port, incoming);
        if (weight > bestWeight || (weight == bestWeight && bestPort != nullptr && port->id < bestPort->id)) {
          bestWeight = weight;
          bestPort = port;
        } else if (bestPort == nullptr && weight == bestWeight) {
          bestPort = port;
        }
      }
      if (bestPort != nullptr) {
        return bestPort;
      }
      return randomPort(incoming, light->getBehaviour());
    }

    uint16_t sum = sumW(model, incoming);
    if (sum == 0) {
      return randomPort(incoming, light->getBehaviour());
    }
    uint16_t rnd = LP_RANDOM(sum);
    for (uint8_t i=0; i<numPorts; i++) {
       Port *port = ports[i];
       if (port == nullptr) {
           continue;
       }
       uint8_t w = model->get(port, incoming);
       if (port == incoming || w == 0) continue;
       if (rnd < w) {
         return port;
       }
       rnd -= w;
    }
    return NULL;
  }
