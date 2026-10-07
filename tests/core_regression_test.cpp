#include <cstdlib>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <lightgraph/engine.hpp>
#include <lightgraph/integration/factory.hpp>

#include "lightgraph/internal/Globals.h"
#include "lightgraph/internal/core/Limits.h"
#include "lightgraph/internal/core/Types.h"
#include "lightgraph/internal/objects.hpp"
#include "lightgraph/internal/rendering.hpp"
#include "lightgraph/internal/runtime.hpp"
#include "lightgraph/internal/runtime/RemoteSnapshotBuilder.h"
#include "lightgraph/internal/topology/TopologySummary.h"
#include "lightgraph/internal/topology.hpp"

namespace {

class TestOwner : public Owner {
  public:
    TestOwner() : Owner(0) {}
    uint8_t getType() override { return TYPE_CONNECTION; }
    void emit(RuntimeLight* const light) const override {
        if (light != nullptr) {
            light->owner = this;
        }
    }
    void update(RuntimeLight* const /*light*/) const override {}
};

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
    for (uint8_t i = 0; i < intersection.numPorts; i++) {
        if (intersection.ports[i] != nullptr) {
            used++;
        }
    }
    return used;
}

class SinglePixelObject : public TopologyObject {
  public:
    SinglePixelObject() : TopologyObject(1) {
        addModel(new Model(0, 10, GROUP1));
    }

    uint16_t* getMirroredPixels(uint16_t, Owner*, bool) override {
        mirroredPixels[0] = 0;
        return mirroredPixels;
    }

    EmitParams getModelParams(int model) const override {
        return EmitParams(model % 1, 1.0f);
    }

  private:
    uint16_t mirroredPixels[2] = {0};
};

ColorRGB sampleBlendResult(BlendMode mode) {
    SinglePixelObject object;
    State state(object);

    BgLight* const base = dynamic_cast<BgLight*>(state.lightLists[0]);
    if (base == nullptr) {
        return ColorRGB(0, 0, 0);
    }

    base->setup(object.pixelCount);
    base->setPalette(Palette({0x646464}, {0.0f}));  // 100,100,100
    base->blendMode = BLEND_NORMAL;
    base->visible = true;

    BgLight* const overlay = new BgLight();
    overlay->model = object.getModel(0);
    overlay->setDuration(INFINITE_DURATION);
    overlay->setup(object.pixelCount);
    overlay->setPalette(Palette({0xC83200}, {0.0f}));  // 200,50,0
    overlay->blendMode = mode;
    overlay->visible = true;
    state.lightLists[1] = overlay;
    state.totalLightLists++;

    state.update();
    return state.getPixel(0);
}

ColorRGB sampleReplaceOnlyResult() {
    SinglePixelObject object;
    State state(object);

    BgLight* const base = dynamic_cast<BgLight*>(state.lightLists[0]);
    if (base == nullptr) {
        return ColorRGB(0, 0, 0);
    }

    base->setup(object.pixelCount);
    base->setPalette(Palette({0x000000}, {0.0f}));
    base->visible = false;

    BgLight* const overlay = new BgLight();
    overlay->model = object.getModel(0);
    overlay->setDuration(INFINITE_DURATION);
    overlay->setup(object.pixelCount);
    overlay->setPalette(Palette({0xC83200}, {0.0f}));  // 200,50,0
    overlay->blendMode = BLEND_REPLACE;
    overlay->visible = true;
    state.lightLists[1] = overlay;
    state.totalLightLists++;

    state.update();
    return state.getPixel(0);
}

void advanceLightCadence(Light& light, const std::vector<unsigned long>& deltas) {
    gMillis = 0;
    lightgraphResetFrameTiming();
    lightgraphAdvanceFrameTiming(gMillis);
    for (const unsigned long delta : deltas) {
        gMillis += delta;
        lightgraphAdvanceFrameTiming(gMillis);
        light.nextFrame();
    }
}

std::vector<float> captureLightPositions(const LightList& list) {
    std::vector<float> positions;
    positions.reserve(list.numLights);
    for (uint16_t i = 0; i < list.numLights; i++) {
        RuntimeLight* const light = list[i];
        positions.push_back(light != nullptr ? light->position : 0.0f);
    }
    return positions;
}

struct StateCadenceSnapshot {
    uint16_t numEmitted = 0;
    std::vector<float> positions;
};

StateCadenceSnapshot runStateCadence(const std::vector<unsigned long>& deltas) {
    Line line(LINE_PIXEL_COUNT);
    State state(line);
    state.lightLists[0]->visible = false;

    EmitParams params(0, 1.0f, 0x00FF00);
    params.setLength(6);
    params.linked = false;
    params.from = 0;

    const int8_t listIndex = state.emit(params);
    if (listIndex < 0) {
        return {};
    }

    LightList* const list = state.lightLists[listIndex];
    gMillis = 0;
    lightgraphResetFrameTiming();
    state.update();
    for (const unsigned long delta : deltas) {
        gMillis += delta;
        state.update();
    }

    StateCadenceSnapshot snapshot;
    snapshot.numEmitted = list->numEmitted;
    snapshot.positions = captureLightPositions(*list);
    return snapshot;
}

}  // namespace

