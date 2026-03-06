#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "Intersection.h"
#include "Connection.h"
#include "Model.h"
#include "../Globals.h"
#include "../runtime/EmitParams.h"

struct PixelGap {
    uint16_t fromPixel;
    uint16_t toPixel;
};

enum class TopologyPortType : uint8_t {
    Internal = 0,
    External = 1,
};

constexpr int16_t TOPOLOGY_TARGET_INTERSECTION_UNSET = -1;

struct TopologyPortSnapshot {
    uint8_t id;
    uint8_t intersectionId;
    uint8_t slotIndex;
    TopologyPortType type;
    bool direction;
    uint8_t group;
    std::array<uint8_t, 6> deviceMac;
    uint8_t targetPortId;
    int16_t targetIntersectionId = TOPOLOGY_TARGET_INTERSECTION_UNSET;
    bool hasTargetPortId = false;
};

struct TopologyWeightConditionalSnapshot {
    uint8_t incomingPortId;
    uint8_t weight;
};

struct TopologyPortWeightSnapshot {
    uint8_t outgoingPortId;
    uint8_t defaultWeight;
    std::vector<TopologyWeightConditionalSnapshot> conditionals;
};

struct TopologyModelSnapshot {
    uint8_t id;
    uint8_t defaultWeight;
    uint8_t emitGroups;
    uint16_t maxLength;
    RoutingStrategy routingStrategy;
    std::vector<TopologyPortWeightSnapshot> weights;
};

struct TopologyIntersectionSnapshot {
    uint8_t id;
    uint8_t numPorts;
    uint16_t topPixel;
    int16_t bottomPixel;
    uint8_t group;
    bool allowEndOfLife = true;
    bool allowEmit = true;
};

struct TopologyConnectionSnapshot {
    uint8_t fromIntersectionId;
    uint8_t toIntersectionId;
    uint8_t group;
    uint16_t numLeds;
};

struct TopologySnapshot {
    uint8_t schemaVersion = 3;
    uint16_t pixelCount;
    std::vector<TopologyIntersectionSnapshot> intersections;
    std::vector<TopologyConnectionSnapshot> connections;
    std::vector<TopologyModelSnapshot> models;
    std::vector<TopologyPortSnapshot> ports;
    std::vector<PixelGap> gaps;
};

struct TopologyIntersectionUpdate {
    uint8_t numPorts = 2;
    uint16_t topPixel = 0;
    int16_t bottomPixel = -1;
    uint8_t group = 0;
    bool allowEndOfLife = true;
    bool allowEmit = true;
};

class TopologyObject {

  public:
    uint16_t pixelCount;
    uint16_t realPixelCount;
    std::vector<Intersection*> inter[MAX_GROUPS];
    std::vector<Connection*> conn[MAX_GROUPS];
    std::vector<Model*> models;
    std::vector<PixelGap> gaps;

    TopologyObject(uint16_t pixelCount);
    virtual ~TopologyObject();

    static constexpr uint8_t groupMaskForIndex(uint8_t index) {
        return static_cast<uint8_t>(1u << index);
    }
    static uint8_t groupIndexForMask(uint8_t groupMask);

