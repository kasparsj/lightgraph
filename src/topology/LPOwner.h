#pragma once

#include "../Config.h"
#include "../runtime/LPLight.h"

class LPOwner
{
  public:
    static const uint8_t TYPE_INTERSECTION = 0;
    static const uint8_t TYPE_CONNECTION = 1;
    
    uint8_t group;

    LPOwner(uint8_t group) : group(group) {}
    virtual ~LPOwner() = default;

    virtual uint8_t getType() = 0;
    virtual void emit(LPLight* const light) const = 0;
    inline void add(LPLight* const light) const {
        light->owner = this;
        light->owner->update(light);
    }
    virtual void update(LPLight* const light) const = 0;
};
