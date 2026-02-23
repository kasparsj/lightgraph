#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../src/BgLight.h"
#include "../src/Config.h"
#include "../src/EmitParams.h"
#include "../src/Globals.h"
#include "../src/Palette.h"
#include "../src/State.h"
#include "../src/objects/Cross.h"
#include "../src/objects/Line.h"
#include "../src/objects/Triangle.h"

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

class SinglePixelObject : public LPObject {
  public:
    SinglePixelObject() : LPObject(1) {
        addModel(new Model(0, 10, GROUP1));
    }

    uint16_t* getMirroredPixels(uint16_t, LPOwner*, bool) override {
        mirroredPixels[0] = 0;
        return mirroredPixels;
    }

    EmitParams* getModelParams(int model) override {
        return new EmitParams(model % 1, 1.0f);
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

}  // namespace

int main() {
    std::srand(2);
    gMillis = 0;

    // Off-by-one regression: model selector should allow LAST enum values.
    {
        Line line(LINE_PIXEL_COUNT);
        EmitParams* lineParams = line.getModelParams(L_BOUNCE);
        if (lineParams->model != L_BOUNCE) {
            delete lineParams;
            return fail("Line::getModelParams should preserve L_BOUNCE model");
        }
        delete lineParams;
    }
    {
        Cross cross(CROSS_PIXEL_COUNT);
        EmitParams* crossParams = cross.getModelParams(C_DIAGONAL);
        if (crossParams->model != C_DIAGONAL) {
            delete crossParams;
            return fail("Cross::getModelParams should preserve C_DIAGONAL model");
        }
        delete crossParams;
    }
    {
        Triangle triangle(TRIANGLE_PIXEL_COUNT);
        EmitParams* triangleParams = triangle.getModelParams(T_COUNTER_CLOCKWISE);
        if (triangleParams->model != T_COUNTER_CLOCKWISE) {
            delete triangleParams;
            return fail("Triangle::getModelParams should preserve T_COUNTER_CLOCKWISE model");
        }
        delete triangleParams;
    }

    // Palette wrap behavior should be deterministic in repeat mode.
    {
        std::vector<ColorRGB> colors = {ColorRGB(1, 2, 3), ColorRGB(10, 20, 30)};
        const ColorRGB& wrapped = Palette::wrapColors(2, 0, colors, WRAP_REPEAT);
        if (wrapped.R != 1 || wrapped.G != 2 || wrapped.B != 3) {
            return fail("Palette::wrapColors repeat mode returned unexpected color");
        }
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
    }

    return 0;
}
