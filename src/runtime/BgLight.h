#pragma once

#include "LightList.h"
#include "../Globals.h"

/**
 * BgLight - A memory-efficient version of LightList specifically for background color
 *
 * This class extends LightList but doesn't maintain an array of LPLight objects.
 * Instead, it directly renders a single color to all LEDs, which is more memory
 * efficient for background lights.
 */
class BgLight : public LightList {
public:
    float offset;
    float position;
    
    // Override constructor to avoid allocating lights array
    BgLight() : LightList() {
        // Initialize key parameters
        this->behaviour = NULL;
        this->linked = true;
        this->lifeMillis = INFINITE_DURATION;

        // Note: We don't initialize 'lights' array as we don't use it
        // Set numLights to 0 - we don't track individual lights
        this->numLights = 0;
        this->numEmitted = 0;
        
        this->editable = true;
        this->speed = 0.f;
        
        this->position = 0.f;
    }

    ~BgLight() {
        // Override destructor to avoid deleting lights array that doesn't exist
        // We only delete the behaviour if it exists - this is handled by the parent class

        // Important: Set lights to null before parent destructor runs
        lights = NULL;
    }

    // Override setup to avoid creating actual lights
    void setup(uint16_t numPixels, uint8_t maxBri = 255) override {
        // Just store the pixel count but don't actually create any lights
        this->length = numPixels;
        this->maxBri = maxBri;
    }
    
    void reset() override {
        internalTime = 0;
        position = 0.f;
    }

    void setPalette(const Palette& newPalette) override {
        palette = newPalette;
        colors = palette.interpolate(length);
    }

    // Internal time value to use with easing functions
    float internalTime = 0;
    
    // Override update to render the background directly
    bool update() override {
        // Check expiration
        if (lifeMillis > 0 && gMillis > lifeMillis) {
            return true;  // Light has expired
        }
        
        // Update our internal time counter - this is what the easing is applied to
        internalTime += speed;
        
        // Keep the internal time within a reasonable range
        while (internalTime > 1000) internalTime -= 1000;
        while (internalTime < 0) internalTime += 1000;
        
        // Apply easing to determine the actual offset
        // For EASE_NONE, we do direct linear mapping
        if (easeIndex == EASE_NONE) {
            position = offset + fmod(internalTime, length);
        } else {
            // For all other easings, we map through a repeating pattern
            // Scale internal time to 0-1 range for each cycle
            float cyclePosition = fmod(internalTime, 100) / 100.0f;
            
            // Apply easing to this cycle position (0-1 range)
            float easedPosition = ofxeasing::map(cyclePosition, 0, 1, 0, 1, ease);
            
            // Map the eased position to the length of colors
            position = offset + easedPosition * length;
        }
        
        // Background lights don't need to be updated as they just provide a static color
        return false;  // Not expired
    }
    
    ColorRGB getColor(int16_t pixel) const override {
        if (pixel >= 0 && this->behaviour && this->behaviour->randomColor()) {
            ColorRGB randomColor;
            randomColor.setRandom();
            return randomColor;
        }
        
        if (colors.empty()) {
            return Palette::noColor;
        }
        ColorRGB color = getLightColor(uint32_t(position + pixel) % length);
        if (maxBri < 255) {
            return color.Dim(maxBri);
        }
        return color;
    }
    
    const ColorRGB& getLightColor(uint32_t i) const override {
        return Palette::wrapColors(i, length, colors, palette.getWrapMode(), palette.getSegmentation());
    }

    // Override init to avoid allocating lights array
    void init(uint16_t numPixels) override {
        // Just store the count but don't allocate anything
        this->length = numPixels;
    }
    
    float getOffset() const override {
        return offset;
    }
    
    void setOffset(float newPosition) override {
        offset = newPosition;
    }
};
