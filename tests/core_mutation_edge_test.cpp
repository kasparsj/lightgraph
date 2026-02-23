#include <iostream>
#include <string>

#include "../src/runtime/EmitParams.h"
#include "../src/topology/Connection.h"
#include "../src/topology/Intersection.h"
#include "../src/topology/LPObject.h"
#include "../src/topology/Model.h"

namespace {

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
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

class MinimalObject : public LPObject {
  public:
    MinimalObject() : LPObject(16) { addModel(new Model(0, 10, GROUP1)); }

    uint16_t* getMirroredPixels(uint16_t, LPOwner*, bool) override {
        mirrored_[0] = 0;
        return mirrored_;
    }

    EmitParams getModelParams(int model) const override { return EmitParams(model % 1, 1.0f); }

  private:
    uint16_t mirrored_[2] = {0};
};

} // namespace

int main() {
    MinimalObject object;
    Intersection* from = object.addIntersection(new Intersection(3, 10, -1, GROUP1));
    Intersection* to = object.addIntersection(new Intersection(3, 20, -1, GROUP1));

    Connection* connection = object.addConnection(new Connection(from, to, GROUP1, 0));
    if (countConnectedPorts(*from) != 1 || countConnectedPorts(*to) != 1) {
        return fail("Initial connection should consume one port on each endpoint");
    }

    if (object.removeConnection(nullptr)) {
        return fail("removeConnection(nullptr) should return false");
    }
    if (object.removeConnection(0, 99)) {
        return fail("removeConnection(group,index) should fail for out-of-range index");
    }
    if (object.removeConnection(99, 0)) {
        return fail("removeConnection(group,index) should fail for out-of-range group");
    }

    if (!object.removeConnection(connection)) {
        return fail("removeConnection(pointer) failed for valid pointer");
    }
    if (object.removeConnection(connection)) {
        return fail("removeConnection(pointer) should fail when connection was already removed");
    }
    if (countConnectedPorts(*from) != 0 || countConnectedPorts(*to) != 0) {
        return fail("Endpoint ports were not detached after pointer removal");
    }

    object.addConnection(new Connection(from, to, GROUP1, 0));
    if (!object.removeConnection(0, 0)) {
        return fail("removeConnection(group,index) failed for valid index");
    }
    if (!object.conn[0].empty()) {
        return fail("Connection list should be empty after indexed removal");
    }
    if (countConnectedPorts(*from) != 0 || countConnectedPorts(*to) != 0) {
        return fail("Endpoint ports were not detached after indexed removal");
    }

    return 0;
}
