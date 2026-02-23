#include "Palette.h"
#include "../../vendor/ofxColorTheory/src/ColorWheelSchemes.h"

ColorRGB Palette::noColor = ColorRGB(0, 0, 0);

Palette::Palette() : colorRule(-1), rgbCacheDirty(true) {
    // Initialize with empty palette
}

Palette::Palette(const std::vector<int64_t>& colors) : colors(colors), colorRule(-1), rgbCacheDirty(true) {
    generateDefaultPositions();
}

Palette::Palette(const std::vector<int64_t>& colors, const std::vector<float>& positions) 
    : colors(colors), colorRule(-1), rgbCacheDirty(true) {
    
    // Use provided positions if they match the number of colors
    if (positions.size() == colors.size()) {
        this->positions = positions;
    } else {
        generateDefaultPositions();
    }
    
    // Sort colors by position
    sortByPosition();
}

void Palette::addColor(int64_t color, float position) {
    colors.push_back(color);
    
    // If position is -1 (default), calculate a position
    if (position < 0.0f) {
        // If it's the first color, position at 0.0
        if (colors.size() == 1) {
            positions.push_back(0.0f);
        } else if (colors.size() > 1) {
            // Otherwise, position at the end (1.0)
            positions.push_back(1.0f);
        }
    } else {
        // Use the provided position
        positions.push_back(std::max(0.0f, std::min(1.0f, position)));
    }
    
    rgbCacheDirty = true;
}

void Palette::removeColor(size_t index) {
    if (index < colors.size()) {
        colors.erase(colors.begin() + index);
        positions.erase(positions.begin() + index);
        rgbCacheDirty = true;
    }
}

void Palette::removeColor(int64_t color) {
    for (size_t i = 0; i < colors.size(); i++) {
        if (colors[i] == color) {
            removeColor(i);
            break;
        }
    }
}

void Palette::setColor(size_t index, int64_t color) {
    if (index < colors.size()) {
        colors[index] = color;
        rgbCacheDirty = true;
    }
}

void Palette::setPosition(size_t index, float position) {
    if (index < positions.size()) {
        positions[index] = std::max(0.0f, std::min(1.0f, position));
        rgbCacheDirty = true;
    }
}

void Palette::sortByPosition() {
    if (colors.size() != positions.size() || colors.empty()) {
        return;
    }
    
    // Create pairs of (position, color)
    std::vector<std::pair<float, int64_t>> pairs;
    for (size_t i = 0; i < colors.size(); i++) {
        pairs.push_back(std::make_pair(positions[i], colors[i]));
    }
    
    // Sort by position
    std::sort(pairs.begin(), pairs.end(), 
              [](const std::pair<float, int64_t>& a, const std::pair<float, int64_t>& b) {
                  return a.first < b.first;
              });
    
    // Rebuild the sorted vectors
    colors.clear();
    positions.clear();
    
    for (const auto& pair : pairs) {
        positions.push_back(pair.first);
        colors.push_back(pair.second);
    }
    
    rgbCacheDirty = true;
}

void Palette::clear() {
    colors.clear();
    positions.clear();
    rgbColors.clear();
    rgbCacheDirty = true;
}

const std::vector<int64_t>& Palette::getColors() const {
    return colors;
}

const std::vector<float>& Palette::getPositions() const {
    return positions;
}

const std::vector<ColorRGB>& Palette::getRGBColors() {
    if (rgbCacheDirty) {
        updateRGBColors();
    }
    return rgbColors;
}

void Palette::setColors(const std::vector<int64_t>& newColors) {
    colors = newColors;
    
    // Update positions if the number of colors has changed
    if (positions.size() != colors.size()) {
        generateDefaultPositions();
    }
    
    rgbCacheDirty = true;
}

void Palette::setColors(const std::vector<ColorRGB>& newColors) {
    colors.clear();
    for (size_t i = 0; i < newColors.size(); i++) {
        colors.push_back(newColors[i].get());
    }
    
    // Update positions if the number of colors has changed
    if (positions.size() != colors.size()) {
        generateDefaultPositions();
    }
    
    rgbCacheDirty = true;
}

void Palette::setPositions(const std::vector<float>& newPositions) {
    // Validate all positions are between 0 and 1
    std::vector<float> validatedPositions;
    for (float pos : newPositions) {
        validatedPositions.push_back(std::max(0.0f, std::min(1.0f, pos)));
    }
    
    // Only update if the number of positions matches the number of colors
    if (validatedPositions.size() == colors.size()) {
        positions = validatedPositions;
        rgbCacheDirty = true;
    } else if (colors.size() > 0) {
        // If we have colors but the positions don't match,
        // generate default positions instead
        generateDefaultPositions();
    }
}

size_t Palette::size() const {
    return colors.size();
}

int64_t Palette::operator[](size_t index) const {
    if (index < colors.size()) {
        return colors[index];
    }
    return 0; // Default fallback
}

void Palette::generateDefaultPositions() {
    positions.clear();
    
    // Generate evenly spaced positions from 0 to 1
    if (colors.size() == 1) {
        positions.push_back(0.0f);
    } else if (colors.size() > 1) {
        for (size_t i = 0; i < colors.size(); i++) {
            positions.push_back(static_cast<float>(i) / (colors.size() - 1));
        }
    }
}

void Palette::updateRGBColors() {
    rgbColors.clear();
    generateColors();
}

int8_t Palette::getColorRule() const {
    return colorRule;
}

void Palette::setColorRule(int8_t rule) {
    colorRule = rule;
    rgbCacheDirty = true;
}

int8_t Palette::getInterMode() const {
    return interMode;
}

void Palette::setInterMode(int8_t mode) {
    interMode = mode;
}

int8_t Palette::getWrapMode() const {
    return wrapMode;
}

void Palette::setWrapMode(int8_t mode) {
    wrapMode = mode;
}


float Palette::getSegmentation() const {
    return segmentation;
}

void Palette::setSegmentation(float seg) {
    // Ensure value is not negative
    segmentation = seg < 0.0f ? 0.0f : seg;
}

inline ColorRGB toRGB(int64_t hex) {
    ColorRGB rgb;
    if (hex == RANDOM_COLOR)
        rgb.setRandom();
    else
        rgb.set(hex);
    return rgb;
}

void Palette::generateColors() {
    // Check if a color rule is selected
    if (colorRule >= 0 && colorRule <= 7) {
        // Use color wheel scheme with rule
        auto colorScheme = ofxColorTheory::ColorWheelSchemes_<ColorRGB>::get(
            static_cast<ofxColorTheory::ColorRule>(colorRule));

        if (colorScheme) {
            colorScheme->getColors().clear();
            
            // Apply the rule to each color in the palette
            for (const auto& color : colors) {
                colorScheme->setPrimaryColor(toRGB(color));
                colorScheme->generate();
            }
            
            rgbColors = colorScheme->getColors();
        }
    }
    else {
        for (const auto& color : colors) {
            rgbColors.push_back(toRGB(color));
        }
    }
}
