#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @file palette_names.hpp
 * @brief Human-readable names for built-in palette indices.
 */

namespace lightgraph::integration {

inline constexpr const char* kPredefinedPaletteNames[] = {
    "Sunset Real",
    "ES Rivendell",
    "Ocean Breeze",
    "RGI 15",
    "Retro 2",
    "Analogous 1",
    "Pink Splash 08",
    "Coral Reef",
    "Ocean Breeze 068",
    "Pink Splash 07",
    "Vintage 01",
    "Departure",
    "Landscape 64",
    "Landscape 33",
    "Rainbow Sherbet",
    "Green Hult 65",
    "Green Hult 64",
    "Dry Wet",
    "July 01",
    "Vintage 57",
    "IB 15",
    "Fuschia 7",
    "Emerald Dragon",
    "Lava",
    "Fire",
    "Colorful",
    "Magenta Evening",
    "Pink Purple",
    "Autumn 19",
    "Black Blue Magenta White",
    "Black Magenta Red",
    "Black Red Magenta Yellow",
    "Blue Cyan Yellow",
};

inline constexpr uint8_t predefinedPaletteNameCount() {
  return static_cast<uint8_t>(sizeof(kPredefinedPaletteNames) / sizeof(kPredefinedPaletteNames[0]));
}

inline constexpr const char* predefinedPaletteNameAt(uint8_t index) {
  const uint8_t count = predefinedPaletteNameCount();
  if (count == 0) {
    return "";
  }
  return kPredefinedPaletteNames[index % count];
}

} // namespace lightgraph::integration
