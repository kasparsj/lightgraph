#include "Port.h"

#include <cstring>
#include <limits>
#include "Connection.h"
#include "Intersection.h"
#include "../runtime/Behaviour.h"
#include "../runtime/LightList.h"
#include "../runtime/RuntimeLight.h"

// Initialize function pointer to null
bool (*sendLightViaESPNow)(const uint8_t* mac, uint8_t id, RuntimeLight* const light, bool sendList) = nullptr;

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
    for (uint16_t attempts = 0; attempts <= std::numeric_limits<uint8_t>::max(); ++attempts) {
        const uint8_t candidate = nextPortId++;
        bool inUse = false;
        for (uint8_t i = 0; i < poolSize; i++) {
            if (portPool[i] != nullptr && portPool[i]->id == candidate) {
                inUse = true;
                break;
            }
        }
        if (!inUse) {
            return candidate;
        }
    }

    // All IDs are currently occupied; return next rollover value as a last resort.
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
                           const uint8_t device[6], uint8_t targetId, int16_t slotIndex,
                           int16_t targetIntersectionId)
    : Port(connection, intersection, direction, group, slotIndex) {
    memcpy(this->device.data(), device, 6);
    this->targetId = targetId;
    this->targetIntersectionId = targetIntersectionId;
}

void Port::handleColorChange(RuntimeLight* const light) const {
    if (light == nullptr) {
        return;
    }
    const Behaviour* behaviour = light->getBehaviour();
    if (behaviour == nullptr) {
        return;
    }
    if (behaviour->colorChangeGroups & group) {
        light->setColor(behaviour->getColor(light, group));
    }
}

void InternalPort::sendOut(RuntimeLight* const light, bool /*sendList*/) {
    if (light == nullptr) {
        return;
    }
    if (light->outPort == nullptr) {
        // Remote-injected lights arrive without routing context.
        // Treat this internal port as the ingress direction for this connection pass.
        const int8_t intersectionId = (intersection != nullptr) ? static_cast<int8_t>(intersection->id) : -1;
        light->setOutPort(this, intersectionId);
    }
    handleColorChange(light);
    connection->add(light);
}

void ExternalPort::sendOut(RuntimeLight* const light, bool sendList) {
    if (light == nullptr) {
        return;
    }

    LightList* const list = light->list;
    const bool shouldBatchSequentially =
        sendList && list != nullptr && list->order == LIST_ORDER_SEQUENTIAL;
    bool sendAsBatch = false;
    bool shouldSend = true;
    if (shouldBatchSequentially) {
        if (list->hasExternalBatchForwardedTo(device.data(), targetId)) {
            shouldSend = false;
        } else {
            sendAsBatch = true;
        }
    }

    bool sendSucceeded = true;
    if (shouldSend) {
        sendSucceeded = sendLightViaESPNow != nullptr &&
                        sendLightViaESPNow(device.data(), targetId, light, sendAsBatch);
        if (sendSucceeded && sendAsBatch) {
            list->markExternalBatchForwarded(device.data(), targetId);
        }
    }

    if (sendSucceeded) {
        // Remove each light only when it actually reaches the external port.
        light->isExpired = true;
        return;
    }

    // Failed sends stay local and re-enter normal routing on the next frame.
    light->isExpired = false;
    if (intersection != nullptr) {
        light->owner = intersection;
        light->setOutPort(nullptr, static_cast<int8_t>(intersection->id));
    } else {
        light->owner = nullptr;
        light->setOutPort(nullptr);
    }
}