    void addGap(uint16_t fromPixel, uint16_t toPixel);
    virtual Model* addModel(Model *model);
    virtual Intersection* addIntersection(Intersection *intersection);
    virtual Connection* addConnection(Connection *connection);
    virtual ExternalPort* addExternalPort(Intersection* intersection, uint8_t slotIndex, bool direction,
                                          uint8_t group, const uint8_t device[6], uint8_t targetPortId,
                                          int16_t targetIntersectionId = TOPOLOGY_TARGET_INTERSECTION_UNSET,
                                          bool hasTargetPortId = true,
                                          bool allowDuplicateEndpoint = false);
    bool removeExternalPort(Port* port);
    bool removeIntersection(uint8_t groupIndex, size_t index);
    bool removeIntersection(Intersection* intersection);
    bool removeConnection(uint8_t groupIndex, size_t index);
    bool removeConnection(Connection* connection);
    bool updateIntersection(Intersection* intersection, const TopologyIntersectionUpdate& update);
    Intersection* findIntersectionById(uint8_t intersectionId) const;
    Intersection* findIntersectionByIdAndGroup(uint8_t intersectionId, uint8_t requestedGroup) const;
    Intersection* findIntersectionContainingInternalPortId(uint16_t internalPortId) const;
    ExternalPort* findExternalPortByExactParams(const uint8_t deviceMac[6], bool hasTargetPortId,
                                                uint8_t targetPortId, int16_t targetIntersectionId,
                                                bool direction, uint8_t group) const;
    Port* findPortById(uint16_t portId) const;
    bool hasAvailablePort(const Intersection* intersection) const;
    int16_t findFirstFreePortSlotIndex(const Intersection* intersection) const;
    bool ensureIntersectionHasFreePortSlot(Intersection* intersection, uint8_t maxPorts = 9);
    bool areIntersectionsConnected(const Intersection* inter1, const Intersection* inter2) const;
    bool hasIntersectionBetween(const Intersection* inter1, const Intersection* inter2) const;
    void recalculateConnections(bool preserveVirtualConnections = true);
    bool exportSnapshot(TopologySnapshot& snapshot) const;
    bool importSnapshot(const TopologySnapshot& snapshot, bool replaceModels = true);
    virtual Connection* addBridge(uint16_t fromPixel, uint16_t toPixel, uint8_t group, uint8_t numPorts = 2);
    LightgraphRuntimeContext& runtimeContext() { return runtimeContext_; }
    const LightgraphRuntimeContext& runtimeContext() const { return runtimeContext_; }
    void setNowMillis(unsigned long nowMillis) { lightgraphSetNowMillis(runtimeContext_, nowMillis); }
    unsigned long nowMillis() const {
        return runtimeContext_.hasExplicitNowMillis ? runtimeContext_.nowMillis : gMillis;
    }
    void setExternalSendHook(LightgraphExternalSendHook hook) { runtimeContext_.externalSendHook = hook; }
    LightgraphExternalSendHook externalSendHook() const { return runtimeContext_.externalSendHook; }
    size_t portCount() const { return portRegistry_.size(); }
    Model* getModel(int i) {
      return i >= 0 && static_cast<size_t>(i) < models.size() ? models[i] : nullptr;
    }
    Intersection* getIntersection(uint8_t i, uint8_t groups);
    Intersection* getEmittableIntersection(uint8_t i, uint8_t groups);
    uint8_t countIntersections(uint8_t groups) {
        uint8_t count = 0;
        for (uint8_t i=0; i<MAX_GROUPS; i++) {
            if (groups == 0 || (groups & groupMaskForIndex(i))) {
                count += inter[i].size();
            }
        }
        return count;
    }
    uint8_t countEmittableIntersections(uint8_t groups) const {
        uint8_t count = 0;
        for (uint8_t i = 0; i < MAX_GROUPS; i++) {
            if (groups != 0 && (groups & groupMaskForIndex(i)) == 0) {
                continue;
            }
            for (const Intersection* intersection : inter[i]) {
                if (intersection != nullptr && intersection->allowEmit) {
                    count++;
                }
            }
        }
        return count;
    }
    Connection* getConnection(uint8_t i, uint8_t groups);
    uint8_t countConnections(uint8_t groups) {
        uint8_t count = 0;
        for (uint8_t i=0; i<MAX_GROUPS; i++) {
            if (groups == 0 || (groups & groupMaskForIndex(i))) {
                count += conn[i].size();
            }
        }
        return count;
    }
    
    virtual bool isMirrorSupported() { return false; }
    virtual uint16_t* getMirroredPixels(uint16_t pixel, Owner* mirrorFlipEmitter, bool mirrorRotate) = 0;
    
    int16_t translateToRealPixel(uint16_t logicalPixel);
    uint16_t translateToLogicalPixel(uint16_t realPixel);
    bool isPixelInGap(uint16_t logicalPixel);
    
    virtual std::optional<EmitParams> getParams(char command) const {
        switch (command) {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7': {
                int model = command - '1';
                return getModelParams(model);
            }
            case '/': {
                EmitParams params;
                params.behaviourFlags |= B_RENDER_SEGMENT;
                params.setLength(1);
                return params;
            }
            case '?': { // emitNoise
                EmitParams params;
                //params->order = LIST_NOISE;
                params.behaviourFlags |= B_BRI_CONST_NOISE;
                return params;
            }
        }
        return std::nullopt;
    }
    virtual EmitParams getModelParams(int model) const = 0;

  protected:
    void releaseOwnership(Connection* connection);
    void releaseOwnership(Intersection* intersection);
    void releaseOwnership(Port* port);
    bool registerPort(Port* port, std::optional<uint16_t> preferredId = std::nullopt);
    bool reassignPortId(Port* port, uint16_t preferredId);
    void unregisterPort(const Port* port);
    void resetPortRegistry();

  private:
    void removePortFromModels(const Port* port);
    void trimTrailingEmptyPortSlots(Intersection* intersection, uint8_t minPorts = 2);
    uint16_t allocatePortId() const;
    std::vector<std::unique_ptr<Intersection>> ownedIntersections_;
    std::vector<std::unique_ptr<Connection>> ownedConnections_;
    std::vector<std::unique_ptr<Model>> ownedModels_;
    std::vector<std::unique_ptr<Port>> ownedExternalPorts_;
    std::unordered_map<uint16_t, Port*> portRegistry_;
    mutable uint16_t nextPortId_ = 0;
    LightgraphRuntimeContext runtimeContext_;

    friend class Port;
};
