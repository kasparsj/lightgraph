#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/Types.h"
#include "../core/Limits.h"
#include "../../vendor/ofxColorTheory/src/ColorScheme.h"
#include "../../vendor/ofxColorTheory/src/ColorWheelSchemes.h"

#define WRAP_NOWRAP -1
#define WRAP_CLAMP_TO_EDGE 0
#define WRAP_REPEAT 1
#define WRAP_REPEAT_MIRROR 2

inline size_t wrapIndex(size_t i, size_t numColors, int8_t wrapMode) {
    switch (wrapMode) {
        case WRAP_REPEAT_MIRROR: {
            size_t period = 2 * numColors - 2; // full cycle: up and back (excluding repeated ends)
            if (period == 0) {
                return 0;
            }
            size_t modIndex = i % period;
            return modIndex < numColors ? modIndex : (period - modIndex);
        }
        case WRAP_REPEAT: {
            return i % numColors;
        }
        case WRAP_CLAMP_TO_EDGE:
            return numColors - 1;
        case WRAP_NOWRAP:
        default:
            return i;
    }
}

class Palette {
public:
    static ColorRGB noColor;
    static const ColorRGB& wrapColors(uint32_t i, uint32_t total, const std::vector<ColorRGB>& colors, int8_t wrapMode, float segmentation = 0.0f) {
        if (colors.empty()) return noColor;
        
        size_t numColors = colors.size();
        if (numColors == 1) {
            return colors[0];
        }
        
        size_t colorIndex = i;
                
        // Apply segmentation if enabled
        if (segmentation > 0.0f && total > 0) {
            const size_t segmentSize = std::max<size_t>(1, static_cast<size_t>(total / segmentation));
            float segmentNum;
            const float segmentFrac = std::modf(static_cast<float>(colorIndex) / segmentSize, &segmentNum);
            
            // Check for segment boundary condition - first segment's last color clamp
            if (wrapMode == WRAP_CLAMP_TO_EDGE && segmentation >= 2.0f && segmentNum >= 1) {
                colorIndex = numColors - 1;
            } else if (wrapMode > WRAP_NOWRAP || segmentNum < 1) {
                colorIndex = segmentFrac * numColors;
                
                if (wrapMode == WRAP_REPEAT_MIRROR && (int) segmentNum % 2 == 1) {
                    colorIndex = numColors - 1 - colorIndex;
                }
                else if (wrapMode == WRAP_REPEAT && colorIndex >= numColors) {
                    colorIndex = wrapIndex(colorIndex, numColors, wrapMode);
                }
            }
            else {
                return noColor;
            }
        }
        else if (colorIndex >= numColors) {
            colorIndex = wrapIndex(colorIndex, numColors, wrapMode);
        }
        
        if (colorIndex < numColors) {
            return colors[colorIndex];
        }
        return noColor;
    }
    
    // Constructors
    Palette();
    Palette(const std::vector<int64_t>& colors);
    Palette(const std::vector<int64_t>& colors, const std::vector<float>& positions);
    
    // Methods
    void addColor(int64_t color, float position = -1.0f);
    void removeColor(size_t index);
    void removeColor(int64_t color);
    void setColor(size_t index, int64_t color);
    void setPosition(size_t index, float position);
    
    // Color rule generation and interpolation
    void generateColors();
    
    // Sort colors by position
    void sortByPosition();
    
    // Clear the palette
    void clear();
    
    // Get colors
    const std::vector<int64_t>& getColors() const;
    const std::vector<float>& getPositions() const;
    const std::vector<ColorRGB>& getRGBColors();
    std::vector<ColorRGB> interpolate(uint16_t maxColors) {
        std::vector<ColorRGB> rgbColors = getRGBColors();
        if (interMode < 0 || rgbColors.size() < 2) {
            return rgbColors;
        }
        ofxColorTheory::ColorScheme_<ColorRGB> basicScheme(rgbColors);
        return basicScheme.interpolate(maxColors, interMode, &positions);
    }
    
    // Set colors
    void setColors(const std::vector<int64_t>& colors);
    void setColors(const std::vector<ColorRGB>&);
    void setPositions(const std::vector<float>& positions);
    
    // Get/set color rule
    int8_t getColorRule() const;
    void setColorRule(int8_t rule);
    
    // Get/set interpolation mode
    int8_t getInterMode() const;
    void setInterMode(int8_t mode);
    
    // Get/set wrap mode
    int8_t getWrapMode() const;
    void setWrapMode(int8_t mode);
    
    
    // Get/set segmentation
    float getSegmentation() const;
    void setSegmentation(float seg);
    
    // Get size
    size_t size() const;
    
    // Operators
    int64_t operator[](size_t index) const;
    
private:
    std::vector<int64_t> colors;
    std::vector<float> positions;
    mutable std::vector<ColorRGB> rgbColors; // Cache for RGB colors, updated as needed
    int8_t colorRule = -1;
    int8_t interMode = 1; // 0 = RGB, 1 = HSB, 2 = CIELCh, -1 = none. Default: HSB
    int8_t wrapMode = -1; // 0 = clamp to edge, 1 = wrap, 2 = wrapMirror, -1 = none. Default: none
    float segmentation = 0.0f; // Segmentation value - 0 means no segmentation
    
    // Generate default positions for colors
    void generateDefaultPositions();
    
    // Update RGB colors cache
    void updateRGBColors();
    
    // Flag to track if the RGB cache is dirty
    mutable bool rgbCacheDirty;
};
