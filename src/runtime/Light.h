#ifndef PACKAGES_LIGHTGRAPH_SRC_RUNTIME_LIGHT_H_
#define PACKAGES_LIGHTGRAPH_SRC_RUNTIME_LIGHT_H_

#pragma once

#include <algorithm>

#include "RuntimeLight.h"
#include "../Globals.h"

class Light : public RuntimeLight {

  public:

    Light(LightList *parent, float speedValue, uint32_t expirationMillis, uint16_t idx = 0,
          uint8_t maxBrightness = 255);
    
    Light(uint8_t maxBrightness, float speedValue, uint32_t expirationMillis)
        : Light(nullptr, speedValue, expirationMillis, 0, maxBrightness) {
    }
    
    Light(uint8_t maxBrightness) : Light(maxBrightness, DEFAULT_SPEED, INFINITE_DURATION) {
    }
    
    Light() : Light(255) {
    }

    float getSpeed() const override {
        return speed;
    }
    void setSpeed(float speedValue) {
        speed = speedValue;
    }
    uint32_t getLife() const override {
        return lifeMillis;
    }
    void setDuration(uint32_t durMillis) override {
        lifeMillis = static_cast<uint32_t>(std::min(
            static_cast<unsigned long>(runtimeContext().nowMillis + durMillis),
            static_cast<unsigned long>(INFINITE_DURATION)));
    }
    ColorRGB getColor() const override {
        return color;
    }
    void setColor(ColorRGB colorValue) override {
      color = colorValue;
    }

    uint8_t getBrightness() const override;
    ColorRGB getPixelColorAt(int16_t pixel) const override;
    ColorRGB getPixelColor() const override;
    void nextFrame() override;
    bool shouldExpire() const override;
    
    const Model* getModel() const override;
    const Behaviour* getBehaviour() const override;

  private:

    float speed = DEFAULT_SPEED;
    ColorRGB color; // 3 bytes
    // int16_t pixel2 = -1; // 4 bytes
  
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_RUNTIME_LIGHT_H_
