#include "Palettes.h"

// Function to create a Palette from position and RGB values
Palette createPalette(const uint8_t* positions, const uint8_t* r, const uint8_t* g, const uint8_t* b, uint8_t count) {
    std::vector<int64_t> colors;
    std::vector<float> pos;

    for (uint8_t i = 0; i < count; i++) {
        // Create color as 24-bit RGB
        int64_t color = ((int64_t)r[i] << 16) | ((int64_t)g[i] << 8) | b[i];
        colors.push_back(color);

        // Convert position from 0-255 to 0-1 range
        pos.push_back((float)positions[i] / 255.0f);
    }

    return Palette(colors, pos);
}

// Gradient palette "ib_jul01"
Palette ib_jul01_palette() {
    const uint8_t positions[] = {0, 94, 132, 255};
    const uint8_t r[] = {194, 1, 57, 113};
    const uint8_t g[] = {1, 29, 131, 1};
    const uint8_t b[] = {1, 18, 28, 1};
    return createPalette(positions, r, g, b, 4);
}

// Gradient palette "es_vintage_57"
Palette es_vintage_57_palette() {
    const uint8_t positions[] = {0, 53, 104, 153, 255};
    const uint8_t r[] = {2, 18, 69, 167, 46};
    const uint8_t g[] = {1, 1, 29, 135, 56};
    const uint8_t b[] = {1, 0, 1, 10, 4};
    return createPalette(positions, r, g, b, 5);
}

// Gradient palette "es_vintage_01"
Palette es_vintage_01_palette() {
    const uint8_t positions[] = {0, 51, 76, 101, 127, 153, 229, 255};
    const uint8_t r[] = {4, 16, 97, 255, 67, 16, 4, 4};
    const uint8_t g[] = {1, 0, 104, 131, 9, 0, 1, 1};
    const uint8_t b[] = {1, 1, 3, 19, 4, 1, 1, 1};
    return createPalette(positions, r, g, b, 8);
}

// Gradient palette "es_rivendell_15"
Palette es_rivendell_15_palette() {
    const uint8_t positions[] = {0, 101, 165, 242, 255};
    const uint8_t r[] = {1, 16, 56, 150, 150};
    const uint8_t g[] = {14, 36, 68, 156, 156};
    const uint8_t b[] = {5, 14, 30, 99, 99};
    return createPalette(positions, r, g, b, 5);
}

// Gradient palette "rgi_15"
Palette rgi_15_palette() {
    const uint8_t positions[] = {0, 31, 63, 95, 127, 159, 191, 223, 255};
    const uint8_t r[] = {4, 55, 197, 59, 6, 39, 112, 56, 22};
    const uint8_t g[] = {1, 1, 3, 2, 2, 6, 13, 9, 6};
    const uint8_t b[] = {31, 16, 7, 17, 34, 33, 32, 35, 38};
    return createPalette(positions, r, g, b, 9);
}

// Gradient palette "retro2_16"
Palette retro2_16_palette() {
    const uint8_t positions[] = {0, 255};
    const uint8_t r[] = {188, 46};
    const uint8_t g[] = {135, 7};
    const uint8_t b[] = {1, 1};
    return createPalette(positions, r, g, b, 2);
}

// Gradient palette "Analogous_1"
Palette Analogous_1_palette() {
    const uint8_t positions[] = {0, 63, 127, 191, 255};
    const uint8_t r[] = {3, 23, 67, 142, 255};
    const uint8_t g[] = {0, 0, 0, 0, 0};
    const uint8_t b[] = {255, 255, 255, 45, 0};
    return createPalette(positions, r, g, b, 5);
}

// Gradient palette "es_pinksplash_08"
Palette es_pinksplash_08_palette() {
    const uint8_t positions[] = {0, 127, 175, 221, 255};
    const uint8_t r[] = {126, 197, 210, 157, 157};
    const uint8_t g[] = {11, 1, 157, 3, 3};
    const uint8_t b[] = {255, 22, 172, 112, 112};
    return createPalette(positions, r, g, b, 5);
}

// Gradient palette "es_pinksplash_07"
Palette es_pinksplash_07_palette() {
    const uint8_t positions[] = {0, 61, 101, 127, 153, 193, 255};
    const uint8_t r[] = {229, 242, 255, 249, 255, 244, 232};
    const uint8_t g[] = {1, 4, 12, 81, 11, 5, 1};
    const uint8_t b[] = {1, 63, 255, 252, 235, 68, 5};
    return createPalette(positions, r, g, b, 7);
}

