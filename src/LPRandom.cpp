#include "LPRandom.h"
#include <algorithm>

#include "core/Platform.h"

float LPRandom::MIN_SPEED = 0.5f;
float LPRandom::MAX_SPEED = 10.f;
uint32_t LPRandom::MIN_DURATION = 120 * 16;
uint32_t LPRandom::MAX_DURATION = 1440 * 16;
uint16_t LPRandom::MIN_LENGTH = 1;
uint16_t LPRandom::MAX_LENGTH = 100;
uint8_t LPRandom::MIN_SATURATION = 255 * 0.7f;
uint8_t LPRandom::MAX_SATURATION = 255;
uint8_t LPRandom::MIN_VALUE = 255 * 0.7f;
uint8_t LPRandom::MAX_VALUE = 255;
uint16_t LPRandom::MIN_NEXT = 2000; // ms, ~125 frames (avg fps is 62.5)
uint16_t LPRandom::MAX_NEXT = 20000; // ms, ~1250 frames (avg fps is 62.5)

float LPRandom::randomSpeed() {
  return MIN_SPEED + LP_RANDOM(std::max(MAX_SPEED - MIN_SPEED, 0.f));
}

uint32_t LPRandom::randomDuration() {
  return MIN_DURATION + LP_RANDOM(std::max(MAX_DURATION - MIN_DURATION, static_cast<uint32_t>(0)));
}

uint16_t LPRandom::randomLength() {
  return static_cast<uint16_t>(MIN_LENGTH + LP_RANDOM(std::max(MAX_LENGTH - MIN_LENGTH, 0)));
}

uint8_t LPRandom::randomHue() {
  return LP_RANDOM(256);
}

uint8_t LPRandom::randomSaturation() {
  return MIN_SATURATION + LP_RANDOM(std::max(MAX_SATURATION - MIN_SATURATION, 0));
}

uint8_t LPRandom::randomValue() {
  return MIN_VALUE + LP_RANDOM(std::max(MAX_VALUE - MIN_VALUE, 0));
}

uint16_t LPRandom::randomNextEmit() {
  return MIN_NEXT + LP_RANDOM(std::max(MAX_NEXT - MIN_NEXT, 0));
}
