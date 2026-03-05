#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "../src/runtime/EmitParams.h"
#include "../src/runtime/LightList.h"
#include "../src/topology/Connection.h"
#include "../src/topology/Intersection.h"
#include "../src/topology/TopologyObject.h"
#include "../src/topology/Model.h"

namespace {

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

struct ExternalSendRecord {
    std::array<uint8_t, 6> mac = {0};
    uint8_t targetPortId = 0;
    bool sendList = false;
};

std::vector<ExternalSendRecord> gExternalSendRecords;
bool gExternalSendShouldSucceed = true;

bool sendLightViaESPNowTestHook(const uint8_t* mac, uint8_t targetPortId,
                                RuntimeLight* const /*light*/, bool sendList) {
    ExternalSendRecord record;
    if (mac != nullptr) {
        for (size_t i = 0; i < record.mac.size(); i++) {
            record.mac[i] = mac[i];
        }
    }
    record.targetPortId = targetPortId;
    record.sendList = sendList;
    gExternalSendRecords.push_back(record);
    return gExternalSendShouldSucceed;
}

void resetExternalSendHook(bool shouldSucceed) {
    gExternalSendRecords.clear();
    gExternalSendShouldSucceed = shouldSucceed;
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

bool areIntersectionsConnected(const TopologyObject& object, const Intersection* first,
                               const Intersection* second) {
    for (uint8_t groupIndex = 0; groupIndex < MAX_GROUPS; ++groupIndex) {
        for (Connection* connection : object.conn[groupIndex]) {
            if (connection == nullptr) {
                continue;
            }
            if ((connection->from == first && connection->to == second) ||
                (connection->from == second && connection->to == first)) {
                return true;
            }
        }
    }
    return false;
}

bool hasIntersectionBetween(const TopologyObject& object, const Intersection* left,
                            const Intersection* right) {
    if (left == nullptr || right == nullptr) {
        return false;
    }

    uint16_t start = left->topPixel;
    uint16_t end = right->topPixel;
    if (start > end) {
        const uint16_t temp = start;
        start = end;
        end = temp;
    }

    for (uint8_t groupIndex = 0; groupIndex < MAX_GROUPS; ++groupIndex) {
        for (Intersection* probe : object.inter[groupIndex]) {
            if (probe == nullptr || probe == left || probe == right) {
                continue;
            }
            if (probe->group != left->group || probe->group != right->group) {
                continue;
            }
            if (probe->topPixel > start && probe->topPixel < end) {
                return true;
            }
        }
    }
    return false;
}

bool hasAvailablePort(const TopologyObject& object, const Intersection* intersection) {
    if (intersection == nullptr) {
        return false;
    }
    uint8_t usedPorts = 0;
    for (uint8_t groupIndex = 0; groupIndex < MAX_GROUPS; ++groupIndex) {
        for (Connection* connection : object.conn[groupIndex]) {
            if (connection != nullptr &&
                (connection->from == intersection || connection->to == intersection)) {
                ++usedPorts;
            }
        }
    }
    return usedPorts < intersection->numPorts;
}

void recalculateConnectionsLikeFirmware(TopologyObject& object) {
    std::vector<std::pair<uint8_t, size_t>> toRemove;

    for (uint8_t groupIndex = 0; groupIndex < MAX_GROUPS; ++groupIndex) {
        auto& connections = object.conn[groupIndex];
        for (size_t index = 0; index < connections.size(); ++index) {
            Connection* connection = connections[index];
            if (connection == nullptr || connection->numLeds == 0) {
                continue;
            }
            if (hasIntersectionBetween(object, connection->from, connection->to)) {
                toRemove.push_back({groupIndex, index});
            }
        }
    }

    for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
        object.removeConnection(it->first, it->second);
    }

    for (uint8_t g1 = 0; g1 < MAX_GROUPS; ++g1) {
        for (size_t i1 = 0; i1 < object.inter[g1].size(); ++i1) {
            Intersection* first = object.inter[g1][i1];
            if (first == nullptr) {
                continue;
            }
            for (uint8_t g2 = g1; g2 < MAX_GROUPS; ++g2) {
                const size_t startI2 = (g2 == g1) ? i1 + 1 : 0;
                for (size_t i2 = startI2; i2 < object.inter[g2].size(); ++i2) {
                    Intersection* second = object.inter[g2][i2];
                    if (second == nullptr || first->group != second->group) {
                        continue;
                    }
                    if (areIntersectionsConnected(object, first, second) ||
                        hasIntersectionBetween(object, first, second)) {
                        continue;
                    }
                    if (!hasAvailablePort(object, first) || !hasAvailablePort(object, second)) {
                        continue;
                    }
                    const uint16_t leds =
                        static_cast<uint16_t>(std::abs(static_cast<int>(second->topPixel) -
                                                       static_cast<int>(first->topPixel)) -
                                              1);
                    object.addConnection(new Connection(first, second, first->group, leds));
                }
            }
        }
    }
}

class MinimalObject : public TopologyObject {
  public:
    MinimalObject() : TopologyObject(16) { addModel(new Model(0, 10, GROUP1)); }

