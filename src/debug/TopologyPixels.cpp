#include "TopologyPixels.h"

#include <array>
#include <limits>

#include "../core/Platform.h"
#include "../topology/TopologyObject.h"

TopologyPixels::TopologyPixels(TopologyObject& object) : object(object) {
  refresh();
}

TopologyPixels::~TopologyPixels() {
  freeBuffers();
}

void TopologyPixels::freeBuffers() {
  if (weightPixels != nullptr) {
    for (size_t i = 0; i < weightRows; i++) {
      delete[] weightPixels[i];
      weightPixels[i] = nullptr;
    }
    delete[] weightPixels;
    weightPixels = nullptr;
  }
  delete[] interPixels;
  interPixels = nullptr;
  delete[] connPixels;
  connPixels = nullptr;
  weightRows = 0;
}

void TopologyPixels::allocateBuffers() {
  const uint16_t pixelCount = object.pixelCount;
  const size_t modelCount = object.models.size();

  interPixels = new bool[pixelCount]{false};
  connPixels = new bool[pixelCount]{false};

  weightRows = modelCount;
  weightPixels = new bool*[modelCount];
  for (size_t i = 0; i < modelCount; i++) {
    weightPixels[i] = new bool[pixelCount]{false};
  }
}

void TopologyPixels::refresh() {
  freeBuffers();
  allocateBuffers();

  const uint16_t pixelCount = object.pixelCount;
  constexpr size_t kPortIdCount = static_cast<size_t>(std::numeric_limits<uint8_t>::max()) + 1u;
  std::array<Port*, kPortIdCount> ports{};

  for (uint8_t i = 0; i < MAX_GROUPS; i++) {
    for (uint32_t j = 0; j < object.inter[i].size(); j++) {
      Intersection* intersection = object.inter[i][j];
      if (intersection == nullptr) {
        continue;
      }
      if (intersection->topPixel < pixelCount) {
        interPixels[intersection->topPixel] = true;
      }
      if (intersection->bottomPixel >= 0 &&
          static_cast<uint16_t>(intersection->bottomPixel) < pixelCount) {
        interPixels[intersection->bottomPixel] = true;
      }
      for (uint8_t k = 0; k < intersection->numPorts; k++) {
        Port* port = intersection->ports[k];
        if (port == nullptr) {
          continue;
        }
        const size_t portId = port->id;
        if (portId < ports.size()) {
          ports[portId] = port;
        }
      }
    }

    for (uint32_t j = 0; j < object.conn[i].size(); j++) {
      Connection* connection = object.conn[i][j];
      if (connection == nullptr) {
        continue;
      }
      if (connection->fromPixel < pixelCount) {
        connPixels[connection->fromPixel] = true;
      }
      if (connection->toPixel < pixelCount) {
        connPixels[connection->toPixel] = true;
      }
    }
  }

  for (size_t i = 0; i < object.models.size(); i++) {
    Model* model = object.models[i];
    if (model == nullptr || model->id >= object.models.size()) {
      continue;
    }
    for (const auto& entry : model->weights) {
      const size_t portId = entry.first;
      if (portId >= ports.size()) {
        continue;
      }
      Port* port = ports[portId];
      if (port == nullptr || port->intersection == nullptr) {
        continue;
      }
      const uint16_t pixel = port->intersection->topPixel;
      if (pixel >= pixelCount) {
        continue;
      }
      weightPixels[model->id][pixel] = true;
    }
  }
}

bool TopologyPixels::isModelWeight(uint8_t id, uint16_t i) const {
  if (id >= weightRows || i >= object.pixelCount || weightPixels[id] == nullptr) {
    return false;
  }
  return weightPixels[id][i];
}

bool TopologyPixels::isIntersection(uint16_t i) const {
  if (i >= object.pixelCount || interPixels == nullptr) {
    return false;
  }
  return interPixels[i];
}

bool TopologyPixels::isConnection(uint16_t i) const {
  if (i >= object.pixelCount || connPixels == nullptr) {
    return false;
  }
  return connPixels[i];
}

void TopologyPixels::dumpConnections() const {
  LG_LOGLN("--- CONNECTIONS ---");
  for (uint8_t i = 0; i < MAX_GROUPS; i++) {
    for (uint32_t j = 0; j < object.conn[i].size(); j++) {
      Connection* connection = object.conn[i][j];
      if (connection == nullptr) {
        continue;
      }
      LG_LOGF("Connection%d %d - %d\n", i, connection->fromPixel, connection->toPixel);
    }
  }
}

void TopologyPixels::dumpIntersections() const {
  LG_LOGLN("--- INTERSECTIONS ---");
  for (uint8_t i = 0; i < MAX_GROUPS; i++) {
    for (uint32_t j = 0; j < object.inter[i].size(); j++) {
      Intersection* intersection = object.inter[i][j];
      if (intersection == nullptr) {
        continue;
      }
      LG_LOGF("Intersection%d %d\n", i, intersection->id);
    }
  }
}
