#pragma once

#include "LightListBuild.h"
#include "RemoteSnapshotBuilder.h"
#include "../rendering/Palette.h"

namespace remote_ingress {

struct EmitIntentDescriptor {
    uint16_t length = 1;
    uint16_t trail = 0;
    uint32_t remainingLife = 0;
    ListOrder order = LIST_ORDER_SEQUENTIAL;
    ListHead head = LIST_HEAD_FRONT;
    bool linked = false;
    float speed = 0.0f;
    uint8_t easeIndex = 0;
    uint8_t fadeSpeed = 0;
    uint8_t fadeThresh = 0;
    uint8_t fadeEaseIndex = 0;
    uint8_t minBri = 0;
    uint8_t maxBri = FULL_BRIGHTNESS;
    BlendMode blendMode = BLEND_NORMAL;
    uint16_t behaviourFlags = 0;
    uint8_t colorChangeGroups = 0;
    Model* model = nullptr;
    Palette palette;
    uint8_t senderPixelDensity = 1;
    uint8_t receiverPixelDensity = 1;
};

inline void normalizeSnapshotList(LightList* list) {
    if (list == nullptr) {
        return;
    }

    list->compensateHiddenIngressContinuity = true;
    for (uint16_t i = 0; i < list->numLights; i++) {
        RuntimeLight* light = (*list)[i];
        if (light == nullptr) {
            continue;
        }
        light->owner = nullptr;
        light->isExpired = false;
        light->setInPort(nullptr);
        light->setOutPort(nullptr);
        light->lifeMillis = list->lifeMillis;
    }
}

inline bool activateList(State& state, Owner& emitter, LightList* list, uint8_t emitOffset = 0,
                         bool normalizeSnapshot = false) {
    if (list == nullptr) {
        return false;
    }
    list->compensateHiddenIngressContinuity = true;
    if (normalizeSnapshot) {
        normalizeSnapshotList(list);
    }
    const uint8_t replayEmitOffset =
        (normalizeSnapshot && emitOffset == 0 && list->length > 1) ? static_cast<uint8_t>(1) : emitOffset;
    state.activateList(&emitter, list, replayEmitOffset, false);
    return true;
}

inline LightList* buildEmitIntentList(const EmitIntentDescriptor& descriptor) {
    const uint8_t senderDensity = remote_snapshot::sanitizePixelDensity(descriptor.senderPixelDensity);
    const uint8_t receiverDensity = remote_snapshot::sanitizePixelDensity(descriptor.receiverPixelDensity);
    uint16_t scaledLength =
        lightlist_build::scaleLengthForDensity(
            (descriptor.length > 0) ? descriptor.length : static_cast<uint16_t>(1),
            senderDensity,
            receiverDensity);
    if (scaledLength == 0) {
        scaledLength = 1;
    }

    uint16_t scaledTrail = 0;
    if (descriptor.trail > 0) {
        scaledTrail =
            lightlist_build::scaleLengthForDensity(descriptor.trail, senderDensity, receiverDensity);
        if (scaledTrail >= scaledLength) {
            scaledTrail = static_cast<uint16_t>(scaledLength - 1);
        }
    }

    lightlist_build::Spec spec;
    spec.population = lightlist_build::PopulationKind::DerivedFromLength;
    spec.length = scaledLength;
    spec.trail = scaledTrail;
    spec.durationMillis = descriptor.remainingLife;
    spec.style.order = descriptor.order;
    spec.style.head = descriptor.head;
    spec.style.linked = descriptor.linked;
    spec.style.speed =
        lightlist_build::scaleSpeedForDensity(descriptor.speed, senderDensity, receiverDensity);
    spec.style.easeIndex = descriptor.easeIndex;
    spec.style.fadeSpeed = descriptor.fadeSpeed;
    spec.style.fadeThresh = descriptor.fadeThresh;
    spec.style.fadeEaseIndex = descriptor.fadeEaseIndex;
    spec.style.minBri = descriptor.minBri;
    spec.style.maxBri = descriptor.maxBri;
    spec.style.blendMode = descriptor.blendMode;
    spec.style.behaviourFlags = descriptor.behaviourFlags;
    spec.style.colorChangeGroups = descriptor.colorChangeGroups;
    spec.style.model = descriptor.model;
    spec.style.palette = descriptor.palette;

    lightlist_build::Policy policy;
    policy.allocateBehaviour = (descriptor.behaviourFlags != 0) || (descriptor.colorChangeGroups != 0);
    policy.behaviourFailureSite = LightgraphAllocationFailureSite::RemoteBehaviourAllocation;
    policy.listFailureSite = LightgraphAllocationFailureSite::RemoteListAllocation;
    policy.lightFailureSite = LightgraphAllocationFailureSite::RemoteLightAllocation;
    policy.exceptionFailureSite = LightgraphAllocationFailureSite::RemoteLightAllocation;
    return lightlist_build::buildLightList(spec, policy);
}

} // namespace remote_ingress
