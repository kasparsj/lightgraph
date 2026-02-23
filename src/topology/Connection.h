#pragma once

#include "../Config.h"
#include "LPOwner.h"
#include "Port.h"
#include "../runtime/LPLight.h"

class Intersection;

class Connection : public LPOwner {

  public:
    Intersection* from;
    Intersection* to;
    Port* fromPort;
    Port* toPort;
    uint16_t numLeds = 0;
    bool pixelDir;
    uint16_t fromPixel;
    uint16_t toPixel;
    
    Connection(Intersection *from, Intersection *to, uint8_t group, int16_t numLeds = -1);
    ~Connection() override;
    
    uint8_t getType() override { return TYPE_CONNECTION; };
    inline void add(LPLight* const light) const {
        if (numLeds > 0) {
            LPOwner::add(light);
        }
        else {
            outgoing(light);
        }
    }
    void emit(LPLight* const light) const override;
    void update(LPLight* const light) const override;
    uint16_t getPixel(uint16_t i) const {
      return fromPixel + (i * (pixelDir ? 1 : -1));
    }
    uint16_t getFromPixel() const;
    uint16_t getToPixel() const;
    
  private:
    void outgoing(LPLight* const light) const;
    bool shouldExpire(const LPLight* const light) const;
    bool render(LPLight* const light) const;
};
