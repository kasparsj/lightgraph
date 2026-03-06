#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "TopologyObject.h"
#include "Port.h"

namespace {

struct IntersectionSlotKey {
    uint8_t intersectionId;
    uint8_t slotIndex;

    bool operator==(const IntersectionSlotKey& other) const {
        return intersectionId == other.intersectionId && slotIndex == other.slotIndex;
    }
};

struct IntersectionSlotKeyHash {
    size_t operator()(const IntersectionSlotKey& key) const {
        return (static_cast<size_t>(key.intersectionId) << 8) | key.slotIndex;
    }
};

bool isZeroMac(const std::array<uint8_t, 6>& mac) {
    for (uint8_t octet : mac) {
        if (octet != 0) {
            return false;
        }
    }
    return true;
}

bool matchesExternalEndpoint(const ExternalPort* port, const uint8_t device[6], uint8_t targetPortId,
                            bool direction, uint8_t group) {
    return port != nullptr && port->targetId == targetPortId && port->direction == direction && port->group == group &&
           std::memcmp(port->device.data(), device, 6) == 0;
}

} // namespace

TopologyObject* TopologyObject::instance = 0;

TopologyObject::TopologyObject(uint16_t pixelCount) : pixelCount(pixelCount), realPixelCount(pixelCount) {
    instance = this;
}

uint8_t TopologyObject::groupIndexForMask(uint8_t groupMask) {
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        if (groupMask & groupMaskForIndex(i)) {
            return i;
        }
    }
    return MAX_GROUPS;
}

TopologyObject::~TopologyObject() {
    // Keep public views stable while releasing ownership via smart pointers.
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        conn[i].clear();
    }
    ownedConnections_.clear();

    ownedExternalPorts_.clear();

    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        inter[i].clear();
    }
    ownedIntersections_.clear();

    models.clear();
    ownedModels_.clear();

    gaps.clear();
    instance = nullptr;
}

// Initialization methods removed - vectors handle dynamic sizing

Model* TopologyObject::addModel(Model *model) {
    if (model == nullptr) {
        return nullptr;
    }
    ownedModels_.emplace_back(model);

    // Ensure vector is large enough
    while (models.size() <= model->id) {
        models.push_back(nullptr);
    }
    models[model->id] = model;
    return model;
}

Intersection* TopologyObject::addIntersection(Intersection *intersection) {
    ownedIntersections_.emplace_back(intersection);

    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        if (intersection->group & groupMaskForIndex(i)) {
            inter[i].push_back(intersection);
            break;
        }
    }
    return intersection;
}

Connection* TopologyObject::addConnection(Connection *connection) {
    if (connection == nullptr || connection->from == nullptr || connection->to == nullptr ||
        connection->fromPort == nullptr || connection->toPort == nullptr) {
        delete connection;
        return nullptr;
    }

    ownedConnections_.emplace_back(connection);

    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        if (connection->group & groupMaskForIndex(i)) {
            conn[i].push_back(connection);
            break;
        }
    }
    return connection;
}

ExternalPort* TopologyObject::addExternalPort(Intersection* intersection, uint8_t slotIndex, bool direction,
                                              uint8_t group, const uint8_t device[6], uint8_t targetPortId,
                                              int16_t targetIntersectionId,
                                              bool allowDuplicateEndpoint) {
    if (intersection == nullptr || slotIndex >= intersection->numPorts || intersection->ports[slotIndex] != nullptr) {
        return nullptr;
    }
    if (!allowDuplicateEndpoint) {
        for (const auto& ownedPort : ownedExternalPorts_) {
            const auto* existing = static_cast<const ExternalPort*>(ownedPort.get());
            if (matchesExternalEndpoint(existing, device, targetPortId, direction, group)) {
                return nullptr;
            }
        }
    }
    auto created = std::make_unique<ExternalPort>(nullptr, intersection, direction, group, device, targetPortId,
                                                  static_cast<int16_t>(slotIndex), targetIntersectionId);
    ExternalPort* raw = created.get();
    ownedExternalPorts_.push_back(std::move(created));
    return raw;
}