// Gradient palette "Coral_reef"
Palette Coral_reef_palette() {
    const uint8_t positions[] = {0, 50, 96, 96, 139, 255};
    const uint8_t r[] = {40, 10, 1, 43, 10, 1};
    const uint8_t g[] = {199, 152, 111, 127, 73, 34};
    const uint8_t b[] = {197, 155, 120, 162, 111, 71};
    return createPalette(positions, r, g, b, 6);
}

// Gradient palette "es_ocean_breeze_068"
Palette es_ocean_breeze_068_palette() {
    const uint8_t positions[] = {0, 51, 101, 104, 178, 255};
    const uint8_t r[] = {100, 1, 1, 35, 0, 1};
    const uint8_t g[] = {156, 99, 68, 142, 63, 10};
    const uint8_t b[] = {153, 137, 84, 168, 117, 10};
    return createPalette(positions, r, g, b, 6);
}

// Gradient palette "es_ocean_breeze_036"
Palette es_ocean_breeze_036_palette() {
    const uint8_t positions[] = {0, 89, 153, 255};
    const uint8_t r[] = {1, 1, 144, 0};
    const uint8_t g[] = {6, 99, 209, 73};
    const uint8_t b[] = {7, 111, 255, 82};
    return createPalette(positions, r, g, b, 4);
}

// Gradient palette "departure"
Palette departure_palette() {
    const uint8_t positions[] = {0, 42, 63, 84, 106, 116, 138, 148, 170, 191, 212, 255};
    const uint8_t r[] = {8, 23, 75, 169, 213, 255, 135, 22, 0, 0, 0, 0};
    const uint8_t g[] = {3, 7, 38, 99, 169, 255, 255, 255, 255, 136, 55, 55};
    const uint8_t b[] = {0, 0, 6, 38, 119, 255, 138, 24, 0, 0, 0, 0};
    return createPalette(positions, r, g, b, 12);
}

// Gradient palette "es_landscape_64"
Palette es_landscape_64_palette() {
    const uint8_t positions[] = {0, 37, 76, 127, 128, 130, 153, 204, 255};
    const uint8_t r[] = {0, 2, 15, 79, 126, 188, 144, 59, 1};
    const uint8_t g[] = {0, 25, 115, 213, 211, 209, 182, 117, 37};
    const uint8_t b[] = {0, 1, 5, 1, 47, 247, 205, 250, 192};
    return createPalette(positions, r, g, b, 9);
}

// Gradient palette "es_landscape_33"
Palette es_landscape_33_palette() {
    const uint8_t positions[] = {0, 19, 38, 63, 66, 255};
    const uint8_t r[] = {1, 32, 161, 229, 39, 1};
    const uint8_t g[] = {5, 23, 55, 144, 142, 4};
    const uint8_t b[] = {0, 1, 1, 1, 74, 1};
    return createPalette(positions, r, g, b, 6);
}

// Gradient palette "rainbowsherbet"
Palette rainbowsherbet_palette() {
    const uint8_t positions[] = {0, 43, 86, 127, 170, 209, 255};
    const uint8_t r[] = {255, 255, 255, 255, 255, 42, 87};
    const uint8_t g[] = {33, 68, 7, 82, 255, 255, 255};
    const uint8_t b[] = {4, 25, 25, 103, 242, 22, 65};
    return createPalette(positions, r, g, b, 7);
}

// Gradient palette "gr65_hult"
Palette gr65_hult_palette() {
    const uint8_t positions[] = {0, 48, 89, 160, 216, 255};
    const uint8_t r[] = {247, 255, 220, 7, 1, 1};
    const uint8_t g[] = {176, 136, 29, 82, 124, 124};
    const uint8_t b[] = {247, 255, 226, 178, 109, 109};
    return createPalette(positions, r, g, b, 6);
}

// Gradient palette "gr64_hult"
Palette gr64_hult_palette() {
    const uint8_t positions[] = {0, 66, 104, 130, 150, 201, 239, 255};
    const uint8_t r[] = {1, 1, 52, 115, 52, 1, 0, 0};
    const uint8_t g[] = {124, 93, 65, 127, 65, 86, 55, 55};
    const uint8_t b[] = {109, 79, 1, 1, 1, 72, 45, 45};
    return createPalette(positions, r, g, b, 8);
}

