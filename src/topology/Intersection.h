#pragma once

#include <cstdint>
#include <vector>
#include "LPOwner.h"
#include "Port.h"

class Behaviour;
class Model;

class Intersection : public LPOwner {

  public:

    static uint8_t nextId;
  
    uint8_t id;
    uint8_t numPorts;
    std::vector<Port*> ports; // typically 2, 3, or 4 ports
    uint16_t topPixel;
    int16_t bottomPixel;

    Intersection(uint8_t numPorts, uint16_t topPixel, int16_t bottomPixel, uint8_t group);
    Intersection(uint16_t topPixel, int16_t bottomPixel, uint8_t group) : Intersection(4, topPixel, bottomPixel, group) {
    }
    Intersection(uint16_t topPixel, uint8_t group) : Intersection(2, topPixel, -1, group) {

    }
  
    ~Intersection() override = default;
    
    uint8_t getType() override { return TYPE_INTERSECTION; };
    void addPort(Port *p);
    void removePort(const Port* p);
    void emit(LPLight* const light) const override;
    void update(LPLight* const light) const override;

  private:

    uint16_t sumW(const Model* const model, const Port* const incoming) const;
    Port* randomPort(const Port* const incoming, const Behaviour* const behaviour) const;
    Port* choosePort(const Model* const model, const LPLight* const light) const;
    Port* getOutPortFor(const LPLight* const light) const;
    Port* getPrevOutPort(const LPLight* const light) const;
  
};
