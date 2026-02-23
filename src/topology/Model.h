#pragma once

#include "Weight.h"
#include "Port.h"
#include "Connection.h"
#include "Intersection.h"

class Model {

  public:

    static uint8_t maxWeights;
     
    uint8_t id;
    uint8_t defaultW;
    uint8_t emitGroups;
    uint16_t maxLength;
    HashMap<uint8_t, Weight*> *weights;
    
    Model(uint8_t id, uint8_t defaultW, uint8_t emitGroups, uint16_t maxLength = 0)
        : id(id), defaultW(defaultW), emitGroups(emitGroups), maxLength(maxLength) {
        weights = new HashMap<uint8_t, Weight*>(maxWeights);
        weights->setNullValue(NULL);
    }
  
    ~Model() {
        for (unsigned int i = 0; i < weights->size(); i++) {
            delete weights->valueAt(i);
        }
        delete weights;
    }
    
    void put(Port *outgoing, Port *incoming, uint8_t weight) {
      _getOrCreate(outgoing, defaultW)->add(incoming, weight);
      _getOrCreate(incoming, defaultW)->add(outgoing, weight);
    }
    
    void put(Port *outgoing, uint8_t w) {
      _getOrCreate(outgoing, w);
    }
    
    void put(Connection *con, uint8_t w1, uint8_t w2) {
      put(con->fromPort, w1);
      put(con->toPort, w2);
    }
    
    void put(Connection *con, uint8_t w) {
      put(con, w, w);
    }
    
    uint8_t get(const Port *outgoing, const Port *incoming) const {
      if (outgoing == incoming) {
        return 0;
      }
      Weight *weight = weights->get(outgoing->id);
      if (weight != NULL) {
        return weight->get(incoming);      
      }
      return defaultW;
    }
    
    Weight *_getOrCreate(Port *outgoing, uint8_t w) {
        Weight *weight = weights->get(outgoing->id);
        if (weight == NULL) {
            weight = new Weight(w);
            weights->set(outgoing->id, weight);
        }
        return weight;
    }

    uint16_t getMaxLength() const;
};
