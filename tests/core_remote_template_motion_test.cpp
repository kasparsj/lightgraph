#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "lightgraph/internal/Globals.h"
#include "lightgraph/integration/remote_ingress.hpp"
#include "lightgraph/internal/runtime.hpp"
#include "lightgraph/internal/runtime/LightListBuild.h"
#include "lightgraph/internal/runtime/RemoteSnapshotBuilder.h"
#include "lightgraph/internal/topology.hpp"

namespace {

constexpr uint8_t kPixelDensity = 60;
constexpr uint8_t kRemoteSlot = 1;
constexpr uint8_t kExpectedRemoteTargetPortId = 5;
constexpr uint16_t kSlowRegressionLength = 100;
constexpr float kSlowRegressionSpeed = 0.05f;

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

bool isNonBlack(const ColorRGB& color) {
    return color.R > 0 || color.G > 0 || color.B > 0;
}

bool isApproxColor(const ColorRGB& actual, const ColorRGB& expected, uint8_t tolerance = 1) {
    const auto within = [tolerance](uint8_t a, uint8_t b) {
        const int delta = static_cast<int>(a) - static_cast<int>(b);
        return std::abs(delta) <= static_cast<int>(tolerance);
    };
    return within(actual.R, expected.R) && within(actual.G, expected.G) && within(actual.B, expected.B);
}

std::string describeColor(const ColorRGB& color) {
    return "(" + std::to_string(color.R) + "," + std::to_string(color.G) + "," + std::to_string(color.B) + ")";
}

std::string describeContributorsAtPixel(LightList* list, uint16_t pixel) {
    if (list == nullptr) {
        return "none";
    }

    std::ostringstream out;
    bool found = false;
    for (uint16_t i = 0; i < list->numLights; ++i) {
        RuntimeLight* light = (*list)[i];
        if (light == nullptr) {
            continue;
        }

        const bool contributesPrimary =
            light->pixel1 == static_cast<int16_t>(pixel) && light->pixel1Weight > 0;
        const bool contributesSecondary =
            light->pixel2 == static_cast<int16_t>(pixel) && light->pixel2Weight > 0;
        if (!contributesPrimary && !contributesSecondary) {
            continue;
        }

        if (found) {
            out << " | ";
        }
        found = true;
        out << "idx=" << i
            << " pos=" << light->position
            << " owner=";
        if (light->owner == nullptr) {
            out << "null";
        } else {
            Owner* owner = const_cast<Owner*>(light->owner);
            out << (owner->getType() == Owner::TYPE_INTERSECTION ? "I" : "C");
        }
        out << " p1=" << light->pixel1 << "/" << static_cast<int>(light->pixel1Weight)
            << " p2=" << light->pixel2 << "/" << static_cast<int>(light->pixel2Weight);
    }

    return found ? out.str() : "none";
}

ColorRGB scaleColor(ColorRGB color, uint8_t weight) {
    if (weight == FULL_BRIGHTNESS) {
        return color;
    }
    return ColorRGB(
        static_cast<uint8_t>((static_cast<uint16_t>(color.R) * weight + 127u) / 255u),
        static_cast<uint8_t>((static_cast<uint16_t>(color.G) * weight + 127u) / 255u),
        static_cast<uint8_t>((static_cast<uint16_t>(color.B) * weight + 127u) / 255u));
}

ColorRGB accumulateColor(ColorRGB total, const ColorRGB& contribution) {
    total.R = static_cast<uint8_t>(std::min<uint16_t>(
        FULL_BRIGHTNESS,
        static_cast<uint16_t>(total.R) + static_cast<uint16_t>(contribution.R)));
    total.G = static_cast<uint8_t>(std::min<uint16_t>(
        FULL_BRIGHTNESS,
        static_cast<uint16_t>(total.G) + static_cast<uint16_t>(contribution.G)));
    total.B = static_cast<uint8_t>(std::min<uint16_t>(
        FULL_BRIGHTNESS,
        static_cast<uint16_t>(total.B) + static_cast<uint16_t>(contribution.B)));
    return total;
}

struct LitSpan {
    std::vector<uint16_t> pixels;

