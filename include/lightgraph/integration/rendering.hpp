#pragma once

#include "rendering/Palette.h"
#include "rendering/Palettes.h"

/**
 * @file rendering.hpp
 * @brief Public palette and rendering utilities.
 */

namespace lightgraph::integration {

using Palette = ::Palette;

constexpr int8_t kWrapNoWrap = WRAP_NOWRAP;
constexpr int8_t kWrapClampToEdge = WRAP_CLAMP_TO_EDGE;
constexpr int8_t kWrapRepeat = WRAP_REPEAT;
constexpr int8_t kWrapRepeatMirror = WRAP_REPEAT_MIRROR;

/**
 * @brief Return the number of built-in palettes.
 */
inline uint8_t paletteCount() { return ::getPaletteCount(); }

/**
 * @brief Get a built-in palette by index.
 */
inline Palette paletteAt(uint8_t index) { return ::getPalette(index); }

} // namespace lightgraph::integration
