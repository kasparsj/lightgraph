#pragma once

#include <cstdint>

namespace lightpath {
namespace core {

constexpr uint8_t kMaxGroups = 5;
constexpr uint8_t kMaxLightLists = 20;
constexpr uint8_t kMaxConditionalWeights = 10;
constexpr uint16_t kMaxTotalLights = 1500;
constexpr uint8_t kMaxNotesSet = 7;
constexpr uint8_t kConnectionMaxMult = 10;
constexpr uint16_t kConnectionMaxLights = 340;
constexpr uint16_t kConnectionMaxLeds = 48;
constexpr uint8_t kOutPortsMemory = 3;

constexpr int8_t kRandomModel = -1;
constexpr int8_t kRandomSpeed = -1;
constexpr uint32_t kRandomDuration = 0;
constexpr int64_t kRandomColor = -1;

constexpr uint32_t kInfiniteDuration = 0x7FFFFFFF;
constexpr float kDefaultSpeed = 1.0f;
constexpr uint8_t kFullBrightness = 255;

}  // namespace core
}  // namespace lightpath

#ifndef MAX_GROUPS
#define MAX_GROUPS lightpath::core::kMaxGroups
#endif

#ifndef MAX_LIGHT_LISTS
#define MAX_LIGHT_LISTS lightpath::core::kMaxLightLists
#endif

#ifndef MAX_CONDITIONAL_WEIGHTS
#define MAX_CONDITIONAL_WEIGHTS lightpath::core::kMaxConditionalWeights
#endif

#ifndef MAX_TOTAL_LIGHTS
#define MAX_TOTAL_LIGHTS lightpath::core::kMaxTotalLights
#endif

#ifndef MAX_NOTES_SET
#define MAX_NOTES_SET lightpath::core::kMaxNotesSet
#endif

#ifndef CONNECTION_MAX_MULT
#define CONNECTION_MAX_MULT lightpath::core::kConnectionMaxMult
#endif

#ifndef CONNECTION_MAX_LIGHTS
#define CONNECTION_MAX_LIGHTS lightpath::core::kConnectionMaxLights
#endif

#ifndef CONNECTION_MAX_LEDS
#define CONNECTION_MAX_LEDS lightpath::core::kConnectionMaxLeds
#endif

#ifndef OUT_PORTS_MEMORY
#define OUT_PORTS_MEMORY lightpath::core::kOutPortsMemory
#endif

#ifndef RANDOM_MODEL
#define RANDOM_MODEL lightpath::core::kRandomModel
#endif

#ifndef RANDOM_SPEED
#define RANDOM_SPEED lightpath::core::kRandomSpeed
#endif

#ifndef RANDOM_DURATION
#define RANDOM_DURATION lightpath::core::kRandomDuration
#endif

#ifndef RANDOM_COLOR
#define RANDOM_COLOR lightpath::core::kRandomColor
#endif

#ifndef INFINITE_DURATION
#define INFINITE_DURATION lightpath::core::kInfiniteDuration
#endif

#ifndef DEFAULT_SPEED
#define DEFAULT_SPEED lightpath::core::kDefaultSpeed
#endif

#ifndef FULL_BRIGHTNESS
#define FULL_BRIGHTNESS lightpath::core::kFullBrightness
#endif
