#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../LPRandom.h"

struct ColorRGB {
    uint8_t R;
    uint8_t G;
    uint8_t B;

    // For compatibility with ColorUtil template interface.
    uint8_t r;
    uint8_t g;
    uint8_t b;

    ColorRGB(uint8_t r, uint8_t g, uint8_t b) : R(r), G(g), B(b), r(r), g(g), b(b) {}

    explicit ColorRGB(uint32_t rgb) {
        set(rgb);
    }

    ColorRGB() : ColorRGB(0) {}

    ColorRGB Dim(uint8_t ratio) const {
        return ColorRGB(_elementDim(R, ratio), _elementDim(G, ratio), _elementDim(B, ratio));
    }

    uint32_t get() const {
        return (static_cast<uint32_t>(R) << 16) |
               (static_cast<uint32_t>(G) << 8) |
               static_cast<uint32_t>(B);
    }

    void set(uint32_t rgb) {
        R = (rgb >> 16) & 0xFF;
        G = (rgb >> 8) & 0xFF;
        B = rgb & 0xFF;
        r = R;
        g = G;
        b = B;
    }

    void setRandom() {
        fromHSV(LPRandom::randomHue(), LPRandom::randomSaturation(), LPRandom::randomValue());
    }

    void fromHSV(uint8_t h, uint8_t s, uint8_t v) {
        const float hf = (h / 255.0f) * 360.0f;
        const float sf = s / 255.0f;
        const float vf = v / 255.0f;

        const float c = vf * sf;
        const float x = c * (1 - std::fabs(std::fmod(hf / 60.0f, 2) - 1));
        const float m = vf - c;

        float rf;
        float gf;
        float bf;

        if (hf < 60) {
            rf = c;
            gf = x;
            bf = 0;
        } else if (hf < 120) {
            rf = x;
            gf = c;
            bf = 0;
        } else if (hf < 180) {
            rf = 0;
            gf = c;
            bf = x;
        } else if (hf < 240) {
            rf = 0;
            gf = x;
            bf = c;
        } else if (hf < 300) {
            rf = x;
            gf = 0;
            bf = c;
        } else {
            rf = c;
            gf = 0;
            bf = x;
        }

        R = static_cast<uint8_t>((rf + m) * 255);
        G = static_cast<uint8_t>((gf + m) * 255);
        B = static_cast<uint8_t>((bf + m) * 255);
        r = R;
        g = G;
        b = B;
    }

    inline static uint8_t _elementDim(uint8_t value, uint8_t ratio) {
        return (static_cast<uint16_t>(value) * (static_cast<uint16_t>(ratio) + 1)) >> 8;
    }

    float getHueAngle() const {
        const float rf = R / 255.0f;
        const float gf = G / 255.0f;
        const float bf = B / 255.0f;

        const float maxValue = std::max(std::max(rf, gf), bf);
        const float minValue = std::min(std::min(rf, gf), bf);
        const float delta = maxValue - minValue;

        if (delta == 0) {
            return 0;
        }

        float hue;
        if (maxValue == rf) {
            hue = 60.0f * std::fmod(((gf - bf) / delta), 6.0f);
        } else if (maxValue == gf) {
            hue = 60.0f * (((bf - rf) / delta) + 2.0f);
        } else {
            hue = 60.0f * (((rf - gf) / delta) + 4.0f);
        }

        if (hue < 0) {
            hue += 360.0f;
        }
        return hue;
    }

    float getHue() const {
        return getHueAngle() / 360.0f * 255.0f;
    }

    float getSaturation() const {
        const float rf = R / 255.0f;
        const float gf = G / 255.0f;
        const float bf = B / 255.0f;

        const float maxValue = std::max(std::max(rf, gf), bf);
        const float minValue = std::min(std::min(rf, gf), bf);

        if (maxValue == 0) {
            return 0;
        }
        return ((maxValue - minValue) / maxValue) * 255.0f;
    }

    float getBrightness() const {
        return std::max(std::max(R, G), B);
    }

