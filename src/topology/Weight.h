#pragma once

#include "../core/Limits.h"
#include "Port.h"
#include "../HashMap.h"

class Weight {

  public:
    
    Weight(uint8_t w) : w(w) {
        conditional = new HashMap<uint8_t, uint8_t>(MAX_CONDITIONAL_WEIGHTS);
    }
    ~Weight() {
        delete conditional;
    }
    
    void add(const Port *incoming, uint8_t w);
    uint8_t get(const Port *incoming) const;
    void remove(const Port *incoming);
    
  private:
    uint8_t w;
    HashMap<uint8_t, uint8_t> *conditional;
  
};