    bool empty() const { return pixels.empty(); }
    size_t size() const { return pixels.size(); }
    uint16_t first() const { return pixels.front(); }
    uint16_t last() const { return pixels.back(); }
};

std::string describeSpan(const LitSpan& span) {
    if (span.empty()) {
        return "empty";
    }
    return "count=" + std::to_string(span.size()) +
           " first=" + std::to_string(span.first()) +
           " last=" + std::to_string(span.last());
}

LitSpan collectLitPixels(State& state, uint16_t startInclusive, uint16_t endInclusive) {
    LitSpan span;
    if (startInclusive > endInclusive) {
        return span;
    }

    span.pixels.reserve(static_cast<size_t>(endInclusive - startInclusive + 1));
    for (uint16_t pixel = startInclusive; pixel <= endInclusive; ++pixel) {
        if (isNonBlack(state.getPixel(pixel))) {
            span.pixels.push_back(pixel);
        }
    }
    return span;
}

bool isContiguous(const LitSpan& span) {
    if (span.pixels.empty()) {
        return true;
    }

    for (size_t i = 1; i < span.pixels.size(); ++i) {
        if (span.pixels[i] != static_cast<uint16_t>(span.pixels[i - 1] + 1)) {
            return false;
        }
    }
    return true;
}

uint16_t colorEnergy(const ColorRGB& color) {
    return static_cast<uint16_t>(color.R) + static_cast<uint16_t>(color.G) + static_cast<uint16_t>(color.B);
}

bool findDominantContributionAtPixel(LightList* list, uint16_t pixel, ColorRGB& contributionOut) {
    if (list == nullptr) {
        return false;
    }

    bool found = false;
    uint16_t bestEnergy = 0;
    for (uint16_t i = 0; i < list->numLights; ++i) {
        RuntimeLight* light = (*list)[i];
        if (light == nullptr) {
            continue;
        }

        if (light->pixel1 == static_cast<int16_t>(pixel)) {
            ColorRGB contribution = light->getPixelColorAt(light->pixel1);
#if LIGHTGRAPH_FRACTIONAL_RENDERING
            contribution = scaleColor(contribution, light->pixel1Weight);
#endif
            const uint16_t energy = colorEnergy(contribution);
            if (!found || energy > bestEnergy) {
                found = true;
                bestEnergy = energy;
                contributionOut = contribution;
            }
        }
#if LIGHTGRAPH_FRACTIONAL_RENDERING
        if (light->pixel2 == static_cast<int16_t>(pixel) && light->pixel2Weight > 0) {
            ColorRGB contribution = light->getPixelColorAt(light->pixel2);
            contribution = scaleColor(contribution, light->pixel2Weight);
            const uint16_t energy = colorEnergy(contribution);
            if (!found || energy > bestEnergy) {
                found = true;
                bestEnergy = energy;
                contributionOut = contribution;
            }
        }
#endif
    }

    return found;
}

bool findAccumulatedContributionAtPixel(LightList* list, uint16_t pixel, ColorRGB& contributionOut) {
    if (list == nullptr) {
        return false;
    }

    bool found = false;
    ColorRGB total(0, 0, 0);
    for (uint16_t i = 0; i < list->numLights; ++i) {
        RuntimeLight* light = (*list)[i];
        if (light == nullptr) {
            continue;
        }

        if (light->pixel1 == static_cast<int16_t>(pixel)) {
            ColorRGB contribution = light->getPixelColorAt(light->pixel1);
#if LIGHTGRAPH_FRACTIONAL_RENDERING
            contribution = scaleColor(contribution, light->pixel1Weight);
#endif
            total = accumulateColor(total, contribution);
            found = true;
        }
#if LIGHTGRAPH_FRACTIONAL_RENDERING
        if (light->pixel2 == static_cast<int16_t>(pixel) && light->pixel2Weight > 0) {
            ColorRGB contribution = light->getPixelColorAt(light->pixel2);
            contribution = scaleColor(contribution, light->pixel2Weight);
            total = accumulateColor(total, contribution);
            found = true;
        }
#endif
    }

    if (found) {
        contributionOut = total;
    }
    return found;
}

bool findRemoteIngressExpectedContributionAtPixel(LightList* list,
                                                  uint16_t pixel,
                                                  uint16_t adjacentPixel,
                                                  ColorRGB& contributionOut) {
    if (list == nullptr) {
        return false;
    }

    bool found = false;
    ColorRGB total(0, 0, 0);
    for (uint16_t i = 0; i < list->numLights; ++i) {
        RuntimeLight* light = (*list)[i];
        if (light == nullptr) {
            continue;
        }

        if (light->pixel1 == static_cast<int16_t>(pixel)) {
            ColorRGB contribution = light->getPixelColorAt(light->pixel1);
#if LIGHTGRAPH_FRACTIONAL_RENDERING
            const RuntimeLight* previous = light->getPrev();
            const bool compensateHiddenIngress =
                list->compensateHiddenIngressContinuity &&
                light->inPort == nullptr &&
                previous != nullptr &&
                previous->pixel1 == static_cast<int16_t>(adjacentPixel) &&
                previous->pixel1Weight > 0 &&
                previous->owner != nullptr &&
                const_cast<Owner*>(previous->owner)->getType() == Owner::TYPE_CONNECTION;
            if (!compensateHiddenIngress) {
                contribution = scaleColor(contribution, light->pixel1Weight);
            }
#endif
            total = accumulateColor(total, contribution);
            found = true;
        }
#if LIGHTGRAPH_FRACTIONAL_RENDERING
        if (light->pixel2 == static_cast<int16_t>(pixel) && light->pixel2Weight > 0) {
            ColorRGB contribution = light->getPixelColorAt(light->pixel2);
            contribution = scaleColor(contribution, light->pixel2Weight);
            total = accumulateColor(total, contribution);
            found = true;
        }
#endif
    }

    if (found) {
        contributionOut = total;
    }
    return found;
}

class DeviceObject : public TopologyObject {
  public:
    explicit DeviceObject(uint16_t pixelCount) : TopologyObject(pixelCount) {}

    uint16_t* getMirroredPixels(uint16_t, Owner*, bool) override {
        mirrored_[0] = 0;
        return mirrored_;
    }

    EmitParams getModelParams(int model) const override {
        EmitParams params(model % 2, 1.0f);
        params.setLength(8);
        params.duration = INFINITE_DURATION;
        params.emitGroups = GROUP1;
        return params;
    }

  private:
    uint16_t mirrored_[2] = {0};
};

struct DeviceFixture {
    explicit DeviceFixture(uint16_t pixelCount) : object(pixelCount), state(object) {}

