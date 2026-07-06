#ifndef PACKAGES_LIGHTGRAPH_SRC_DEBUG_DEBUGGER_H_
#define PACKAGES_LIGHTGRAPH_SRC_DEBUG_DEBUGGER_H_

#pragma once

#include <cstdint>

#define AVG_FPS_FRAMES 120

class Debugger
{

  public:

    Debugger();
    ~Debugger();
    
    void update(unsigned long millis);
    void countEmit();
    float getFPS();
    float getNumEmits();

  private:
    float fps[AVG_FPS_FRAMES] = {0.f};
    uint16_t numEmits[AVG_FPS_FRAMES] = {0};
    uint8_t fpsIndex = 0;
    uint8_t emitsIndex = 0;
    unsigned long prevMillis = 0;

};

#endif  // PACKAGES_LIGHTGRAPH_SRC_DEBUG_DEBUGGER_H_