int main() {
    std::srand(2);
    gMillis = 0;
    lightgraphResetFrameTiming();

    // Off-by-one regression: model selector should allow LAST enum values.
    {
        Line line(LINE_PIXEL_COUNT);
        EmitParams lineParams = line.getModelParams(L_BOUNCE);
        if (lineParams.model != L_BOUNCE) {
            return fail("Line::getModelParams should preserve L_BOUNCE model");
        }
    }
    {
        Cross cross(CROSS_PIXEL_COUNT);
        EmitParams crossParams = cross.getModelParams(C_DIAGONAL);
        if (crossParams.model != C_DIAGONAL) {
            return fail("Cross::getModelParams should preserve C_DIAGONAL model");
        }
    }
    {
        Triangle triangle(TRIANGLE_PIXEL_COUNT);
        EmitParams triangleParams = triangle.getModelParams(T_COUNTER_CLOCKWISE);
        if (triangleParams.model != T_COUNTER_CLOCKWISE) {
            return fail("Triangle::getModelParams should preserve T_COUNTER_CLOCKWISE model");
        }
    }

    // Command parameter lookup should be non-owning and optional-based.
    {
        Line line(LINE_PIXEL_COUNT);
        const std::optional<EmitParams> one = line.getParams('1');
        if (!one.has_value()) {
            return fail("Line::getParams('1') should return parameters");
        }
        const std::optional<EmitParams> segment = line.getParams('/');
        if (!segment.has_value() || segment->getLength() != 1) {
            return fail("Line::getParams('/') should produce 1-length render-segment params");
        }
        if (line.getParams('x').has_value()) {
            return fail("Line::getParams('x') should return no params");
        }
    }

    // Topology editing regression: removing a connection must detach intersection ports.
    {
        SinglePixelObject object;
        Intersection* from = object.addIntersection(new Intersection(3, 10, -1, GROUP1));
        Intersection* to = object.addIntersection(new Intersection(3, 20, -1, GROUP1));
        Connection* initial = object.addConnection(new Connection(from, to, GROUP1, 0));

        if (countConnectedPorts(*from) != 1 || countConnectedPorts(*to) != 1) {
            return fail("Initial connection should consume exactly one port on each intersection");
        }

        if (!object.removeConnection(initial)) {
            return fail("TopologyObject::removeConnection(pointer) failed to remove an existing connection");
        }
        if (!object.conn[0].empty()) {
            return fail("TopologyObject::removeConnection(pointer) should erase connection from group list");
        }
        if (countConnectedPorts(*from) != 0 || countConnectedPorts(*to) != 0) {
            return fail("Connection removal should clear ports from both endpoint intersections");
        }

        object.addConnection(new Connection(from, to, GROUP1, 0));
        if (countConnectedPorts(*from) != 1 || countConnectedPorts(*to) != 1) {
            return fail("Reconnecting after removal should reuse cleared intersection port slots");
        }

        if (!object.removeConnection(0, 0)) {
            return fail("TopologyObject::removeConnection(group,index) failed for a valid connection slot");
        }
        if (countConnectedPorts(*from) != 0 || countConnectedPorts(*to) != 0) {
            return fail("Indexed connection removal should also detach endpoint ports");
        }
    }

    // Palette wrap behavior should be deterministic in repeat mode.
    {
        std::vector<ColorRGB> colors = {ColorRGB(1, 2, 3), ColorRGB(10, 20, 30)};
        const ColorRGB& wrapped = Palette::wrapColors(2, 0, colors, WRAP_REPEAT);
        if (wrapped.R != 1 || wrapped.G != 2 || wrapped.B != 3) {
            return fail("Palette::wrapColors repeat mode returned unexpected color");
        }
    }

    // RANDOM_COLOR palettes should resolve once and remain stable until mutated.
    {
        Palette palette({RANDOM_COLOR});
        const uint32_t first = palette.getRGBColors().front().get();
        const uint32_t second = palette.getRGBColors().front().get();
        const uint32_t third = palette.getRGBColors().front().get();
        if (first != second || second != third) {
            return fail("Palette::getRGBColors should not re-randomize unchanged RANDOM_COLOR stops");
        }
    }

    // Compatibility aliases should keep the old and new dim/interpolation APIs equivalent.
    {
        const ColorRGB original(0x804020);
        const ColorRGB legacyDimmed = original.Dim(127);
        const ColorRGB renamedDimmed = original.dim(127);
        if (!isApproxColor(legacyDimmed, renamedDimmed.R, renamedDimmed.G, renamedDimmed.B, 0)) {
            return fail("ColorRGB dim aliases should return identical values");
        }

        Palette palette({0x112233, 0x445566}, {0.0f, 1.0f});
        palette.setInterpolationMode(2);
        if (palette.getInterpolationMode() != 2 || palette.getInterMode() != 2) {
            return fail("Palette interpolation aliases should stay in sync");
        }
        palette.setInterMode(1);
        if (palette.getInterpolationMode() != 1) {
            return fail("Legacy Palette interpolation setter should update renamed API state");
        }
    }

    // Remote template speed should scale with sender->receiver pixel density ratio.
    {
        remote_snapshot::TemplateSnapshotDescriptor descriptor = {};
        descriptor.numLights = 4;
        descriptor.length = 6;
        descriptor.speed = 2.0f;
        descriptor.lifeMillis = 1200;
        descriptor.duration = 2400;
        descriptor.head = LIST_HEAD_FRONT;
        descriptor.interpolationMode = 2;
        descriptor.senderPixelDensity = 144;
        descriptor.receiverPixelDensity = 60;

        const std::vector<int64_t> colors = {0x22AA44};
        const std::vector<float> positions = {0.0f};

        LightList* list = remote_snapshot::buildTemplateSnapshot(descriptor, colors, positions);
        if (list == nullptr) {
            return fail("Remote template snapshot should materialize for valid descriptor");
        }

        const float expectedSpeed = 2.0f * (60.0f / 144.0f);
        if (std::fabs(list->speed - expectedSpeed) > 0.0001f) {
            delete list;
            return fail("Remote template speed should be scaled by receiver/sender pixel density");
        }
        const uint16_t expectedBodyLights = remote_snapshot::scaleLengthForDensity(
            descriptor.numLights, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
        const uint16_t expectedLength = remote_snapshot::scaleLengthForDensity(
            descriptor.length, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
        if (list->numLights != expectedLength) {
            delete list;
            return fail("Remote template snapshot should materialize the full scaled logical length");
        }
        const uint16_t reconstructedBodyLights =
            static_cast<uint16_t>(list->numLights - list->lead - list->trail);
        if (reconstructedBodyLights != expectedBodyLights) {
            delete list;
            return fail("Remote template snapshot should preserve scaled body-light count via lead/trail reconstruction");
        }
        const uint16_t expectedEdgeLights =
            (expectedLength > expectedBodyLights) ? static_cast<uint16_t>(expectedLength - expectedBodyLights) : 0;
        const uint16_t expectedLead = (descriptor.head == LIST_HEAD_FRONT && expectedEdgeLights > 0) ? 1 : 0;
        const uint16_t expectedTrail = (expectedEdgeLights > expectedLead)
            ? static_cast<uint16_t>(expectedEdgeLights - expectedLead)
            : 0;
        if (list->lead != expectedLead || list->trail != expectedTrail) {
            delete list;
            return fail("Remote template snapshot should reconstruct lead/trail from logical length");
        }
        for (uint16_t i = 0; i < list->numLights; i++) {
            if ((*list)[i] == nullptr) {
                delete list;
                return fail("Remote template snapshot should materialize each logical light");
            }
        }
        delete list;
    }

    // Remote sparse snapshots should scale indices and keep the brightest duplicate entry.
    {
        remote_snapshot::SequentialSnapshotDescriptor descriptor = {};
        descriptor.numLights = 3;
        descriptor.positionOffset = -5;
        descriptor.speed = 1.5f;
        descriptor.lifeMillis = 900;
        descriptor.senderPixelDensity = 60;
        descriptor.receiverPixelDensity = 60;

        std::vector<remote_snapshot::SequentialEntry> entries;
        entries.push_back({0, 40, 10, 20, 30});
        entries.push_back({4, 180, 100, 110, 120});
        entries.push_back({4, 120, 1, 2, 3});

        LightList* list = remote_snapshot::buildSequentialSnapshot(descriptor, entries);
        if (list == nullptr) {
            return fail("Remote sparse snapshot should materialize for valid descriptor");
        }

        const uint16_t expectedLength = remote_snapshot::scaleLengthForDensity(
            static_cast<uint16_t>(std::abs(descriptor.positionOffset)),
            descriptor.senderPixelDensity,
            descriptor.receiverPixelDensity);
        if (list->length != expectedLength || list->numLights != expectedLength) {
            delete list;
            return fail("Remote sparse snapshot should materialize the full logical length");
        }

        if (list->lead != 1 || list->trail != 1) {
            delete list;
            return fail("Remote sparse snapshot should reconstruct lead/trail from length minus body lights");
        }

        const uint16_t lastIdx = expectedLength - 1;
        RuntimeLight* brightest = (*list)[lastIdx];
        if (brightest == nullptr) {
            delete list;
            return fail("Remote sparse snapshot should map the last source light into the scaled target list");
        }
        const ColorRGB brightestColor = brightest->getColor();
        if (brightestColor.R != 100 || brightestColor.G != 110 || brightestColor.B != 120) {
            delete list;
            return fail("Remote sparse snapshot should keep the brightest duplicate mapped light");
        }
        delete list;
    }

    // Motion should advance by elapsed time, not by update count.
    {
        LightList list;
        list.speed = 1.0f;

        Light lightA(&list, list.speed, INFINITE_DURATION, 0, 255);
        Light lightB(&list, list.speed, INFINITE_DURATION, 0, 255);
        lightA.position = 0.0f;
        lightB.position = 0.0f;

        gMillis = 0;
        lightgraphResetFrameTiming();
        lightgraphAdvanceFrameTiming(gMillis);
        for (uint8_t i = 0; i < 10; i++) {
            gMillis += 16;
            lightgraphAdvanceFrameTiming(gMillis);
            lightA.nextFrame();
        }

        gMillis = 0;
        lightgraphResetFrameTiming();
        lightgraphAdvanceFrameTiming(gMillis);
        for (uint8_t i = 0; i < 5; i++) {
            gMillis += 32;
            lightgraphAdvanceFrameTiming(gMillis);
            lightB.nextFrame();
        }

#if LIGHTGRAPH_FPS_INDEPENDENT_SPEED
        if (std::fabs(lightA.position - lightB.position) > 0.001f) {
            return fail("Light motion should be FPS independent for equal elapsed time");
        }
        if (std::fabs(lightA.position - 10.0f) > 0.001f) {
            return fail("Light motion should preserve legacy speed at the reference frame rate");
        }
#else
        if (!(lightA.position > lightB.position + 0.001f)) {
            return fail("Legacy frame-driven motion should depend on update count");
        }
#endif
    }

    // State updates should preserve staggered sequential emission across coarse frame deltas.
    {
        const StateCadenceSnapshot fineCadence = runStateCadence({16, 16, 16, 16});
        const StateCadenceSnapshot coarseCadence = runStateCadence({64});

        if (fineCadence.positions.empty() || coarseCadence.positions.empty()) {
            return fail("State cadence test failed to materialize an emitted list");
        }
        if (fineCadence.numEmitted != coarseCadence.numEmitted) {
            return fail("State::update should emit the same number of sequential lights across equivalent elapsed time");
        }
        if (fineCadence.positions.size() != coarseCadence.positions.size()) {
            return fail("State::update cadence comparison should preserve light counts");
        }
        for (size_t i = 0; i < fineCadence.positions.size(); i++) {
            if (std::fabs(fineCadence.positions[i] - coarseCadence.positions[i]) > 0.001f) {
                return fail("State::update should preserve per-light positions across coarse frame deltas");
            }
        }
    }

    // Fade progression should now follow elapsed time instead of update cadence.
    {
        LightList fadeListA;
        fadeListA.setFade(5, 0, EASE_NONE);
        Light fadeLightA(&fadeListA, 0.0f, INFINITE_DURATION, 0, 255);
        fadeLightA.bri = 0;

        LightList fadeListB;
        fadeListB.setFade(5, 0, EASE_NONE);
        Light fadeLightB(&fadeListB, 0.0f, INFINITE_DURATION, 0, 255);
        fadeLightB.bri = 0;

        advanceLightCadence(fadeLightA, {16, 16, 16, 16, 16, 16, 16, 16, 16, 16});
        advanceLightCadence(fadeLightB, {32, 32, 32, 32, 32});

#if LIGHTGRAPH_FPS_INDEPENDENT_SPEED
        if (fadeLightA.bri != fadeLightB.bri || fadeLightA.brightness != fadeLightB.brightness) {
            return fail("Fade progression should preserve brightness across equivalent elapsed time");
        }
#endif
    }

    // Lifecycle smoke: emit should render at least one pixel and stopAll should clear active lists.
    {
        Line line(LINE_PIXEL_COUNT);
        State state(line);
        state.lightLists[0]->visible = false;  // Ignore background for this check.

        EmitParams params(0, 1.0f, 0x00FF00);
        params.setLength(3);
        params.noteId = 42;

        const int8_t listIndex = state.emit(params);
        if (listIndex < 0) {
            return fail("State::emit failed unexpectedly for valid line model");
        }

        const uint16_t listId = state.lightLists[listIndex]->id;
        if (state.findListById(listId) != state.lightLists[listIndex]) {
            return fail("State::findListById did not return the expected list pointer");
        }

        state.update();

        uint16_t litPixels = 0;
        for (uint16_t p = 0; p < line.pixelCount; p++) {
            if (isNonBlack(state.getPixel(p))) {
                litPixels++;
            }
        }
        if (litPixels == 0) {
            return fail("State::update produced no lit pixels after emit");
        }

        // Same note ID should reuse the existing list slot.
        EmitParams paramsSameNote(0, 1.0f, 0x0000FF);
        paramsSameNote.setLength(5);
        paramsSameNote.noteId = 42;
        const int8_t reusedIndex = state.emit(paramsSameNote);
        if (reusedIndex != listIndex) {
            return fail("State should reuse list index for the same noteId");
        }

        state.stopNote(42);
        for (int i = 0; i < 8; i++) {
            gMillis += 250;
            state.update();
        }
        if (state.totalLights != 0) {
            return fail("State::stopNote did not clear active lights for the note");
        }

        // Note IDs above 255 should not truncate and collide.
        EmitParams paramsWideNote(0, 1.0f, 0x00FFFF);
        paramsWideNote.setLength(4);
        paramsWideNote.noteId = 300;
        const int8_t wideIndex = state.emit(paramsWideNote);
        if (wideIndex < 0) {
            return fail("State::emit failed for noteId > 255");
        }

        EmitParams paramsWideNoteAgain(0, 1.0f, 0x00AAFF);
        paramsWideNoteAgain.setLength(2);
        paramsWideNoteAgain.noteId = 300;
        const int8_t wideReused = state.emit(paramsWideNoteAgain);
        if (wideReused != wideIndex) {
            return fail("State should reuse list index for noteId > 255");
        }

        const uint8_t listCountBeforeActivate = state.totalLightLists;
        Owner* const originalEmitter = state.lightLists[wideIndex]->emitter;
        TestOwner ingressOwner;
        state.activateList(&ingressOwner, state.lightLists[wideIndex], 0, false);
        if (state.totalLightLists != listCountBeforeActivate) {
            return fail("State::activateList(countTotals=false) should not increment list counters");
        }
        if (state.lightLists[wideIndex]->emitter != &ingressOwner) {
            return fail("State::activateList should attach the provided emitter");
        }
        state.activateList(&ingressOwner, state.lightLists[wideIndex], 7, false);
        if (state.lightLists[wideIndex]->emitOffset != 7) {
            return fail("State::activateList should persist emitOffset on the list");
        }
        state.lightLists[wideIndex]->emitter = originalEmitter;

        state.stopNote(300);
        for (int i = 0; i < 8; i++) {
            gMillis += 250;
            state.update();
        }
        if (state.findList(300) != -1 || state.totalLights != 0) {
            return fail("State::stopNote should stop active list for noteId > 255");
        }

        EmitParams stopAllParams(0, 1.0f, 0xAA00FF);
        stopAllParams.setLength(4);
        stopAllParams.noteId = 43;
        if (state.emit(stopAllParams) < 0) {
            return fail("State::emit failed before stopAll verification");
        }

        state.stopAll();
        for (int i = 0; i < 8; i++) {
            gMillis += 250;
            state.update();
        }
        if (state.totalLights != 0) {
            return fail("State::stopAll did not clear active emitted lights");
        }
    }

    // Failed emits must not consume a runtime slot or evict an existing list.
    {
        Line line(LINE_PIXEL_COUNT);
        State state(line);
        state.lightLists[0]->visible = false;

        EmitParams valid(0, 1.0f, 0x44CC88);
        valid.setLength(4);
        valid.noteId = 77;
        const int8_t validIndex = state.emit(valid);
        if (validIndex < 0) {
            return fail("Baseline emit failed before failed-emit slot regression");
        }

        LightList* const original = state.lightLists[validIndex];
        if (original == nullptr) {
            return fail("Baseline emit did not materialize a light list");
        }

        int occupiedBefore = 0;
        for (uint8_t i = 0; i < MAX_LIGHT_LISTS; i++) {
            if (state.lightLists[i] != nullptr) {
                occupiedBefore++;
            }
        }

        EmitParams invalidEmitter(0, 1.0f, 0xAA5500);
        invalidEmitter.setLength(2);
        invalidEmitter.emitGroups = GROUP2;
        if (state.emit(invalidEmitter) != -1) {
            return fail("Emit with no matching emitter should fail");
        }

        int occupiedAfter = 0;
        for (uint8_t i = 0; i < MAX_LIGHT_LISTS; i++) {
            if (state.lightLists[i] != nullptr) {
                occupiedAfter++;
            }
        }

        if (occupiedAfter != occupiedBefore) {
            return fail("Failed emit should not consume a light-list slot");
        }
        if (state.lightLists[validIndex] != original) {
            return fail("Failed emit should not replace an existing active list");
        }
        if (state.findList(77) != validIndex) {
            return fail("Failed emit should preserve existing note-to-slot mapping");
        }
    }

    // Recoloring an active list should refresh both palette metadata and live light colors.
    {
        Line line(LINE_PIXEL_COUNT);
        State state(line);
        state.lightLists[0]->visible = false;

        EmitParams params(0, 1.0f, 0x112233);
        params.setLength(3);
        const int8_t listIndex = state.emit(params);
        if (listIndex < 0) {
            return fail("State::emit failed unexpectedly for recolor regression");
        }

        LightList* const list = state.lightLists[listIndex];
        Light* const firstLight = dynamic_cast<Light*>((*list)[0]);
        if (firstLight == nullptr) {
            return fail("Emitter-created list should materialize Light instances");
        }

        const ColorRGB before = firstLight->getColor();
        std::srand(12345);
        state.colorAll();

        const std::vector<ColorRGB>& paletteColors = list->palette.getRGBColors();
        if (paletteColors.empty()) {
            return fail("State::colorAll should keep at least one palette color");
        }

        const ColorRGB after = firstLight->getColor();
        const ColorRGB expected = paletteColors[0];
        if (after.get() != expected.get()) {
            return fail("State::colorAll should refresh active light colors to match the new palette");
        }
        if (after.get() == before.get()) {
            return fail("State::colorAll should update the active list color");
        }
    }

    // Counter regression: reusing a counted zero-light list must keep list count stable.
    {
        Line line(LINE_PIXEL_COUNT);
        State state(line);
        state.lightLists[0]->visible = false;

        LightList* const reused = new LightList();
        reused->noteId = 999;
        reused->length = 1;
        reused->model = line.getModel(0);
        reused->emitter = line.getIntersection(0, GROUP1);
        if (reused->emitter == nullptr) {
            delete reused;
            return fail("Expected line topology to provide a default emitter");
        }
        state.lightLists[1] = reused;
        state.totalLightLists++;

        const uint8_t listCountBeforeReuse = state.totalLightLists;
        EmitParams params(0, 1.0f, 0x11AAFF);
        params.setLength(4);
        params.noteId = 999;

        const int8_t reusedIndex = state.emit(params);
        if (reusedIndex != 1) {
            return fail("State should reuse pre-existing zero-light slot for matching noteId");
        }
        if (state.totalLightLists != listCountBeforeReuse) {
            return fail("State::emit should not inflate totalLightLists on zero-light list reuse");
        }
    }

    // Centered lists reserve stable slots and update only their symmetric coverage window.
    {
        Line line(64);
        line.setNowMillis(0);
        State state(line);
        EmitParams params(0, 0.5f, 0xFFFFFF);
        params.setLength(4);
        params.noteId = 42;
        params.duration = 100;
        params.lengthMode = lightgraph::LengthMode::Centered;
        params.visibleLength = 2.0f;
        const int8_t index = state.emit(params);
        if (index < 1) {
            return fail("Centered list emit should succeed on a local sequential topology");
        }
        LightList* list = state.lightLists[index];
        if (list == nullptr || list->numLights != 4 || list->trail != 0 ||
            list->lengthMode != lightgraph::LengthMode::Centered) {
            return fail("Centered list should reserve its full capacity without an automatic trail");
        }
        if (list->centeredCoverage(0) != 0 || list->centeredCoverage(1) != 255 ||
            list->centeredCoverage(2) != 255 || list->centeredCoverage(3) != 0) {
            return fail("Centered even-length coverage should be symmetric");
        }
        LightList oddWindow;
        oddWindow.lengthMode = lightgraph::LengthMode::Centered;
        oddWindow.numLights = 5;
        oddWindow.visibleLength = 3.0f;
        if (oddWindow.centeredCoverage(0) != 0 || oddWindow.centeredCoverage(1) != 255 ||
            oddWindow.centeredCoverage(2) != 255 || oddWindow.centeredCoverage(3) != 255 ||
            oddWindow.centeredCoverage(4) != 0) {
            return fail("Centered odd-length coverage should remain symmetric");
        }
        std::vector<RuntimeLight*> stableLights(list->lights, list->lights + list->numLights);
        for (uint16_t lightIndex = 0; lightIndex < list->numLights; ++lightIndex) {
            if (list->lights[lightIndex] == nullptr ||
                list->lights[lightIndex]->getLife() != list->lifeMillis) {
                return fail("Centered list lights should share one expiration deadline");
            }
        }

        const lightgraph::ListLengthUpdate half[] = {{42, 1.0f}};
        if (!state.setListLengths(half, 1) || list->visibleLength != 1.0f ||
            list->centeredCoverage(1) < 127 || list->centeredCoverage(1) > 128 ||
            list->centeredCoverage(2) < 127 || list->centeredCoverage(2) > 128) {
            return fail("Fractional centered-length updates should split coverage symmetrically");
        }
        for (uint16_t lightIndex = 0; lightIndex < list->numLights; ++lightIndex) {
            if (list->lights[lightIndex] != stableLights[lightIndex]) {
                return fail("Centered resize must not allocate or replace reserved light slots");
            }
        }
        const lightgraph::ListLengthUpdate unknown[] = {{65000, 3.0f}};
        if (!state.setListLengths(unknown, 1)) {
            return fail("Valid unknown list IDs should be ignored");
        }
        const lightgraph::ListLengthUpdate negativeUnknown[] = {{65000, -1.0f}};
        if (state.setListLengths(negativeUnknown, 1)) {
            return fail("Invalid lengths must reject the whole batch even for unknown IDs");
        }
        const lightgraph::ListLengthUpdate duplicate[] = {{42, 0.0f}, {42, 2.0f}};
        if (state.setListLengths(duplicate, 2) || list->visibleLength != 1.0f) {
            return fail("Duplicate IDs must reject atomically without changing visible length");
        }

        EmitParams legacy(0, 0.0f, 0xFFFFFF);
        legacy.setLength(1);
        legacy.noteId = 44;
        legacy.duration = 10;
        const int8_t legacyIndex = state.emit(legacy);
        if (legacyIndex < 0) {
            return fail("Legacy expiry fixture should emit successfully");
        }
        line.setNowMillis(10);
        const lightgraph::ListLengthUpdate expiredAndLiving[] = {{44, 1.0f}, {42, 2.0f}};
        if (state.setListLengths(expiredAndLiving, 2) || list->visibleLength != 1.0f) {
            return fail("A deadline-passed legacy list must remain incompatible until route-aware expiry");
        }
        LightList* const expiredLegacy = state.lightLists[legacyIndex];
        for (uint16_t i = 0; i < expiredLegacy->numLights; ++i) {
            if (expiredLegacy->lights[i] != nullptr) {
                expiredLegacy->lights[i]->isExpired = true;
            }
        }
        if (!state.setListLengths(expiredAndLiving, 2) || list->visibleLength != 2.0f) {
            return fail("Actually expired pre-sweep IDs should be ignored without rejecting living updates");
        }

        line.setNowMillis(100);
        state.update();
        if (state.findList(42) >= 0) {
            return fail("Centered lists should expire as a whole at their common deadline");
        }

        EmitParams unsupported(0, 0.5f, 0xFFFFFF);
        unsupported.setLength(4);
        unsupported.noteId = 43;
        unsupported.lengthMode = lightgraph::LengthMode::Centered;
        unsupported.order = LIST_ORDER_RANDOM;
        if (state.emit(unsupported) >= 0) {
            return fail("Centered lists should reject random ordering");
        }
    }

    // Centered lists keep moving through graph junctions while fully hidden, then grow in place.
    {
        Cross cross(CROSS_PIXEL_COUNT);
        cross.setNowMillis(0);
        State state(cross);
        state.lightLists[0]->visible = false;

        int8_t leftIndex = -1;
        for (uint8_t i = 0; i < cross.countIntersections(GROUP1); ++i) {
            const Intersection* const intersection = cross.getIntersection(i, GROUP1);
            if (intersection != nullptr && intersection->topPixel == 0) {
                leftIndex = static_cast<int8_t>(i);
                break;
            }
        }
        if (leftIndex < 0) {
            return fail("Centered hidden-motion fixture could not resolve the left intersection");
        }

        EmitParams params(C_HORIZONTAL, 1.0f, 0x44CC88);
        params.setLength(4);
        params.noteId = 45;
        params.from = leftIndex;
        params.duration = 10000;
        params.lengthMode = lightgraph::LengthMode::Centered;
        params.visibleLength = 0.0f;
        const int8_t index = state.emit(params);
        if (index < 1) {
            return fail("Centered zero-length list should emit on the cross topology");
        }
        LightList* const list = state.lightLists[index];
        const uint32_t deadline = list->lifeMillis;

        RuntimeLight* routedLight = nullptr;
        for (uint16_t frame = 1; frame <= 160 && routedLight == nullptr; ++frame) {
            cross.setNowMillis(static_cast<unsigned long>(frame) * 16UL);
            state.update();
            for (uint16_t slot = 1; slot <= 2; ++slot) {
                RuntimeLight* const candidate = list->lights[slot];
                if (candidate != nullptr && candidate->owner != nullptr && candidate->pixel1 > 72 &&
                    candidate->pixel1 < 143) {
                    routedLight = candidate;
                    break;
                }
            }
        }
        if (routedLight == nullptr) {
            return fail("A hidden centered list should cross the center junction and retain its route");
        }

        RuntimeLight* const stableLight = routedLight;
        const Owner* const stableOwner = routedLight->owner;
        Port* const stableInPort = routedLight->inPort;
        Port* const stableOutPort = routedLight->outPort;
        const float stablePosition = routedLight->position;
        const lightgraph::ListLengthUpdate grow[] = {{45, 2.0f}};
        if (!state.setListLengths(grow, 1) || list->lights[routedLight->idx] != stableLight ||
            routedLight->owner != stableOwner || routedLight->inPort != stableInPort ||
            routedLight->outPort != stableOutPort || routedLight->position != stablePosition ||
            list->lifeMillis != deadline) {
            return fail("Growing a hidden centered list must preserve identity, route, position, and deadline");
        }

        cross.setNowMillis(cross.nowMillis() + 16UL);
        state.update();
        if (routedLight->pixel1 < 0 ||
            !isNonBlack(state.getPixel(static_cast<uint16_t>(routedLight->pixel1)))) {
            return fail("A centered list should become visible at its retained post-junction route");
        }
    }

    // A centered re-emit reserves the requested maximum exactly, even with legacy smoothing enabled.
    {
        Line line(32);
        State state(line);
        state.lightLists[0]->visible = false;
        EmitParams params(0, 0.0f, 0xFFFFFF);
        params.setLength(4);
        params.noteId = 54;
        params.lengthMode = lightgraph::LengthMode::Centered;
        params.visibleLength = 0.0f;
        if (state.emit(params) < 1) {
            return fail("Initial centered capacity fixture should emit");
        }
        params.setLength(8);
        params.behaviourFlags = B_SMOOTH_CHANGES;
        const int8_t resizedIndex = state.emit(params);
        if (resizedIndex < 1 || state.lightLists[resizedIndex]->numLights != 8 ||
            state.lightLists[resizedIndex]->length != 8 || state.totalLights != 8) {
            return fail("Centered re-emit should bypass legacy length smoothing for reserved capacity");
        }
    }

    // Centered mode rejects configurations that cannot preserve stable path capacity.
    {
        Line line(32);
        if (line.hasExternalPorts()) {
            return fail("Local topology should not report external ports");
        }
        State state(line);
        state.lightLists[0]->visible = false;

        const auto rejected = [&](uint16_t noteId, bool linked, uint16_t flags) {
            EmitParams params(0, 0.0f, 0xFFFFFF);
            params.setLength(4);
            params.noteId = noteId;
            params.linked = linked;
            params.behaviourFlags = flags;
            params.lengthMode = lightgraph::LengthMode::Centered;
            return state.emit(params) < 0;
        };
        if (!rejected(46, false, 0) || !rejected(47, true, B_RENDER_SEGMENT) ||
            !rejected(48, true, B_FILL_EASE)) {
            return fail("Centered lists should reject unlinked, segment-render, and fill configurations");
        }

        SinglePixelObject externalObject;
        Intersection* const ingress = externalObject.addIntersection(
            new Intersection(2, 0, -1, GROUP1));
        const uint8_t remoteDevice[6] = {1, 2, 3, 4, 5, 6};
        if (ingress == nullptr ||
            externalObject.addExternalPort(ingress, 0, false, GROUP1, remoteDevice, 9) == nullptr) {
            return fail("Centered cross-device rejection fixture could not create an external port");
        }
        if (!externalObject.hasExternalPorts()) {
            return fail("Topology should report registered external ports without snapshot allocation");
        }
        State externalState(externalObject);
        EmitParams external(0, 0.0f, 0xFFFFFF);
        external.setLength(4);
        external.noteId = 49;
        external.lengthMode = lightgraph::LengthMode::Centered;
        if (externalState.emit(external) >= 0) {
            return fail("Centered lists should reject topologies with external device ports");
        }
    }

    // Reserved centered capacity counts against both the light and list limits.
    {
        Line line(32);
        State state(line);
        state.lightLists[0]->visible = false;
        EmitParams capacity(0, 0.0f, 0xFFFFFF);
        capacity.setLength(MAX_TOTAL_LIGHTS);
        capacity.noteId = 50;
        capacity.lengthMode = lightgraph::LengthMode::Centered;
        capacity.visibleLength = 0.0f;
        if (state.emit(capacity) < 1 || state.totalLights != MAX_TOTAL_LIGHTS) {
            return fail("Centered capacity should reserve and count every hidden light slot");
        }
        EmitParams overflow = capacity;
        overflow.setLength(1);
        overflow.noteId = 51;
        if (state.emit(overflow) >= 0 || state.totalLights != MAX_TOTAL_LIGHTS) {
            return fail("Centered capacity should respect the total-light resource limit atomically");
        }
    }
    {
        Line line(32);
        State state(line);
        state.lightLists[0]->visible = false;
        for (uint16_t i = 1; i < MAX_LIGHT_LISTS; ++i) {
            EmitParams params(0, 0.0f, 0xFFFFFF);
            params.setLength(1);
            params.noteId = static_cast<uint16_t>(100 + i);
            params.lengthMode = lightgraph::LengthMode::Centered;
            params.visibleLength = 0.0f;
            if (state.emit(params) < 1) {
                return fail("Centered list-limit fixture should fill every available local list slot");
            }
        }
        EmitParams overflow(0, 0.0f, 0xFFFFFF);
        overflow.setLength(1);
        overflow.noteId = 200;
        overflow.lengthMode = lightgraph::LengthMode::Centered;
        if (state.emit(overflow) >= 0 || state.totalLightLists != MAX_LIGHT_LISTS) {
            return fail("Centered lists should respect the reserved list-slot limit");
        }
    }

    // Blend-mode regressions: deterministic 1-pixel compositing across all modes.
    {
        struct BlendExpectation {
            BlendMode mode;
            uint8_t r;
            uint8_t g;
            uint8_t b;
            const char* name;
        };

        // Golden values for base(100,100,100) + overlay(200,50,0) stacked BgLight blend.
        const std::vector<BlendExpectation> expectations = {
            {BLEND_NORMAL, 150, 75, 50, "BLEND_NORMAL"},
            {BLEND_ADD, 255, 150, 100, "BLEND_ADD"},
            {BLEND_MULTIPLY, 78, 19, 0, "BLEND_MULTIPLY"},
            {BLEND_SCREEN, 221, 130, 99, "BLEND_SCREEN"},
            {BLEND_OVERLAY, 156, 39, 0, "BLEND_OVERLAY"},
            {BLEND_REPLACE, 200, 50, 0, "BLEND_REPLACE"},
            {BLEND_SUBTRACT, 0, 50, 100, "BLEND_SUBTRACT"},
            {BLEND_DIFFERENCE, 100, 50, 100, "BLEND_DIFFERENCE"},
            {BLEND_EXCLUSION, 143, 110, 100, "BLEND_EXCLUSION"},
            {BLEND_DODGE, 255, 124, 100, "BLEND_DODGE"},
            {BLEND_BURN, 57, 0, 0, "BLEND_BURN"},
            {BLEND_HARD_LIGHT, 188, 39, 0, "BLEND_HARD_LIGHT"},
            {BLEND_SOFT_LIGHT, 133, 63, 39, "BLEND_SOFT_LIGHT"},
            {BLEND_LINEAR_LIGHT, 244, 0, 0, "BLEND_LINEAR_LIGHT"},
            {BLEND_VIVID_LIGHT, 231, 0, 0, "BLEND_VIVID_LIGHT"},
            {BLEND_PIN_LIGHT, 145, 100, 0, "BLEND_PIN_LIGHT"},
        };

        for (const BlendExpectation& expected : expectations) {
            const ColorRGB actual = sampleBlendResult(expected.mode);
            if (!isApproxColor(actual, expected.r, expected.g, expected.b, 2)) {
                return fail(std::string(expected.name) + " produced unexpected blended color");
            }
        }

        const ColorRGB replaceOnly = sampleReplaceOnlyResult();
        if (!isApproxColor(replaceOnly, 200, 50, 0, 2)) {
            return fail("BLEND_REPLACE should set color on first write without prior contributors");
        }
    }

    // Fade threshold boundary: 255 must not divide by zero and should produce zero brightness.
    {
        LightList list;
        list.fadeThresh = 255;
        list.minBri = 0;
        list.maxBri = 200;
        list.fadeEase = ofxeasing::linear::easeNone;

        RuntimeLight runtimeLight(&list, 0, 200);
        runtimeLight.bri = 255;
        if (runtimeLight.getBrightness() != 0) {
            return fail("RuntimeLight::getBrightness should be zero when fadeThresh is 255");
        }

        Light light(&list, 1.0f, 0, 0, 200);
        light.bri = 255;
        if (light.getBrightness() != 0) {
            return fail("Light::getBrightness should be zero when fadeThresh is 255");
        }
    }

    // Noise brightness must follow the list's bound runtime context, never shared process state.
    {
        LightgraphRuntimeContext firstContext;
        LightgraphRuntimeContext secondContext;
        firstContext.perlinNoise.SetSeed(101);
        secondContext.perlinNoise.SetSeed(202);

        LightList list;
        RuntimeLight light(&list);
        Behaviour noiseBehaviour(B_BRI_CONST_NOISE);

        int16_t sampledPixel = -1;
        uint16_t firstExpected = 0;
        uint16_t secondExpected = 0;
        for (int16_t pixel = 0; pixel < 128; ++pixel) {
            const float firstValue = firstContext.perlinNoise.GetValue(list.id * 10, pixel * 100);
            const float secondValue = secondContext.perlinNoise.GetValue(list.id * 10, pixel * 100);
            if (firstValue >= 0.0f && secondValue >= 0.0f) {
                const uint16_t firstCandidate = static_cast<uint16_t>(firstValue * 255);
                const uint16_t secondCandidate = static_cast<uint16_t>(secondValue * 255);
                if (firstCandidate != secondCandidate) {
                    sampledPixel = pixel;
                    firstExpected = firstCandidate;
                    secondExpected = secondCandidate;
                    break;
                }
            }
        }
        if (sampledPixel < 0) {
            return fail("Noise context regression fixture should find distinct seeded samples");
        }

        light.pixel1 = sampledPixel;
        list.bindRuntimeContext(firstContext);
        if (noiseBehaviour.getBri(&light) != firstExpected) {
            return fail("B_BRI_CONST_NOISE should sample the list's first runtime context");
        }
        list.bindRuntimeContext(secondContext);
        if (noiseBehaviour.getBri(&light) != secondExpected) {
            return fail("B_BRI_CONST_NOISE should follow a newly bound runtime context");
        }
    }

    // LIST_ORDER_NOISE must use its bound context and preserve the default-context fallback.
    {
        LightList list;
        LightgraphRuntimeContext firstContext;
        LightgraphRuntimeContext secondContext;
        uint16_t firstExpected = 0;
        uint16_t secondExpected = 0;
        bool foundDistinctSample = false;
        for (int firstSeed = 1; firstSeed < 64 && !foundDistinctSample; ++firstSeed) {
            firstContext.perlinNoise.SetSeed(firstSeed);
            const float firstValue = firstContext.perlinNoise.GetValue(list.id * 10, 0);
            if (firstValue < 0.0f) {
                continue;
            }
            for (int secondSeed = firstSeed + 1; secondSeed < 64; ++secondSeed) {
                secondContext.perlinNoise.SetSeed(secondSeed);
                const float secondValue = secondContext.perlinNoise.GetValue(list.id * 10, 0);
                const uint16_t firstCandidate = static_cast<uint16_t>(firstValue * FULL_BRIGHTNESS);
                if (secondValue >= 0.0f) {
                    const uint16_t secondCandidate =
                        static_cast<uint16_t>(secondValue * FULL_BRIGHTNESS);
                    if (firstCandidate != secondCandidate) {
                        firstExpected = firstCandidate;
                        secondExpected = secondCandidate;
                        foundDistinctSample = true;
                        break;
                    }
                }
            }
        }
        if (!foundDistinctSample) {
            return fail("List noise regression fixture should find distinct seeded samples");
        }

        list.order = LIST_ORDER_NOISE;
        list.setup(1);
        list.bindRuntimeContext(firstContext);
        list.initEmit();
        if (list[0] == nullptr || list[0]->bri != firstExpected) {
            return fail("LIST_ORDER_NOISE should sample the list's first runtime context");
        }
        list.bindRuntimeContext(secondContext);
        list.initEmit();
        if (list[0] == nullptr || list[0]->bri != secondExpected) {
            return fail("LIST_ORDER_NOISE should follow a newly bound runtime context");
        }

        LightgraphRuntimeContext& defaultContext = lightgraphDefaultRuntimeContext();
        const int originalDefaultSeed = defaultContext.perlinNoise.GetSeed();

        LightList fallbackList;
        float fallbackValue = -1.0f;
        for (int fallbackSeed = 1; fallbackSeed < 128 && fallbackValue < 0.0f; ++fallbackSeed) {
            defaultContext.perlinNoise.SetSeed(fallbackSeed);
            fallbackValue = defaultContext.perlinNoise.GetValue(fallbackList.id * 10, 0);
        }
        if (fallbackValue < 0.0f) {
            defaultContext.perlinNoise.SetSeed(originalDefaultSeed);
            return fail("Default noise regression fixture should find a non-negative sample");
        }
        fallbackList.order = LIST_ORDER_NOISE;
        fallbackList.setup(1);
        fallbackList.initEmit();
        const uint16_t fallbackExpected =
            static_cast<uint16_t>(fallbackValue * FULL_BRIGHTNESS);
        const bool fallbackMatches =
            fallbackList[0] != nullptr && fallbackList[0]->bri == fallbackExpected;
        defaultContext.perlinNoise.SetSeed(originalDefaultSeed);
        if (!fallbackMatches) {
            return fail("Unbound LIST_ORDER_NOISE should sample the default runtime context");
        }
    }

    // Long-run lifecycle regression: repeated emit/update/stop cycles should remain bounded.
    {
        std::srand(7);
        gMillis = 0;

        Line line(LINE_PIXEL_COUNT);
        State state(line);
        state.lightLists[0]->visible = false;  // Ignore background layer for this stress check.

        for (int frame = 0; frame < 1200; frame++) {
            if (frame % 40 == 0) {
                EmitParams params(0, 1.0f, 0x55AAFF);
                params.setLength(6);
                params.noteId = static_cast<uint8_t>((frame / 40) % 4 + 1);
                if (state.emit(params) < 0) {
                    return fail("Long-run emit loop unexpectedly failed");
                }
            }

            if (frame % 120 == 0) {
                const uint8_t noteToStop = static_cast<uint8_t>((frame / 120) % 4 + 1);
                state.stopNote(noteToStop);
            }

            gMillis += 16;
            state.update();

            if (state.totalLights > MAX_TOTAL_LIGHTS) {
                return fail("Long-run loop exceeded MAX_TOTAL_LIGHTS bound");
            }
            if (state.totalLightLists > MAX_LIGHT_LISTS) {
                return fail("Long-run loop exceeded MAX_LIGHT_LISTS bound");
            }
        }

        state.stopAll();
        // A full Line traversal can span nearly LINE_PIXEL_COUNT frames before expiry.
        for (int i = 0; i < (LINE_PIXEL_COUNT + 64); i++) {
            gMillis += 16;
            state.update();
        }
        if (state.totalLights != 0) {
            int activeLists = 0;
            for (int i = 0; i < MAX_LIGHT_LISTS; i++) {
                if (state.lightLists[i] != nullptr) {
                    activeLists++;
                }
            }
            return fail("Long-run loop did not drain lights after stopAll (totalLights=" +
                        std::to_string(state.totalLights) +
                        ", totalLightLists=" + std::to_string(state.totalLightLists) +
                        ", nonNullLists=" + std::to_string(activeLists) + ")");
        }
        if (state.lightLists[0] == nullptr) {
            return fail("Background slot should remain allocated after stopAll drain");
        }
        if (state.totalLightLists != 1) {
            return fail("Expected only background light list after stopAll drain");
        }
    }

    // Built-in factory regression: stable Engine and integration factory must resolve the same objects.
    {
        struct FactoryCase {
            lightgraph::ObjectType stableType;
            lightgraph::integration::BuiltinObjectType integrationType;
            uint16_t requestedPixelCount;
            uint16_t expectedPixelCount;
        };

        const FactoryCase cases[] = {
            {lightgraph::ObjectType::Heptagon919,
             lightgraph::integration::BuiltinObjectType::Heptagon919,
             0,
             HEPTAGON919_PIXEL_COUNT},
            {lightgraph::ObjectType::Heptagon3024,
             lightgraph::integration::BuiltinObjectType::Heptagon3024,
             0,
             HEPTAGON3024_PIXEL_COUNT},
            {lightgraph::ObjectType::Line,
             lightgraph::integration::BuiltinObjectType::Line,
             144,
             144},
            {lightgraph::ObjectType::Cross,
             lightgraph::integration::BuiltinObjectType::Cross,
             180,
             180},
            {lightgraph::ObjectType::Triangle,
             lightgraph::integration::BuiltinObjectType::Triangle,
             96,
             96},
        };

        for (const FactoryCase& testCase : cases) {
            lightgraph::EngineConfig config;
            config.object_type = testCase.stableType;
            config.pixel_count = testCase.requestedPixelCount;
            lightgraph::Engine engine(config);

            std::unique_ptr<lightgraph::integration::Object> object =
                lightgraph::integration::makeObject(
                    testCase.integrationType,
                    testCase.requestedPixelCount);
            if (object == nullptr) {
                return fail("Built-in factory test failed to create integration object");
            }
            if (engine.pixelCount() != object->pixelCount ||
                engine.pixelCount() != testCase.expectedPixelCount) {
                return fail("Stable Engine and integration factory should resolve matching pixel counts");
            }
        }
    }

    // Heptagon layout regression: descriptor-driven setup must preserve topology shape and geometry data.
    {
        const auto verifyHeptagon = [&](const TopologyObject& object,
                                        uint16_t expectedPixelCount,
                                        uint16_t expectedGapCount,
                                        uint16_t expectedFirstMiddleTopPixel,
                                        uint16_t expectedFirstInnerTopPixel) -> int {
            const TopologySummary summary = buildTopologySummary(object);
            if (summary.pixelCount != expectedPixelCount) {
                return fail("Heptagon summary pixel count mismatch");
            }
            if (summary.modelCount != 7 || summary.models.size() != 7) {
                return fail("Heptagon summary should expose seven models");
            }
            if (summary.intersections.size() != 28) {
                return fail("Heptagon summary should expose twenty-eight intersections");
            }
            if (summary.connections.size() != 42) {
                return fail("Heptagon summary should expose forty-two connections");
            }
            if (summary.gapCount != expectedGapCount || summary.gaps.size() != expectedGapCount) {
                return fail("Heptagon summary gap count mismatch");
            }
            if (summary.intersections[14].topPixel != expectedFirstMiddleTopPixel) {
                return fail("Heptagon summary first middle intersection mismatch");
            }
            if (summary.intersections[21].topPixel != expectedFirstInnerTopPixel) {
                return fail("Heptagon summary first inner intersection mismatch");
            }
            return 0;
        };

        {
            Heptagon919 object;
            if (verifyHeptagon(object, HEPTAGON919_PIXEL_COUNT, 0, 612, 597) != 0) {
                return 1;
            }
        }
        {
            Heptagon3024 object;
            if (verifyHeptagon(object, HEPTAGON3024_PIXEL_COUNT, 6, 2014, 1964) != 0) {
                return 1;
            }
            if (object.realPixelCount !=
                static_cast<uint16_t>(HEPTAGON3024_REAL_PIXEL_COUNT1 + HEPTAGON3024_REAL_PIXEL_COUNT2)) {
                return fail("Heptagon3024 real pixel count should match firmware-facing strip constants");
            }
            if (object.gaps.size() != 6 ||
                object.gaps[4].fromPixel != 2016 ||
                object.gaps[4].toPixel != 2735) {
                return fail("Heptagon3024 gap layout should preserve the large middle gap");
            }
            if (object.translateToLogicalPixel(static_cast<uint16_t>(object.realPixelCount - 1)) != 2879) {
                return fail("Heptagon3024 last real pixel should map to the last non-gap logical pixel");
            }
            if (object.translateToLogicalPixel(object.realPixelCount) != object.pixelCount) {
                return fail("Heptagon3024 physical pixel span should end exactly at the logical topology boundary");
            }
        }
    }

    if (Port::poolCount() != 0) {
        return fail("Port pool should be empty after scoped object teardown");
    }

    return 0;
}
