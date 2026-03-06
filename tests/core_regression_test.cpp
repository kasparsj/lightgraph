#include <cstdlib>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "../src/runtime/BgLight.h"
#include "../src/runtime/Light.h"
#include "../src/runtime/RemoteSnapshotBuilder.h"
#include "../src/topology/Connection.h"
#include "../src/core/Types.h"
#include "../src/core/Limits.h"
#include "../src/runtime/EmitParams.h"
#include "../src/Globals.h"
#include "../src/topology/Intersection.h"
#include "../src/rendering/Palette.h"
#include "../src/topology/Port.h"
#include "../src/runtime/State.h"
#include "../src/objects/Cross.h"
#include "../src/objects/Line.h"
#include "../src/objects/Triangle.h"

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

}  // namespace

int main() {
    std::srand(2);
    gMillis = 0;

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
        const uint16_t expectedNumLights = remote_snapshot::scaleLengthForDensity(
            descriptor.numLights, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
        if (list->numLights != expectedNumLights) {
            delete list;
            return fail("Remote template snapshot should materialize the scaled logical light count");
        }
        const uint16_t expectedLength = remote_snapshot::scaleLengthForDensity(
            descriptor.length, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
        const uint16_t expectedEdgeLights =
            (expectedLength > expectedNumLights) ? static_cast<uint16_t>(expectedLength - expectedNumLights) : 0;
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
        descriptor.numLights = 5;
        descriptor.positionOffset = -5;
        descriptor.speed = 1.5f;
        descriptor.lifeMillis = 900;
        descriptor.senderPixelDensity = 120;
        descriptor.receiverPixelDensity = 60;

        std::vector<remote_snapshot::SequentialEntry> entries;
        entries.push_back({0, 40, 10, 20, 30});
        entries.push_back({4, 180, 100, 110, 120});
        entries.push_back({4, 120, 1, 2, 3});

        LightList* list = remote_snapshot::buildSequentialSnapshot(descriptor, entries);
        if (list == nullptr) {
            return fail("Remote sparse snapshot should materialize for valid descriptor");
        }

        const uint16_t expectedNumLights = remote_snapshot::scaleLengthForDensity(
            descriptor.numLights, descriptor.senderPixelDensity, descriptor.receiverPixelDensity);
        if (list->numLights != expectedNumLights) {
            delete list;
            return fail("Remote sparse snapshot should materialize the scaled logical light count");
        }

        const uint16_t lastIdx = expectedNumLights - 1;
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

    if (Port::poolCount() != 0) {
        return fail("Port pool should be empty after scoped object teardown");
    }

    return 0;
}
