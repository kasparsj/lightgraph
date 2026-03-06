#include "Port.h"

#include <cstring>
#include "Connection.h"
#include "Intersection.h"
#include "TopologyObject.h"
#include "../runtime/Behaviour.h"
#include "../runtime/LightList.h"
#include "../runtime/RuntimeLight.h"

// Initialize function pointer to null
bool (*sendLightViaESPNow)(const uint8_t* mac, uint8_t id, RuntimeLight* const light, bool sendList) = nullptr;

namespace {

uint16_t gLegacyLivePortCount = 0;
uint16_t gLegacyNextPortIdHint = 0;

} // namespace

Port::Port(Connection* connection, Intersection* intersection, bool direction, uint8_t group, int16_t slotIndex) {
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
    gLegacyLivePortCount++;
}

Port::~Port() {
    if (object != nullptr) {
        object->unregisterPort(this);
        object = nullptr;
    }
    if (intersection != nullptr) {
        intersection->removePort(this);
    }
    if (gLegacyLivePortCount > 0) {
        gLegacyLivePortCount--;
    }
}

uint16_t Port::poolCount() {
    return gLegacyLivePortCount;
}

void Port::setNextId(uint16_t id) {
    gLegacyNextPortIdHint = id;
}

uint16_t Port::nextId() {
    return gLegacyNextPortIdHint;
}

InternalPort::InternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group,
                           int16_t slotIndex)
    : Port(connection, intersection, direction, group, slotIndex) {
}

ExternalPort::ExternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group,
                           const uint8_t device[6], uint8_t targetId, int16_t slotIndex,
                           int16_t targetIntersectionId, bool hasTargetId)
    : Port(connection, intersection, direction, group, slotIndex) {
    memcpy(this->device.data(), device, 6);
    this->targetId = targetId;
    this->hasTargetId = hasTargetId;
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
        if (list->hasExternalBatchForwardedTo(device.data(), targetId, targetIntersectionId, hasTargetId)) {
            shouldSend = false;
        } else {
            sendAsBatch = true;
        }
    }

    bool sendSucceeded = true;
    if (shouldSend) {
        const LightgraphExternalSendHook sendHook =
            (object != nullptr && object->externalSendHook() != nullptr)
                ? object->externalSendHook()
                : sendLightViaESPNow;
        sendSucceeded = sendHook != nullptr &&
                        sendHook(device.data(), targetId, light, sendAsBatch);
        if (sendSucceeded && sendAsBatch) {
            list->markExternalBatchForwarded(device.data(), targetId, targetIntersectionId, hasTargetId);
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
