#ifndef PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_OWNER_H_
#define PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_OWNER_H_

#pragma once

#include <cstdint>

class RuntimeLight;

class Owner
{
  public:
    static const uint8_t TYPE_INTERSECTION = 0;
    static const uint8_t TYPE_CONNECTION = 1;
    
    uint8_t group;

    Owner(uint8_t group) : group(group) {}
    virtual ~Owner() = default;

    virtual uint8_t getType() = 0;
    virtual void emit(RuntimeLight* const light) const = 0;
    void add(RuntimeLight* const light) const;
    virtual void update(RuntimeLight* const light) const = 0;
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_OWNER_H_
