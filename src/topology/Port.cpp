#include "Port.h"
#include "Connection.h"
#include "Intersection.h"
#include "LPLight.h"

// Initialize function pointer to null
void (*sendLightViaESPNow)(const uint8_t* mac, uint8_t id, LPLight* const light, bool sendList) = nullptr;

// Initialize static members for Port pool
Port* Port::portPool[Port::MAX_PORTS] = {nullptr};
uint8_t Port::poolSize = 0;

Port::Port(Connection* connection, Intersection* intersection, bool direction, uint8_t group) {
    this->id = getNextId();
    this->connection = connection;
    this->intersection = intersection;
    this->direction = direction;
    this->group = group;
    this->intersection->addPort(this);
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

InternalPort::InternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group)
    : Port(connection, intersection, direction, group) {
}

ExternalPort::ExternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group, uint8_t device[6])
    : Port(connection, intersection, direction, group) {
    memcpy(this->device, device, 6);
}

void Port::handleColorChange(LPLight* const light) const {
    const Behaviour* behaviour = light->getBehaviour();
    if (behaviour->colorChangeGroups & group) {
        light->setColor(behaviour->getColor(light, group));
    }
}

void InternalPort::sendOut(LPLight* const light, bool /*sendList*/) {
    handleColorChange(light);
    connection->add(light);
}

void ExternalPort::sendOut(LPLight* const light, bool sendList) {
    if (sendLightViaESPNow) {
        light->isExpired = true;
        sendLightViaESPNow(device, targetId, light, sendList);
    }
}
