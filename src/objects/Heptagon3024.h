#pragma once

#include "HeptagonStar.h"

#define HEPTAGON3024_PIXEL_COUNT1 2160
#define HEPTAGON3024_PIXEL_COUNT2 864
#define HEPTAGON3024_PIXEL_COUNT (HEPTAGON3024_PIXEL_COUNT1 + HEPTAGON3024_PIXEL_COUNT2) // 3024
#define HEPTAGON3024_REAL_PIXEL_COUNT1 1584
#define HEPTAGON3024_REAL_PIXEL_COUNT2 144

class Heptagon3024 : public HeptagonStar {
    
public:
    Heptagon3024() : HeptagonStar(HEPTAGON3024_PIXEL_COUNT) {
        Heptagon3024::setup();
    }
    
private:
  void setup();
    
};