bool TopologyObject::removeExternalPort(Port* port) {
    if (port == nullptr || !port->isExternal()) {
        return false;
    }
    auto it = std::find_if(
        ownedExternalPorts_.begin(),
        ownedExternalPorts_.end(),
        [port](const std::unique_ptr<Port>& candidate) { return candidate.get() == port; });
    if (it == ownedExternalPorts_.end()) {
        return false;
    }
    ownedExternalPorts_.erase(it);
    return true;
}

bool TopologyObject::removeIntersection(uint8_t groupIndex, size_t index) {
    if (groupIndex >= MAX_GROUPS || index >= inter[groupIndex].size()) {
        return false;
    }
    return removeIntersection(inter[groupIndex][index]);
}

bool TopologyObject::removeIntersection(Intersection* intersection) {
    if (intersection == nullptr) {
        return false;
    }

    std::vector<Connection*> toRemove;
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        for (Connection* connection : conn[i]) {
            if (connection != nullptr &&
                (connection->from == intersection || connection->to == intersection)) {
                toRemove.push_back(connection);
            }
        }
    }

    for (Connection* connection : toRemove) {
        removeConnection(connection);
    }

    std::vector<Port*> externalPortsToRemove;
    for (const std::unique_ptr<Port>& ownedPort : ownedExternalPorts_) {
        if (ownedPort && ownedPort->intersection == intersection) {
            externalPortsToRemove.push_back(ownedPort.get());
        }
    }
    for (Port* port : externalPortsToRemove) {
        removeExternalPort(port);
    }

    bool removedFromView = false;
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        auto& intersections = inter[i];
        const size_t before = intersections.size();
        intersections.erase(
            std::remove(intersections.begin(), intersections.end(), intersection),
            intersections.end());
        removedFromView = removedFromView || intersections.size() != before;
    }

    const bool owned = std::any_of(
        ownedIntersections_.begin(),
        ownedIntersections_.end(),
        [intersection](const std::unique_ptr<Intersection>& candidate) {
            return candidate.get() == intersection;
        });

    if (owned) {
        releaseOwnership(intersection);
    }

    return removedFromView || owned;
}

bool TopologyObject::removeConnection(uint8_t groupIndex, size_t index) {
    if (groupIndex >= MAX_GROUPS || index >= conn[groupIndex].size()) {
        return false;
    }
    Connection* connection = conn[groupIndex][index];
    conn[groupIndex].erase(conn[groupIndex].begin() + static_cast<std::ptrdiff_t>(index));
    releaseOwnership(connection);
    return true;
}

bool TopologyObject::removeConnection(Connection* connection) {
    if (connection == nullptr) {
        return false;
    }
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        auto it = std::find(conn[i].begin(), conn[i].end(), connection);
        if (it != conn[i].end()) {
            conn[i].erase(it);
            releaseOwnership(connection);
            return true;
        }
    }
    return false;
}

Intersection* TopologyObject::findIntersectionById(uint8_t intersectionId) const {
    for (uint8_t group = 0; group < MAX_GROUPS; group++) {
        for (Intersection* intersection : inter[group]) {
            if (intersection != nullptr && intersection->id == intersectionId) {
                return intersection;
            }
        }
    }
    return nullptr;
}

Intersection* TopologyObject::findIntersectionByIdAndGroup(uint8_t intersectionId, uint8_t requestedGroup) const {
    const uint8_t maxGroupMask = static_cast<uint8_t>((1u << MAX_GROUPS) - 1u);

    if (requestedGroup < MAX_GROUPS) {
        for (Intersection* intersection : inter[requestedGroup]) {
            if (intersection != nullptr && intersection->id == intersectionId) {
                return intersection;
            }
        }
    }

    if (requestedGroup > 0 && requestedGroup <= maxGroupMask && (requestedGroup & (requestedGroup - 1)) == 0) {
        for (uint8_t g = 0; g < MAX_GROUPS; g++) {
            if (!(requestedGroup & groupMaskForIndex(g))) {
                continue;
            }
            for (Intersection* intersection : inter[g]) {
                if (intersection != nullptr && intersection->id == intersectionId) {
                    return intersection;
                }
            }
        }
    }

    for (uint8_t g = 0; g < MAX_GROUPS; g++) {
        for (Intersection* intersection : inter[g]) {
            if (intersection != nullptr && intersection->id == intersectionId && intersection->group == requestedGroup) {
                return intersection;
            }
        }
    }

    return nullptr;
}

