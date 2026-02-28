#include "Port.h"

#include <cstring>
#include "Connection.h"
#include "Intersection.h"
#include "../runtime/Behaviour.h"
#include "../runtime/RuntimeLight.h"

// Initialize function pointer to null
void (*sendLightViaESPNow)(const uint8_t* mac, uint8_t id, RuntimeLight* const light, bool sendList) = nullptr;

// Initialize static members for Port pool
Port* Port::portPool[Port::MAX_PORTS] = {nullptr};
uint8_t Port::poolSize = 0;
uint8_t Port::nextPortId = 0;

Port::Port(Connection* connection, Intersection* intersection, bool direction, uint8_t group, int16_t slotIndex) {
    this->id = allocateId();
    this->connection = connection;
    this->intersection = intersection;
    this->direction = direction;
    this->group = group;
    if (this->intersection != nullptr) {
        if (slotIndex >= 0) {
            this->intersection->addPortAt(this, static_cast<uint8_t>(slotIndex));
        } else {
            this->intersection->addPort(this);
        }
    }
    addToPool(this);
}

Port::~Port() {
    if (intersection != nullptr) {
        intersection->removePort(this);
    }
    removeFromPool(this);
}

Port* Port::findById(uint8_t id) {
    for (uint8_t i = 0; i < poolSize; i++) {
        if (portPool[i] && portPool[i]->id == id) {
            return portPool[i];
        }
    }
    return nullptr;
}

void Port::addToPool(Port* port) {
    if (port && poolSize < MAX_PORTS) {
        portPool[poolSize++] = port;
    }
}

void Port::removeFromPool(Port* port) {
    if (!port) return;
    
    for (uint8_t i = 0; i < poolSize; i++) {
        if (portPool[i] == port) {
            // Shift remaining ports down
            for (uint8_t j = i; j < poolSize - 1; j++) {
                portPool[j] = portPool[j + 1];
            }
            portPool[--poolSize] = nullptr;
            break;
        }
    }
}

uint8_t Port::allocateId() {
    return nextPortId++;
}

void Port::setNextId(uint8_t id) {
    nextPortId = id;
}

InternalPort::InternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group,
                           int16_t slotIndex)
    : Port(connection, intersection, direction, group, slotIndex) {
}

ExternalPort::ExternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group,
                           const uint8_t device[6], uint8_t targetId, int16_t slotIndex)
    : Port(connection, intersection, direction, group, slotIndex) {
    memcpy(this->device.data(), device, 6);
    this->targetId = targetId;
}

void Port::handleColorChange(RuntimeLight* const light) const {
    const Behaviour* behaviour = light->getBehaviour();
    if (behaviour->colorChangeGroups & group) {
        light->setColor(behaviour->getColor(light, group));
    }
}

void InternalPort::sendOut(RuntimeLight* const light, bool /*sendList*/) {
    handleColorChange(light);
    connection->add(light);
}

void ExternalPort::sendOut(RuntimeLight* const light, bool sendList) {
    if (sendLightViaESPNow) {
        light->isExpired = true;
        sendLightViaESPNow(device.data(), targetId, light, sendList);
    }
}
