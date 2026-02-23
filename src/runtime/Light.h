#pragma once

#include <algorithm>

#include "LPLight.h"
#include "../Globals.h"

class Light : public LPLight {

  public:

    Light(LightList *parent, float speed, uint32_t lifeMillis, uint16_t idx = 0, uint8_t maxBri = 255);
    
    Light(uint8_t maxBri, float speed, uint32_t lifeMillis) : Light(0, maxBri, speed, lifeMillis, 0) {
    }
    
    Light(uint8_t maxBri) : Light(maxBri, DEFAULT_SPEED, INFINITE_DURATION) {
    }
    
    Light() : Light(255) {
    }

    float getSpeed() const {
        return speed;
    }
    void setSpeed(float speed) {
        this->speed = speed;
    }
    uint32_t getLife() const {
        return lifeMillis;
    }
    void setDuration(uint32_t durMillis) {
        lifeMillis = static_cast<uint32_t>(std::min(
            static_cast<unsigned long>(gMillis + durMillis),
            static_cast<unsigned long>(INFINITE_DURATION)));
    }
    ColorRGB getColor() const {
        return color;
    }
    void setColor(ColorRGB color) {
      this->color = color;
    }

    uint8_t getBrightness() const;
    ColorRGB getPixelColor() const;
    void nextFrame();
    bool shouldExpire() const;
    
    const Model* getModel() const;
    const Behaviour* getBehaviour() const;

  private:

    float speed = DEFAULT_SPEED;
    ColorRGB color; // 3 bytes
    // int16_t pixel2 = -1; // 4 bytes
  
};
