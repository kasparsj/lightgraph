#pragma once

#include "../core/Types.h"
#include "../core/Limits.h"
#include "../LPRandom.h"
#include "../topology/LPObject.h"

enum HeptagonStarModel {
    M_DEFAULT = 0,
    M_STAR = 1,
    M_OUTER_SUN = 2,
    M_INNER_TRIS = 3,
    M_SMALL_STAR = 4,
    M_INNER_SUN = 5,
    M_SPLATTER = 6,
    M_FIRST = M_DEFAULT,
    M_LAST = M_SPLATTER,
};

class HeptagonStar : public LPObject {

  public:
  
    HeptagonStar(uint16_t pixelCount) : LPObject(pixelCount) {
    }
    
    ~HeptagonStar() override = default;
    
    bool isMirrorSupported() override { return true; }
    uint16_t* getMirroredPixels(uint16_t pixel, LPOwner* mirrorFlipEmitter, bool mirrorRotate) override;
    uint8_t getStarSegmentIndex(uint16_t pixel) const;
    float getProgressOnStarSegment(uint8_t pathIndex, uint16_t pixel) const;
    uint16_t getPixelOnStarSegment(uint8_t pathIndex, float perc) const;
    // todo: implement getXYZ
    
    EmitParams getModelParams(int model) const override {
        if (model <= HeptagonStarModel::M_LAST) {
            return EmitParams(model, LPRandom::randomSpeed());
        }
        else { // key '8' and up
            EmitParams params(M_STAR);
            params.colorChangeGroups |= GROUP1;
            return params;
        }
    }
    std::optional<EmitParams> getParams(char command) const override {
        switch (command) {
            case '+': {
                EmitParams params(M_SPLATTER, LPRandom::randomSpeed());
                params.linked = false;
                params.duration = max(1, (int) (LPRandom::MAX_SPEED/params.speed) + 1) * EmitParams::frameMs();
                return params;
            }
            case '*': {
                // works reliably with M_STAR, other models might or might not work
                EmitParams params(M_STAR);
                params.speed = 0;
                params.fadeSpeed = 1;
                params.fadeThresh = 127;
                params.order = LIST_ORDER_RANDOM;
                params.behaviourFlags |= B_POS_CHANGE_FADE;
                return params;
            }
            case '-': { // emitBounce
                EmitParams params(M_STAR);
                params.behaviourFlags |= B_FORCE_BOUNCE;
                return params;
            }
            case 'd': {
                EmitParams params(M_STAR, 0.5);
                params.setLength(3);
                params.from = 1;
                params.duration = INFINITE_DURATION;
                return params;
            }
            default:
                return LPObject::getParams(command);
        }
    }

  private:
    uint16_t mirrorPixels[3];

};