Intersection* TopologyObject::findIntersectionContainingInternalPortId(uint8_t internalPortId) const {
    for (uint8_t group = 0; group < MAX_GROUPS; group++) {
        for (Intersection* intersection : inter[group]) {
            if (intersection == nullptr) {
                continue;
            }
            for (uint8_t slotIndex = 0; slotIndex < intersection->numPorts; slotIndex++) {
                Port* port = intersection->ports[slotIndex];
                if (port != nullptr && !port->isExternal() && port->id == internalPortId) {
                    return intersection;
                }
            }
        }
    }
    return nullptr;
}

ExternalPort* TopologyObject::findExternalPortByExactParams(const uint8_t deviceMac[6], uint8_t targetPortId,
                                                            bool direction, uint8_t group) const {
    for (uint8_t groupIndex = 0; groupIndex < MAX_GROUPS; groupIndex++) {
        for (Intersection* intersection : inter[groupIndex]) {
            if (intersection == nullptr) {
                continue;
            }
            for (uint8_t slotIndex = 0; slotIndex < intersection->numPorts; slotIndex++) {
                Port* port = intersection->ports[slotIndex];
                if (port == nullptr || !port->isExternal()) {
                    continue;
                }

                auto* externalPort = static_cast<ExternalPort*>(port);
                if (externalPort->targetId == targetPortId &&
                    externalPort->direction == direction &&
                    externalPort->group == group &&
                    std::memcmp(externalPort->device.data(), deviceMac, 6) == 0) {
                    return externalPort;
                }
            }
        }
    }
    return nullptr;
}

bool TopologyObject::hasAvailablePort(const Intersection* intersection) const {
    if (intersection == nullptr) {
        return false;
    }

    for (uint8_t i = 0; i < intersection->numPorts; i++) {
        if (intersection->ports[i] == nullptr) {
            return true;
        }
    }

    return false;
}

int16_t TopologyObject::findFirstFreePortSlotIndex(const Intersection* intersection) const {
    if (intersection == nullptr) {
        return -1;
    }

    for (uint8_t slotIndex = 0; slotIndex < intersection->numPorts; slotIndex++) {
        if (intersection->ports[slotIndex] == nullptr) {
            return static_cast<int16_t>(slotIndex);
        }
    }
    return -1;
}

bool TopologyObject::ensureIntersectionHasFreePortSlot(Intersection* intersection, uint8_t maxPorts) {
    if (intersection == nullptr) {
        return false;
    }
    if (hasAvailablePort(intersection)) {
        return true;
    }
    if (intersection->numPorts >= maxPorts) {
        return false;
    }

    const uint8_t nextPortCount = static_cast<uint8_t>(intersection->numPorts + 1);
    std::vector<Port*> resizedPorts(nextPortCount, nullptr);
    for (uint8_t slotIndex = 0; slotIndex < intersection->numPorts; slotIndex++) {
        resizedPorts[slotIndex] = intersection->ports[slotIndex];
    }
    intersection->ports = std::move(resizedPorts);
    intersection->numPorts = nextPortCount;
    return true;
}

bool TopologyObject::areIntersectionsConnected(const Intersection* inter1, const Intersection* inter2) const {
    for (uint8_t g = 0; g < MAX_GROUPS; g++) {
        for (Connection* connection : conn[g]) {
            if (connection == nullptr) {
                continue;
            }
            if ((connection->from == inter1 && connection->to == inter2) ||
                (connection->from == inter2 && connection->to == inter1)) {
                return true;
            }
        }
    }
    return false;
}

