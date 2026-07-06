#ifndef PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_CONNECTION_H_
#define PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_CONNECTION_H_

#pragma once

#include <cstdint>
#include "Owner.h"
#include "Port.h"

class Intersection;
class RuntimeLight;
class TopologyObject;

class Connection : public Owner {

  public:
    Intersection* from;
    Intersection* to;
    Port* fromPort;
    Port* toPort;
    TopologyObject* object = nullptr;
    uint16_t numLeds = 0;
    bool pixelDir;
    uint16_t fromPixel;
    uint16_t toPixel;
    
    Connection(Intersection *from, Intersection *to, uint8_t group, int16_t numLeds = -1);
    ~Connection() override;
    
    uint8_t getType() override { return TYPE_CONNECTION; };
    void add(RuntimeLight* const light) const;
    void emit(RuntimeLight* const light) const override;
    void update(RuntimeLight* const light) const override;
    uint16_t getPixel(uint16_t i) const {
      return fromPixel + (i * (pixelDir ? 1 : -1));
    }
    uint16_t getFromPixel() const;
    uint16_t getToPixel() const;
    void attachToObject(TopologyObject& object);
    
  private:
    void configurePixels(uint16_t objectPixelCount);
    void outgoing(RuntimeLight* const light) const;
    bool shouldExpire(const RuntimeLight* const light) const;
    bool render(RuntimeLight* const light) const;
    int16_t forcedNumLeds_ = -1;
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_CONNECTION_H_
