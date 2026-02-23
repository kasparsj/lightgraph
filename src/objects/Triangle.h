#pragma once

#include "../core/Types.h"
#include "../core/Limits.h"
#include "../LPRandom.h"
#include "../topology/LPObject.h"

#define TRIANGLE_PIXEL_COUNT 900

enum TriangleModel {
    T_DEFAULT = 0,
    T_CLOCKWISE = 1,
    T_COUNTER_CLOCKWISE = 2,
    T_FIRST = T_DEFAULT,
    T_LAST = T_COUNTER_CLOCKWISE,
};

class Triangle : public LPObject {

  public:
  
    Triangle(uint16_t pixelCount) : LPObject(pixelCount) {
        // Define segment boundaries
        segmentSize = pixelCount / 3;
        subSegmentSize = segmentSize / 5;  // Each side has 5 equal sub-segments
        
        segment1Start = 0;
        segment1End = segmentSize - 1;
        segment2Start = segmentSize;
        segment2End = 2 * segmentSize - 1;
        segment3Start = 2 * segmentSize;
        segment3End = pixelCount - 1;
        
        setup();
    }
    
    ~Triangle() override = default;
    
    bool isMirrorSupported() override { return true; }
    uint16_t* getMirroredPixels(uint16_t pixel, LPOwner* mirrorFlipEmitter, bool mirrorRotate) override;
    
    // Methods for mapping between pixels and segment progress
    float getProgressOnSegment(uint16_t pixel, uint8_t segment) const;
    uint16_t getPixelOnSegment(float perc, uint8_t segment) const;
    uint8_t getSegmentForPixel(uint16_t pixel) const;
    
    EmitParams getModelParams(int model) const override {
        return EmitParams(model % (TriangleModel::T_LAST + 1), LPRandom::randomSpeed());
    }

  private:
    void setup();
    
    uint16_t mirrorPixels[2];
    uint16_t segmentSize;
    uint16_t subSegmentSize;
    uint16_t segment1Start;
    uint16_t segment1End;
    uint16_t segment2Start;
    uint16_t segment2End;
    uint16_t segment3Start;
    uint16_t segment3End;
};
