#include "LPDebugger.h"

LPDebugger::LPDebugger(LPObject &object) : object(object) {
    interPixels = new bool[object.pixelCount]{false};
    connPixels = new bool[object.pixelCount]{false};
    Port* ports[255] = {};
    for (uint8_t i=0; i<MAX_GROUPS; i++) {
        for (uint j=0; j<object.inter[i].size(); j++) {
            Intersection* intersection = object.inter[i][j];
            interPixels[intersection->topPixel] = true;
            if (intersection->bottomPixel >= 0) {
                interPixels[intersection->bottomPixel] = true;
            }
            for (uint8_t k=0; k<intersection->numPorts; k++) {
                Port *port = intersection->ports[k];
                if (port == nullptr) {
                    continue;
                }
                ports[port->id] = port;
            }
        }
        for (uint j=0; j<object.conn[i].size(); j++) {
            Connection* connection = object.conn[i][j];
            connPixels[connection->fromPixel] = true;
            connPixels[connection->toPixel] = true;
        }
    }
    weightPixels = new bool*[object.models.size()];
    for (uint8_t i=0; i<object.models.size(); i++) {
        weightPixels[i] = new bool[object.pixelCount]{false};
        Model* model = object.models[i];
        if (model == nullptr) {
            continue;
        }
        for (uint8_t j=0; j<model->weights->size(); j++) {
            uint8_t portId = model->weights->keyAt(j);
            Port* port = ports[portId];
            if (port == nullptr) {
                continue;
            }
            //Weight* weight = model->weights->valueAt(j);
            weightPixels[model->id][port->intersection->topPixel] = true;
        }
    }
}

LPDebugger::~LPDebugger() {
    delete[] interPixels;
    delete[] connPixels;
    for (uint8_t i=0; i<object.models.size(); i++) {
        delete[] weightPixels[i];
        weightPixels[i] = NULL;
    }
    delete[] weightPixels;
}

void LPDebugger::update(unsigned long millis) {
    fps[fpsIndex] = 1000.f / float(millis - prevMillis);
    fpsIndex = (fpsIndex + 1) % AVG_FPS_FRAMES;
    emitsIndex = (emitsIndex + 1) % AVG_FPS_FRAMES;
    numEmits[emitsIndex] = 0;
    prevMillis = millis;
}

void LPDebugger::countEmit() {
    numEmits[emitsIndex]++;
}

float LPDebugger::getFPS() {
    float avg = 0;
    for (uint8_t i=0; i<AVG_FPS_FRAMES; i++) {
        avg += fps[i];
    }
    return avg / AVG_FPS_FRAMES;
}

float LPDebugger::getNumEmits() {
    float sum = 0;
    for (uint8_t i=0; i<AVG_FPS_FRAMES; i++) {
        sum += numEmits[i];
    }
    return sum / AVG_FPS_FRAMES;
}

bool LPDebugger::isModelWeight(uint8_t id, uint16_t i) {
  return weightPixels[id][i];
}

bool LPDebugger::isIntersection(uint16_t i) {
  return interPixels[i];
}

bool LPDebugger::isConnection(uint16_t i) {
  return connPixels[i];
}

void LPDebugger::dumpConnections() {
  LP_LOGLN("--- CONNECTIONS ---");
  for (uint8_t i=0; i<MAX_GROUPS; i++) {
      for (uint8_t j=0; j<object.conn[i].size(); j++) {
        LP_LOGF("Connection%d %d - %d\n", i, object.conn[i][j]->fromPixel, object.conn[i][j]->toPixel);
      }
  }
}

void LPDebugger::dumpIntersections() {
  LP_LOGLN("--- INTERSECTIONS ---");
  for (uint8_t i=0; i<MAX_GROUPS; i++) {
      for (uint8_t j=0; j<object.inter[i].size(); j++) {
        LP_LOGF("Intersection%d %d\n", i, object.inter[i][j]->id);
      }
  }
}