bool TopologyObject::hasIntersectionBetween(const Intersection* inter1, const Intersection* inter2) const {
    if (inter1 == nullptr || inter2 == nullptr) {
        return false;
    }

    uint16_t startPixel = inter1->topPixel;
    uint16_t endPixel = inter2->topPixel;
    if (startPixel > endPixel) {
        const uint16_t temp = startPixel;
        startPixel = endPixel;
        endPixel = temp;
    }

    for (uint8_t g = 0; g < MAX_GROUPS; g++) {
        if (!(inter1->group & TopologyObject::groupMaskForIndex(g)) &&
            !(inter2->group & TopologyObject::groupMaskForIndex(g))) {
            continue;
        }

        for (Intersection* testInter : inter[g]) {
            if (testInter == nullptr || testInter == inter1 || testInter == inter2) {
                continue;
            }

            if (testInter->topPixel > startPixel && testInter->topPixel < endPixel) {
                bool isBlocking = true;
                if (inter1->bottomPixel != -1 && inter2->bottomPixel != -1 && testInter->bottomPixel != -1) {
                    uint16_t startBottom = inter1->bottomPixel;
                    uint16_t endBottom = inter2->bottomPixel;
                    if (startBottom > endBottom) {
                        const uint16_t temp = startBottom;
                        startBottom = endBottom;
                        endBottom = temp;
                    }

                    if (testInter->bottomPixel <= startBottom || testInter->bottomPixel >= endBottom) {
                        isBlocking = false;
                    }
                }

                if (isBlocking) {
                    return true;
                }
            }
        }
    }

    return false;
}

void TopologyObject::recalculateConnections(bool preserveVirtualConnections) {
    std::vector<std::pair<uint8_t, size_t>> connectionsToRemove;

    for (uint8_t g = 0; g < MAX_GROUPS; g++) {
        auto& connections = conn[g];
        for (size_t i = 0; i < connections.size(); i++) {
            Connection* connection = connections[i];
            if (connection == nullptr || connection->from == nullptr || connection->to == nullptr) {
                continue;
            }

            if (preserveVirtualConnections && connection->numLeds == 0) {
                continue;
            }

            if (hasIntersectionBetween(connection->from, connection->to)) {
                connectionsToRemove.push_back({g, i});
            }
        }
    }

    for (auto it = connectionsToRemove.rbegin(); it != connectionsToRemove.rend(); ++it) {
        removeConnection(it->first, it->second);
    }

    for (uint8_t g1 = 0; g1 < MAX_GROUPS; g1++) {
        for (size_t i1 = 0; i1 < inter[g1].size(); i1++) {
            Intersection* inter1 = inter[g1][i1];
            if (inter1 == nullptr) {
                continue;
            }

            for (uint8_t g2 = g1; g2 < MAX_GROUPS; g2++) {
                const size_t startI2 = (g2 == g1) ? i1 + 1 : 0;
                for (size_t i2 = startI2; i2 < inter[g2].size(); i2++) {
                    Intersection* inter2 = inter[g2][i2];
                    if (inter2 == nullptr) {
                        continue;
                    }

                    if (inter1->group != inter2->group) {
                        continue;
                    }
                    if (areIntersectionsConnected(inter1, inter2)) {
                        continue;
                    }
                    if (hasIntersectionBetween(inter1, inter2)) {
                        continue;
                    }
                    if (!hasAvailablePort(inter1) || !hasAvailablePort(inter2)) {
                        continue;
                    }

                    const uint16_t numLeds =
                        static_cast<uint16_t>(std::abs(static_cast<int>(inter2->topPixel) - static_cast<int>(inter1->topPixel)) - 1);
                    addConnection(new Connection(inter1, inter2, inter1->group, numLeds));
                }
            }
        }
    }
}

