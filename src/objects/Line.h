#ifndef PACKAGES_LIGHTGRAPH_SRC_OBJECTS_LINE_H_
#define PACKAGES_LIGHTGRAPH_SRC_OBJECTS_LINE_H_

#pragma once

#include "../core/Types.h"
#include "../core/Limits.h"
#include "../Random.h"
#include "../topology/TopologyObject.h"

#define LINE_PIXEL_COUNT 300  // Line from pixel 0 to 287

enum LineModel {
    L_DEFAULT = 0,
    L_BOUNCE = 1,
    L_FIRST = L_DEFAULT,
    L_LAST = L_BOUNCE,
};

class Line : public TopologyObject {

  public:
  
    Line(uint16_t pixelCount) : TopologyObject(pixelCount) {
        setup();
    }
    
    ~Line() override = default;
    
    bool isMirrorSupported() override { return true; }
    uint16_t* getMirroredPixels(uint16_t pixel, Owner* mirrorFlipEmitter, bool mirrorRotate) override;
    float getProgressOnLine(uint16_t pixel) const;
    uint16_t getPixelOnLine(float perc) const;
    
    EmitParams getModelParams(int model) const override {
        return EmitParams(model % (LineModel::L_LAST + 1), Random::randomSpeed());
    }

  private:
    void setup();
    
    uint16_t mirrorPixels[2];

};

#endif  // PACKAGES_LIGHTGRAPH_SRC_OBJECTS_LINE_H_