// Gradient palette "GMT_drywet"
Palette GMT_drywet_palette() {
    const uint8_t positions[] = {0, 42, 84, 127, 170, 212, 255};
    const uint8_t r[] = {47, 213, 103, 3, 1, 1, 1};
    const uint8_t g[] = {30, 147, 219, 219, 48, 1, 7};
    const uint8_t b[] = {2, 24, 52, 207, 214, 111, 33};
    return createPalette(positions, r, g, b, 7);
}

// Gradient palette "ib15"
Palette ib15_palette() {
    const uint8_t positions[] = {0, 72, 89, 107, 141, 255};
    const uint8_t r[] = {113, 157, 208, 255, 137, 59};
    const uint8_t g[] = {91, 88, 85, 29, 31, 33};
    const uint8_t b[] = {147, 78, 33, 11, 39, 89};
    return createPalette(positions, r, g, b, 6);
}

// Gradient palette "Fuschia_7"
Palette Fuschia_7_palette() {
    const uint8_t positions[] = {0, 63, 127, 191, 255};
    const uint8_t r[] = {43, 100, 188, 161, 135};
    const uint8_t g[] = {3, 4, 5, 11, 20};
    const uint8_t b[] = {153, 103, 66, 115, 182};
    return createPalette(positions, r, g, b, 5);
}

// Gradient palette "es_emerald_dragon_08"
Palette es_emerald_dragon_08_palette() {
    const uint8_t positions[] = {0, 101, 178, 255};
    const uint8_t r[] = {97, 47, 13, 2};
    const uint8_t g[] = {255, 133, 43, 10};
    const uint8_t b[] = {1, 1, 1, 1};
    return createPalette(positions, r, g, b, 4);
}

// Gradient palette "lava"
Palette lava_palette() {
    const uint8_t positions[] = {0, 46, 96, 108, 119, 146, 174, 188, 202, 218, 234, 244, 255};
    const uint8_t r[] = {0, 18, 113, 142, 175, 213, 255, 255, 255, 255, 255, 255, 255};
    const uint8_t g[] = {0, 0, 0, 3, 17, 44, 82, 115, 156, 203, 255, 255, 255};
    const uint8_t b[] = {0, 0, 0, 1, 1, 2, 4, 4, 4, 4, 4, 71, 255};
    return createPalette(positions, r, g, b, 13);
}

// Gradient palette "fire"
Palette fire_palette() {
    const uint8_t positions[] = {0, 76, 146, 197, 240, 250, 255};
    const uint8_t r[] = {1, 32, 192, 220, 252, 252, 255};
    const uint8_t g[] = {1, 5, 24, 105, 255, 255, 255};
    const uint8_t b[] = {0, 0, 0, 5, 31, 111, 255};
    return createPalette(positions, r, g, b, 7);
}

// Gradient palette "Colorfull"
Palette Colorfull_palette() {
    const uint8_t positions[] = {0, 25, 60, 93, 106, 109, 113, 116, 124, 168, 255};
    const uint8_t r[] = {10, 29, 59, 83, 110, 123, 139, 192, 255, 100, 22};
    const uint8_t g[] = {85, 109, 138, 99, 66, 49, 35, 117, 255, 180, 121};
    const uint8_t b[] = {5, 18, 42, 52, 64, 65, 66, 98, 137, 155, 174};
    return createPalette(positions, r, g, b, 11);
}

// Gradient palette "Magenta_Evening"
Palette Magenta_Evening_palette() {
    const uint8_t positions[] = {0, 31, 63, 70, 76, 108, 255};
    const uint8_t r[] = {71, 130, 213, 232, 252, 123, 46};
    const uint8_t g[] = {27, 11, 2, 1, 1, 2, 9};
    const uint8_t b[] = {39, 51, 64, 66, 69, 51, 35};
    return createPalette(positions, r, g, b, 7);
}

// Gradient palette "Pink_Purple"
Palette Pink_Purple_palette() {
    const uint8_t positions[] = {0, 25, 51, 76, 102, 109, 114, 122, 149, 183, 255};
    const uint8_t r[] = {19, 26, 33, 68, 118, 163, 217, 159, 113, 128, 146};
    const uint8_t g[] = {2, 4, 6, 62, 187, 215, 244, 149, 78, 57, 40};
    const uint8_t b[] = {39, 45, 52, 125, 240, 247, 255, 221, 188, 155, 123};
    return createPalette(positions, r, g, b, 11);
}

// Gradient palette "Sunset_Real"
Palette Sunset_Real_palette() {
    const uint8_t positions[] = {0, 22, 51, 85, 135, 198, 255};
    const uint8_t r[] = {120, 179, 255, 167, 100, 16, 0};
    const uint8_t g[] = {0, 22, 104, 22, 0, 0, 0};
    const uint8_t b[] = {0, 0, 0, 18, 103, 130, 160};
    return createPalette(positions, r, g, b, 7);
}