TopologySnapshot TopologyObject::exportSnapshot() const {
    TopologySnapshot snapshot{};
    snapshot.schemaVersion = 3;
    snapshot.pixelCount = pixelCount;
    snapshot.gaps = gaps;

    std::unordered_set<const Intersection*> seenIntersections;
    std::unordered_set<uint8_t> exportedPortIds;
    std::vector<const Intersection*> orderedIntersections;
    orderedIntersections.reserve(64);
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        for (const Intersection* intersection : inter[i]) {
            if (intersection != nullptr && seenIntersections.insert(intersection).second) {
                orderedIntersections.push_back(intersection);
            }
        }
    }

    for (const Intersection* intersection : orderedIntersections) {
        snapshot.intersections.push_back({
            intersection->id,
            intersection->numPorts,
            intersection->topPixel,
            intersection->bottomPixel,
            intersection->group,
            intersection->allowEndOfLife,
            intersection->allowEmit,
        });

        for (uint8_t slot = 0; slot < intersection->numPorts; slot++) {
            const Port* port = intersection->ports[slot];
            if (port == nullptr) {
                continue;
            }
            TopologyPortSnapshot portSnapshot{
                port->id,
                intersection->id,
                slot,
                port->isExternal() ? TopologyPortType::External : TopologyPortType::Internal,
                port->direction,
                port->group,
                {},
                0,
                TOPOLOGY_TARGET_INTERSECTION_UNSET,
            };
            if (port->isExternal()) {
                const auto* externalPort = static_cast<const ExternalPort*>(port);
                portSnapshot.deviceMac = externalPort->device;
                portSnapshot.targetPortId = externalPort->targetId;
                portSnapshot.targetIntersectionId = externalPort->targetIntersectionId;
            }
            snapshot.ports.push_back(portSnapshot);
            exportedPortIds.insert(port->id);
        }
    }

    std::unordered_set<const Connection*> seenConnections;
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        for (const Connection* connection : conn[i]) {
            if (connection == nullptr || !seenConnections.insert(connection).second) {
                continue;
            }
            if (connection->from == nullptr || connection->to == nullptr) {
                continue;
            }
            snapshot.connections.push_back({
                connection->from->id,
                connection->to->id,
                connection->group,
                connection->numLeds,
            });
        }
    }

    for (const Model* model : models) {
        if (model == nullptr) {
            continue;
        }
        TopologyModelSnapshot modelSnapshot{
            model->id,
            model->defaultW,
            model->emitGroups,
            model->maxLength,
            model->getRoutingStrategy(),
            {},
        };

        for (const auto& weightEntry : model->weights) {
            if (weightEntry.second == nullptr) {
                continue;
            }
            if (!exportedPortIds.count(weightEntry.first)) {
                continue;
            }
            TopologyPortWeightSnapshot weightSnapshot{
                weightEntry.first,
                weightEntry.second->defaultWeight(),
                {},
            };
            for (const auto& conditional : weightEntry.second->conditionalWeights()) {
                if (!exportedPortIds.count(conditional.first)) {
                    continue;
                }
                weightSnapshot.conditionals.push_back({
                    conditional.first,
                    conditional.second,
                });
            }
            std::sort(
                weightSnapshot.conditionals.begin(),
                weightSnapshot.conditionals.end(),
                [](const TopologyWeightConditionalSnapshot& left, const TopologyWeightConditionalSnapshot& right) {
                    return left.incomingPortId < right.incomingPortId;
                });

            modelSnapshot.weights.push_back(weightSnapshot);
        }

        std::sort(
            modelSnapshot.weights.begin(),
            modelSnapshot.weights.end(),
            [](const TopologyPortWeightSnapshot& left, const TopologyPortWeightSnapshot& right) {
                return left.outgoingPortId < right.outgoingPortId;
            });

        snapshot.models.push_back(modelSnapshot);
    }

    return snapshot;
}

