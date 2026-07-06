#ifndef PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_MODEL_H_
#define PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_MODEL_H_

#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>
#include "Weight.h"
#include "Port.h"
#include "Connection.h"
#include "Intersection.h"

class TopologyObject;

enum class RoutingStrategy : uint8_t {
    WeightedRandom = 0,
    Deterministic = 1,
};

class Model {

  public:

    static uint8_t maxWeights;
     
    uint8_t id;
    uint8_t defaultW;
    uint8_t emitGroups;
    uint16_t maxLength;
    std::unordered_map<uint16_t, std::unique_ptr<Weight>> weights;
    RoutingStrategy routingStrategy = RoutingStrategy::WeightedRandom;
    
    Model(uint8_t id, uint8_t defaultW, uint8_t emitGroups, uint16_t maxLength = 0,
          RoutingStrategy routingStrategy = RoutingStrategy::WeightedRandom)
        : id(id), defaultW(defaultW), emitGroups(emitGroups), maxLength(maxLength),
          routingStrategy(routingStrategy) {}
  
    ~Model() = default;
    
    void put(Port *outgoing, Port *incoming, uint8_t weight) {
      Weight* outgoingWeight = _getOrCreate(outgoing, defaultW);
      Weight* incomingWeight = _getOrCreate(incoming, defaultW);
      if (outgoingWeight != nullptr) {
        outgoingWeight->add(incoming, weight);
      }
      if (incomingWeight != nullptr) {
        incomingWeight->add(outgoing, weight);
      }
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
      if (outgoing == nullptr) {
        return 0;
      }
      if (outgoing == incoming) {
        return 0;
      }
      const auto it = weights.find(outgoing->id);
      if (it != weights.end() && it->second != nullptr) {
        return it->second->get(incoming);
      }
      return defaultW;
    }
    
    Weight *_getOrCreate(Port *outgoing, uint8_t w) {
        if (outgoing == nullptr) {
            return nullptr;
        }
        auto it = weights.find(outgoing->id);
        if (it == weights.end()) {
            auto created = std::make_unique<Weight>(w);
            Weight* raw = created.get();
            weights.emplace(outgoing->id, std::move(created));
            return raw;
        }
        return it->second.get();
    }

    void removePort(const Port* port) {
      if (port == nullptr) {
        return;
      }
      weights.erase(port->id);
      for (auto& entry : weights) {
        if (entry.second != nullptr) {
          entry.second->remove(port);
        }
      }
    }

    void clearWeights() {
      weights.clear();
    }

    size_t weightCount() const {
      return weights.size();
    }

    void setRoutingStrategy(RoutingStrategy strategy) {
      routingStrategy = strategy;
    }

    RoutingStrategy getRoutingStrategy() const {
      return routingStrategy;
    }

    uint16_t getMaxLength() const;

  private:
    TopologyObject* object_ = nullptr;

    friend class TopologyObject;
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_TOPOLOGY_MODEL_H_
