#pragma once

#include "../../src/core/Types.h"
#include "../../src/core/Limits.h"
#include "../../src/Globals.h"

/**
 * @file types.hpp
 * @brief Public core value types, enums, and limits.
 */

namespace lightpath {

using Color = ::ColorRGB;
using ListOrder = ::ListOrder;
using ListHead = ::ListHead;
using BlendMode = ::BlendMode;
using Ease = ::Ease;

constexpr uint8_t kMaxGroups = MAX_GROUPS;
constexpr uint8_t kMaxLightLists = MAX_LIGHT_LISTS;
constexpr uint16_t kMaxTotalLights = MAX_TOTAL_LIGHTS;
constexpr uint8_t kMaxNotesSet = MAX_NOTES_SET;
constexpr uint32_t kInfiniteDuration = INFINITE_DURATION;
constexpr int64_t kRandomColor = RANDOM_COLOR;
constexpr uint8_t kFullBrightness = FULL_BRIGHTNESS;

constexpr uint8_t kGroup1 = GROUP1;
constexpr uint8_t kGroup2 = GROUP2;
constexpr uint8_t kGroup3 = GROUP3;
constexpr uint8_t kGroup4 = GROUP4;
constexpr uint8_t kGroup5 = GROUP5;

/**
 * @brief Access the global runtime clock used by the legacy engine.
 */
inline unsigned long& millis() {
    return ::gMillis;
}

/**
 * @brief Access the shared global Perlin-noise generator.
 */
inline FastNoise& perlinNoise() {
    return ::gPerlinNoise;
}

}  // namespace lightpath