    DeviceObject object;
    State state;
    Model* primaryModel = nullptr;
    Model* secondaryModel = nullptr;
    Intersection* inter0 = nullptr;
    Intersection* inter1 = nullptr;
    Intersection* inter2 = nullptr;
    Connection* conn01 = nullptr;
    Connection* conn02 = nullptr;
    Connection* conn12 = nullptr;
    ExternalPort* externalPort = nullptr;
};

Owner* resolveIngressOwnerForTest(InternalPort* targetPort) {
    if (targetPort == nullptr) {
        return nullptr;
    }
    if (targetPort->intersection != nullptr) {
        return targetPort->intersection;
    }
    return targetPort->connection;
}

struct TemplateTransportContext {
    State* remoteState = nullptr;
    InternalPort* remoteTargetPort = nullptr;
    uint8_t remoteSlot = kRemoteSlot;
    uint8_t expectedTargetPortId = kExpectedRemoteTargetPortId;
    uint8_t senderPixelDensity = kPixelDensity;
    uint8_t receiverPixelDensity = kPixelDensity;
    size_t templateSendCount = 0;
    size_t unexpectedPerLightSendCount = 0;
    std::string lastError;
};

TemplateTransportContext* gTemplateTransportContext = nullptr;

std::unique_ptr<DeviceFixture> makeDevice154Fixture() {
    auto fixture = std::make_unique<DeviceFixture>(488);
    fixture->state.lightLists[0]->visible = false;

    fixture->primaryModel = fixture->object.addModel(new Model(0, 10, GROUP1));
    fixture->secondaryModel = fixture->object.addModel(new Model(1, 10, GROUP1));

    fixture->inter0 = fixture->object.addIntersection(new Intersection(2, 487, -1, GROUP1));
    fixture->inter1 = fixture->object.addIntersection(new Intersection(2, 0, -1, GROUP1));
    fixture->inter2 = fixture->object.addIntersection(new Intersection(3, 126, -1, GROUP1));

    fixture->conn01 = fixture->object.addConnection(new Connection(fixture->inter0, fixture->inter1, GROUP1, 0));
    fixture->conn02 = fixture->object.addConnection(new Connection(fixture->inter0, fixture->inter2, GROUP1, 360));
    fixture->conn12 = fixture->object.addConnection(new Connection(fixture->inter1, fixture->inter2, GROUP1, 125));

    const uint8_t remoteMac[6] = {0x08, 0x3A, 0xF2, 0x6C, 0xEB, 0x90};
    fixture->externalPort =
        fixture->object.addExternalPort(fixture->inter2, 2, true, GROUP1, remoteMac, kExpectedRemoteTargetPortId);

    return fixture;
}

std::unique_ptr<DeviceFixture> makeDevice150Fixture() {
    auto fixture = std::make_unique<DeviceFixture>(288);
    fixture->state.lightLists[0]->visible = false;

    fixture->primaryModel = fixture->object.addModel(new Model(0, 10, GROUP1));
    fixture->secondaryModel = fixture->object.addModel(new Model(1, 10, GROUP1));

    fixture->inter0 = fixture->object.addIntersection(new Intersection(2, 287, -1, GROUP1));
    fixture->inter1 = fixture->object.addIntersection(new Intersection(2, 0, -1, GROUP1));
    fixture->inter2 = fixture->object.addIntersection(new Intersection(3, 153, -1, GROUP1));

    fixture->conn01 = fixture->object.addConnection(new Connection(fixture->inter0, fixture->inter1, GROUP1, 0));
    fixture->conn02 = fixture->object.addConnection(new Connection(fixture->inter0, fixture->inter2, GROUP1, 133));
    fixture->conn12 = fixture->object.addConnection(new Connection(fixture->inter1, fixture->inter2, GROUP1, 152));

    const uint8_t remoteMac[6] = {0x08, 0x3A, 0xF2, 0x50, 0xBF, 0xA4};
    fixture->externalPort =
        fixture->object.addExternalPort(fixture->inter2, 2, true, GROUP1, remoteMac, kExpectedRemoteTargetPortId);

    fixture->primaryModel->setRoutingStrategy(RoutingStrategy::Deterministic);
    fixture->primaryModel->put(fixture->externalPort, fixture->conn12->toPort, 100);
    fixture->primaryModel->put(fixture->conn02->toPort, fixture->conn12->toPort, 1);

    return fixture;
}

bool sendLightViaESPNowTemplateHook(const uint8_t*,
                                    uint8_t targetPortId,
                                    RuntimeLight* const light,
                                    bool sendList) {
    TemplateTransportContext* context = gTemplateTransportContext;
    if (context == nullptr || light == nullptr || light->list == nullptr) {
        return false;
    }

    if (!sendList) {
        context->unexpectedPerLightSendCount++;
        return true;
    }

    if (targetPortId != context->expectedTargetPortId) {
        context->lastError = "unexpected target port id";
        return false;
    }

    LightList* senderList = light->list;
    const std::vector<int64_t>& colors = senderList->palette.getColors();
    const std::vector<float>& positions = senderList->palette.getPositions();

    remote_snapshot::TemplateSnapshotDescriptor descriptor = {};
    descriptor.numLights = lightlist_build::resolveSequentialBodyLightCount(senderList);
    descriptor.length = (senderList->length > 0) ? senderList->length : senderList->numLights;
    descriptor.speed = light->getSpeed();
    descriptor.lifeMillis = light->getLife();
    descriptor.duration = senderList->duration;
    descriptor.easeIndex = senderList->easeIndex;
    descriptor.fadeSpeed = senderList->fadeSpeed;
    descriptor.fadeThresh = senderList->fadeThresh;
    descriptor.fadeEaseIndex = senderList->fadeEaseIndex;
    descriptor.minBri = senderList->minBri;
    descriptor.maxBri = senderList->maxBri;
    descriptor.head = static_cast<uint8_t>(senderList->head);
    descriptor.linked = senderList->linked;
    descriptor.blendMode = static_cast<uint8_t>(senderList->blendMode);
    descriptor.hasBehaviour = senderList->behaviour != nullptr;
    descriptor.behaviourFlags = (senderList->behaviour != nullptr) ? senderList->behaviour->flags : 0;
    descriptor.colorChangeGroups =
        (senderList->behaviour != nullptr) ? senderList->behaviour->colorChangeGroups : 0;
    descriptor.model = (senderList->model != nullptr && context->remoteState != nullptr)
        ? context->remoteState->object.getModel(senderList->model->id)
        : nullptr;
    descriptor.colorRule = senderList->palette.getColorRule();
    descriptor.interpolationMode = senderList->palette.getInterpolationMode();
    descriptor.wrapMode = senderList->palette.getWrapMode();
    descriptor.segmentation = senderList->palette.getSegmentation();
    descriptor.senderPixelDensity = context->senderPixelDensity;
    descriptor.receiverPixelDensity = context->receiverPixelDensity;

    LightList* remoteList = remote_snapshot::buildTemplateSnapshot(descriptor, colors, positions);
    if (remoteList == nullptr) {
        context->lastError = "template snapshot materialization failed";
        return false;
    }

    Owner* ingressOwner = resolveIngressOwnerForTest(context->remoteTargetPort);
    if (ingressOwner == nullptr) {
        delete remoteList;
        context->lastError = "remote ingress owner was not resolved";
        return false;
    }

    if (!lightgraph::integration::remote_ingress::activateTemplateReplayList(
            *context->remoteState, *ingressOwner, remoteList)) {
        delete remoteList;
        context->lastError = "remote template replay activation failed";
        return false;
    }
    const int8_t intersectionId =
        (context->remoteTargetPort != nullptr && context->remoteTargetPort->intersection != nullptr)
            ? static_cast<int8_t>(context->remoteTargetPort->intersection->id)
            : -1;
    for (uint16_t i = 0; i < remoteList->numLights; ++i) {
        RuntimeLight* remoteLight = (*remoteList)[i];
        if (remoteLight == nullptr || context->remoteTargetPort == nullptr) {
            continue;
        }
        remoteLight->setOutPort(context->remoteTargetPort, intersectionId);
    }
    if (!context->remoteState->replaceListSlot(context->remoteSlot, remoteList)) {
        context->lastError = "remote slot replacement failed";
        return false;
    }

    context->templateSendCount++;
    return true;
}

} // namespace