    void setHue(float hue) {
        const float s = getSaturation() / 255.0f;
        const float v = getBrightness() / 255.0f;

        float h = hue / 255.0f * 360.0f;
        if (s == 0) {
            R = G = B = static_cast<uint8_t>(v * 255);
            r = R;
            g = G;
            b = B;
            return;
        }

        h /= 60.0f;
        const int i = static_cast<int>(std::floor(h));
        const float f = h - i;
        const float p = v * (1 - s);
        const float q = v * (1 - s * f);
        const float t = v * (1 - s * (1 - f));

        switch (i) {
            case 0:
                R = static_cast<uint8_t>(v * 255);
                G = static_cast<uint8_t>(t * 255);
                B = static_cast<uint8_t>(p * 255);
                break;
            case 1:
                R = static_cast<uint8_t>(q * 255);
                G = static_cast<uint8_t>(v * 255);
                B = static_cast<uint8_t>(p * 255);
                break;
            case 2:
                R = static_cast<uint8_t>(p * 255);
                G = static_cast<uint8_t>(v * 255);
                B = static_cast<uint8_t>(t * 255);
                break;
            case 3:
                R = static_cast<uint8_t>(p * 255);
                G = static_cast<uint8_t>(q * 255);
                B = static_cast<uint8_t>(v * 255);
                break;
            case 4:
                R = static_cast<uint8_t>(t * 255);
                G = static_cast<uint8_t>(p * 255);
                B = static_cast<uint8_t>(v * 255);
                break;
            default:
                R = static_cast<uint8_t>(v * 255);
                G = static_cast<uint8_t>(p * 255);
                B = static_cast<uint8_t>(q * 255);
                break;
        }

        r = R;
        g = G;
        b = B;
    }

    void setSaturation(float saturation) {
        const float h = getHue();
        const float b = getBrightness();
        setHsb(h, saturation, b);
    }

    void setBrightness(float brightness) {
        const float h = getHue();
        const float s = getSaturation();
        setHsb(h, s, brightness);
    }

    void setHsb(float hue, float saturation, float brightness) {
        float h = hue / 255.0f * 360.0f;
        const float s = saturation / 255.0f;
        const float v = brightness / 255.0f;

        if (s == 0) {
            R = G = B = static_cast<uint8_t>(v * 255);
            r = R;
            g = G;
            b = B;
            return;
        }

        h /= 60.0f;
        const int i = static_cast<int>(std::floor(h));
        const float f = h - i;
        const float p = v * (1 - s);
        const float q = v * (1 - s * f);
        const float t = v * (1 - s * (1 - f));

        switch (i) {
            case 0:
                R = static_cast<uint8_t>(v * 255);
                G = static_cast<uint8_t>(t * 255);
                B = static_cast<uint8_t>(p * 255);
                break;
            case 1:
                R = static_cast<uint8_t>(q * 255);
                G = static_cast<uint8_t>(v * 255);
                B = static_cast<uint8_t>(p * 255);
                break;
            case 2:
                R = static_cast<uint8_t>(p * 255);
                G = static_cast<uint8_t>(v * 255);
                B = static_cast<uint8_t>(t * 255);
                break;
            case 3:
                R = static_cast<uint8_t>(p * 255);
                G = static_cast<uint8_t>(q * 255);
                B = static_cast<uint8_t>(v * 255);
                break;
            case 4:
                R = static_cast<uint8_t>(t * 255);
                G = static_cast<uint8_t>(p * 255);
                B = static_cast<uint8_t>(v * 255);
                break;
            default:
                R = static_cast<uint8_t>(v * 255);
                G = static_cast<uint8_t>(p * 255);
                B = static_cast<uint8_t>(q * 255);
                break;
        }

        r = R;
        g = G;
        b = B;
    }

    static ColorRGB fromHsb(float hue, float saturation, float brightness) {
        ColorRGB color;
        color.setHsb(hue, saturation, brightness);
        return color;
    }

