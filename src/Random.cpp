#include "Random.h"
#include <algorithm>

#include "core/Platform.h"

float Random::MIN_SPEED = 0.5f;
float Random::MAX_SPEED = 10.f;
uint32_t Random::MIN_DURATION = 120 * 16;
uint32_t Random::MAX_DURATION = 1440 * 16;
uint16_t Random::MIN_LENGTH = 1;
uint16_t Random::MAX_LENGTH = 100;
uint8_t Random::MIN_SATURATION = 255 * 0.7f;
uint8_t Random::MAX_SATURATION = 255;
uint8_t Random::MIN_VALUE = 255 * 0.7f;
uint8_t Random::MAX_VALUE = 255;
uint16_t Random::MIN_NEXT = 2000; // ms, ~125 frames (avg fps is 62.5)
uint16_t Random::MAX_NEXT = 20000; // ms, ~1250 frames (avg fps is 62.5)

float Random::randomSpeed() {
  return MIN_SPEED + LG_RANDOM(std::max(MAX_SPEED - MIN_SPEED, 0.f));
}

uint32_t Random::randomDuration() {
  return MIN_DURATION + LG_RANDOM(std::max(MAX_DURATION - MIN_DURATION, static_cast<uint32_t>(0)));
}

uint16_t Random::randomLength() {
  return static_cast<uint16_t>(MIN_LENGTH + LG_RANDOM(std::max(MAX_LENGTH - MIN_LENGTH, 0)));
}

uint8_t Random::randomHue() {
  return LG_RANDOM(256);
}

uint8_t Random::randomSaturation() {
  return MIN_SATURATION + LG_RANDOM(std::max(MAX_SATURATION - MIN_SATURATION, 0));
}

uint8_t Random::randomValue() {
  return MIN_VALUE + LG_RANDOM(std::max(MAX_VALUE - MIN_VALUE, 0));
}

uint16_t Random::randomNextEmit() {
  return MIN_NEXT + LG_RANDOM(std::max(MAX_NEXT - MIN_NEXT, 0));
}
