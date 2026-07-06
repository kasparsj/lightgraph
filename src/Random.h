#ifndef PACKAGES_LIGHTGRAPH_SRC_RANDOM_H_
#define PACKAGES_LIGHTGRAPH_SRC_RANDOM_H_

#pragma once

#include <stdint.h>

class Random
{
  public:
    static float randomSpeed();
    static uint32_t randomDuration();
    static uint16_t randomLength();
    static uint8_t randomHue();
    static uint8_t randomSaturation();
    static uint8_t randomValue();
    static uint16_t randomNextEmit();

    static float MIN_SPEED;
    static float MAX_SPEED;
    static uint32_t MIN_DURATION;
    static uint32_t MAX_DURATION;
    static uint16_t MIN_LENGTH;
    static uint16_t MAX_LENGTH;
    static uint8_t MIN_SATURATION;
    static uint8_t MAX_SATURATION;
    static uint8_t MIN_VALUE;
    static uint8_t MAX_VALUE;
    static uint16_t MIN_NEXT;
    static uint16_t MAX_NEXT;
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_RANDOM_H_
