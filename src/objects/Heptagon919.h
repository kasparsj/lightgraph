#pragma once

#include "HeptagonStar.h"

#define HEPTAGON919_PIXEL_COUNT1 524
#define HEPTAGON919_PIXEL_COUNT2 395
#define HEPTAGON919_PIXEL_COUNT (HEPTAGON919_PIXEL_COUNT1 + HEPTAGON919_PIXEL_COUNT2) // 919

class Heptagon919 : public HeptagonStar {
    
public:
    Heptagon919() : HeptagonStar(HEPTAGON919_PIXEL_COUNT) {
        Heptagon919::setup();
    }
    
private:
  void setup();
    
};