bool TopologyObject::importSnapshot(const TopologySnapshot& snapshot, bool replaceModels) {
    if (snapshot.schemaVersion != 3 || snapshot.pixelCount == 0) {
        return false;
    }

    std::unordered_set<uint8_t> intersectionIds;
    std::unordered_map<uint8_t, uint8_t> intersectionPortCapacity;
    for (const TopologyIntersectionSnapshot& intersection : snapshot.intersections) {
        if (intersection.numPorts == 0 || !intersectionIds.insert(intersection.id).second) {
            return false;
        }
        intersectionPortCapacity[intersection.id] = intersection.numPorts;
    }

    std::unordered_set<uint8_t> snapshotPortIds;
    std::unordered_set<IntersectionSlotKey, IntersectionSlotKeyHash> occupiedSlots;
    for (const TopologyPortSnapshot& port : snapshot.ports) {
        auto capacityIt = intersectionPortCapacity.find(port.intersectionId);
        if (capacityIt == intersectionPortCapacity.end() || port.slotIndex >= capacityIt->second ||
            !snapshotPortIds.insert(port.id).second) {
            return false;
        }
        if (port.type != TopologyPortType::Internal && port.type != TopologyPortType::External) {
            return false;
        }
        if (port.group == 0) {
            return false;
        }
        if (port.targetIntersectionId != TOPOLOGY_TARGET_INTERSECTION_UNSET &&
            (port.targetIntersectionId < 0 ||
             port.targetIntersectionId > static_cast<int16_t>(std::numeric_limits<uint8_t>::max()))) {
            return false;
        }
        const IntersectionSlotKey slotKey{port.intersectionId, port.slotIndex};
        if (!occupiedSlots.insert(slotKey).second) {
            return false;
        }
        if (port.type == TopologyPortType::Internal &&
            (!isZeroMac(port.deviceMac) || port.targetPortId != 0 ||
             port.targetIntersectionId != TOPOLOGY_TARGET_INTERSECTION_UNSET)) {
            return false;
        }
    }

    for (const TopologyConnectionSnapshot& connection : snapshot.connections) {
        if (!intersectionIds.count(connection.fromIntersectionId) ||
            !intersectionIds.count(connection.toIntersectionId)) {
            return false;
        }
    }

    for (const PixelGap& gap : snapshot.gaps) {
        if (gap.fromPixel > gap.toPixel || gap.toPixel >= snapshot.pixelCount) {
            return false;
        }
    }

    for (uint8_t g = 0; g < MAX_GROUPS; g++) {
        while (!conn[g].empty()) {
            removeConnection(g, conn[g].size() - 1);
        }
    }
    ownedExternalPorts_.clear();
    for (uint8_t g = 0; g < MAX_GROUPS; g++) {
        while (!inter[g].empty()) {
            removeIntersection(g, inter[g].size() - 1);
        }
    }

    if (replaceModels) {
        models.clear();
        ownedModels_.clear();
    }

    gaps.clear();
    pixelCount = snapshot.pixelCount;
    realPixelCount = snapshot.pixelCount;
    Port::setNextId(0);
    for (const PixelGap& gap : snapshot.gaps) {
        addGap(gap.fromPixel, gap.toPixel);
    }

    std::unordered_map<uint8_t, Intersection*> intersectionsById;
    uint16_t maxIntersectionId = 0;
    for (const TopologyIntersectionSnapshot& intersectionSnapshot : snapshot.intersections) {
        Intersection* created = addIntersection(new Intersection(
            intersectionSnapshot.numPorts,
            intersectionSnapshot.topPixel,
            intersectionSnapshot.bottomPixel,
            intersectionSnapshot.group,
            intersectionSnapshot.allowEndOfLife,
            intersectionSnapshot.allowEmit));
        created->id = intersectionSnapshot.id;
        intersectionsById[intersectionSnapshot.id] = created;
        if (intersectionSnapshot.id > maxIntersectionId) {
            maxIntersectionId = intersectionSnapshot.id;
        }
    }

    if (!snapshot.intersections.empty()) {
        const uint8_t nextId =
            static_cast<uint8_t>((maxIntersectionId + 1) % (std::numeric_limits<uint8_t>::max() + 1));
        Intersection::nextId = nextId;
    }

    for (const TopologyConnectionSnapshot& connectionSnapshot : snapshot.connections) {
        Intersection* from = intersectionsById[connectionSnapshot.fromIntersectionId];
        Intersection* to = intersectionsById[connectionSnapshot.toIntersectionId];
        addConnection(new Connection(from, to, connectionSnapshot.group, connectionSnapshot.numLeds));
    }

    std::unordered_map<uint8_t, Port*> remappedPorts;
    uint8_t maxPortId = 0;
    bool hasPortIds = false;

    std::unordered_map<uint8_t, std::vector<const TopologyPortSnapshot*>> portsByIntersection;
    for (const TopologyPortSnapshot& portSnapshot : snapshot.ports) {
        portsByIntersection[portSnapshot.intersectionId].push_back(&portSnapshot);
    }

    for (auto& entry : portsByIntersection) {
        auto intersectionIt = intersectionsById.find(entry.first);
        if (intersectionIt == intersectionsById.end()) {
            return false;
        }
        Intersection* intersection = intersectionIt->second;

        std::vector<const TopologyPortSnapshot*> internalSnapshots;
        std::vector<const TopologyPortSnapshot*> externalSnapshots;
        for (const TopologyPortSnapshot* portSnapshot : entry.second) {
            if (portSnapshot->type == TopologyPortType::External) {
                externalSnapshots.push_back(portSnapshot);
            } else {
                internalSnapshots.push_back(portSnapshot);
            }
        }

        std::sort(
            internalSnapshots.begin(),
            internalSnapshots.end(),
            [](const TopologyPortSnapshot* left, const TopologyPortSnapshot* right) {
                return left->slotIndex < right->slotIndex;
            });
        std::sort(
            externalSnapshots.begin(),
            externalSnapshots.end(),
            [](const TopologyPortSnapshot* left, const TopologyPortSnapshot* right) {
                return left->slotIndex < right->slotIndex;
            });

        std::vector<Port*> runtimeInternalPorts;
        runtimeInternalPorts.reserve(intersection->numPorts);
        for (Port* port : intersection->ports) {
            if (port != nullptr && !port->isExternal()) {
                runtimeInternalPorts.push_back(port);
            }
        }

        if (runtimeInternalPorts.size() != internalSnapshots.size()) {
            return false;
        }

        intersection->ports.assign(intersection->numPorts, nullptr);

        for (const TopologyPortSnapshot* snapshotPort : internalSnapshots) {
            auto bestIt = runtimeInternalPorts.end();
            for (auto it = runtimeInternalPorts.begin(); it != runtimeInternalPorts.end(); ++it) {
                Port* candidate = *it;
                if (candidate->direction == snapshotPort->direction &&
                    candidate->group == snapshotPort->group) {
                    bestIt = it;
                    break;
                }
            }
            if (bestIt == runtimeInternalPorts.end()) {
                if (runtimeInternalPorts.empty()) {
                    return false;
                }
                bestIt = runtimeInternalPorts.begin();
            }

            Port* selected = *bestIt;
            runtimeInternalPorts.erase(bestIt);
            if (!intersection->addPortAt(selected, snapshotPort->slotIndex)) {
                return false;
            }
            selected->id = snapshotPort->id;
            remappedPorts[snapshotPort->id] = selected;
            hasPortIds = true;
            if (snapshotPort->id > maxPortId) {
                maxPortId = snapshotPort->id;
            }
        }

        for (const TopologyPortSnapshot* snapshotPort : externalSnapshots) {
            uint8_t deviceMac[6] = {0};
            std::memcpy(deviceMac, snapshotPort->deviceMac.data(), sizeof(deviceMac));
            ExternalPort* created = addExternalPort(
                intersection,
                snapshotPort->slotIndex,
                snapshotPort->direction,
                snapshotPort->group,
                deviceMac,
                snapshotPort->targetPortId,
                snapshotPort->targetIntersectionId,
                true);
            if (created == nullptr) {
                return false;
            }
            created->id = snapshotPort->id;
            remappedPorts[snapshotPort->id] = created;
            hasPortIds = true;
            if (snapshotPort->id > maxPortId) {
                maxPortId = snapshotPort->id;
            }
        }
    }

    if (remappedPorts.size() != snapshot.ports.size()) {
        return false;
    }

    if (hasPortIds) {
        Port::setNextId(static_cast<uint8_t>(maxPortId + 1));
    }

    for (const TopologyModelSnapshot& modelSnapshot : snapshot.models) {
        if (!replaceModels && modelSnapshot.id < models.size() && models[modelSnapshot.id] != nullptr) {
            continue;
        }

        auto* model = new Model(
            modelSnapshot.id,
            modelSnapshot.defaultWeight,
            modelSnapshot.emitGroups,
            modelSnapshot.maxLength,
            modelSnapshot.routingStrategy);
        addModel(model);

        for (const TopologyPortWeightSnapshot& weightSnapshot : modelSnapshot.weights) {
            auto outgoingIt = remappedPorts.find(weightSnapshot.outgoingPortId);
            if (outgoingIt == remappedPorts.end()) {
                continue;
            }
            Weight* weight = model->_getOrCreate(outgoingIt->second, weightSnapshot.defaultWeight);
            if (weight == nullptr) {
                return false;
            }
            for (const TopologyWeightConditionalSnapshot& conditional : weightSnapshot.conditionals) {
                auto incomingIt = remappedPorts.find(conditional.incomingPortId);
                if (incomingIt == remappedPorts.end()) {
                    continue;
                }
                weight->add(incomingIt->second, conditional.weight);
            }
        }
    }

    return true;
}

