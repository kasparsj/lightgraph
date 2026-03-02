#include "Debugger.h"

#include <array>
#include <limits>

#include "../core/Platform.h"
#include "../topology/TopologyObject.h"

Debugger::Debugger(TopologyObject &object) : object(object) {
    interPixels = new bool[object.pixelCount]{false};
    connPixels = new bool[object.pixelCount]{false};
    constexpr size_t kPortIdCount = static_cast<size_t>(std::numeric_limits<uint8_t>::max()) + 1u;
    std::array<Port*, kPortIdCount> ports{};
    for (uint8_t i=0; i<MAX_GROUPS; i++) {
        for (uint j=0; j<object.inter[i].size(); j++) {
            Intersection* intersection = object.inter[i][j];
            if (intersection->topPixel < object.pixelCount) {
                interPixels[intersection->topPixel] = true;
            }
            if (intersection->bottomPixel >= 0 &&
                static_cast<uint16_t>(intersection->bottomPixel) < object.pixelCount) {
                interPixels[intersection->bottomPixel] = true;
            }
            for (uint8_t k=0; k<intersection->numPorts; k++) {
                Port *port = intersection->ports[k];
                if (port == nullptr) {
                    continue;
                }
                const size_t portId = port->id;
                if (portId < ports.size()) {
                    ports[portId] = port;
                }
            }
        }
        for (uint j=0; j<object.conn[i].size(); j++) {
            Connection* connection = object.conn[i][j];
            if (connection->fromPixel < object.pixelCount) {
                connPixels[connection->fromPixel] = true;
            }
            if (connection->toPixel < object.pixelCount) {
                connPixels[connection->toPixel] = true;
            }
        }
    }
    weightPixels = new bool*[object.models.size()];
    for (size_t i = 0; i < object.models.size(); i++) {
        weightPixels[i] = new bool[object.pixelCount]{false};
        Model* model = object.models[i];
        if (model == nullptr) {
            continue;
        }
        if (model->id >= object.models.size()) {
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
            if (pixel >= object.pixelCount) {
                continue;
            }
            weightPixels[model->id][pixel] = true;
        }
    }
}

Debugger::~Debugger() {
    delete[] interPixels;
    delete[] connPixels;
    for (size_t i = 0; i < object.models.size(); i++) {
        delete[] weightPixels[i];
        weightPixels[i] = NULL;
    }
    delete[] weightPixels;
}

void Debugger::update(unsigned long millis) {
    fps[fpsIndex] = 1000.f / float(millis - prevMillis);
    fpsIndex = (fpsIndex + 1) % AVG_FPS_FRAMES;
    emitsIndex = (emitsIndex + 1) % AVG_FPS_FRAMES;
    numEmits[emitsIndex] = 0;
    prevMillis = millis;
}

void Debugger::countEmit() {
    numEmits[emitsIndex]++;
}

float Debugger::getFPS() {
    float avg = 0;
    for (uint8_t i=0; i<AVG_FPS_FRAMES; i++) {
        avg += fps[i];
    }
    return avg / AVG_FPS_FRAMES;
}

float Debugger::getNumEmits() {
    float sum = 0;
    for (uint8_t i=0; i<AVG_FPS_FRAMES; i++) {
        sum += numEmits[i];
    }
    return sum / AVG_FPS_FRAMES;
}

bool Debugger::isModelWeight(uint8_t id, uint16_t i) {
  if (id >= object.models.size() || i >= object.pixelCount || weightPixels[id] == nullptr) {
    return false;
  }
  return weightPixels[id][i];
}

bool Debugger::isIntersection(uint16_t i) {
  if (i >= object.pixelCount) {
    return false;
  }
  return interPixels[i];
}

bool Debugger::isConnection(uint16_t i) {
  if (i >= object.pixelCount) {
    return false;
  }
  return connPixels[i];
}

void Debugger::dumpConnections() {
  LP_LOGLN("--- CONNECTIONS ---");
  for (uint8_t i=0; i<MAX_GROUPS; i++) {
      for (uint8_t j=0; j<object.conn[i].size(); j++) {
        LP_LOGF("Connection%d %d - %d\n", i, object.conn[i][j]->fromPixel, object.conn[i][j]->toPixel);
      }
  }
}

void Debugger::dumpIntersections() {
  LP_LOGLN("--- INTERSECTIONS ---");
  for (uint8_t i=0; i<MAX_GROUPS; i++) {
      for (uint8_t j=0; j<object.inter[i].size(); j++) {
        LP_LOGF("Intersection%d %d\n", i, object.inter[i][j]->id);
      }
  }
}
