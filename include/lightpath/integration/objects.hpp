#pragma once

#include "../../../src/objects/HeptagonStar.h"
#include "../../../src/objects/Heptagon919.h"
#include "../../../src/objects/Heptagon3024.h"
#include "../../../src/objects/Line.h"
#include "../../../src/objects/Cross.h"
#include "../../../src/objects/Triangle.h"
#include "topology.hpp"

/**
 * @file objects.hpp
 * @brief Built-in topology object implementations and model enums.
 */

namespace lightpath::integration {

using HeptagonStar = ::HeptagonStar;
using Heptagon919 = ::Heptagon919;
using Heptagon3024 = ::Heptagon3024;
using Line = ::Line;
using Cross = ::Cross;
using Triangle = ::Triangle;

using HeptagonStarModel = ::HeptagonStarModel;
using LineModel = ::LineModel;
using CrossModel = ::CrossModel;
using TriangleModel = ::TriangleModel;

constexpr uint16_t kHeptagon919PixelCount = HEPTAGON919_PIXEL_COUNT;
constexpr uint16_t kHeptagon3024PixelCount = HEPTAGON3024_PIXEL_COUNT;
constexpr uint16_t kLinePixelCount = LINE_PIXEL_COUNT;
constexpr uint16_t kCrossPixelCount = CROSS_PIXEL_COUNT;
constexpr uint16_t kTrianglePixelCount = TRIANGLE_PIXEL_COUNT;

}  // namespace lightpath::integration
