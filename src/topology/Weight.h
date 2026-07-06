#ifndef PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_WEIGHT_H_
#define PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_WEIGHT_H_

#pragma once

#include "../core/Limits.h"
#include "Port.h"
#include <unordered_map>

class Weight {

  public:
    
    Weight(uint8_t w) : w(w) {}
    
    void add(const Port *incoming, uint8_t w);
    void add(uint16_t incomingPortId, uint8_t w);
    uint8_t get(const Port *incoming) const;
    void remove(const Port *incoming);
    uint8_t defaultWeight() const { return w; }
    const std::unordered_map<uint16_t, uint8_t>& conditionalWeights() const { return conditional; }
    
  private:
    uint8_t w;
    std::unordered_map<uint16_t, uint8_t> conditional;
  
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_WEIGHT_H_
