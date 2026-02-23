#pragma once

#include "Palette.h"

// Predefined color palettes for use with HomoDeus LED system
// These palettes were originally derived from FastLED's cpt-city palettes
// but have been converted to use our Palette class

// Function to create a Palette from position and RGB values
Palette createPalette(const uint8_t* positions, const uint8_t* r, const uint8_t* g, const uint8_t* b, uint8_t count);

// Predefined palette function declarations
Palette ib_jul01_palette();
Palette es_vintage_57_palette();
Palette es_vintage_01_palette();
Palette es_rivendell_15_palette();
Palette rgi_15_palette();
Palette retro2_16_palette();
Palette Analogous_1_palette();
Palette es_pinksplash_08_palette();
Palette es_pinksplash_07_palette();
Palette Coral_reef_palette();
Palette es_ocean_breeze_068_palette();
Palette es_ocean_breeze_036_palette();
Palette departure_palette();
Palette es_landscape_64_palette();
Palette es_landscape_33_palette();
Palette rainbowsherbet_palette();
Palette gr65_hult_palette();
Palette gr64_hult_palette();
Palette GMT_drywet_palette();
Palette ib15_palette();
Palette Fuschia_7_palette();
Palette es_emerald_dragon_08_palette();
Palette lava_palette();
Palette fire_palette();
Palette Colorfull_palette();
Palette Magenta_Evening_palette();
Palette Pink_Purple_palette();
Palette Sunset_Real_palette();
Palette es_autumn_19_palette();
Palette BlacK_Blue_Magenta_White_palette();
Palette BlacK_Magenta_Red_palette();
Palette BlacK_Red_Magenta_Yellow_palette();
Palette Blue_Cyan_Yellow_palette();

// Function pointer type for palette functions
typedef Palette (*PaletteFunctionPtr)();

// Function to get number of available palettes
uint8_t getPaletteCount();

// Function to get a palette by index
Palette getPalette(uint8_t index);