    ColorRGB lerp(const ColorRGB& target, float amt) const {
        const float invAmt = 1.0f - amt;
        return ColorRGB(
            static_cast<uint8_t>(R * invAmt + target.R * amt),
            static_cast<uint8_t>(G * invAmt + target.G * amt),
            static_cast<uint8_t>(B * invAmt + target.B * amt));
    }

    static float limit() {
        return 255.0f;
    }
};

enum Groups {
    GROUP1 = 1,
    GROUP2 = 2,
    GROUP3 = 4,
    GROUP4 = 8,
    GROUP5 = 16,
    GROUP6 = 32,
    GROUP7 = 64,
    GROUP8 = 128,
};

enum BehaviourFlags {
    B_POS_CHANGE_FADE = 1,
    B_BRI_CONST_NOISE = 2,
    B_RENDER_SEGMENT = 4,
    B_ALLOW_BOUNCE = 8,
    B_FORCE_BOUNCE = 16,
    B_EXPIRE_IMMEDIATE = 32,
    B_EMIT_FROM_CONN = 64,
    B_FILL_EASE = 128,
    B_RANDOM_COLOR = 256,
    B_MIRROR_FLIP = 512,
    B_MIRROR_ROTATE = 1024,
    B_SMOOTH_CHANGES = 2048,
};

enum ListOrder {
    LIST_ORDER_SEQUENTIAL,
    LIST_ORDER_RANDOM,
    LIST_ORDER_NOISE,
    LIST_ORDER_OFFSET,
    LO_FIRST = LIST_ORDER_SEQUENTIAL,
    LO_LAST = LIST_ORDER_OFFSET,
};

enum ListHead {
    LIST_HEAD_FRONT,
    LIST_HEAD_MIDDLE,
    LIST_HEAD_BACK,
};

enum BlendMode {
    BLEND_NORMAL = 0,
    BLEND_ADD = 1,
    BLEND_MULTIPLY = 2,
    BLEND_SCREEN = 3,
    BLEND_OVERLAY = 4,
    BLEND_REPLACE = 5,
    BLEND_SUBTRACT = 6,
    BLEND_DIFFERENCE = 7,
    BLEND_EXCLUSION = 8,
    BLEND_DODGE = 9,
    BLEND_BURN = 10,
    BLEND_HARD_LIGHT = 11,
    BLEND_SOFT_LIGHT = 12,
    BLEND_LINEAR_LIGHT = 13,
    BLEND_VIVID_LIGHT = 14,
    BLEND_PIN_LIGHT = 15,
};

enum Ease {
    EASE_NONE = 0,
    EASE_LINEAR_IN = 1,
    EASE_LINEAR_OUT = 2,
    EASE_LINEAR_INOUT = 3,
    EASE_SINE_IN = 4,
    EASE_SINE_OUT = 5,
    EASE_SINE_INOUT = 6,
    EASE_CIRCULAR_IN = 7,
    EASE_CIRCULAR_OUT = 8,
    EASE_CIRCULAR_INOUT = 9,
    EASE_QUADRATIC_IN = 10,
    EASE_QUADRATIC_OUT = 11,
    EASE_QUADRATIC_INOUT = 12,
    EASE_CUBIC_IN = 13,
    EASE_CUBIC_OUT = 14,
    EASE_CUBIC_INOUT = 15,
    EASE_QUARTIC_IN = 16,
    EASE_QUARTIC_OUT = 17,
    EASE_QUARTIC_INOUT = 18,
    EASE_QUINTIC_IN = 19,
    EASE_QUINTIC_OUT = 20,
    EASE_QUINTIC_INOUT = 21,
    EASE_EXPONENTIAL_IN = 22,
    EASE_EXPONENTIAL_OUT = 23,
    EASE_EXPONENTIAL_INOUT = 24,
    EASE_BACK_IN = 25,
    EASE_BACK_OUT = 26,
    EASE_BACK_INOUT = 27,
    EASE_BOUNCE_IN = 28,
    EASE_BOUNCE_OUT = 29,
    EASE_BOUNCE_INOUT = 30,
    EASE_ELASTIC_IN = 31,
    EASE_ELASTIC_OUT = 32,
    EASE_ELASTIC_INOUT = 33,
};
