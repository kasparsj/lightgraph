#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>
#include "Intersection.h"
#include "Connection.h"
#include "Model.h"
#include "../runtime/EmitParams.h"

struct PixelGap {
    uint16_t fromPixel;
    uint16_t toPixel;
};

class LPObject {

  public:
    static LPObject* instance;
    uint16_t pixelCount;
    uint16_t realPixelCount;
    std::vector<Intersection*> inter[MAX_GROUPS];
    std::vector<Connection*> conn[MAX_GROUPS];
    std::vector<Model*> models;
    std::vector<PixelGap> gaps;

    LPObject(uint16_t pixelCount);
    virtual ~LPObject();

    static constexpr uint8_t groupMaskForIndex(uint8_t index) {
        return static_cast<uint8_t>(1u << index);
    }

    void addGap(uint16_t fromPixel, uint16_t toPixel);
    virtual Model* addModel(Model *model);
    virtual Intersection* addIntersection(Intersection *intersection);
    virtual Connection* addConnection(Connection *connection);
    bool removeConnection(uint8_t groupIndex, size_t index);
    bool removeConnection(Connection* connection);
    virtual Connection* addBridge(uint16_t fromPixel, uint16_t toPixel, uint8_t group, uint8_t numPorts = 2);
    Model* getModel(int i) {
      return i >= 0 && static_cast<size_t>(i) < models.size() ? models[i] : nullptr;
    }
    Intersection* getIntersection(uint8_t i, uint8_t groups);
    uint8_t countIntersections(uint8_t groups) {
        uint8_t count = 0;
        for (uint8_t i=0; i<MAX_GROUPS; i++) {
            if (groups == 0 || (groups & groupMaskForIndex(i))) {
                count += inter[i].size();
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
    virtual uint16_t* getMirroredPixels(uint16_t pixel, LPOwner* mirrorFlipEmitter, bool mirrorRotate) = 0;
    
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

  private:
    std::vector<std::unique_ptr<Intersection>> ownedIntersections_;
    std::vector<std::unique_ptr<Connection>> ownedConnections_;
    std::vector<std::unique_ptr<Model>> ownedModels_;
};
