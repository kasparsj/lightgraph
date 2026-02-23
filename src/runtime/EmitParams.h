#pragma once

#include <algorithm>
#include <optional>
#include "../Config.h"
#include "../LPRandom.h"
#include <vector>
#include "../rendering/Palette.h"

using std::min;
using std::max;

enum EmitParam {
    P_MODEL = 0,
    P_SPEED = 1,
    P_EASE = 2,
    P_FADE = 3,
    P_FADE_THRESH = 4,
    P_FADE_EASE = 5,
    P_LENGTH = 6,
    P_TRAIL = 7,
    P_ORDER = 8,
    P_HEAD = 9,
    P_LINKED = 10,
    P_FROM = 11,
    P_DURATION_MS = 12,
    P_DURATION_FRAMES = 13,
    P_COLOR = 14,
    P_COLOR_INDEX = 15,
    P_NOTE_ID = 16,
    P_MIN_BRI = 17,
    P_MAX_BRI = 18,
    P_BEHAVIOUR = 19,
    P_EMIT_GROUPS = 20,
    P_EMIT_OFFSET = 21,
    P_COLOR_CHANGE_GROUPS = 22,
    P_COLOR_RULE = 23,
};

class EmitParams {

  public:

    static int8_t DEFAULT_MODEL;
    static uint32_t DEFAULT_DURATION;
    static uint8_t DEFAULT_BRIGHTNESS;
    static int64_t DEFAULT_COLOR;
    static float DURATION_FPS;

    int8_t model = DEFAULT_MODEL;
    float speed = DEFAULT_SPEED;
    uint8_t ease = 0;
    uint8_t fadeSpeed = 0;
    uint8_t fadeThresh = 0;
    uint8_t fadeEase = 0;
    std::optional<uint16_t> length;
    uint16_t trail = 0;
    ListOrder order = LIST_ORDER_SEQUENTIAL;
    ListHead head = LIST_HEAD_FRONT;
    bool linked = true;
    int8_t from = -1;
    uint32_t duration = DEFAULT_DURATION;
    Palette palette; // Palette to manage colors and positions
    uint16_t noteId = 0;
    uint8_t minBri = 0;
    uint8_t maxBri = DEFAULT_BRIGHTNESS;
    uint16_t behaviourFlags = 0;
    uint8_t emitGroups = 0;
    uint8_t emitOffset = 0;
    uint8_t colorChangeGroups = 0;

    static uint16_t frameMs() {
        return (1000.f / EmitParams::DURATION_FPS);
    }

    EmitParams(int8_t model, float speed, int64_t color) : model(model), speed(speed) {
        std::vector<int64_t> colors = {color};
        palette = Palette(colors);
    }

    EmitParams(int8_t model, float speed, const std::vector<int64_t>& colorArray) : model(model), speed(speed), palette(colorArray) {
    }

    EmitParams(int8_t model, float speed) : EmitParams(model, speed, DEFAULT_COLOR) {

    }
    EmitParams(int8_t model) : EmitParams(model, DEFAULT_SPEED) {

    }
    EmitParams() : EmitParams(DEFAULT_MODEL, DEFAULT_SPEED) {

    }
    
    ColorRGB getColor(size_t index = 0) const {
        ColorRGB rgb;

        // Check if palette is empty or index is out of bounds
        if (palette.size() == 0 || index >= palette.size()) {
            // Use default behavior (random color)
            rgb.setRandom();
            return rgb;
        }

        int64_t colorValue = palette[index];
        if (colorValue < 0) {
            rgb.setRandom();
        }
        else {
            rgb.set(colorValue);
        }
        return rgb;
    }

    std::vector<ColorRGB> getColors() {
        // If palette is not empty, return its RGB colors
        if (palette.size() > 0) {
            return palette.getRGBColors();
        }
        
        // If no colors defined, add at least one (random) color
        std::vector<ColorRGB> rgbColors;
        ColorRGB randomColor;
        randomColor.setRandom();
        rgbColors.push_back(randomColor);
        
        return rgbColors;
    }
    
    void setColors(std::vector<int64_t>& array) {
        palette.setColors(array);
    }
    
    void setColors(int64_t color) {
        std::vector<int64_t> colors = {color};
        palette.setColors(colors);
    }
    
    void setColorPositions(std::vector<float>& positions) {
        palette.setPositions(positions);
        // Sort colors by position
        palette.sortByPosition();
    }
    
    const std::vector<int64_t>& getColorValues() const {
        return palette.getColors();
    }
    
    const std::vector<float>& getColorPositions() const {
        return palette.getPositions();
    }
    
    float getSpeed() const {
        return speed >= 0 ? speed : LPRandom::randomSpeed();
    }

    uint16_t getLength() const {
        return length.has_value() ? *length : LPRandom::randomLength();
    }
    
    void setLength(uint16_t value) {
        length = value;
    }

    void clearLength() {
        length.reset();
    }
    
    uint16_t getSpeedTrail(float speed, uint16_t length) const {
      uint16_t trail = 0;
      if (order == LIST_ORDER_SEQUENTIAL && linked && !(behaviourFlags & B_RENDER_SEGMENT)) {
        trail = min((int) (speed * max(1, length / 2)), LPRandom::MAX_LENGTH - 1);
      }
      return trail;
    }
    
    uint32_t getDuration() const {
        return duration > 0 ? duration : LPRandom::randomDuration();
    }
    
    uint8_t getMaxBri() const {
        return maxBri > 0 ? maxBri : DEFAULT_BRIGHTNESS;
    }
    
    int8_t getEmit() const {
        return from >= 0 ? from : -1;
    }
    
    uint8_t getEmitGroups(uint8_t defaultValue = 0) const {
        return emitGroups > 0 ? emitGroups : defaultValue;
    }
};
