#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "../src/runtime/EmitParams.h"
#include "../src/HashMap.h"
#include "../src/topology/Model.h"
#include "../src/runtime/LightList.h"
#include "../src/runtime/State.h"
#include "../src/objects/Line.h"
#include "../src/Globals.h"

namespace {

class EmptyObject : public TopologyObject {
  public:
    EmptyObject() : TopologyObject(8) {
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

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

int gAllocationCallbackCount = 0;
LightgraphAllocationFailureSite gObservedSite = LightgraphAllocationFailureSite::Unknown;
uint16_t gObservedDetail0 = 0;
uint16_t gObservedDetail1 = 0;

void allocationFailureObserver(LightgraphAllocationFailureSite site, uint16_t detail0, uint16_t detail1) {
    gAllocationCallbackCount++;
    gObservedSite = site;
    gObservedDetail0 = detail0;
    gObservedDetail1 = detail1;
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

    // Allocation failure observer plumbing should invoke callback and tolerate unset state.
    lightgraphSetAllocationFailureObserver(allocationFailureObserver);
    lightgraphReportAllocationFailure(LightgraphAllocationFailureSite::LightListLightAllocation, 7, 42);
    if (gAllocationCallbackCount != 1 || gObservedSite != LightgraphAllocationFailureSite::LightListLightAllocation ||
        gObservedDetail0 != 7 || gObservedDetail1 != 42) {
        return fail("allocation failure observer callback did not receive expected payload");
    }

    lightgraphSetAllocationFailureObserver(nullptr);
    lightgraphReportAllocationFailure(LightgraphAllocationFailureSite::Unknown, 0, 0);

    return 0;
}
