#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../src/Globals.h"
#include "../src/runtime/EmitParams.h"
#include "../src/runtime/LightList.h"
#include "../src/runtime/LightListBuild.h"
#include "../src/runtime/RemoteSnapshotBuilder.h"
#include "../src/runtime/RuntimeLight.h"
#include "../src/runtime/State.h"
#include "../src/topology/Connection.h"
#include "../src/topology/Intersection.h"
#include "../src/topology/Model.h"
#include "../src/topology/Port.h"
#include "../src/topology/TopologyObject.h"

namespace {

constexpr uint8_t kPixelDensity = 60;
constexpr uint8_t kRemoteSlot = 1;
constexpr uint8_t kExpectedRemoteTargetPortId = 5;

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

bool isNonBlack(const ColorRGB& color) {
    return color.R > 0 || color.G > 0 || color.B > 0;
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

class TestRemoteIngressEmitter : public Owner {
  public:
    TestRemoteIngressEmitter() : Owner(0) {}

    void bind(InternalPort* port) {
        targetPort_ = port;
        group = (port != nullptr) ? port->group : 0;
    }

    uint8_t getType() override { return TYPE_CONNECTION; }

    void emit(RuntimeLight* const light) const override {
        if (targetPort_ == nullptr || light == nullptr || targetPort_->connection == nullptr) {
            if (light != nullptr) {
                light->isExpired = true;
                light->owner = nullptr;
            }
            return;
        }

        if (light->outPort == nullptr) {
            const int8_t intersectionId =
                (targetPort_->intersection != nullptr) ? static_cast<int8_t>(targetPort_->intersection->id) : -1;
            light->setOutPort(targetPort_, intersectionId);
        }

        targetPort_->connection->add(light);
    }

    void update(RuntimeLight* const) const override {}

  private:
    InternalPort* targetPort_ = nullptr;
};

void normalizeRemoteSnapshotListForIngress(LightList* list) {
    if (list == nullptr) {
        return;
    }

    for (uint16_t i = 0; i < list->numLights; ++i) {
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

struct TemplateTransportContext {
    State* remoteState = nullptr;
    InternalPort* remoteTargetPort = nullptr;
    TestRemoteIngressEmitter ingressEmitter = {};
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

    normalizeRemoteSnapshotListForIngress(remoteList);
    context->ingressEmitter.bind(context->remoteTargetPort);
    context->remoteState->activateList(&context->ingressEmitter, remoteList, 0, false);
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

    LitSpan previousSenderStrip = {};
    LitSpan transferSenderBaseline = {};
    LitSpan transferRemoteBaseline = {};
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
            if (remoteStrip.empty()) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote strip should light immediately once the template handoff starts");
            }
            if (senderStrip.empty()) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Sender strip should still show the original tail when the remote strip starts");
            }

            transferSenderBaseline = senderStrip;
            transferRemoteBaseline = remoteStrip;
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
            if (remoteStrip.first() != transferRemoteBaseline.first()) {
                ::sendLightViaESPNow = nullptr;
                gTemplateTransportContext = nullptr;
                return fail("Remote strip should keep its ingress edge anchored during handoff (" +
                            describeSpan(remoteStrip) + ", baseline=" + describeSpan(transferRemoteBaseline) + ")");
            }
            if (remoteStrip.last() != static_cast<uint16_t>(transferRemoteBaseline.last() + step)) {
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

    return 0;
}
