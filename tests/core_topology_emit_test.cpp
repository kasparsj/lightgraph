#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "lightgraph/internal/Globals.h"
#include "lightgraph/internal/objects.hpp"
#include "lightgraph/internal/runtime.hpp"
#include "lightgraph/internal/topology.hpp"

namespace {

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

bool isNonBlack(const ColorRGB& color) {
    return color.R > 0 || color.G > 0 || color.B > 0;
}

bool isApproxColor(const ColorRGB& color, uint8_t r, uint8_t g, uint8_t b, int tolerance = 1) {
    return std::abs(static_cast<int>(color.R) - static_cast<int>(r)) <= tolerance &&
           std::abs(static_cast<int>(color.G) - static_cast<int>(g)) <= tolerance &&
           std::abs(static_cast<int>(color.B) - static_cast<int>(b)) <= tolerance;
}

uint8_t countConnectedPorts(const Intersection& intersection) {
    uint8_t used = 0;
    for (uint8_t i = 0; i < intersection.numPorts; ++i) {
        if (intersection.ports[i] != nullptr) {
            ++used;
        }
    }
    return used;
}

int findIntersectionIndexByTopPixel(TopologyObject& object, uint16_t topPixel, uint8_t groups = GROUP1) {
    const uint8_t count = object.countIntersections(groups);
    for (uint8_t i = 0; i < count; ++i) {
        Intersection* intersection = object.getIntersection(i, groups);
        if (intersection != nullptr && intersection->topPixel == topPixel) {
            return i;
        }
    }
    return -1;
}

Intersection* findIntersectionByTopPixel(TopologyObject& object, uint16_t topPixel, uint8_t groups = GROUP1) {
    const int index = findIntersectionIndexByTopPixel(object, topPixel, groups);
    if (index < 0) {
        return nullptr;
    }
    return object.getIntersection(static_cast<uint8_t>(index), groups);
}

int findPhysicalConnectionIndex(TopologyObject& object, uint8_t groups = GROUP1) {
    const uint8_t count = object.countConnections(groups);
    for (uint8_t i = 0; i < count; ++i) {
        Connection* connection = object.getConnection(i, groups);
        if (connection != nullptr && connection->numLeds > 0) {
            return i;
        }
    }
    return -1;
}

bool connectionMatches(const Connection* connection, uint16_t leftTopPixel, uint16_t rightTopPixel, int16_t numLeds = -1) {
    if (connection == nullptr || connection->from == nullptr || connection->to == nullptr) {
        return false;
    }

    const bool endpointsMatch =
        (connection->from->topPixel == leftTopPixel && connection->to->topPixel == rightTopPixel) ||
        (connection->from->topPixel == rightTopPixel && connection->to->topPixel == leftTopPixel);
    if (!endpointsMatch) {
        return false;
    }

    return numLeds < 0 || connection->numLeds == static_cast<uint16_t>(numLeds);
}

Connection* findConnectionByTopPixels(TopologyObject& object, uint16_t leftTopPixel, uint16_t rightTopPixel, int16_t numLeds = -1) {
    const uint8_t count = object.countConnections(GROUP1);
    for (uint8_t i = 0; i < count; ++i) {
        Connection* connection = object.getConnection(i, GROUP1);
        if (connectionMatches(connection, leftTopPixel, rightTopPixel, numLeds)) {
            return connection;
        }
    }
    return nullptr;
}

Port* portAtIntersection(Connection* connection, const Intersection* intersection) {
    if (connection == nullptr || intersection == nullptr) {
        return nullptr;
    }
    if (connection->from == intersection) {
        return connection->fromPort;
    }
    if (connection->to == intersection) {
        return connection->toPort;
    }
    return nullptr;
}

Port* findPortByConnectionLength(const Intersection& intersection, uint16_t numLeds) {
    for (uint8_t i = 0; i < intersection.numPorts; ++i) {
        Port* port = intersection.ports[i];
        if (port != nullptr && port->connection != nullptr && port->connection->numLeds == numLeds) {
            return port;
        }
    }
    return nullptr;
}

uint16_t countLitPixels(State& state, uint16_t startInclusive, uint16_t endExclusive) {
    uint16_t lit = 0;
    for (uint16_t pixel = startInclusive; pixel < endExclusive; ++pixel) {
        if (isNonBlack(state.getPixel(pixel))) {
            ++lit;
        }
    }
    return lit;
}

void advanceFrame(State& state, unsigned long deltaMillis = 16) {
    gMillis += deltaMillis;
    state.update();
}

class NoMirrorObject : public TopologyObject {
  public:
    explicit NoMirrorObject(uint16_t pixelCount) : TopologyObject(pixelCount) {
        addModel(new Model(0, pixelCount, GROUP1));
    }

