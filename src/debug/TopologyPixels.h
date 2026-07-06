#ifndef PACKAGES_LIGHTGRAPH_SRC_DEBUG_TOPOLOGYPIXELS_H_
#define PACKAGES_LIGHTGRAPH_SRC_DEBUG_TOPOLOGYPIXELS_H_

#pragma once

#include <cstddef>
#include <cstdint>

class TopologyObject;

class TopologyPixels {
  public:
    explicit TopologyPixels(TopologyObject& object);
    virtual ~TopologyPixels();

    void refresh();

    bool isModelWeight(uint8_t id, uint16_t i) const;
    bool isIntersection(uint16_t i) const;
    bool isConnection(uint16_t i) const;
    void dumpConnections() const;
    void dumpIntersections() const;

  protected:
    TopologyObject& object;

  private:
    void freeBuffers();
    void allocateBuffers();

    bool** weightPixels = nullptr;
    bool* interPixels = nullptr;
    bool* connPixels = nullptr;
    size_t weightRows = 0;
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_DEBUG_TOPOLOGYPIXELS_H_