void TopologyObject::releaseOwnership(Connection* connection) {
    auto it = std::find_if(
        ownedConnections_.begin(),
        ownedConnections_.end(),
        [connection](const std::unique_ptr<Connection>& candidate) { return candidate.get() == connection; });

    if (it != ownedConnections_.end()) {
        ownedConnections_.erase(it);
    }
}

void TopologyObject::releaseOwnership(Intersection* intersection) {
    auto it = std::find_if(
        ownedIntersections_.begin(),
        ownedIntersections_.end(),
        [intersection](const std::unique_ptr<Intersection>& candidate) {
            return candidate.get() == intersection;
        });

    if (it != ownedIntersections_.end()) {
        ownedIntersections_.erase(it);
    }
}

void TopologyObject::releaseOwnership(Port* port) {
    auto it = std::find_if(
        ownedExternalPorts_.begin(),
        ownedExternalPorts_.end(),
        [port](const std::unique_ptr<Port>& candidate) { return candidate.get() == port; });

    if (it != ownedExternalPorts_.end()) {
        ownedExternalPorts_.erase(it);
    }
}

void TopologyObject::addGap(uint16_t fromPixel, uint16_t toPixel) {
    gaps.push_back({fromPixel, toPixel});
    
    // Recalculate real pixel count
    uint16_t gapPixels = 0;
    for (const PixelGap& gap : gaps) {
        gapPixels += (gap.toPixel - gap.fromPixel + 1);
    }
    realPixelCount = pixelCount - gapPixels;
}

