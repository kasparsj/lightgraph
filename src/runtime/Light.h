#ifndef PACKAGES_LIGHTGRAPH_SRC_RUNTIME_LIGHT_H_
#define PACKAGES_LIGHTGRAPH_SRC_RUNTIME_LIGHT_H_

#pragma once

#include <algorithm>

#include "RuntimeLight.h"
#include "../Globals.h"

class Light : public RuntimeLight {

  public:

    Light(LightList *parent, float speed, uint32_t lifeMillis, uint16_t idx = 0, uint8_t maxBri = 255);
    
    Light(uint8_t maxBri, float speed, uint32_t lifeMillis) : Light(nullptr, speed, lifeMillis, 0, maxBri) {
    }
    
    Light(uint8_t maxBri) : Light(maxBri, DEFAULT_SPEED, INFINITE_DURATION) {
    }
    
    Light() : Light(255) {
    }

    float getSpeed() const override {
        return speed;
    }
    void setSpeed(float speed) {
        this->speed = speed;
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
    void setColor(ColorRGB color) override {
      this->color = color;
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