int main() {
    std::srand(41);
    gMillis = 0;
    LightList::nextId = 0;
    Intersection::nextId = 0;
    Port::setNextId(0);

    std::unique_ptr<DeviceFixture> remote = makeDevice154Fixture();

    Intersection::nextId = 0;
    std::unique_ptr<DeviceFixture> sender = makeDevice150Fixture();

    TemplateTransportContext transport = {};
    transport.remoteState = &remote->state;
    transport.remoteTargetPort = static_cast<InternalPort*>(remote->conn02->toPort);

    ::sendLightViaESPNow = sendLightViaESPNowTemplateHook;
    gTemplateTransportContext = &transport;

    EmitParams params(0, 1.0f, 0x38C172);
    params.setLength(8);
    params.duration = INFINITE_DURATION;
    params.emitGroups = GROUP1;
    params.order = LIST_ORDER_SEQUENTIAL;
    params.head = LIST_HEAD_FRONT;
    params.linked = true;
    params.behaviourFlags |= B_EMIT_FROM_CONN;
    params.from = 2;

    const int8_t listIndex = sender->state.emit(params);
    if (listIndex < 0) {
        ::sendLightViaESPNow = nullptr;
        gTemplateTransportContext = nullptr;
        return fail("Failed to emit sender-side sequential list on device 150 fixture");
    }

    LightList* senderList = sender->state.lightLists[listIndex];
    if (senderList == nullptr) {
        ::sendLightViaESPNow = nullptr;
        gTemplateTransportContext = nullptr;
        return fail("Sender-side sequential list was not retained in state");
    }

    const uint16_t expectedLead = senderList->lead;
    const uint16_t expectedTrail = senderList->trail;
    const uint16_t senderStripStart = sender->conn12->fromPixel;
    const uint16_t senderStripEnd = sender->inter2->topPixel;
    const uint16_t remoteStripStart = remote->conn02->toPixel;
    const uint16_t remoteStripEnd = remote->conn02->fromPixel;
    const uint16_t senderExitPixel = sender->inter2->topPixel;
    const uint16_t remoteIngressPixel = remote->inter2->topPixel;

    LitSpan previousSenderStrip = {};
    LitSpan transferSenderBaseline = {};
    LitSpan transferRemoteBaseline = {};
    uint16_t transferRemoteAnchor = remoteStripStart;
    size_t expectedTransferFrames = 0;
    size_t checkedTransferFrames = 0;
    bool sawTemplateSend = false;

    const size_t maxFrames = static_cast<size_t>(sender->conn12->numLeds) + static_cast<size_t>(senderList->length) + 16;
    for (size_t frame = 0; frame < maxFrames; ++frame) {
        gMillis += EmitParams::frameMs();
        sender->state.update();
        remote->state.update();

        const LitSpan senderStrip = collectLitPixels(sender->state, senderStripStart, senderStripEnd);
        const LitSpan remoteStrip = collectLitPixels(remote->state, remoteStripStart, remoteStripEnd);
        const bool senderExitLit = isNonBlack(sender->state.getPixel(senderExitPixel));
        const bool remoteIngressLit = isNonBlack(remote->state.getPixel(remoteIngressPixel));

        if (!isContiguous(senderStrip)) {
            ::sendLightViaESPNow = nullptr;
            gTemplateTransportContext = nullptr;
            return fail("Sender strip should stay contiguous throughout the handoff");
        }
        if (!isContiguous(remoteStrip)) {
            ::sendLightViaESPNow = nullptr;
            gTemplateTransportContext = nullptr;
            return fail("Remote strip should stay contiguous throughout the handoff");
        }

        if (!previousSenderStrip.empty() && !sawTemplateSend) {
            if (senderStrip.empty()) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender strip should remain visible while approaching the remote port");
            }
            if (senderStrip.last() != static_cast<uint16_t>(previousSenderStrip.last() + 1)) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender head should advance by exactly 1 pixel per frame before template forwarding");
            }
        }

        if (!sawTemplateSend && transport.templateSendCount == 1) {
            sawTemplateSend = true;

            LightList* remoteList = remote->state.lightLists[kRemoteSlot];
            if (remoteList == nullptr) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Template forwarding should materialize a remote list");
            }
            if (remoteList->emitter != resolveIngressOwnerForTest(transport.remoteTargetPort)) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Template forwarding should retain the real ingress owner as the list emitter");
            }
            if (remoteList->lead != expectedLead || remoteList->trail != expectedTrail) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Template forwarding should preserve reconstructed lead/trail on the remote strip");
            }
            if (remoteList->length != senderList->length) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Template forwarding should preserve logical length at equal pixel density");
            }
            if (!remoteIngressLit) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote ingress intersection should light immediately once the template handoff starts");
            }
            if (remoteStrip.size() > 1) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote strip should start with at most the ingress-adjacent connection pixel when template forwarding begins");
            }
            if (!remoteStrip.empty() && remoteStrip.first() != remoteStripStart) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote strip should start from the ingress-adjacent connection pixel when template forwarding begins");
            }
            if (senderStrip.empty()) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender strip should still show the original tail when the remote strip starts");
            }
            if (!senderExitLit) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender exit intersection should stay lit when the remote handoff starts");
            }

            transferSenderBaseline = senderStrip;
            transferRemoteBaseline = remoteStrip;
            transferRemoteAnchor = remoteStripStart;
            expectedTransferFrames = senderStrip.size() - 1;
        } else if (sawTemplateSend && checkedTransferFrames < expectedTransferFrames) {
            if (senderStrip.empty()) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender strip tail disappeared before the per-frame handoff completed");
            }
            if (remoteStrip.empty()) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote strip should keep growing once template forwarding starts");
            }
            if (!senderExitLit) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender exit intersection should stay continuously lit during handoff");
            }
            const size_t step = checkedTransferFrames + 1;
            if (senderStrip.size() != transferSenderBaseline.size() - step) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender strip tail should shrink by exactly 1 pixel per frame (" +
                            describeSpan(senderStrip) + ", baseline=" + describeSpan(transferSenderBaseline) +
                            ", step=" + std::to_string(step) + ")");
            }
            if (senderStrip.first() != static_cast<uint16_t>(transferSenderBaseline.first() + step)) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender strip tail edge should move forward by exactly 1 pixel per frame (" +
                            describeSpan(senderStrip) + ", baseline=" + describeSpan(transferSenderBaseline) +
                            ", step=" + std::to_string(step) + ")");
            }
            if (senderStrip.last() != transferSenderBaseline.last()) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender strip head should stay pinned to the exit intersection during handoff (" +
                            describeSpan(senderStrip) + ", baseline=" + describeSpan(transferSenderBaseline) + ")");
            }

            if (remoteStrip.size() != transferRemoteBaseline.size() + step) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote strip tail should grow by exactly 1 pixel per frame (" +
                            describeSpan(remoteStrip) + ", baseline=" + describeSpan(transferRemoteBaseline) +
                            ", step=" + std::to_string(step) + ")");
            }
            const uint16_t expectedRemoteFirst =
                transferRemoteBaseline.empty() ? transferRemoteAnchor : transferRemoteBaseline.first();
            if (remoteStrip.first() != expectedRemoteFirst) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote strip should keep its ingress edge anchored during handoff (" +
                            describeSpan(remoteStrip) + ", baseline=" + describeSpan(transferRemoteBaseline) + ")");
            }
            const uint16_t expectedRemoteLast = transferRemoteBaseline.empty()
                ? static_cast<uint16_t>(transferRemoteAnchor + step - 1)
                : static_cast<uint16_t>(transferRemoteBaseline.last() + step);
            if (remoteStrip.last() != expectedRemoteLast) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote strip growth should advance by exactly 1 pixel per frame (" +
                            describeSpan(remoteStrip) + ", baseline=" + describeSpan(transferRemoteBaseline) +
                            ", step=" + std::to_string(step) + ")");
            }

            checkedTransferFrames++;
        }

        previousSenderStrip = senderStrip;
    }

    ::sendLightViaESPNow = nullptr;
    gTemplateTransportContext = nullptr;

    if (!sawTemplateSend) {
        return fail("Sequential list never reached the remote-port template handoff");
    }
    if (!transport.lastError.empty()) {
        return fail("Template handoff transport reported error: " + transport.lastError);
    }
    if (transport.templateSendCount != 1) {
        return fail("Template handoff should emit exactly one batch send");
    }
    if (transport.unexpectedPerLightSendCount != 0) {
        return fail("Template handoff should not fall back to per-light sends");
    }
    if (expectedTransferFrames == 0 || checkedTransferFrames != expectedTransferFrames) {
        return fail("Template handoff did not complete the expected per-frame tail transfer");
    }

    {
        gMillis = 0;
        LightList::nextId = 0;
        Intersection::nextId = 0;
        Port::setNextId(0);

        std::unique_ptr<DeviceFixture> slowRemote = makeDevice154Fixture();

        Intersection::nextId = 0;
        std::unique_ptr<DeviceFixture> slowSender = makeDevice150Fixture();

        TemplateTransportContext slowTransport = {};
        slowTransport.remoteState = &slowRemote->state;
        slowTransport.remoteTargetPort = static_cast<InternalPort*>(slowRemote->conn02->toPort);

        ::sendLightViaESPNow = sendLightViaESPNowTemplateHook;
        gTemplateTransportContext = &slowTransport;

        EmitParams slowParams(0, kSlowRegressionSpeed, 0x204830);
        slowParams.setLength(kSlowRegressionLength);
        slowParams.duration = INFINITE_DURATION;
        slowParams.emitGroups = GROUP1;
        slowParams.order = LIST_ORDER_SEQUENTIAL;
        slowParams.head = LIST_HEAD_FRONT;
        slowParams.linked = true;
        slowParams.maxBri = 96;
        slowParams.behaviourFlags |= B_EMIT_FROM_CONN;
        slowParams.from = 2;

        const int8_t slowListIndex = slowSender->state.emit(slowParams);
        if (slowListIndex < 0) {
            ::sendLightViaESPNow = nullptr;
            gTemplateTransportContext = nullptr;
            return fail("Slow template regression failed to emit sender-side list");
        }

        LightList* slowSenderList = slowSender->state.lightLists[slowListIndex];
        if (slowSenderList == nullptr) {
            ::sendLightViaESPNow = nullptr;
            gTemplateTransportContext = nullptr;
            return fail("Slow template regression sender list was not retained in state");
        }

        bool sawSlowTemplateSend = false;
        const uint16_t senderConnectionPixel = slowSender->conn12->getPixel(slowSender->conn12->numLeds - 1);
        const uint16_t remoteConnectionPixel = slowRemote->conn02->getPixel(slowRemote->conn02->numLeds - 1);
        size_t checkedSenderConnectionFrames = 0;
        size_t checkedSenderIntersectionFrames = 0;
        size_t checkedRemoteConnectionFrames = 0;
        size_t checkedRemoteIntersectionFrames = 0;
        const size_t framesPerPixel = static_cast<size_t>(std::ceil(1.0f / slowParams.getSpeed()));
        const ColorRGB expectedBodyColor = scaleColor(ColorRGB(0x204830), slowParams.maxBri);
        const size_t requiredSenderStableBodyFrames = framesPerPixel;
        const size_t requiredRemoteStableBodyFrames = framesPerPixel;
        size_t senderStableBodyFrames = 0;
        size_t remoteStableBodyFrames = 0;
        bool sawSenderStableBodyStart = false;
        bool sawRemoteStableBodyStart = false;
        const size_t slowMaxFrames =
            (static_cast<size_t>(slowSender->conn12->numLeds) + static_cast<size_t>(slowSenderList->length) + 12) *
            framesPerPixel;

        for (size_t frame = 0; frame < slowMaxFrames; ++frame) {
            gMillis += EmitParams::frameMs();
            slowSender->state.update();
            slowRemote->state.update();

            if (!sawSlowTemplateSend && slowTransport.templateSendCount == 1) {
                sawSlowTemplateSend = true;
            }
            if (!sawSlowTemplateSend) {
                continue;
            }

            const ColorRGB senderConnectionActual = slowSender->state.getPixel(senderConnectionPixel);
            const ColorRGB senderIntersectionActual = slowSender->state.getPixel(slowSender->inter2->topPixel);
            ColorRGB expectedSenderConnection = {};
            if (findAccumulatedContributionAtPixel(slowSenderList, senderConnectionPixel, expectedSenderConnection)) {
                if (!isApproxColor(senderConnectionActual, expectedSenderConnection, 1)) {
                    ::sendLightViaESPNow = nullptr;
                    gTemplateTransportContext = nullptr;
                    return fail("Sender connection pixel should match the accumulated list contribution during slow template handoff"
                                " (actual=" + describeColor(senderConnectionActual) + ", expected=" + describeColor(expectedSenderConnection) +
                                ", frame=" + std::to_string(frame) + ")");
                }
                checkedSenderConnectionFrames++;
            }

            ColorRGB expectedSenderIntersection = {};
            if (findDominantContributionAtPixel(slowSenderList, slowSender->inter2->topPixel,
                                                expectedSenderIntersection)) {
                if (!isApproxColor(senderIntersectionActual, expectedSenderIntersection, 1)) {
                    ::sendLightViaESPNow = nullptr;
                    gTemplateTransportContext = nullptr;
                    return fail("Sender exit intersection should match the dominant single-light contribution during slow template handoff"
                                " (actual=" + describeColor(senderIntersectionActual) + ", expected=" + describeColor(expectedSenderIntersection) +
                                ", frame=" + std::to_string(frame) + ")");
                }
                checkedSenderIntersectionFrames++;
            }

            const bool senderBodyPlateauFrame =
                isApproxColor(senderConnectionActual, expectedBodyColor, 1) &&
                isApproxColor(senderIntersectionActual, expectedBodyColor, 1);
            if (senderBodyPlateauFrame) {
                sawSenderStableBodyStart = true;
                senderStableBodyFrames++;
            } else if (sawSenderStableBodyStart && senderStableBodyFrames < requiredSenderStableBodyFrames) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Slow template regression sender exit body plateau should stay stable"
                            " (connection=" + describeColor(senderConnectionActual) +
                            ", intersection=" + describeColor(senderIntersectionActual) +
                            ", expected=" + describeColor(expectedBodyColor) +
                            ", connection_contributors=" + describeContributorsAtPixel(slowSenderList, senderConnectionPixel) +
                            ", intersection_contributors=" + describeContributorsAtPixel(slowSenderList, slowSender->inter2->topPixel) +
                            ", frame=" + std::to_string(frame) + ")");
            } else if (!sawSenderStableBodyStart) {
                senderStableBodyFrames = 0;
            }

            LightList* slowRemoteList = slowRemote->state.lightLists[kRemoteSlot];
            const ColorRGB remoteConnectionActual = slowRemote->state.getPixel(remoteConnectionPixel);
            const ColorRGB remoteIntersectionActual = slowRemote->state.getPixel(slowRemote->inter2->topPixel);
            ColorRGB expectedRemoteConnection = {};
            if (findAccumulatedContributionAtPixel(slowRemoteList, remoteConnectionPixel, expectedRemoteConnection)) {
                if (!isApproxColor(remoteConnectionActual, expectedRemoteConnection, 1)) {
                    ::sendLightViaESPNow = nullptr;
                    gTemplateTransportContext = nullptr;
                    return fail("Remote connection pixel should match the accumulated list contribution during slow template handoff"
                                " (actual=" + describeColor(remoteConnectionActual) + ", expected=" + describeColor(expectedRemoteConnection) +
                                ", frame=" + std::to_string(frame) + ")");
                }
                checkedRemoteConnectionFrames++;
            }

            ColorRGB expectedRemoteIntersection = {};
            if (findRemoteIngressExpectedContributionAtPixel(slowRemoteList,
                                                             slowRemote->inter2->topPixel,
                                                             remoteConnectionPixel,
                                                             expectedRemoteIntersection)) {
                if (!isApproxColor(remoteIntersectionActual, expectedRemoteIntersection, 1)) {
                    ::sendLightViaESPNow = nullptr;
                    gTemplateTransportContext = nullptr;
                    return fail("Remote ingress intersection should match the compensated ingress contribution during slow template handoff"
                                " (actual=" + describeColor(remoteIntersectionActual) + ", expected=" + describeColor(expectedRemoteIntersection) +
                                ", frame=" + std::to_string(frame) + ")");
                }
                checkedRemoteIntersectionFrames++;
            }

            const bool remoteBodyPlateauFrame =
                isApproxColor(remoteConnectionActual, expectedBodyColor, 1) &&
                isApproxColor(remoteIntersectionActual, expectedBodyColor, 1);
            if (remoteBodyPlateauFrame) {
                sawRemoteStableBodyStart = true;
                remoteStableBodyFrames++;
            } else if (sawRemoteStableBodyStart && remoteStableBodyFrames < requiredRemoteStableBodyFrames) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Slow template regression remote ingress body plateau should stay stable"
                            " (connection=" + describeColor(remoteConnectionActual) +
                            ", intersection=" + describeColor(remoteIntersectionActual) +
                            ", expected=" + describeColor(expectedBodyColor) +
                            ", connection_contributors=" + describeContributorsAtPixel(slowRemoteList, remoteConnectionPixel) +
                            ", intersection_contributors=" + describeContributorsAtPixel(slowRemoteList, slowRemote->inter2->topPixel) +
                            ", frame=" + std::to_string(frame) + ")");
            } else if (!sawRemoteStableBodyStart) {
                remoteStableBodyFrames = 0;
            }
        }

        ::sendLightViaESPNow = nullptr;
        gTemplateTransportContext = nullptr;

        if (!sawSlowTemplateSend) {
            return fail("Slow template regression never reached the remote-port template handoff");
        }
        if (slowTransport.templateSendCount != 1) {
            return fail("Slow template regression should emit exactly one template batch send");
        }
        if (checkedSenderConnectionFrames == 0 || checkedSenderIntersectionFrames == 0 ||
            checkedRemoteConnectionFrames == 0 || checkedRemoteIntersectionFrames == 0) {
            return fail("Slow template regression did not observe both connection pixels and both intersection pixels during handoff");
        }
        if (senderStableBodyFrames < requiredSenderStableBodyFrames) {
            return fail("Slow template regression never observed a stable sender exit body plateau");
        }
        if (remoteStableBodyFrames < requiredRemoteStableBodyFrames) {
            return fail("Slow template regression never observed a stable remote ingress body plateau");
        }
    }

    {
        gMillis = 0;
        LightList::nextId = 0;
        Intersection::nextId = 0;
        Port::setNextId(0);

        std::unique_ptr<DeviceFixture> internal = makeDevice150Fixture();
        internal->primaryModel->put(internal->externalPort, internal->conn12->toPort, 0);
        internal->primaryModel->put(internal->conn02->toPort, internal->conn12->toPort, 100);

        EmitParams internalParams(0, kSlowRegressionSpeed, 0x204830);
        internalParams.setLength(kSlowRegressionLength);
        internalParams.duration = INFINITE_DURATION;
        internalParams.emitGroups = GROUP1;
        internalParams.order = LIST_ORDER_SEQUENTIAL;
        internalParams.head = LIST_HEAD_FRONT;
        internalParams.linked = true;
        internalParams.maxBri = 96;
        internalParams.behaviourFlags |= B_EMIT_FROM_CONN;
        internalParams.from = 2;

        const int8_t internalListIndex = internal->state.emit(internalParams);
        if (internalListIndex < 0) {
            return fail("Slow internal regression failed to emit sender-side list");
        }

        LightList* internalList = internal->state.lightLists[internalListIndex];
        if (internalList == nullptr || internalList->numLights == 0 || (*internalList)[0] == nullptr) {
            return fail("Slow internal regression did not materialize the sequential list");
        }

        const uint16_t incomingPixel = internal->conn12->getPixel(internal->conn12->numLeds - 1);
        const uint16_t intersectionPixel = internal->inter2->topPixel;
        const uint16_t outgoingPixel = internal->conn02->getPixel(internal->conn02->numLeds - 1);
        const size_t internalFramesPerPixel = static_cast<size_t>(std::ceil(1.0f / internalParams.getSpeed()));
        const size_t internalMaxFrames =
            (static_cast<size_t>(internal->conn12->numLeds) + static_cast<size_t>(internal->conn02->numLeds) +
             static_cast<size_t>(internalList->length) + 4) *
            internalFramesPerPixel;
        const ColorRGB expectedBodyColor = scaleColor(ColorRGB(0x204830), internalParams.maxBri);
        const size_t requiredStableBodyFrames = internalFramesPerPixel;

        size_t incomingObservedFrames = 0;
        size_t intersectionObservedFrames = 0;
        size_t outgoingObservedFrames = 0;
        size_t stableBodyFrames = 0;
        bool sawStableBodyStart = false;

        for (size_t frame = 0; frame < internalMaxFrames; ++frame) {
            gMillis += EmitParams::frameMs();
            internal->state.update();

            const ColorRGB incomingActual = internal->state.getPixel(incomingPixel);
            const ColorRGB intersectionActual = internal->state.getPixel(intersectionPixel);
            const ColorRGB outgoingActual = internal->state.getPixel(outgoingPixel);

            ColorRGB expectedIncoming = {};
            if (findAccumulatedContributionAtPixel(internalList, incomingPixel, expectedIncoming)) {
                if (!isApproxColor(incomingActual, expectedIncoming, 1)) {
                    return fail("Slow internal regression incoming connection pixel should match the accumulated list contribution"
                                " (actual=" + describeColor(incomingActual) + ", expected=" + describeColor(expectedIncoming) +
                                ", frame=" + std::to_string(frame) + ")");
                }
                incomingObservedFrames++;
            }

            ColorRGB expectedIntersection = {};
            if (findAccumulatedContributionAtPixel(internalList, intersectionPixel, expectedIntersection)) {
                if (!isApproxColor(intersectionActual, expectedIntersection, 1)) {
                    return fail("Slow internal regression intersection pixel should match the accumulated list contribution"
                                " (actual=" + describeColor(intersectionActual) + ", expected=" + describeColor(expectedIntersection) +
                                ", frame=" + std::to_string(frame) + ")");
                }
                intersectionObservedFrames++;
            }

            ColorRGB expectedOutgoing = {};
            if (findAccumulatedContributionAtPixel(internalList, outgoingPixel, expectedOutgoing)) {
                if (!isApproxColor(outgoingActual, expectedOutgoing, 1)) {
                    return fail("Slow internal regression outgoing connection pixel should match the accumulated list contribution"
                                " (actual=" + describeColor(outgoingActual) + ", expected=" + describeColor(expectedOutgoing) +
                                ", frame=" + std::to_string(frame) + ")");
                }
                outgoingObservedFrames++;
            }

            const bool bodyPlateauFrame =
                isApproxColor(incomingActual, expectedBodyColor, 1) &&
                isApproxColor(intersectionActual, expectedBodyColor, 1) &&
                isApproxColor(outgoingActual, expectedBodyColor, 1);
            if (bodyPlateauFrame) {
                sawStableBodyStart = true;
                stableBodyFrames++;
            } else if (sawStableBodyStart && stableBodyFrames < requiredStableBodyFrames) {
                return fail("Slow internal regression body plateau should stay stable across incoming/intersection/outgoing pixels"
                            " (incoming=" + describeColor(incomingActual) +
                            ", intersection=" + describeColor(intersectionActual) +
                            ", outgoing=" + describeColor(outgoingActual) +
                            ", expected=" + describeColor(expectedBodyColor) +
                            ", incoming_contributors=" + describeContributorsAtPixel(internalList, incomingPixel) +
                            ", intersection_contributors=" + describeContributorsAtPixel(internalList, intersectionPixel) +
                            ", outgoing_contributors=" + describeContributorsAtPixel(internalList, outgoingPixel) +
                            ", frame=" + std::to_string(frame) + ")");
            } else if (!sawStableBodyStart) {
                stableBodyFrames = 0;
            }
        }

        if (incomingObservedFrames == 0 || intersectionObservedFrames == 0 || outgoingObservedFrames == 0) {
            return fail("Slow internal regression did not observe all three handoff pixels");
        }
        if (stableBodyFrames < requiredStableBodyFrames) {
            return fail("Slow internal regression never observed a stable body plateau across the watched pixels");
        }
    }

    {
        gMillis = 0;
        LightList::nextId = 0;
        Intersection::nextId = 0;
        Port::setNextId(0);

        std::unique_ptr<DeviceFixture> remoteSparse = makeDevice154Fixture();
        InternalPort* ingressPort = static_cast<InternalPort*>(remoteSparse->conn02->toPort);
        if (ingressPort == nullptr || ingressPort->connection == nullptr) {
            return fail("Sparse replay regression fixture did not resolve a valid ingress connection");
        }

        remote_snapshot::SequentialSnapshotDescriptor descriptor = {};
        descriptor.numLights = 8;
        descriptor.positionOffset = -8;
        descriptor.speed = 1.0f;
        descriptor.lifeMillis = INFINITE_DURATION;
        descriptor.model = remoteSparse->object.getModel(0);
        descriptor.senderPixelDensity = kPixelDensity;
        descriptor.receiverPixelDensity = kPixelDensity;

        std::vector<remote_snapshot::SequentialEntry> entries(1);
        entries[0].lightIdx = 0;
        entries[0].brightness = FULL_BRIGHTNESS;
        entries[0].colorR = 0x38;
        entries[0].colorG = 0xC1;
        entries[0].colorB = 0x72;

        std::unique_ptr<LightList> sparseSnapshot(remote_snapshot::buildSequentialSnapshot(descriptor, entries));
        if (sparseSnapshot == nullptr) {
            return fail("Sparse replay regression fixture failed to build the snapshot list");
        }

        const int8_t intersectionId =
            (ingressPort->intersection != nullptr) ? static_cast<int8_t>(ingressPort->intersection->id) : -1;
        sparseSnapshot->bindRuntimeContext(remoteSparse->object.runtimeContext());
        sparseSnapshot->emitOffset = 0;
        sparseSnapshot->numSplits = 0;
        sparseSnapshot->emitter = resolveIngressOwnerForTest(ingressPort);

        RuntimeLight* latentLight = (*sparseSnapshot)[0];
        if (latentLight == nullptr) {
            return fail("Sparse replay regression fixture did not allocate the latent edge light");
        }
        if (latentLight->position >= 0.0f) {
            return fail("Sparse replay regression fixture expected a negative-position latent light");
        }

        latentLight->owner = nullptr;
        latentLight->isExpired = false;
        latentLight->setInPort(nullptr);
        latentLight->setOutPort(ingressPort, intersectionId);
        ingressPort->connection->add(latentLight);

        if (latentLight->owner != ingressPort->connection) {
            return fail("Sparse replay latent light should stay owned by the ingress connection");
        }
        if (latentLight->pixel1 >= 0) {
            return fail("Sparse replay latent light should not render before reaching the strip");
        }
    }

    return 0;
}
