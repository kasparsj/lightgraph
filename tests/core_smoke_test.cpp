#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "../src/runtime/EmitParams.h"
#include "../src/HashMap.h"
#include "../src/topology/Model.h"
#include "../src/runtime/State.h"
#include "../src/objects/Line.h"

namespace {

class EmptyObject : public LPObject {
  public:
    EmptyObject() : LPObject(8) {
        addModel(new Model(0, 10, GROUP1));
    }

    uint16_t* getMirroredPixels(uint16_t, LPOwner*, bool) override {
        mirroredPixels[0] = 0;
        return mirroredPixels;
    }

    EmitParams getModelParams(int model) const override {
        return EmitParams(model % 1, 1.0f);
    }

  private:
    uint16_t mirroredPixels[2] = {0};
};

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

}  // namespace

int main() {
    std::srand(1);

    // HashMap const access should not mutate or recurse.
    HashMap<int, int> map(4);
    map.setNullValue(-1);
    map.set(3, 9);
    const HashMap<int, int>& constMap = map;
    if (constMap[3] != 9) {
        return fail("HashMap const lookup returned wrong value");
    }
    if (constMap[2] != -1) {
        return fail("HashMap const missing-key lookup did not return nil value");
    }

    // LightList duration setter should write duration argument.
    LightList lightList;
    lightList.setup(1, 255);
    lightList.setDuration(1234);
    if (lightList.duration != 1234) {
        return fail("LightList::setDuration did not update duration");
    }

    // Emit should fail safely when model index is invalid.
    Line line(LINE_PIXEL_COUNT);
    State lineState(line);
    EmitParams invalidParams(99, 1.0f);
    invalidParams.setLength(3);
    if (lineState.emit(invalidParams) != -1) {
        return fail("State::emit accepted invalid model index");
    }

    // Emit should fail safely (without crashes) when no emitters are available.
    EmptyObject emptyObject;
    State emptyState(emptyObject);
    EmitParams emptyParams(0, 1.0f);
    emptyParams.setLength(3);
    if (emptyState.emit(emptyParams) != -1) {
        return fail("State::emit should fail when object has no emit candidates");
    }

    return 0;
}