Connection* TopologyObject::addBridge(uint16_t fromPixel, uint16_t toPixel, uint8_t group, uint8_t numPorts) {
    Intersection *from = new Intersection(numPorts, fromPixel, -1, group);
    Intersection *to = new Intersection(numPorts, toPixel, -1, group);
    addIntersection(from);
    addIntersection(to);
    return addConnection(new Connection(from, to, group));
}

Intersection* TopologyObject::getIntersection(uint8_t i, uint8_t groups) {
    for (uint8_t j = 0; j < MAX_GROUPS; j++) {
        if (groups == 0 || (groups & groupMaskForIndex(j))) {
            if (i < inter[j].size()) {
                return inter[j][i];
            }
            i -= inter[j].size();
        }
    }
    return nullptr;
}

Intersection* TopologyObject::getEmittableIntersection(uint8_t i, uint8_t groups) {
    for (uint8_t groupIndex = 0; groupIndex < MAX_GROUPS; groupIndex++) {
        if (groups != 0 && (groups & groupMaskForIndex(groupIndex)) == 0) {
            continue;
        }
        for (Intersection* intersection : inter[groupIndex]) {
            if (intersection == nullptr || !intersection->allowEmit) {
                continue;
            }
            if (i == 0) {
                return intersection;
            }
            i--;
        }
    }
    return nullptr;
}

Connection* TopologyObject::getConnection(uint8_t i, uint8_t groups) {
    for (uint8_t j = 0; j < MAX_GROUPS; j++) {
        if (groups == 0 || (groups & groupMaskForIndex(j))) {
            if (i < conn[j].size()) {
                return conn[j][i];
            }
            i -= conn[j].size();
        }
    }
    return nullptr;
}

// Gap initialization removed - vectors handle dynamic sizing

bool TopologyObject::isPixelInGap(uint16_t logicalPixel) {
    for (const PixelGap& gap : gaps) {
        if (logicalPixel >= gap.fromPixel && logicalPixel <= gap.toPixel) {
            return true;
        }
    }
    return false;
}

int16_t TopologyObject::translateToRealPixel(uint16_t logicalPixel) {
    if (gaps.empty()) {
        return logicalPixel;
    }
    
    uint16_t realPixel = logicalPixel;
    for (const PixelGap& gap : gaps) {
        if (logicalPixel > gap.toPixel) {
            // Subtract the gap size from the real pixel position
            realPixel -= (gap.toPixel - gap.fromPixel + 1);
        } else if (logicalPixel >= gap.fromPixel) {
            // Pixel is in a gap, return invalid position
            return -1;
        }
    }
    return realPixel;
}

uint16_t TopologyObject::translateToLogicalPixel(uint16_t realPixel) {
    if (gaps.empty()) {
        return realPixel;
    }
    
    uint16_t logicalPixel = realPixel;
    for (const PixelGap& gap : gaps) {
        if (logicalPixel >= gap.fromPixel) {
            // Add the gap size to the logical pixel position
            logicalPixel += (gap.toPixel - gap.fromPixel + 1);
        }
    }
    return logicalPixel;
}