    uint16_t* getMirroredPixels(uint16_t, Owner*, bool) override {
        mirroredPixels_[0] = 0;
        return mirroredPixels_;
    }

    EmitParams getModelParams(int model) const override {
        return EmitParams(model % 1, 0.0f);
    }

  private:
    uint16_t mirroredPixels_[2] = {0};
};

class StaticOwner : public Owner {
  public:
    StaticOwner() : Owner(GROUP1) {}

    uint8_t getType() override { return TYPE_CONNECTION; }

    void emit(RuntimeLight* const light) const override {
        if (light != nullptr) {
            light->owner = this;
        }
    }

    void update(RuntimeLight* const /*light*/) const override {}
};

class SolidRuntimeList : public LightList {
  public:
    explicit SolidRuntimeList(ColorRGB color) : color_(color) {}

    ColorRGB getColor(int16_t /*pixel*/ = -1) const override {
        return color_;
    }

  private:
    ColorRGB color_;
};

class GradientRuntimeList : public LightList {
  public:
    ColorRGB getColor(int16_t pixel = -1) const override {
        if (pixel <= 0) {
            return ColorRGB(200, 0, 0);
        }
        return ColorRGB(0, 0, 200);
    }
};

} // namespace

int main() {
    std::srand(23);
    gMillis = 0;

    // Topology setup: line should expose one bridge and one physical run.
    {
        Line line(300);
        const uint16_t expectedPhysicalLength = static_cast<uint16_t>(line.pixelCount - 3);
        if (line.countIntersections(GROUP1) != 2) {
            return fail("Line topology should create 2 intersections");
        }
        if (line.countConnections(GROUP1) != 2) {
            return fail("Line topology should create 2 connections");
        }
        if (line.getModel(L_DEFAULT) == nullptr || line.getModel(L_BOUNCE) == nullptr) {
            return fail("Line topology should create default and bounce models");
        }

        uint8_t zeroLengthConnections = 0;
        uint8_t physicalConnections = 0;
        const uint8_t connectionCount = line.countConnections(GROUP1);
        for (uint8_t i = 0; i < connectionCount; ++i) {
            Connection* connection = line.getConnection(i, GROUP1);
            if (connection == nullptr) {
                return fail("Line connection list should not contain null pointers");
            }
            if (connection->numLeds == 0) {
                ++zeroLengthConnections;
            }
            if (connection->numLeds == expectedPhysicalLength) {
                ++physicalConnections;
            }
        }
        if (zeroLengthConnections != 1 || physicalConnections != 1) {
            return fail("Line topology has unexpected connection lengths");
        }

        const uint8_t intersectionCount = line.countIntersections(GROUP1);
        for (uint8_t i = 0; i < intersectionCount; ++i) {
            Intersection* intersection = line.getIntersection(i, GROUP1);
            if (intersection == nullptr) {
                return fail("Line intersection list should not contain null pointers");
            }
            if (intersection->numPorts != 2) {
                return fail("Line intersections should expose exactly 2 ports");
            }
            if (countConnectedPorts(*intersection) != 2) {
                return fail("Line intersections should have both ports connected");
            }
        }
    }

    // Topology setup: cross should include center intersection and expected segment types.
    {
        Cross cross(CROSS_PIXEL_COUNT);
        const uint16_t expectedMediumLength = static_cast<uint16_t>(cross.pixelCount / 4 - 3);
        const uint16_t expectedLongLength = static_cast<uint16_t>(cross.pixelCount / 2 - 2);
        if (cross.countIntersections(GROUP1) != 5) {
            return fail("Cross topology should create 5 intersections");
        }
        if (cross.countConnections(GROUP1) != 8) {
            return fail("Cross topology should create 8 connections");
        }
        if (cross.getModel(C_DEFAULT) == nullptr ||
            cross.getModel(C_HORIZONTAL) == nullptr ||
            cross.getModel(C_VERTICAL) == nullptr ||
            cross.getModel(C_DIAGONAL) == nullptr) {
            return fail("Cross topology should create all model variants");
        }

        uint8_t zeroLengthConnections = 0;
        uint8_t mediumConnections = 0;
        uint8_t longConnections = 0;
        const uint8_t connectionCount = cross.countConnections(GROUP1);
        for (uint8_t i = 0; i < connectionCount; ++i) {
            Connection* connection = cross.getConnection(i, GROUP1);
            if (connection == nullptr) {
                return fail("Cross connection list should not contain null pointers");
            }
            if (connection->numLeds == 0) {
                ++zeroLengthConnections;
            } else if (connection->numLeds == expectedMediumLength) {
                ++mediumConnections;
            } else if (connection->numLeds == expectedLongLength) {
                ++longConnections;
            }
        }
        if (zeroLengthConnections != 2 || mediumConnections != 4 || longConnections != 2) {
            return fail("Cross topology has unexpected connection-length distribution");
        }

        Intersection* center = findIntersectionByTopPixel(cross, CROSS_PIXEL_COUNT / 4);
        if (center == nullptr) {
            return fail("Cross topology should include a center intersection at pixel 72");
        }
        if (center->bottomPixel != static_cast<int16_t>(CROSS_PIXEL_COUNT / 4 * 3)) {
            return fail("Cross center intersection has unexpected bottom pixel");
        }
        if (center->numPorts != 4 || countConnectedPorts(*center) != 4) {
            return fail("Cross center intersection should expose 4 connected ports");
        }

        const std::array<uint16_t, 4> edgePixels = {0, CROSS_PIXEL_COUNT / 2 - 1, CROSS_PIXEL_COUNT / 2, CROSS_PIXEL_COUNT - 1};
        for (uint16_t edgePixel : edgePixels) {
            Intersection* edge = findIntersectionByTopPixel(cross, edgePixel);
            if (edge == nullptr) {
                return fail("Cross topology missing expected edge intersection");
            }
            if (edge->numPorts != 3 || countConnectedPorts(*edge) != 3) {
                return fail("Cross edge intersections should expose 3 connected ports");
            }
        }
    }

    // Crossing behavior: deterministic horizontal routing should pass through center and stay on horizontal branch.
    {
        Cross cross(CROSS_PIXEL_COUNT);
        State state(cross);
        state.lightLists[0]->visible = false;

        Model* horizontalModel = cross.getModel(C_HORIZONTAL);
        if (horizontalModel == nullptr) {
            return fail("Cross horizontal model is missing");
        }

        Intersection* left = findIntersectionByTopPixel(cross, 0);
        Intersection* center = findIntersectionByTopPixel(cross, CROSS_PIXEL_COUNT / 4);
        Intersection* right = findIntersectionByTopPixel(cross, CROSS_PIXEL_COUNT / 2 - 1);
        if (left == nullptr || center == nullptr || right == nullptr) {
            return fail("Cross horizontal route endpoints could not be resolved");
        }

        const uint16_t expectedMediumLength = static_cast<uint16_t>(cross.pixelCount / 4 - 3);
        const uint16_t expectedLongLength = static_cast<uint16_t>(cross.pixelCount / 2 - 2);
        Connection* leftToCenter = findConnectionByTopPixels(cross, left->topPixel, center->topPixel, expectedMediumLength);
        Connection* centerToRight = findConnectionByTopPixels(cross, center->topPixel, right->topPixel, expectedMediumLength);
        Connection* leftToRightBridge = findConnectionByTopPixels(cross, left->topPixel, right->topPixel, expectedLongLength);
        if (leftToCenter == nullptr || centerToRight == nullptr || leftToRightBridge == nullptr) {
            return fail("Cross horizontal routing connections are missing");
        }

        Port* leftToCenterPortAtLeft = portAtIntersection(leftToCenter, left);
        Port* bridgePortAtLeft = portAtIntersection(leftToRightBridge, left);
        Port* centerToRightPortAtCenter = portAtIntersection(centerToRight, center);
        if (leftToCenterPortAtLeft == nullptr || bridgePortAtLeft == nullptr || centerToRightPortAtCenter == nullptr) {
            return fail("Unable to resolve ports needed for deterministic cross routing");
        }

        horizontalModel->clearWeights();
        horizontalModel->defaultW = 0;
        horizontalModel->setRoutingStrategy(RoutingStrategy::Deterministic);
        horizontalModel->put(leftToCenterPortAtLeft, 40);
        horizontalModel->put(bridgePortAtLeft, 1);
        horizontalModel->put(centerToRightPortAtCenter, 40);

        EmitParams params(C_HORIZONTAL, 1.0f, 0x00FF00);
        params.setLength(1);
        params.linked = false;
        params.from = findIntersectionIndexByTopPixel(cross, left->topPixel);
        if (params.from < 0) {
            return fail("Could not resolve source intersection index for cross routing test");
        }

        if (state.emit(params) < 0) {
            return fail("Cross routing emit failed unexpectedly");
        }

        bool sawCenter = false;
        bool sawRightHorizontal = false;
        bool sawVerticalBranch = false;
        for (int frame = 0; frame < 80; ++frame) {
            advanceFrame(state);
            if (isNonBlack(state.getPixel(center->topPixel))) {
                sawCenter = true;
            }
            if (countLitPixels(state, static_cast<uint16_t>(center->topPixel + 1), CROSS_PIXEL_COUNT / 2) > 0) {
                sawRightHorizontal = true;
            }
            if (countLitPixels(state, CROSS_PIXEL_COUNT / 2, CROSS_PIXEL_COUNT) > 0) {
                sawVerticalBranch = true;
            }
        }

        if (!sawCenter) {
            return fail("Cross routing never rendered the center intersection");
        }
        if (!sawRightHorizontal) {
            return fail("Cross routing never rendered pixels on the right horizontal branch");
        }
        if (sawVerticalBranch) {
            return fail("Cross routing leaked into the vertical branch in horizontal scenario");
        }
    }

    // Emit scenario: intersection emission should first render at emitter intersection pixel.
    {
        Line line(30);
        State state(line);
        state.lightLists[0]->visible = false;

        EmitParams params(L_BOUNCE, 1.0f, 0xCC2200);
        params.setLength(1);
        params.linked = false;
        params.from = findIntersectionIndexByTopPixel(line, 0);
        if (params.from < 0) {
            return fail("Unable to resolve line intersection index for emit test");
        }

        if (state.emit(params) < 0) {
            return fail("Line intersection emit failed unexpectedly");
        }
        advanceFrame(state);

        const ColorRGB sourceColor = state.getPixel(0);
        if (!isApproxColor(sourceColor, 0xCC, 0x22, 0x00, 1)) {
            return fail(
                "Intersection emit did not render expected color on source pixel (actual: " +
                std::to_string(sourceColor.R) + "," +
                std::to_string(sourceColor.G) + "," +
                std::to_string(sourceColor.B) + ")");
        }
        if (isNonBlack(state.getPixel(1))) {
            return fail("Intersection emit should not start on the first connection LED");
        }
    }

    // Regression: finite-duration sequential lists emitted after long device uptime should still
    // leave the source intersection instead of expiring at the handoff boundary.
    {
        Line line(30);
        State state(line);
        state.lightLists[0]->visible = false;

        gMillis = 5000;
        line.setNowMillis(gMillis);

        EmitParams params(L_BOUNCE, 1.0f, 0x22CC44);
        params.setLength(6);
        params.duration = 1000;
        params.from = findIntersectionIndexByTopPixel(line, 0);
        if (params.from < 0) {
            return fail("Unable to resolve line intersection for duration rebase regression");
        }

        if (state.emit(params) < 0) {
            return fail("Finite-duration emit failed unexpectedly after long uptime");
        }

        for (uint8_t frame = 0; frame < 4; ++frame) {
            gMillis += 16;
            line.setNowMillis(gMillis);
            state.update();
        }

        if (countLitPixels(state, 1, 6) == 0) {
            return fail("Finite-duration sequential emit should advance onto the outgoing connection after long uptime");
        }
    }

    // Emit scenario: connection emission + offset should start on shifted physical LED.
    {
        Line line(30);
        State state(line);
        state.lightLists[0]->visible = false;

        const int physicalConnectionIndex = findPhysicalConnectionIndex(line);
        if (physicalConnectionIndex < 0) {
            return fail("Unable to resolve physical line connection index");
        }

        EmitParams params(L_BOUNCE, 1.0f, 0x00CC00);
        params.setLength(1);
        params.linked = false;
        params.behaviourFlags = B_EMIT_FROM_CONN;
        params.from = physicalConnectionIndex;
        params.emitOffset = 5;

        if (state.emit(params) < 0) {
            return fail("Line connection emit with offset failed unexpectedly");
        }
        advanceFrame(state);

        if (!isApproxColor(state.getPixel(6), 0x00, 0xCC, 0x00, 1)) {
            return fail("Connection emit with offset did not render at expected pixel");
        }
        if (isNonBlack(state.getPixel(1))) {
            return fail("Connection emit offset should shift away from the unshifted origin pixel");
        }
    }

    // Fractional connection rendering should either split across neighboring LEDs or remain snapped
    // in compatibility builds.
    {
        Line line(30);
        State state(line);
        state.lightLists[0]->visible = false;

        const int physicalConnectionIndex = findPhysicalConnectionIndex(line);
        if (physicalConnectionIndex < 0) {
            return fail("Unable to resolve physical line connection for fractional connection test");
        }

        EmitParams params(L_BOUNCE, 0.0f, 0x00CC00);
        params.setLength(1);
        params.linked = false;
        params.behaviourFlags = B_EMIT_FROM_CONN;
        params.from = physicalConnectionIndex;

        const int8_t listIndex = state.emit(params);
        if (listIndex < 0) {
            return fail("Line fractional connection emit failed unexpectedly");
        }
        advanceFrame(state);

        LightList* list = state.lightLists[listIndex];
        RuntimeLight* light = (list != nullptr) ? list->lights[0] : nullptr;
        Connection* connection = line.getConnection(static_cast<uint8_t>(physicalConnectionIndex), GROUP1);
        if (light == nullptr || connection == nullptr) {
            return fail("Fractional connection test fixture is incomplete");
        }

        light->owner = connection;
        light->position = 5.25f;
        light->setOutPort(connection->fromPort, static_cast<int8_t>(connection->from->id));
        state.update();

#if LIGHTGRAPH_FRACTIONAL_RENDERING
        if (!isApproxColor(state.getPixel(6), 0, 153, 0, 1) ||
            !isApproxColor(state.getPixel(7), 0, 51, 0, 1)) {
            return fail("Fractional connection rendering should split 75/25 across adjacent LEDs");
        }
#else
        if (!isApproxColor(state.getPixel(6), 0, 204, 0, 1) || isNonBlack(state.getPixel(7))) {
            return fail("Compatibility build should keep fractional connection positions snapped");
        }
#endif

        light->owner = connection;
        light->position = 5.0f;
        light->setOutPort(connection->fromPort, static_cast<int8_t>(connection->from->id));
        state.update();

        if (!isApproxColor(state.getPixel(6), 0, 204, 0, 1) || isNonBlack(state.getPixel(7))) {
            return fail("Exact integer connection positions should render on a single LED");
        }

        const uint16_t finalConnectionPixel = connection->getPixel(connection->numLeds - 1);
        const uint16_t destinationIntersectionPixel = connection->to->topPixel;
        light->owner = connection;
        light->position = static_cast<float>(connection->numLeds) - 0.75f;
        light->setOutPort(connection->fromPort, static_cast<int8_t>(connection->from->id));
        state.update();

#if LIGHTGRAPH_FRACTIONAL_RENDERING
        if (!isApproxColor(state.getPixel(finalConnectionPixel), 0, 153, 0, 1) ||
            !isApproxColor(state.getPixel(destinationIntersectionPixel), 0, 51, 0, 1)) {
            return fail("Fractional arrival should blend between the last connection LED and destination intersection");
        }
#else
        if (!isApproxColor(state.getPixel(finalConnectionPixel), 0, 204, 0, 1) ||
            isNonBlack(state.getPixel(destinationIntersectionPixel))) {
            return fail("Compatibility build should keep connection arrival snapped to the final LED");
        }
#endif
    }

    // Fractional intersection handoff should either blend into the first outgoing LED or remain snapped
    // in compatibility and unsupported-port cases.
    {
        Line line(30);
        State state(line);
        state.lightLists[0]->visible = false;

        EmitParams params(L_BOUNCE, 0.0f, 0xCC2200);
        params.setLength(1);
        params.linked = false;
        params.from = findIntersectionIndexByTopPixel(line, 0);
        if (params.from < 0) {
            return fail("Unable to resolve line intersection for fractional handoff test");
        }

        const int8_t listIndex = state.emit(params);
        if (listIndex < 0) {
            return fail("Line fractional handoff emit failed unexpectedly");
        }
        advanceFrame(state);

        LightList* list = state.lightLists[listIndex];
        RuntimeLight* light = (list != nullptr) ? list->lights[0] : nullptr;
        Intersection* source = findIntersectionByTopPixel(line, 0);
        if (light == nullptr || source == nullptr) {
            return fail("Fractional handoff test fixture is incomplete");
        }

        Port* physicalPort =
            findPortByConnectionLength(*source, static_cast<uint16_t>(line.pixelCount - 3));
        if (physicalPort == nullptr) {
            return fail("Unable to resolve outgoing physical port for fractional handoff test");
        }

        light->owner = source;
        light->position = 0.25f;
        light->setOutPort(physicalPort, static_cast<int8_t>(source->id));
        state.update();

#if LIGHTGRAPH_FRACTIONAL_RENDERING
        if (!isApproxColor(state.getPixel(0), 153, 25, 0, 1) ||
            !isApproxColor(state.getPixel(1), 51, 8, 0, 1)) {
            return fail("Fractional intersection handoff should split between the node and first LED");
        }
#else
        if (!isApproxColor(state.getPixel(0), 204, 34, 0, 1) || isNonBlack(state.getPixel(1))) {
            return fail("Compatibility build should keep fractional handoff snapped to the intersection");
        }
#endif

        Port* zeroLengthPort = findPortByConnectionLength(*source, 0);
        if (zeroLengthPort == nullptr) {
            return fail("Unable to resolve zero-length port for fractional handoff fallback test");
        }

        light->owner = source;
        light->position = 0.5f;
        light->setOutPort(zeroLengthPort, static_cast<int8_t>(source->id));
        state.update();

        if (!isApproxColor(state.getPixel(0), 204, 34, 0, 1) || isNonBlack(state.getPixel(1))) {
            return fail("Zero-length outgoing ports should keep intersection rendering snapped");
        }
    }

    // External outgoing ports should keep the intersection render snapped even when fractional
    // rendering is enabled.
    {
        NoMirrorObject object(12);
        Intersection* intersection = object.addIntersection(new Intersection(2, 3, -1, GROUP1));
        if (intersection == nullptr) {
            return fail("Failed to create external-port handoff fixture");
        }
        const uint8_t device[6] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
        ExternalPort* externalPort = object.addExternalPort(intersection, 0, false, GROUP1, device, 9);
        if (externalPort == nullptr) {
            return fail("Failed to create external port for handoff fallback test");
        }

        State state(object);
        state.lightLists[0]->visible = false;

        EmitParams params(0, 0.0f, 0x33AA55);
        params.setLength(1);
        params.linked = false;
        params.from = 0;

        const int8_t listIndex = state.emit(params);
        if (listIndex < 0) {
            return fail("External-port handoff emit failed unexpectedly");
        }
        advanceFrame(state);

        LightList* list = state.lightLists[listIndex];
        RuntimeLight* light = (list != nullptr) ? list->lights[0] : nullptr;
        if (light == nullptr) {
            return fail("External-port handoff light fixture is missing");
        }

        light->owner = intersection;
        light->position = 0.5f;
        light->setOutPort(externalPort, static_cast<int8_t>(intersection->id));
        state.update();

        if (!isApproxColor(state.getPixel(3), 0x33, 0xAA, 0x55, 1) || countLitPixels(state, 0, 12) != 1) {
            return fail("External outgoing ports should keep intersection rendering snapped");
        }
    }

#if LIGHTGRAPH_FRACTIONAL_RENDERING
    // RuntimeLight split rendering should sample each target pixel's color independently.
    {
        NoMirrorObject object(2);
        State state(object);
        state.lightLists[0]->visible = false;

        GradientRuntimeList list;
        list.setup(1, 255);
        list.speed = 0.0f;
        list.lifeMillis = INFINITE_DURATION;

        RuntimeLight* light = list[0];
        if (light == nullptr) {
            return fail("Gradient runtime-light fixture did not allocate a light");
        }
        light->brightness = 255;
        light->setRenderedPixels(0, 1, 128);

        state.updateLight(light);

        if (!isApproxColor(state.getPixel(0), 100, 0, 0, 1) ||
            !isApproxColor(state.getPixel(1), 0, 0, 100, 1)) {
            return fail("Split RuntimeLight rendering should sample color from each target pixel");
        }
    }

    // Fractional contributions from the same list should accumulate before the list is blended
    // into the framebuffer, otherwise split lights visibly pump brightness when they overlap.
    {
        NoMirrorObject object(3);
        State state(object);
        state.lightLists[0]->visible = false;

        StaticOwner owner;
        SolidRuntimeList* list = new SolidRuntimeList(ColorRGB(0, 204, 0));
        list->model = object.getModel(0);
        list->setup(2, 255);
        list->speed = 0.0f;
        list->lifeMillis = INFINITE_DURATION;
        state.lightLists[1] = list;
        state.activateList(&owner, list);

        list->numEmitted = list->numLights;
        for (uint16_t i = 0; i < list->numLights; ++i) {
            RuntimeLight* light = list->lights[i];
            if (light == nullptr) {
                return fail("Same-list fractional accumulation fixture is incomplete");
            }
            light->owner = &owner;
        }

        list->lights[0]->setRenderedPixels(0, 1, 64);
        list->lights[1]->setRenderedPixels(1, 2, 64);

        gMillis = 0;
        lightgraphResetFrameTiming();
        state.update();

        if (!isApproxColor(state.getPixel(0), 0, 153, 0, 1) ||
            !isApproxColor(state.getPixel(1), 0, 204, 0, 1) ||
            !isApproxColor(state.getPixel(2), 0, 51, 0, 1)) {
            return fail("Same-list fractional contributions should accumulate before BLEND_NORMAL compositing");
        }
    }
#endif

    // Emit scenario: render-segment mode should paint the full connection span (including endpoints).
    {
        Line line(30);
        State state(line);
        state.lightLists[0]->visible = false;

        const int physicalConnectionIndex = findPhysicalConnectionIndex(line);
        if (physicalConnectionIndex < 0) {
            return fail("Unable to resolve physical line connection for segment test");
        }

        EmitParams params(L_BOUNCE, 1.0f, 0x2255FF);
        params.setLength(1);
        params.linked = false;
        params.behaviourFlags = static_cast<uint16_t>(B_EMIT_FROM_CONN | B_RENDER_SEGMENT);
        params.from = physicalConnectionIndex;

        if (state.emit(params) < 0) {
            return fail("Line render-segment emit failed unexpectedly");
        }
        advanceFrame(state);

        if (!isNonBlack(state.getPixel(0)) || !isNonBlack(state.getPixel(29))) {
            return fail("Render-segment emit should include both line endpoints");
        }
        if (countLitPixels(state, 0, 30) < 20) {
            return fail("Render-segment emit did not light enough pixels");
        }
    }

    // Render-segment mode must scale beyond CONNECTION_MAX_LEDS-sized connections.
    {
        Line line(64);
        State state(line);
        state.lightLists[0]->visible = false;

        const int physicalConnectionIndex = findPhysicalConnectionIndex(line);
        if (physicalConnectionIndex < 0) {
            return fail("Unable to resolve physical line connection for long segment test");
        }
        Connection* const physicalConnection = line.getConnection(static_cast<uint8_t>(physicalConnectionIndex), GROUP1);
        if (physicalConnection == nullptr) {
            return fail("Unable to materialize physical line connection for long segment test");
        }

        EmitParams params(L_BOUNCE, 1.0f, 0x22AA55);
        params.setLength(1);
        params.linked = false;
        params.behaviourFlags = static_cast<uint16_t>(B_EMIT_FROM_CONN | B_RENDER_SEGMENT);
        params.from = physicalConnectionIndex;

        if (state.emit(params) < 0) {
            return fail("Long render-segment emit failed unexpectedly");
        }
        advanceFrame(state);

        const uint16_t expectedLitPixels = static_cast<uint16_t>(physicalConnection->numLeds + 2);
        if (countLitPixels(state, 0, 64) != expectedLitPixels) {
            return fail("Render-segment emit should cover the full long connection span");
        }
    }

    // Emit scenario: mirror-rotate mode should render mirrored pixel pairs on line topology.
    {
        Line line(30);
        State state(line);
        state.lightLists[0]->visible = false;

        EmitParams params(L_BOUNCE, 1.0f, 0x0000EE);
        params.setLength(1);
        params.linked = false;
        params.behaviourFlags = B_MIRROR_ROTATE;
        params.from = findIntersectionIndexByTopPixel(line, 0);
        if (params.from < 0) {
            return fail("Unable to resolve line intersection for mirror scenario");
        }

        if (state.emit(params) < 0) {
            return fail("Mirror-rotate emit failed unexpectedly");
        }
        advanceFrame(state);

        if (!isApproxColor(state.getPixel(0), 0x00, 0x00, 0xEE, 1)) {
            return fail("Mirror-rotate emit did not render expected source pixel color");
        }
        if (!isApproxColor(state.getPixel(29), 0x00, 0x00, 0xEE, 1)) {
            return fail("Mirror-rotate emit did not render expected mirrored pixel color");
        }
    }

    // Emit scenario: max brightness should scale rendered LED intensity.
    {
        Line line(30);
        State state(line);
        state.lightLists[0]->visible = false;

        EmitParams params(L_BOUNCE, 1.0f, 0xC86432);
        params.setLength(1);
        params.linked = false;
        params.maxBri = 128;
        params.from = findIntersectionIndexByTopPixel(line, 0);
        if (params.from < 0) {
            return fail("Unable to resolve line intersection for brightness scenario");
        }

        if (state.emit(params) < 0) {
            return fail("Brightness-limited emit failed unexpectedly");
        }
        advanceFrame(state);

        const ColorRGB actual = state.getPixel(0);
        if (!isApproxColor(actual, 100, 50, 25, 2)) {
            return fail("Brightness-limited emit rendered unexpected color intensity");
        }
    }

    return 0;
}
