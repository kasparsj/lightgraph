#pragma once

#include "State.h"
#include "LightListBuild.h"
#include "RemoteSnapshotBuilder.h"
#include "../rendering/Palette.h"

class Owner;

namespace remote_ingress {

struct ActivationOptions {
    bool normalizeSnapshot = false;
    bool compensateHiddenIngressContinuity = false;
    bool advanceReplayOffset = false;
};

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

inline bool activatePreparedList(State& state,
                                 Owner& emitter,
                                 LightList* list,
                                 uint8_t emitOffset,
                                 const ActivationOptions& options = ActivationOptions()) {
    if (list == nullptr) {
        return false;
    }
    if (options.normalizeSnapshot) {
        normalizeSnapshotList(list);
    }
    const uint8_t resolvedEmitOffset =
        (options.advanceReplayOffset && emitOffset == 0 && list->length > 1)
            ? static_cast<uint8_t>(1)
            : emitOffset;
    list->compensateHiddenIngressContinuity = options.compensateHiddenIngressContinuity;
    state.activateList(&emitter, list, resolvedEmitOffset, false);
    return true;
}

inline bool activateList(State& state, Owner& emitter, LightList* list, uint8_t emitOffset = 0,
                         bool normalizeSnapshot = false) {
    ActivationOptions options;
    options.normalizeSnapshot = normalizeSnapshot;
    return activatePreparedList(state, emitter, list, emitOffset, options);
}

inline bool activateTemplateReplayList(State& state, Owner& emitter, LightList* list,
                                       uint8_t emitOffset = 0) {
    ActivationOptions options;
    options.normalizeSnapshot = true;
    options.compensateHiddenIngressContinuity = true;
    options.advanceReplayOffset = true;
    return activatePreparedList(state, emitter, list, emitOffset, options);
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

    lightlist_build::StyleSpec style;
    style.order = descriptor.order;
    style.head = descriptor.head;
    style.linked = descriptor.linked;
    style.speed =
        lightlist_build::scaleSpeedForDensity(descriptor.speed, senderDensity, receiverDensity);
    style.easeIndex = descriptor.easeIndex;
    style.fadeSpeed = descriptor.fadeSpeed;
    style.fadeThresh = descriptor.fadeThresh;
    style.fadeEaseIndex = descriptor.fadeEaseIndex;
    style.minBri = descriptor.minBri;
    style.maxBri = descriptor.maxBri;
    style.blendMode = descriptor.blendMode;
    style.behaviourFlags = descriptor.behaviourFlags;
    style.colorChangeGroups = descriptor.colorChangeGroups;
    style.model = descriptor.model;
    style.palette = descriptor.palette;

    const lightlist_build::Spec spec =
        lightlist_build::makeDerivedFromLengthSpec(style, scaledLength, scaledTrail, descriptor.remainingLife);
    return lightlist_build::buildLightList(
        spec,
        lightlist_build::makeRemoteListPolicy(
            (descriptor.behaviourFlags != 0) || (descriptor.colorChangeGroups != 0)));
}

} // namespace remote_ingress