// Gradient palette "es_autumn_19"
Palette es_autumn_19_palette() {
    const uint8_t positions[] = {0, 51, 84, 104, 112, 122, 124, 135, 142, 163, 204, 249, 255};
    const uint8_t r[] = {26, 67, 118, 137, 113, 133, 137, 113, 139, 113, 55, 17, 17};
    const uint8_t g[] = {1, 4, 14, 152, 65, 149, 152, 65, 154, 13, 3, 1, 1};
    const uint8_t b[] = {1, 1, 1, 52, 1, 59, 52, 1, 46, 1, 1, 1, 1};
    return createPalette(positions, r, g, b, 13);
}

// Gradient palette "BlacK_Blue_Magenta_White"
Palette BlacK_Blue_Magenta_White_palette() {
    const uint8_t positions[] = {0, 42, 84, 127, 170, 212, 255};
    const uint8_t r[] = {0, 0, 0, 42, 255, 255, 255};
    const uint8_t g[] = {0, 0, 0, 0, 0, 55, 255};
    const uint8_t b[] = {0, 45, 255, 255, 255, 255, 255};
    return createPalette(positions, r, g, b, 7);
}

// Gradient palette "BlacK_Magenta_Red"
Palette BlacK_Magenta_Red_palette() {
    const uint8_t positions[] = {0, 63, 127, 191, 255};
    const uint8_t r[] = {0, 42, 255, 255, 255};
    const uint8_t g[] = {0, 0, 0, 0, 0};
    const uint8_t b[] = {0, 45, 255, 45, 0};
    return createPalette(positions, r, g, b, 5);
}

// Gradient palette "BlacK_Red_Magenta_Yellow"
Palette BlacK_Red_Magenta_Yellow_palette() {
    const uint8_t positions[] = {0, 42, 84, 127, 170, 212, 255};
    const uint8_t r[] = {0, 42, 255, 255, 255, 255, 255};
    const uint8_t g[] = {0, 0, 0, 0, 0, 55, 255};
    const uint8_t b[] = {0, 0, 0, 45, 255, 45, 0};
    return createPalette(positions, r, g, b, 7);
}

// Gradient palette "Blue_Cyan_Yellow"
Palette Blue_Cyan_Yellow_palette() {
    const uint8_t positions[] = {0, 63, 127, 191, 255};
    const uint8_t r[] = {0, 0, 0, 42, 255};
    const uint8_t g[] = {0, 55, 255, 255, 255};
    const uint8_t b[] = {255, 255, 255, 45, 0};
    return createPalette(positions, r, g, b, 5);
}

// Array of functions returning palettes
static const PaletteFunctionPtr paletteFunctions[] = {
    &Sunset_Real_palette,
    &es_rivendell_15_palette,
    &es_ocean_breeze_036_palette,
    &rgi_15_palette,
    &retro2_16_palette,
    &Analogous_1_palette,
    &es_pinksplash_08_palette,
    &Coral_reef_palette,
    &es_ocean_breeze_068_palette,
    &es_pinksplash_07_palette,
    &es_vintage_01_palette,
    &departure_palette,
    &es_landscape_64_palette,
    &es_landscape_33_palette,
    &rainbowsherbet_palette,
    &gr65_hult_palette,
    &gr64_hult_palette,
    &GMT_drywet_palette,
    &ib_jul01_palette,
    &es_vintage_57_palette,
    &ib15_palette,
    &Fuschia_7_palette,
    &es_emerald_dragon_08_palette,
    &lava_palette,
    &fire_palette,
    &Colorfull_palette,
    &Magenta_Evening_palette,
    &Pink_Purple_palette,
    &es_autumn_19_palette,
    &BlacK_Blue_Magenta_White_palette,
    &BlacK_Magenta_Red_palette,
    &BlacK_Red_Magenta_Yellow_palette,
    &Blue_Cyan_Yellow_palette
};

uint8_t getPaletteCount() {
    return sizeof(paletteFunctions) / sizeof(paletteFunctions[0]);
}

// Function to get a palette by index
Palette getPalette(uint8_t index) {
    static const uint8_t paletteCount = getPaletteCount();

    // Ensure index is in bounds
    if (index >= paletteCount) {
        index = index % paletteCount;
    }

    // Return the palette
    return paletteFunctions[index]();
}
