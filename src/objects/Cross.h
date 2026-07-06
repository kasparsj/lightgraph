#ifndef PACKAGES_LIGHTGRAPH_SRC_OBJECTS_CROSS_H_
#define PACKAGES_LIGHTGRAPH_SRC_OBJECTS_CROSS_H_

#pragma once

#include "../core/Types.h"
#include "../core/Limits.h"
#include "../Random.h"
#include "../topology/TopologyObject.h"

#define CROSS_PIXEL_COUNT 288  // 2 lines with 144 pixels each

enum CrossModel {
    C_DEFAULT = 0,
    C_HORIZONTAL = 1,
    C_VERTICAL = 2,
    C_DIAGONAL = 3,
    C_FIRST = C_DEFAULT,
    C_LAST = C_DIAGONAL,
};

class Cross : public TopologyObject {

  public:
  
    Cross(uint16_t pixelCount) : TopologyObject(pixelCount) {
        horizontalLineStart = 0;
        horizontalLineEnd = pixelCount / 2 - 1;
        verticalLineStart = pixelCount / 2;
        verticalLineEnd = pixelCount - 1;
        horizontalCross = pixelCount / 4;
        verticalCross = pixelCount / 4 * 3;
        
        setup();
    }
    
    ~Cross() override = default;
    
    bool isMirrorSupported() override { return true; }
    uint16_t* getMirroredPixels(uint16_t pixel, Owner* mirrorFlipEmitter, bool mirrorRotate) override;
    float getProgressOnLine(uint16_t pixel, bool isVertical) const;
    uint16_t getPixelOnLine(float perc, bool isVertical) const;
    
    EmitParams getModelParams(int model) const override {
        return EmitParams(model % (CrossModel::C_LAST + 1), Random::randomSpeed());
    }

  private:
    void setup();
    
    uint16_t mirrorPixels[2];
    uint16_t horizontalLineStart;
    uint16_t horizontalLineEnd;
    uint16_t verticalLineStart;
    uint16_t verticalLineEnd;
    uint16_t horizontalCross;
    uint16_t verticalCross;
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_OBJECTS_CROSS_H_
