#pragma once

#include <cstdint>
#include <memory>

#include "objects.hpp"

/**
 * @file factory.hpp
 * @brief Convenience constructors for built-in topology objects.
 */

namespace lightpath::integration {

enum class BuiltinObjectType {
    Heptagon919,
    Heptagon3024,
    Line,
    Cross,
    Triangle,
};

/**
 * @brief Create one of the built-in topology objects.
 * @param type Built-in object kind.
 * @param pixelCount Optional pixel count override for line/cross/triangle.
 */
inline std::unique_ptr<Object> makeObject(BuiltinObjectType type, uint16_t pixelCount = 0) {
    switch (type) {
    case BuiltinObjectType::Heptagon919:
        return std::unique_ptr<Object>(new Heptagon919());
    case BuiltinObjectType::Heptagon3024:
        return std::unique_ptr<Object>(new Heptagon3024());
    case BuiltinObjectType::Line:
        return std::unique_ptr<Object>(new Line(pixelCount > 0 ? pixelCount : kLinePixelCount));
    case BuiltinObjectType::Cross:
        return std::unique_ptr<Object>(new Cross(pixelCount > 0 ? pixelCount : kCrossPixelCount));
    case BuiltinObjectType::Triangle:
        return std::unique_ptr<Object>(
            new Triangle(pixelCount > 0 ? pixelCount : kTrianglePixelCount));
    }

    return std::unique_ptr<Object>(new Line(pixelCount > 0 ? pixelCount : kLinePixelCount));
}

} // namespace lightpath::integration