    uint16_t* getMirroredPixels(uint16_t, Owner*, bool) override {
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

    // Over-capacity protection: connection creation should fail once endpoint slots are exhausted.
    {
        MinimalObject saturatedObject;
        Intersection* saturatedFrom =
            saturatedObject.addIntersection(new Intersection(3, 100, -1, GROUP1));
        Intersection* saturatedTo =
            saturatedObject.addIntersection(new Intersection(3, 120, -1, GROUP1));

        for (uint8_t i = 0; i < 3; i++) {
            Connection* created = saturatedObject.addConnection(
                new Connection(saturatedFrom, saturatedTo, GROUP1, 1));
            if (created == nullptr) {
                return fail("Expected to fill all 3 connection slots before saturation");
            }
        }

        if (countConnectedPorts(*saturatedFrom) != 3 || countConnectedPorts(*saturatedTo) != 3) {
            return fail("Saturated intersections should report all 3 ports in use");
        }

        Connection* overflow =
            saturatedObject.addConnection(new Connection(saturatedFrom, saturatedTo, GROUP1, 1));
        if (overflow != nullptr) {
            return fail("addConnection should reject creation when no intersection slots remain");
        }
        if (saturatedObject.conn[0].size() != 3) {
            return fail("Rejected overflow connection should not mutate connection list");
        }
    }

    // Higher-degree intersections should support more than 4 ports.
    {
        MinimalObject highDegreeObject;
        Intersection* hub = highDegreeObject.addIntersection(new Intersection(6, 200, -1, GROUP1));

        std::vector<Intersection*> leaves;
        for (uint8_t i = 0; i < 6; i++) {
            leaves.push_back(highDegreeObject.addIntersection(
                new Intersection(2, static_cast<uint16_t>(220 + i * 10), -1, GROUP1)));
        }

        for (Intersection* leaf : leaves) {
            Connection* created =
                highDegreeObject.addConnection(new Connection(hub, leaf, GROUP1, 1));
            if (created == nullptr) {
                return fail("Failed to attach expected edge to 6-port hub intersection");
            }
        }

        if (countConnectedPorts(*hub) != 6) {
            return fail("6-port intersection did not retain all attached connections");
        }
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

    // Intersection removal should be ownership-safe and remove attached connections.
    Intersection* isolated = object.addIntersection(new Intersection(2, 30, -1, GROUP1));
    if (!object.removeIntersection(isolated)) {
        return fail("removeIntersection(pointer) failed for isolated intersection");
    }
    for (Intersection* intersection : object.inter[0]) {
        if (intersection == isolated) {
            return fail("removeIntersection(pointer) left removed intersection in group view");
        }
    }

    Intersection* junctionA = object.addIntersection(new Intersection(2, 40, -1, GROUP1));
    Intersection* junctionB = object.addIntersection(new Intersection(2, 50, -1, GROUP1));
    object.addConnection(new Connection(junctionA, junctionB, GROUP1, 0));
    if (!object.removeIntersection(junctionA)) {
        return fail(
            "removeIntersection(pointer) failed when intersection had attached connections");
    }
    if (!object.conn[0].empty()) {
        return fail("removeIntersection(pointer) should remove attached connections");
    }
    if (countConnectedPorts(*junctionB) != 0) {
        return fail(
            "Neighbor intersection ports were not detached when removing attached intersection");
    }

    // Firmware-style recalculate should split and reconnect around inserted/removed intersections.
    MinimalObject editorObject;
    Intersection* a = editorObject.addIntersection(new Intersection(2, 0, -1, GROUP1));
    Intersection* b = editorObject.addIntersection(new Intersection(2, 10, -1, GROUP1));
    editorObject.addConnection(new Connection(a, b, GROUP1, 9));
    if (editorObject.conn[0].size() != 1) {
        return fail("Expected one initial connection before mutation recalc");
    }

    Intersection* inserted = editorObject.addIntersection(new Intersection(2, 5, -1, GROUP1));
    recalculateConnectionsLikeFirmware(editorObject);
    if (editorObject.conn[0].size() != 2) {
        return fail("Recalculate after insertion should split one segment into two");
    }
    if (!areIntersectionsConnected(editorObject, a, inserted) ||
        !areIntersectionsConnected(editorObject, inserted, b)) {
        return fail("Inserted intersection should connect to both neighbors after recalc");
    }
    if (areIntersectionsConnected(editorObject, a, b)) {
        return fail("Original direct connection should be removed when intersection exists between "
                    "endpoints");
    }

    if (!editorObject.removeIntersection(inserted)) {
        return fail("Failed to remove inserted intersection before reconnection recalc");
    }
    recalculateConnectionsLikeFirmware(editorObject);
    if (!areIntersectionsConnected(editorObject, a, b)) {
        return fail("Recalculate after removal should restore direct connection");
    }

    // Snapshot round-trip should preserve graph structure and model metadata.
    MinimalObject snapshotObject;
    Intersection* s1 = snapshotObject.addIntersection(new Intersection(2, 1, -1, GROUP1));
    Intersection* s2 = snapshotObject.addIntersection(new Intersection(2, 7, -1, GROUP1));
    Connection* sConnection = snapshotObject.addConnection(new Connection(s1, s2, GROUP1, 5));
    snapshotObject.addGap(3, 4);
    Model* deterministicModel =
        snapshotObject.addModel(new Model(1, 7, GROUP1, 32, RoutingStrategy::Deterministic));
    deterministicModel->put(sConnection->fromPort, sConnection->toPort, 99);

    const TopologySnapshot snapshot = snapshotObject.exportSnapshot();

    MinimalObject importedObject;
    if (!importedObject.importSnapshot(snapshot, true)) {
        return fail("importSnapshot failed on exported snapshot payload");
    }
    if (importedObject.countIntersections(GROUP1) != snapshotObject.countIntersections(GROUP1)) {
        return fail("importSnapshot did not preserve intersection count");
    }
    if (importedObject.countConnections(GROUP1) != snapshotObject.countConnections(GROUP1)) {
        return fail("importSnapshot did not preserve connection count");
    }
    if (importedObject.gaps.size() != 1 || importedObject.gaps[0].fromPixel != 3 ||
        importedObject.gaps[0].toPixel != 4) {
        return fail("importSnapshot did not preserve gap definitions");
    }
    Model* importedModel = importedObject.getModel(1);
    if (importedModel == nullptr) {
        return fail("importSnapshot did not recreate model");
    }
    if (importedModel->getRoutingStrategy() != RoutingStrategy::Deterministic) {
        return fail("importSnapshot did not preserve routing strategy");
    }
    if (importedModel->weightCount() == 0) {
        return fail("importSnapshot did not restore routing weights");
    }

    // Snapshot round-trip should preserve external ports and next port ID allocation.
    MinimalObject externalSnapshotObject;
    Intersection* extA = externalSnapshotObject.addIntersection(new Intersection(4, 2, -1, GROUP1));
    Intersection* extB =
        externalSnapshotObject.addIntersection(new Intersection(4, 11, -1, GROUP1));
    externalSnapshotObject.addConnection(new Connection(extA, extB, GROUP1, 8));
    const uint8_t remoteMac[6] = {0xAA, 0xBB, 0xCC, 0x01, 0x02, 0x03};
    ExternalPort* outgoing =
        externalSnapshotObject.addExternalPort(extA, 3, true, GROUP1, remoteMac, 42);
    if (outgoing == nullptr) {
        return fail("addExternalPort failed to create test fixture");
    }
    ExternalPort* duplicateExact =
        externalSnapshotObject.addExternalPort(extB, 2, true, GROUP1, remoteMac, 42);
    if (duplicateExact != nullptr) {
        return fail("addExternalPort should reject exact duplicate external mappings");
    }
    ExternalPort* oppositeDirection =
        externalSnapshotObject.addExternalPort(extB, 3, false, GROUP1, remoteMac, 42);
    if (oppositeDirection == nullptr) {
        return fail("addExternalPort should allow same endpoint mapping for opposite port roles");
    }

    const TopologySnapshot externalSnapshot = externalSnapshotObject.exportSnapshot();
    MinimalObject importedExternalObject;
    if (!importedExternalObject.importSnapshot(externalSnapshot, true)) {
        return fail("importSnapshot failed for topology containing external ports");
    }

    Intersection* importedExtA = importedExternalObject.getIntersection(0, GROUP1);
    if (importedExtA == nullptr) {
        return fail("Imported topology missing expected intersection");
    }
    Port* importedPort = importedExtA->ports[3];
    if (importedPort == nullptr || !importedPort->isExternal()) {
        return fail("External port was not restored to expected slot");
    }
    const auto* restoredExternal = static_cast<const ExternalPort*>(importedPort);
    if (restoredExternal->targetId != 42 || restoredExternal->device[0] != 0xAA ||
        restoredExternal->device[5] != 0x03) {
        return fail("External port metadata did not round-trip");
    }

    uint8_t maxPortId = 0;
    for (const auto& portSnapshot : externalSnapshot.ports) {
        if (portSnapshot.id > maxPortId) {
            maxPortId = portSnapshot.id;
        }
    }

    const uint8_t remoteMac2[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    ExternalPort* nextPort =
        importedExternalObject.addExternalPort(importedExtA, 2, false, GROUP1, remoteMac2, 11);
    if (nextPort == nullptr) {
        return fail("Unable to create new external port after import");
    }
    if (nextPort->id != static_cast<uint8_t>(maxPortId + 1)) {
        return fail("Port ID allocator did not continue from imported maximum ID");
    }

    // Sequential external forwarding should batch-send once and expire lights one-by-one.
    {
        resetExternalSendHook(true);
        ::sendLightViaESPNow = sendLightViaESPNowTestHook;

        MinimalObject forwardingObject;
        Intersection* forwardingIntersection =
            forwardingObject.addIntersection(new Intersection(4, 30, -1, GROUP1));
        if (forwardingIntersection == nullptr) {
            return fail("Failed to create forwarding intersection fixture");
        }
        const uint8_t forwardingMac[6] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36};
        ExternalPort forwardingPort(nullptr, forwardingIntersection, true, GROUP1, forwardingMac, 9);

        LightList sequentialList;
        sequentialList.order = LIST_ORDER_SEQUENTIAL;
        sequentialList.setup(2, 255);

        RuntimeLight* firstLight = sequentialList[0];
        RuntimeLight* secondLight = sequentialList[1];
        if (firstLight == nullptr || secondLight == nullptr) {
            return fail("Sequential forwarding fixture did not create expected lights");
        }

        forwardingPort.sendOut(firstLight, true);
        if (!firstLight->isExpired || secondLight->isExpired) {
            return fail("External forwarding should only expire the light that reached the port");
        }
        if (gExternalSendRecords.size() != 1 || !gExternalSendRecords[0].sendList) {
            return fail("First sequential forwarding event should issue exactly one batch send");
        }

        sequentialList.update();
        RuntimeLight* relinkedSecondLight = sequentialList[1];
        if (relinkedSecondLight == nullptr || relinkedSecondLight->idx != 0) {
            return fail("Expected surviving light to reindex to idx=0 after first light expiry");
        }

        forwardingPort.sendOut(relinkedSecondLight, true);
        if (!relinkedSecondLight->isExpired) {
            return fail("Second light should expire when it reaches external forwarding port");
        }
        if (gExternalSendRecords.size() != 1) {
            return fail("Sequential forwarding should not emit duplicate batch sends after reindexing");
        }
    }

    // Non-sequential forwarding should remain per-light without batch state side effects.
    {
        resetExternalSendHook(true);
        ::sendLightViaESPNow = sendLightViaESPNowTestHook;

        MinimalObject nonSequentialObject;
        Intersection* nonSequentialIntersection =
            nonSequentialObject.addIntersection(new Intersection(4, 35, -1, GROUP1));
        if (nonSequentialIntersection == nullptr) {
            return fail("Failed to create non-sequential forwarding intersection fixture");
        }
        const uint8_t nonSequentialMac[6] = {0x51, 0x52, 0x53, 0x54, 0x55, 0x56};
        ExternalPort nonSequentialPort(nullptr, nonSequentialIntersection, true, GROUP1,
                                       nonSequentialMac, 10);

        LightList nonSequentialList;
        nonSequentialList.order = LIST_ORDER_RANDOM;
        nonSequentialList.setup(2, 255);

        RuntimeLight* firstLight = nonSequentialList[0];
        RuntimeLight* secondLight = nonSequentialList[1];
        if (firstLight == nullptr || secondLight == nullptr) {
            return fail("Non-sequential forwarding fixture did not create expected lights");
        }

        nonSequentialPort.sendOut(firstLight, true);
        nonSequentialPort.sendOut(secondLight, true);
        if (!firstLight->isExpired || !secondLight->isExpired) {
            return fail("Non-sequential forwarding should expire each light as it reaches external port");
        }
        if (gExternalSendRecords.size() != 2) {
            return fail("Non-sequential forwarding should send each light independently");
        }
        if (gExternalSendRecords[0].sendList || gExternalSendRecords[1].sendList) {
            return fail("Non-sequential forwarding should not send list batches");
        }
    }

    // Failed external forwarding should keep light local and reroute on next frame.
    {
        resetExternalSendHook(false);
        ::sendLightViaESPNow = sendLightViaESPNowTestHook;

        MinimalObject failureObject;
        Intersection* failureIntersection =
            failureObject.addIntersection(new Intersection(4, 40, -1, GROUP1));
        if (failureIntersection == nullptr) {
            return fail("Failed to create failure routing intersection fixture");
        }
        const uint8_t failureMac[6] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
        ExternalPort failurePort(nullptr, failureIntersection, true, GROUP1, failureMac, 11);

        LightList failedList;
        failedList.order = LIST_ORDER_SEQUENTIAL;
        failedList.setup(1, 255);
        RuntimeLight* light = failedList[0];
        if (light == nullptr) {
            return fail("Failed forwarding fixture did not create expected light");
        }
        light->setOutPort(&failurePort, static_cast<int8_t>(failureIntersection->id));
        light->owner = nullptr;

        failurePort.sendOut(light, false);
        if (gExternalSendRecords.size() != 1) {
            return fail("Failed forwarding path should still attempt one transport send");
        }
        if (light->isExpired) {
            return fail("Failed external forwarding should not expire local light");
        }
        if (light->owner != failureIntersection) {
            return fail("Failed external forwarding should reattach light to local intersection owner");
        }
        if (light->outPort != nullptr) {
            return fail("Failed external forwarding should clear out-port for rerouting");
        }
    }

    ::sendLightViaESPNow = nullptr;

    return 0;
}
